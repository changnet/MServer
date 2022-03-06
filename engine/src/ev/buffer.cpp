#include "buffer.hpp"
#include "../system/static_global.hpp"

// 临时连续缓冲区
static const size_t continuous_size         = MAX_PACKET_LEN;
static char continuous_ctx[continuous_size] = {0};

////////////////////////////////////////////////////////////////////////////////
Buffer::LargeBuffer::LargeBuffer()
{
    _ctx = nullptr;
    _len = 0;
}

Buffer::LargeBuffer::~LargeBuffer()
{
    if (_ctx) delete[] _ctx;
}

char Buffer::LargeBuffer::get(size_t len)
{
    if (_len >= len) return _ctx;

    if (_ctx) delete[] _ctx;

    // 以1M为基数，每次翻倍，只增不减
    if (0 == _len) _len = 1024 * 1024;
    while (_len < len) _len *= 2;

    _ctx = new char[_len];

    return _ctx;
}
////////////////////////////////////////////////////////////////////////////////

Buffer::Buffer()
{
    _chunk_size = 0;
    _chunk_max = 1;
    _front = _back = nullptr;
}

Buffer::~Buffer()
{
    while (_front)
    {
        Chunk *tmp = _front;
        _front     = _front->_next;

        del_chunk(tmp);
    }

    _front = _back = nullptr;
}

bool Buffer::reserved()
{
    if (EXPECT_FALSE(!_back || 0 == _back->get_free_size()))
    {
        Chunk *tmp = new_chunk();
        if (_back)
        {
            _back = _back->_next = new_chunk();
        }
        else
        {
            _back = _front = tmp;
        }
    }

    return true;
}

char **Buffer::get_large_buffer(size_t len)
{
    // 原本想在LargeBuffer里加个锁，所有线程共用缓冲区
    // 但缓冲区通常要持有一段时间，这导致这个锁非常难管理，需要手动加锁解锁
    // 那干脆用thread_local，多耗点内存，但不用考虑锁的问题
    thread_local LargeBuffer buffer;

    return buffer.get(len);
}

ChunkPool *Buffer::get_chunk_pool()
{
    return StaticGlobal::buffer_chunk_pool();
}

void Buffer::clear()
{
    std::lock_guard lg(_lock);

    if (!_front) return;

    while (_front->_next)
    {
        Chunk *tmp = _front;
        _front     = _front->_next;

        del_chunk(tmp);
    }

    // 默认保留一个缓冲区 TODO: 是否有必要保留??
    _front->clear();
    assert(_back == _front);
}

void Buffer::append(const void *data, const size_t len)
{
    const char *data = reinterpret_cast<const char *>(data);
    size_t append_sz = 0;

    std::lock_guard lg(_lock);
    do
    {
        if (!reserved())
        {
            FATAL("buffer append reserved fail");
            return;
        }

        size_t free_sz = _back->get_free_size();

        size_t size = std::min(free_sz, len - append_sz);
        _back->append(data + append_sz, size);

        append_sz += size;

        // 大多数情况下，一次应该可以添加完数据
        // 如果不能，考虑调整单个chunk的大小，否则影响效率
    } while (EXPECT_FALSE(append_sz < len));
}

void Buffer::remove(size_t len)
{
    std::lock_guard lg(_lock);
    do
    {
        size_t used = _front->get_used_size();

        if (used > len)
        {
            // 这个chunk还有其他数据
            _front->remove(len);
            break;
        }
        else if (used == len)
        {
            // 这个chunk只剩下这个数据包
            Chunk *next = _front->_next;
            if (next)
            {
                // 还有下一个chunk，则指向下一个chunk
                del_chunk(_front);
                _front = next;
            }
            else
            {
                _front->clear(); // 在无数据的时候重重置缓冲区
            }
            break;
        }
        else
        {
            // 这个数据包分布在多个chunk，一个个删
            Chunk *tmp = _front;

            _front = _front->_next;
            assert(_front && len > used);

            len -= used;
            del_chunk(tmp);
        }
    } while (len > 0);

    // 冗余校验：只有一个缓冲区，或者有多个缓冲区但第一个已用完
    // 否则就是链表管理出错了
    assert(_front == _back || 0 == _front->get_free_size());
}

/* 检测指定长度的有效数据是否在连续内存，不在的话要合并成连续内存
 * protobuf这些都要求内存在连续缓冲区才能解析
 * TODO:这是采用这种设计缺点之二
 */
const char *Buffer::to_continuous_ctx(size_t len)
{
    // 大多数情况下，是在同一个chunk的，如果不是，调整下chunk的大小，否则影响效率
    if (EXPECT_TRUE(_front->used_size() >= len))
    {
        return _front->used_ctx();
    }

    // 前期用来检测二次拷贝出现的情况，确认没问题这个可以去掉
    PLOG("using continuous buffer:%d", len);

    size_t used       = 0;
    const Chunk *next = _front;

    do
    {
        size_t next_used = std::min(len - used, next->used_size());
        assert(used + next_used <= continuous_size);

        memcpy(continuous_ctx + used, next->used_ctx(), next_used);

        used += next_used;
        next = next->_next;
    } while (next && used < len);

    assert(used == len);
    return continuous_ctx;
}

// 把所有数据放到一块连续缓冲区中
const char *Buffer::all_to_continuous_ctx(size_t &len)
{
    // 没有执行reserver就调用这个函数就会出现这种情况
    // 在websocket那边，可以只发ctrl包，不包含数据就不会预分配内存
    // if (EXPECT_FALSE(!_front))
    // {
    //     len = 0;
    //     return nullptr;
    // }
    if (EXPECT_TRUE(!_front->_next))
    {
        len = _front->used_size();
        return _front->used_ctx();
    }

    size_t used       = 0;
    const Chunk *next = _front;

    do
    {
        size_t next_used = next->used_size();
        assert(used + next_used <= continuous_size);

        memcpy(continuous_ctx + used, next->used_ctx(), next_used);

        used += next_used;
        next = next->_next;
    } while (next);

    len = used;
    assert(used > 0);

    // 前期用来检测二次拷贝出现的情况，确认没问题这个可以去掉
    PLOG("using all continuous buffer:%d", len);

    return continuous_ctx;
}
