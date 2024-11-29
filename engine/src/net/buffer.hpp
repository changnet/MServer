#pragma once

#include "pool/object_pool.hpp"
#include "thread/spin_lock.hpp"

/**
 * 单个chunk
 *    +---------------------------------------------------------------+
 *    |    悬空区   |        有效数据区        |      空白区(space)   |
 *    +---------------------------------------------------------------+
 * ctx_          beg_                        end_                   max_
 *
 */

/**
 * 网络收发缓冲区
 *
 * 1. 游戏的数据包一般是小包，因此这里都是按小包来设计和优化的
 *    频繁发送大包(缓冲区超过8k)会导致链表操作，有一定的效率损失
 * 
 * 2. 每条连接默认分配一个8k的收缓冲区和一个8k的发缓冲区。
 *    数据量超过时，后续的包以链表形式链在一起
 *    10240条链接，收最大64k，发64k，那最差的情况是1280M内存
 * 
 * 3. 有时候为了优化拷贝，会直接从缓冲区中预分配一块内存，把收发的数据直接写入
 *    缓冲区。这时候预分配的缓冲区必须是连续的，但缓冲区可能只能以链表提供
 *    这时候只能临时分配一块内存，再拷贝到缓冲区，效率不高
 */

/**
 * @brief 网络收发缓冲区
*/
class Buffer final
{
public:
    /**
     * @brief 单个缓冲区块，多个块以链表形式组成一个完整的缓冲区
    */
    class Chunk final
    {
    public:
        static const size_t MAX_CTX = 8192; //8k
    public:
        Chunk()
        {
            ctx_[0]   = 0; // avoid C26495 warning
            next_     = nullptr;
            used_pos_ = free_pos_ = 0;
        }
        ~Chunk()
        {
        }

        /**
         * @brief 移除已使用缓冲区
         * @param len 移除的长度
         */
        inline void remove_used(size_t len)
        {
            used_pos_ += len;
            assert(free_pos_ >= used_pos_);
        }
        /**
         * @brief 添加已使用缓冲区
         * @param len 添加的长度
        */
        inline void add_used(size_t len)
        {
            free_pos_ += len;
            assert(MAX_CTX >= free_pos_);
        }

        /**
         * @brief 添加缓冲区数据
         * @param data 需要添加的数据
         * @param len 数据的长度
        */
        inline void append(const void *data, const size_t len)
        {
            memcpy(ctx_ + free_pos_, data, len);
            add_used(len);
        }

        /**
         * @brief 获取已使用缓冲区数据指针
         * @return 
        */
        inline const char *get_used_ctx() const
        {
            return ctx_ + used_pos_;
        }

        /**
         * @brief 获取空闲缓冲区指针
         * @return 
        */
        inline char *get_free_ctx()
        {
            return ctx_ + free_pos_;
        }

        /**
         * @brief 重置整个缓冲区
        */
        inline void clear()
        {
            used_pos_ = free_pos_ = 0;
        }
        /**
         * @brief 获取已使用缓冲区大小
         * @return 
        */
        inline size_t get_used_size() const
        {
            return free_pos_ - used_pos_;
        }

        /**
         * @brief 获取空闲缓冲区大小
         * @return 
        */
        inline size_t get_free_size() const
        {
            return MAX_CTX - free_pos_;
        }
    public:
        char ctx_[MAX_CTX]; // 缓冲区指针

        size_t used_pos_; // 已使用缓冲区开始位置
        size_t free_pos_; // 空闲缓冲区开始位置

        Chunk *next_; // 链表下一节点
    };

    /**
     * @brief 一大块连续的缓冲区
    */
    class LargeBuffer final
    {
    public:
        LargeBuffer();
        ~LargeBuffer();

        /**
         * @brief 获取指定长度的连续缓冲区
         * @param len 缓冲区的长度
         * @return 缓冲区指针
        */
        char *get(size_t len);

    private:
        char *ctx_;  // 缓冲区指针
        size_t len_; // 缓冲区长度
    };

    /**
     * @brief 对buff执行多步操作的事件结构，可自动解锁。is movable, but not copyable 
     */
    class Transaction final
    {
    public:
        explicit Transaction(SpinLock &lock) : ul_(lock)
        {
            len_ = 0;
            ctx_ = 0;
            internal_ = false;
        }
        Transaction(Transaction &&t) noexcept : ul_(std::move(t.ul_))
        {
            len_ = t.len_;
            ctx_ = t.ctx_;
            internal_ = t.internal_;

            t.len_ = 0;
            t.ctx_ = 0;
            t.internal_ = false;
        }
        Transaction &operator=(Transaction &&t) noexcept
        {
            len_ = t.len_;
            ctx_ = t.ctx_;
            internal_ = t.internal_;
            ul_  = std::move(t.ul_);

            t.len_ = 0;
            t.ctx_ = 0;
            t.internal_ = false;
            // t.ul_不用处理，ul_  = std::move(ul);里已经做了处理
            return *this;
        }

        ~Transaction() = default;

        Transaction() = delete;
        Transaction(const Transaction &) = delete;
        Transaction &operator=(const Transaction &) = delete;

    public:
        bool internal_; // 是否使用内部buff
        char *ctx_;     // 预留的缓冲区指针
        int32_t len_;   // 缓冲区的长度

    private:
        std::unique_lock<SpinLock> ul_;
    };

    /// 小块缓冲区对象池
    using ChunkPool = ObjectPoolLock<Chunk, 1024, 64>;
public:
    Buffer();
    ~Buffer();

    /**
     * @brief 重置当前缓冲区
    */
    void clear();

    /**
     * @brief 删除缓冲区中的数据
     * @param len 要删除的数据长度
     * @return 是否还有数据需要处理
    */
    bool remove(size_t len);

    /**
     * @brief 添加数据到缓冲区
     * @param data 要添加的数据 
     * @param len 要添加的数据长度
     * @return 0成功， 1溢出
    */
    int32_t append(const void *data, const size_t len);

    /**
     * @brief 预分配任意空间
     * @param no_overflow 为true表示如果当前分配空间超出则不再分配
     * @return 一个包括锁和缓冲区指针等数据的事务对象
    */
    Transaction any_seserve(bool no_overflow);

    /**
     * @brief 预分配一块连续(不包含多个chunk)的缓冲区
     * @param len 预分配的长度
     * @return 一个包括锁和缓冲区指针等数据的事务对象
    */
    Transaction flat_reserve(size_t len);

    /**
     * @brief 把指定长度的缓存放到连续的缓冲区
     * @param len 缓存的长度
     * @return 缓冲区的指针，如果长度不足则返回nullptr
    */
    const char *to_flat_ctx(size_t len);

    /**
     * @brief 检测当前有效数据的大小是否 >= 指定值
     * @param len 检测的长度
     * @return bool 当前有效数据的大小是否 >= 指定值
    */
    bool check_used_size(size_t len) const;

     /**
      * @brief 把缓冲区中所有的buff都存放到一块连续的缓冲区
      * @param len 缓冲区的数据长度
      * @return 连续缓冲区的指针
     */
     const char *all_to_flat_ctx(size_t &len);

     /**
      * @brief 提交flat_reserve预分配的数据
      * @param len 最终提交数据长度
      * @param ts 提交的缓存事务对象
     */
     void commit(const Transaction &ts, int32_t len);

    /**
      * @brief 获取第一个chunk的数据指针及数据大小
      * @param size 第一个chunk的数据大小
      * @param next 是否还有下一个数据块
      * @return 第一个chunk的数据指针
      */
     const char *get_front_used(size_t &size, bool &next) const;

    /**
      * @brief 只获取第一个chunk的有效数据大小
      * @return
      */
     inline size_t get_front_used_size() const
     {
         std::lock_guard<SpinLock> guard(lock_);
         return front_ ? front_->get_used_size() : 0;
     }

    /**
      * @brief 获取chunk的数量
      * @return
      */
     inline size_t get_chunk_size() const
     {
         std::lock_guard<SpinLock> guard(lock_);
         return chunk_size_;
     }

     /**
      * @brief 获取所有已分配chunk的大小，用于统计
      * @return
      */
     inline size_t get_chunk_mem_size() const
     {
         std::lock_guard<SpinLock> guard(lock_);
         return chunk_size_ * sizeof(Chunk);
     }

    /**
      * @brief 获取当前所有的chunk数据长度
      * @return
      */
     size_t get_all_used_size() const;

    /**
      * @brief 设置chunk的最大数量，超过此数量视为溢出
      * @param max 允许的chunk最大数量
      */
     void set_chunk_size(int32_t max);

    // 当前缓冲区是否溢出
     inline bool is_overflow() const
     {
         std::lock_guard<SpinLock> guard(lock_);
         return chunk_size_ > chunk_max_;
     }

private:
    ChunkPool *get_chunk_pool();

    inline Chunk *new_chunk()
    {
        chunk_size_++;
        return get_chunk_pool()->construct();
    }

    inline void del_chunk(Chunk *chunk)
    {
        assert(chunk_size_ > 0);

        chunk_size_--;
        get_chunk_pool()->destroy(chunk);
    }

    /**
     * @brief 预分配缓冲区空间，如果当前空间为空则分配一个新的chunk
     * @return 返回当前可用缓冲区大小
    */
    size_t reserve();

    /**
     * @brief 获取连续的缓冲区
     * @param len 缓冲区的长度
     * @return 
    */
    char *get_large_buffer(size_t len);

    /**
     * @brief 同append，但不加锁，仅内部使用
     * @param data 要添加的数据
     * @param len 要添加的数据长度
    */
    void __append(const void *data, const size_t len);

private:
    mutable SpinLock lock_;  // 多线程锁
    Chunk *front_;      // 数据包链表头
    Chunk *back_;       // 数据包链表尾

    // 已申请chunk数量
    int32_t chunk_size_;
    // 该缓冲区允许申请chunk的最大数量，超过此数量视为缓冲区溢出
    int32_t chunk_max_;
};
