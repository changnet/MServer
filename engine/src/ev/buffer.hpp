#pragma once

#include "../pool/object_pool.hpp"
#include "../thread/spin_lock.hpp"

/**
 * 单个chunk
 *    +---------------------------------------------------------------+
 *    |    悬空区   |        有效数据区        |      空白区(space)   |
 *    +---------------------------------------------------------------+
 * _ctx          _beg                        _end                   _max
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
private:
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
            _next     = nullptr;
            _used_pos = _free_pos = 0;
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
            _used_pos += len;
            assert(_free_pos >= _used_pos);
        }
        /**
         * @brief 添加已使用缓冲区
         * @param len 添加的长度
        */
        inline void add_used(size_t len)
        {
            _free_pos += len;
            assert(MAX_CTX >= _free_pos);
        }

        /**
         * @brief 添加缓冲区数据
         * @param data 需要添加的数据
         * @param len 数据的长度
        */
        inline void append(const void *data, const size_t len)
        {
            memcpy(_ctx + _free_pos, data, len);
            add_used(len);
        }

        /**
         * @brief 获取已使用缓冲区数据指针
         * @return 
        */
        inline const char *get_used_ctx() const
        {
            return _ctx + _used_pos;
        }

        /**
         * @brief 获取空闲缓冲区指针
         * @return 
        */
        inline char *get_free_ctx()
        {
            return _ctx + _free_pos;
        }

        /**
         * @brief 重置整个缓冲区
        */
        inline void clear()
        {
            _used_pos = _free_pos = 0;
        }
        /**
         * @brief 获取已使用缓冲区大小
         * @return 
        */
        inline size_t get_used_size() const
        {
            return _free_pos - _used_pos;
        }

        /**
         * @brief 获取空闲缓冲区大小
         * @return 
        */
        inline size_t get_free_size() const
        {
            return MAX_CTX - _free_pos;
        }
    public:
        char _ctx[MAX_CTX]; // 缓冲区指针

        size_t _used_pos; // 已使用缓冲区开始位置
        size_t _free_pos; // 空闲缓冲区开始位置

        Chunk *_next; // 链表下一节点
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
        char *_ctx;  // 缓冲区指针
        size_t _len; // 缓冲区长度
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
    */
    void remove(size_t len);

    /**
     * @brief 添加数据到缓冲区
     * @param data 要添加的数据 
     * @param len 要添加的数据长度
    */
    void append(const void *data, const size_t len);

    ////////////////////////////////////////////////////////////////////////////
    // 这里的static缓冲区有问题，想办法放到static_global
    // 放到static_global需要额外用一个锁 ChunkPool也有同样的问题

    const char *to_continuous_ctx(size_t len);
    const char *all_to_continuous_ctx(size_t &len);

    // 只获取第一个chunk的有效数据大小，用于socket发送
    inline size_t get_used_size() const
    {
        return _front ? _front->used_size() : 0;
    }

    // 获取chunk的数量
    inline size_t get_chunk_size() const { return _chunk_size; }
    // 获取所有chunk分配的内存大小
    inline size_t get_chunk_mem_size() const
    {
        size_t mem        = 0;
        const Chunk *next = _front;

        while (next)
        {
            mem += next->_max;
            next = next->_next;
        };

        return mem;
    }

    // 只获取第一个chunk的有效数据指针，用于socket发送
    inline const char *get_used_ctx() const { return _front->used_ctx(); };

    /* 检测当前有效数据的大小是否 >= 指定值
     * 这个函数必须在确定已有数据的情况下调用，不检测next是否为空
     * TODO:用于数据包分在不同chunk的情况，这是采用这种设计缺点之一
     */
    inline bool check_used_size(size_t len) const
    {
        size_t used       = 0;
        const Chunk *next = _front;

        do
        {
            used += next->used_size();

            next = next->_next;
        } while (EXPECT_FALSE(next && used < len));

        return used >= len;
    }

    // 获取当前所有的数据长度
    inline size_t get_all_used_size() const
    {
        size_t used       = 0;
        const Chunk *next = _front;

        while (next)
        {
            used += next->used_size();

            next = next->_next;
        }

        return used;
    }

    // 获取空闲缓冲区大小，只获取一个chunk的，用于socket接收
    inline size_t get_space_size() { return _back ? _back->space_size() : 0; }
    // 获取空闲缓冲区指针，只获取一个chunk的，用于socket接收
    inline char *get_space_ctx() { return _back->space_ctx(); };
    // 增加有效数据长度，比如从socket读数据时，先拿缓冲区，然后才知道读了多少数据
    inline void add_used_offset(size_t len) { _back->add_used_offset(len); }

    // 设置缓冲区参数
    // @param maxchunk最大数量
    // @param ctx_max 默认
    void set_buffer_size(size_t max, size_t ctx_size)
    {
        _chunk_max      = max;
        _chunk_ctx_size = ctx_size;

        // 设置的chunk大小必须是等长内存池的N倍
        assert(0 == ctx_size % BUFFER_CHUNK);
    }

    // 当前缓冲区是否溢出
    inline bool is_overflow() const { return _chunk_size > _chunk_max; }

private:
    ChunkPool *get_chunk_pool();

    inline Chunk *new_chunk()
    {
        _chunk_size++;
        return get_chunk_pool()->construct();
    }

    inline void del_chunk(Chunk *chunk)
    {
        assert(_chunk_size > 0);

        _chunk_size--;
        get_chunk_pool()->destroy(chunk);
    }

    /**
     * @brief 预分配缓冲区空间
     * @param len 预分配的长度
     * @return 是否分配成功
    */
    bool reserved();

    /**
     * @brief 获取连续的缓冲区
     * @param len 缓冲区的长度
     * @return 
    */
    char *get_large_buffer(size_t len);

private:
    SpinLock _lock;  // 多线程锁
    Chunk *_front;      // 数据包链表头
    Chunk *_back;       // 数据包链表尾

    // 已申请chunk数量
    int32_t _chunk_size;
    // 该缓冲区允许申请chunk的最大数量，超过此数量视为缓冲区溢出
    int32_t _chunk_max;
};
