#pragma once

#include "pool/flexible_pool.hpp"
#include <atomic>

/**
网络收发缓冲区

# 核心需求
    1. 使用定长内存池减少内存碎片
    2. 缓冲区使用流式而不是一个消息一个缓冲区，减少浪费
    3. 初始8kb缓冲区足够大部分游戏连接使用，极少重新分配内存
        10000个玩家在线约 10240 * 8k * 2 = 160M，这个是可以接受的消耗
    4. FlexiblePool支持变长chunk，解决偶尔收发大数据效率低的问题
    5. 以前设计过一版加锁的，网络线程和逻辑线程无法并发，改为无锁SPSC

# 无锁SPSC
SPSC即Single Producer Single Consumer，单生产者单消费者。单即意味着一个Buffer对象
只能有一个线程入，一个线程出。比如写的时候，先获取write_ptr，再把pos_加上去，这两
个步骤不是原子性，只是针对单个线程安全，如果是多个线程写入就会出错。

无锁即全部用atomic操作，为了效率，无锁SPSC严格地使用atomic的各种内存顺序，
非常容易出错，一定要小心。
Producer负责修改tail_和pos_，那它读取这两个值时就可以用relaxed
Consumer负责head_和read_offset_，读取这两个值时就可以用relaxed

但如果是Consumer要读取pos_，就要用acquire，用错了就会出bug，而且是不稳定重现的bug

最理想情况下，一次produce包含4个atomic操作(Consume也基本是这个情况)，效率存疑
    1. 获取tail_指针
    2. 获取pos_计算空间
    3. 把读取到的单个chunk.pos加上去
    4. 把总len加上去

# 流量控制
流量分为单个消息大小和总接收数据大小。单个大小和buffer没关这里不讨论。总数据接收大小
主要分两种情况：
1. 被恶意发包攻击
2. 服务器卡了处理不过来（比如断点调试）

每个缓冲区都可以设置总大小，以及达到最大值后是断开还是sleep等待。对于玩家直接断掉
就行。对于服务器，直接sleep一小段时间

# UDP扩展
缓冲区使用了流式，和udp的消息模式是相反的。我本身也没预留udp，因为原生的udp会丢包
没什么用。如果有需求考虑直接用quic或者kcp

如果实在要用udp，buffer的设计可以不变。每个udp用“长度+消息”流式填充到buffer中，
再实现对应的udp_io.cpp和udp_packet.cpp就可以。
 */

class Buffer;

/**
 * @brief 网络收发缓冲区
 */
class Buffer final
{
public:
    class Chunk final
    {
    public:
        // FlexiblePool required
        int32_t mask_;

        int64_t capacity_;          // 整个chunk的总容量
        std::atomic<int64_t> pos_;  // 已写入的数据(write pos)
        std::atomic<Chunk *> next_; // 下一个链表元素

        explicit Chunk(int64_t cap) : capacity_(cap)
        {
            mask_ = 0;
            pos_.store(0, std::memory_order_release);
            next_.store(nullptr, std::memory_order_release);
        }

        inline char *data()
        {
            // Chunk后面紧接着就是数据
            return reinterpret_cast<char *>(this + 1);
        }

        inline char *write_ptr(int64_t &size)
        {
            int64_t pos = pos_.load(std::memory_order_acquire);
            size        = capacity_ - pos;
            return data() + pos;
        }
    };

    // 单个chunk的容量，让内存池单个元素刚好对齐8k
    static constexpr int64_t CHUNK_SIZE = 8192 - sizeof(Buffer::Chunk);
    using ChunkPool                     = FlexiblePool<8192, 128, std::mutex>;

public:
    Buffer();
    ~Buffer();

    /**
     * @brief 重置当前缓冲区
     */
    void clear();

    /**
     * @brief 删除缓冲区中的数据(Consumer)
     * @param len 要删除的数据长度
     */
    void remove_head_data(int64_t len);

    /**
     * @brief 添加数据到缓冲区(Producer)
     * @param data 要添加的数据
     * @param len 要添加的数据长度
     */
    void append(const void *data, int64_t len);

    /**
     * @brief (Producer)按自定义方式添加数据到缓冲区
     * @param data 要添加的数据
     * @param len 要添加的数据长度
     * @processor 自定义数据处理函数
     */
    template <typename F>
    void append_from_processor(const char *data, int64_t len, F &&processor)
    {
        if (0 == len) return;

        int64_t left = len;

        // 因为 tail_ 只有 producer 修改，relaxed 读取即可
        // (Read-Your-Own-Writes) 发送那边只用next_，不用tail_的
        Chunk *tail = tail_.load(std::memory_order_relaxed);

        int64_t space    = 0;
        char *buffer_ptr = tail->write_ptr(space);
        if (space > 0)
        {
            int64_t write_len = std::min(left, space);
            processor(data, buffer_ptr, write_len);

            tail->pos_.fetch_add(write_len, std::memory_order_release);
            len_.fetch_add(write_len, std::memory_order_acq_rel);

            left -= write_len;
            data += write_len;
        }

        if (0 == left) return;

        // 一般游戏的数据不会大于8kb，上面的缓冲区应该能应付完
        // 假如是服务器卡了，则left应该是比较小，这里一个循环也能搞定
        // 假如是发送超大数据，<= 8个chunk则用多个chunk装，否则一次性分配一个超大chunk
        static const int64_t LARGE_SIZE = Buffer::CHUNK_SIZE * 8;
        while (left > 0)
        {
            int64_t alloc_size = left > LARGE_SIZE ? left : Buffer::CHUNK_SIZE;

            Chunk *new_chk = allocate_chunk(alloc_size);

            int64_t write_len = std::min(left, new_chk->capacity_);
            processor(data, new_chk->data(), write_len);
            new_chk->pos_.store(write_len, std::memory_order_release);

            len_.fetch_add(write_len, std::memory_order_acq_rel);

            left -= write_len;
            data += write_len;

            // 当前线程是唯一修改 tail_ 的人，relaxed 读取也是安全的
            Chunk *curr_tail = tail_.load(std::memory_order_relaxed);
            curr_tail->next_.store(new_chk, std::memory_order_release);
            tail_.store(new_chk, std::memory_order_release);
        }
    }

    /**
     * @brief (Producer)按自定义方式添加数据到缓冲区
     * @param data 要添加的数据
     * @param len 要添加的数据长度
     * @generator 自定义数据处理函数(返回的值和read一致)
     * @return > 0表示读写成功，0表示断开，-1表示出错，-2表示缓冲区满
     */
    template <typename F> int32_t append_from_generator(F &&generator)
    {
        Buffer::Chunk *tail = tail_.load(std::memory_order_relaxed);

        int64_t space  = 0;
        char *data_ptr = tail->write_ptr(space);
        if (space > 0)
        {
            int64_t len = generator(data_ptr, space);
            if (len > 0)
            {
                tail->pos_.fetch_add(len, std::memory_order_release);
                len_.fetch_add(len, std::memory_order_acq_rel);

                if (len < space) return 1;
            }
            else
            {
                return 0 == len ? 0 : -1;
            }
        }

        // 第二次开始，分配新chunk
        int64_t alloc_size = Buffer::CHUNK_SIZE;
        while (true)
        {
            if (is_overflow()) return -2; // 缓冲区已满

            Buffer::Chunk *chk = allocate_chunk(alloc_size);

            int64_t len = generator(chk->data(), chk->capacity_);
            if (len > 0)
            {
                append_chunk(chk, len);

                if (len < chk->capacity_) return 1;

                alloc_size *= 2; // 无法预知后续大小，指数增长
            }
            else
            {
                deallocate_chunk(chk);
                return 0 == len ? 0 : -1;
            }
        }
    }

    /**
     * @brief (Consumer)遍历所有数据，把各个chunk的数据传给processor处理
     * @param data 要添加的数据
     * @param len 要添加的数据长度
     * @processor 自定义数据处理函数(返回的值和write一致)
     * @return >0表示读写成功，0表示断开，-1表示出错，-2表示数据无法消耗完
     */
    template <typename F> int32_t iterate_to_processor(F &&processor)
    {
        int64_t read_offset = read_offset_;
        Chunk *head         = head_.load(std::memory_order_relaxed);
        bool term           = false;
        while (!term)
        {
            int64_t pos = head->pos_.load(std::memory_order_acquire);
            int64_t len = pos - read_offset;
            if (len > 0)
            {
                int64_t ret = processor(term, head->data() + read_offset, len);
                if (unlikely(ret <= 0)) // 未消耗任何数据
                {
                    return (int32_t)ret;
                }

                len_.fetch_sub(len, std::memory_order_acq_rel);
                if (ret < len) // 数据未消耗完
                {
                    read_offset_ = read_offset + ret;
                    return -2;
                }
                else if (pos < head->capacity_) // 数据消耗完但没有数据了
                {
                    read_offset_ = read_offset + ret;
                    return 1;
                }
            }
            else if (pos < head->capacity_)
            {
                return 1;
            }

            /*
             * 这是一个无锁SPSC，head_和tail_的操作线程安全但不具备原子性
             * Producer只能修改tail_、next、pos_，Consumer只能修改head_、read_offset_
             * 所以这里只剩下最后一个chunk时，即使数据发送完，Consumer也不能
             * 清空或者删除当前这个chunk，因为Producer可能已经在申请新的chunk了
             */
            Chunk *next = head->next_.load(std::memory_order_acquire);
            if (next)
            {
                read_offset = read_offset_ = 0;
                deallocate_chunk(head);

                head = next;
                head_.store(head, std::memory_order_release);
            }
            else
            {
                read_offset_ = pos;
                assert(pos == head->capacity_);
                return 1;
            }

            read_offset = 0;
        }
        return 1;
    }

    /**
     * @brief 获取所有有效数据长度
     */
    inline int64_t length() const
    {
        return len_.load(std::memory_order_acquire);
    }

    /**
     * @brief 设置缓冲区最大容量
     */
    inline void set_max_size(int64_t max)
    {
        max_ = max;
    }

    /**
     * @brief 是否溢出
     */
    inline bool is_overflow() const
    {
        // 是否溢出不需要很精准，relaxed即可
        return len_.load(std::memory_order_relaxed) > max_;
    }

    /**
     * @brief 检测当前缓冲区中是否存在>=sizeof(T)的数据，如果存在则复制到对象中
     */
    template <typename T> bool peek(T *t)
    {
        // T应该是比较小的一个类型才行，保证最多只分布在2个chunk。否则应该用peek_buffer
        static_assert(sizeof(T) <= CHUNK_SIZE);

        Chunk *head = head_.load(std::memory_order_relaxed);
        int64_t pos = head->pos_.load(std::memory_order_acquire);

        int64_t data_len = pos - read_offset_;
        if (data_len >= (int64_t)sizeof(T)) // 大部分应该都会在第一个chunk中
        {
            memcpy(t, head->data() + read_offset_, sizeof(T));
            return true;
        }

        // 极少情况，数据分布在不同chunk中（第一个chunk空间可能为空）
        Chunk *next = head->next_.load(std::memory_order_acquire);
        if (!next) return false;

        int64_t next_len = next->pos_.load(std::memory_order_acquire);
        if (next_len + data_len < (int64_t)sizeof(T)) return false;

        if (data_len > 0) memcpy(t, head->data() + read_offset_, data_len);
        memcpy(((char *)t) + data_len, head->data(), sizeof(T) - data_len);

        return true;
    }

    /**
     * @brief 检测当前缓冲区中是否存在>=size的数据
     */
    bool peek_size(int64_t size)
    {
        return len_.load(std::memory_order_acquire) >= size;
    }

    /**
     * @brief 从当前缓冲区中获取一段大小为size的数据
     * @param rwflag 1读，2写，一个线程读写可能会同时存在
     */
    char *peek_buffer(int64_t size, int32_t rwflag);

private:
    /**
     * @brief 申请一个trunk，但不会添加到列表
     * @param alloc_size chunk的容量大小（不需要包含chunk本身）
     */
    Chunk *allocate_chunk(int64_t alloc_size);

    /**
     * @brief 释放一个trunk
     */
    void deallocate_chunk(Chunk *chunk);

    /**
     * @brief 直接添加一个已填充数据的chunk(Producer)
     */
    void append_chunk(Chunk *chunk, int64_t len);

private:
    std::atomic<Chunk *> head_; // 由Consumer修改
    std::atomic<Chunk *> tail_; // 由Producer修改

    int64_t read_offset_; // 消费者在head_中的读取偏移，仅Consumer会读取

    std::atomic<int64_t> len_; // 当前总数据量

    int64_t max_; // 允许的最大数据，在初始化完后不会变
};
