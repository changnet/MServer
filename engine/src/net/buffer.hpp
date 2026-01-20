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
    5. 以前设计过一般加锁的，网络线程和逻辑线程无法并发，改为无锁SPSC

# 无锁SPSC
SPSC即Single Producer Single Consumer，单生产者单消费者，无锁即全部用atomic操作

最理想情况下，一次读包含5个atomic操作
    1. 获取tail_指针
    2. 计算space
    3. 获取data指针
    4. 把读取到的单个chunk.pos加上去
    5. 把总len加上去
如果一次读不完，那就需要读多次。写入也基本是这个情况。

为了效率，无锁SPSC严格地使用atomic的各种内存顺序，非常容易出错，一定要小心。比如
tail_只有Producer会修改，Consumer用的是next_来消费数据，所以tail_的读写是用relaxed。

而数据总长度只用来做流量控制，能容忍不精确，所以也用的relaxed。而其他的像pos_和next_
则必须用acquire和release。

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

        int32_t capacity_;          // 整个chunk的总容量
        std::atomic<int32_t> pos_;  // 已写入的数据(write pos)
        std::atomic<Chunk *> next_; // 下一个链表元素

        explicit Chunk(int32_t cap) : capacity_(cap)
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

        inline char *write_ptr()
        {
            return data() + pos_.load(std::memory_order_relaxed);
        }

        inline int32_t space() const
        {
            return capacity_ - pos_.load(std::memory_order_relaxed);
        }
    };

    // 单个chunk的容量，让内存池单个元素刚好对齐8k
    static constexpr size_t CHUNK_SIZE = 8192 - sizeof(Buffer::Chunk);
    using ChunkPool                    = FlexiblePool<8192, 128, std::mutex>;

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
    void remove(size_t len);

    /**
     * @brief 添加数据到缓冲区(Producer)
     * @param data 要添加的数据
     * @param len 要添加的数据长度
     */
    void append(const void *data, size_t len);

    /**
     * @brief 直接添加一个已填充数据的chunk(Producer)
     */
    void append_chunk(Chunk *chunk, int32_t len);

    /**
     * @brief 申请一个trunk，但不会添加到列表
     * @param alloc_size chunk的容量大小（不需要包含chunk本身）
     */
    Chunk *allocate_chunk(size_t alloc_size);

    /**
     * @brief 释放一个trunk
     */
    void deallocate_chunk(Chunk *chunk);

    /**
     * @brief 获取连续的数数据用于发送/读取(Consumer)
     * @param size 输出可用的数据长度
     * @return 数据指针
     */
    const char *get_front(size_t &size);

    /**
     * @brief 获取所有有效数据长度
     */
    inline size_t get_used_size() const
    {
        return len_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 设置缓冲区最大容量
     */
    inline void set_max_size(size_t max)
    {
        max_ = max;
    }

    /**
     * @brief 是否溢出
     */
    inline bool is_overflow() const
    {
        return len_.load(std::memory_order_relaxed) > max_;
    }

    /**
     * @brief 获取尾部chunk用于写入(Producer)
     * @return 可能为nullptr
     */
    Chunk *get_back()
    {
        return tail_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取头部chunk(Consumer)
     */
    Chunk *get_front()
    {
        return head_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 增加有效数据长度(用于外部直接写入chunk后更新)
     */
    inline void add_used(Chunk *chunk, size_t len)
    {
        chunk->pos_.fetch_add(len, std::memory_order_release);
        len_.fetch_add(len, std::memory_order_relaxed);
    }

    /**
     * @brief 把缓冲区中所有的buff都存放到一块连续的缓冲区
     *        注意：会分配新内存，使用完需要delete[]
     */
    const char *all_to_flat_ctx(size_t &len);

private:
    std::atomic<Chunk *> head_;
    std::atomic<Chunk *> tail_;

    // 消费者在head_中的读取偏移
    size_t read_offset_;

    // 当前总数据量
    std::atomic<size_t> len_;
    // 最大限制
    size_t max_;
};
