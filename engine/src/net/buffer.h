#pragma once

#include "../pool/object_pool.h"
#include "../pool/ordered_pool.h"

/* 单个chunk
 *    +---------------------------------------------------------------+
 *    |    悬空区   |        有效数据区        |      空白区(space)       |
 *    +---------------------------------------------------------------+
 * _buff          _beg                    _end
 *
 *收发缓冲区，要考虑游戏场景中的几个特殊情况：
 * 1.游戏的通信包一般都很小，reserved出现的概率很小。偶尔出现，memcpy的效率也是可以接受的
 * 2.缓冲区可能出现边读取边接收，边发送边写入的情况。因此，当前面的协议未读取完，又接收到新的
 *   协议，或者前面的数据未发送完，又写入新的数据，会造成前面一段缓冲区悬空，没法利用。旧框架
 *   的办法是每读取完或发送完一轮数据包，就用memmove把后面的数据往前面移动，这在粘包频繁出现
 *   并且包不完整的情况下效率很低(进程间的socket粘包很严重)。因此，我们忽略前面的悬空缓冲区，
 *   直到我们需要调整内存时，才用memmove移动内存。
 */

/* 网络收发缓冲区
 *
 * 1. 连续缓冲区
 *    整个缓冲区只用一块内存，不够了按2倍重新分配，把旧数据拷贝到新缓冲区。分配了就不再释放
 *    1). 缓冲区是连续的，存取效率高。socket读写可以直接用缓冲区，不需要二次拷贝,
 *        不需要二次读写。websocket、bson等打包数据时，可以直接用缓冲区，无需二次拷贝.
 *    2). 发生迁移拷贝时效率不高，缓冲区在极限情况下可达1G。
 *        比如断点调试，线上服务器出现过因为数据库阻塞用了1G多内存的
 *    3). 分配了不释放，内存利用率不高。当链接很多时，内存占用很高
 * 2. 小包链表设计
 *    每个包一个小块，用链表管理
 *    1). 不用考虑合并包
 *    2). 当包长无法预估时，一样需要重新分配，发生拷贝
 *        有些项目直接全部分配最大包，超过包大小则由上层逻辑分包，但这样包利用率较低
 *    3). 收包时需要额外处理包不在同连续内存块的情况，protobuf这些要求同一个数据包的buff是
 *        连续的才能解析
 *    4). socket读写时需要多次读写。因为可读、可写的大小不确定，而包比较小，可能读写不完
 * 3. 中包链表设计
 *    预分配一个比较大的包(比如服务器连接为8M)，数据依次添加到这个包。
 *    正常情况下，这个数据包无需扩展。超过这个包大小，会继续分配更多的包到链表上
 *   1). 数组包基本是连续的，利用率较高
 *   2). 包可以预分配比较大(超过协议最大长度)，很多情况下是可以当作连续缓冲区用的
 *   3). socket读写时不需要多次读写。因为包已经很大了，一次读写不太可能超过包大小
 *       epoll为ET模式也需要多次读写，但这里现在用LT
 *   4). 需要处理超过包大小时，合并、拆分的情况
 */

class Buffer
{
private:
    class Chunk
    {
    public:
        char *_ctx;    /* 缓冲区指针 */
        uint32_t _max; /* 缓冲区总大小 */

        uint32_t _beg; /* 有效数据开始位置 */
        uint32_t _end; /* 有效数据结束位置 */

        Chunk *_next; /* 链表下一节点 */

        inline void remove(uint32_t len)
        {
            _beg += len;
            ASSERT(_end >= _beg, "chunk remove corruption");
        }
        inline void add_used_offset(uint32_t len)
        {
            _end += len;
            ASSERT(_max >= _end, "chunk append corruption");
        }
        inline void append(const void *data, const uint32_t len)
        {
            memcpy(_ctx + _end, data, len);
            add_used_offset(len);
        }

        // 有效数据指针
        inline const char *used_ctx() const { return _ctx + _beg; }
        inline char *space_ctx() { return _ctx + _end; } // 空闲缓冲区指针

        inline void clear() { _beg = _end = 0; } // 重置有效数据
        inline uint32_t used_size() const
        {
            return _end - _beg;
        } // 有效数据大小
        inline uint32_t space_size() const
        {
            return _max - _end;
        } // 空闲缓冲区大小
    };

    typedef OrderedPool<BUFFER_CHUNK> ctx_pool_t;
    typedef ObjectPool<Chunk, 1024, 64> chunk_pool_t;

public:
    Buffer();
    ~Buffer();

    void clear();
    void remove(uint32_t len);
    void append(const void *data, const uint32_t len);

    const char *to_continuous_ctx(uint32_t len);
    const char *all_to_continuous_ctx(uint32_t &len);

    // 只获取第一个chunk的有效数据大小，用于socket发送
    inline uint32_t get_used_size() const
    {
        return _front ? _front->used_size() : 0;
    }

    // 获取chunk的数量
    inline uint32_t get_chunk_size() const { return _chunk_size; }
    // 获取所有chunk分配的内存大小
    inline uint32_t get_chunk_mem_size() const
    {
        uint32_t mem      = 0;
        const Chunk *next = _front;

        while (next)
        {
            mem += next->_max;
            ;
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
    inline bool check_used_size(uint32_t len) const
    {
        uint32_t used     = 0;
        const Chunk *next = _front;

        do
        {
            used += next->used_size();

            next = next->_next;
        } while (EXPECT_FALSE(next && used < len));

        return used >= len;
    }

    // 获取当前所有的数据长度
    inline uint32_t get_all_used_size() const
    {
        uint32_t used     = 0;
        const Chunk *next = _front;

        while (next)
        {
            used += next->used_size();

            next = next->_next;
        }

        return used;
    }

    // 获取空闲缓冲区大小，只获取一个chunk的，用于socket接收
    inline uint32_t get_space_size() { return _back ? _back->space_size() : 0; }
    // 获取空闲缓冲区指针，只获取一个chunk的，用于socket接收
    inline char *get_space_ctx() { return _back->space_ctx(); };
    // 增加有效数据长度，比如从socket读数据时，先拿缓冲区，然后才知道读了多少数据
    inline void add_used_offset(uint32_t len) { _back->add_used_offset(len); }

    /* 预分配一块连续的空闲缓冲区，大小不能超过单个chunk
     * @len:len为0表示不需要确定预分配
     * 注意当len不为0时而当前chunk空间不足，会直接申请下一个chunk，数据包并不是连续的
     */
    inline bool __attribute__((warn_unused_result)) reserved(uint32_t len = 0)
    {
        // 正常情况下不会分配这么大，但防止websocket时别人恶意传长度
        if (EXPECT_FALSE(len > BUFFER_CHUNK * 10)) return false;

        if (EXPECT_FALSE(!_front))
        {
            _back = _front = new_chunk(len);
            return true;
        }

        uint32_t space = _back->space_size();
        if (0 == space || len > space)
        {
            _back = _back->_next = new_chunk(len);
            // 不允许前面有一个空的chunk
            if (0 == _front->used_size())
            {
                ASSERT(_back == _front->_next, "buffer link corruption");

                del_chunk(_front);
                _front = _back;
            }
        }

        return true;
    }

    // 设置缓冲区参数
    // @max:chunk最大数量
    // @ctx_max:默认
    void set_buffer_size(uint32_t max, uint32_t ctx_size)
    {
        _chunk_max      = max;
        _chunk_ctx_size = ctx_size;

        // 设置的chunk大小必须是等长内存池的N倍
        ASSERT(0 == ctx_size % BUFFER_CHUNK, "illegal buffer chunk size");
    }

    // 当前缓冲区是否超了设定值
    inline bool is_overflow() const { return _chunk_size > _chunk_max; }

private:
    inline Chunk *new_chunk(uint32_t ctx_size = 0)
    {
        chunk_pool_t *pool = get_chunk_pool();

        Chunk *chunk = pool->construct();
        memset(chunk, 0, sizeof(Chunk));

        chunk->_ctx = new_ctx(ctx_size);
        chunk->_max = ctx_size;

        _chunk_size++;
        return chunk;
    }

    inline void del_chunk(Chunk *chunk)
    {
        ASSERT(_chunk_size > 0, "chunk size corruption");

        _chunk_size--;
        del_ctx(chunk->_ctx, chunk->_max);

        chunk_pool_t *pool = get_chunk_pool();
        pool->destroy(chunk);
    }

    // @size:要分配的缓冲区大小，会被修正为最终分配的大小
    inline char *new_ctx(uint32_t &size)
    {
        // 如果分配太小，则修正为指定socket的大小。避免reserved分配太小的内存块
        uint32_t new_size = MATH_MAX(size, _chunk_ctx_size);

        // 大小只能是BUFFER_CHUNK的N倍，如果不是则修正
        if (0 != new_size % BUFFER_CHUNK)
        {
            new_size = (new_size / BUFFER_CHUNK + 1) * BUFFER_CHUNK;
        }

        ASSERT(new_size > 0, "buffer chunk ctx size error");

        size = new_size;

        /* 分配的内存块太大每次将只分配一块，其他分配8块 */
        ctx_pool_t *pool = get_ctx_pool();
        return pool->ordered_malloc(new_size / BUFFER_CHUNK,
                                    new_size >= BUFFER_LARGE ? 1 : 8);
    }

    inline void del_ctx(char *ctx, uint32_t size)
    {
        ASSERT(size > 0 && (0 == size % BUFFER_CHUNK),
               "illegal buffer chunk ctx size error");

        ctx_pool_t *pool = get_ctx_pool();
        pool->ordered_free(ctx, size / BUFFER_CHUNK);
    }

private:
    // 采用局部static，这样就不会影响static_global中的内存统计
    chunk_pool_t *get_chunk_pool()
    {
        static chunk_pool_t chunk_pool("buffer_chunk");
        return &chunk_pool;
    }
    ctx_pool_t *get_ctx_pool()
    {
        static ctx_pool_t ctx_pool("buffer_ctx");
        return &ctx_pool;
    }

private:
    Chunk *_front;        // 数据包链表头
    Chunk *_back;         // 数据包链表尾
    uint32_t _chunk_size; // 已申请chunk数量

    uint32_t _chunk_max;      // 允许申请chunk的最大数量
    uint32_t _chunk_ctx_size; // 单个chunk的缓冲区大小
};
