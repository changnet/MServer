#include "buffer.hpp"

// 临时连续缓冲区
static const size_t continuous_size         = MAX_PACKET_LEN;
static char continuous_ctx[continuous_size] = {0};

Buffer::Buffer()
{
    _chunk_size = 0; // 已申请chunk数量

    _chunk_max = 1; // 允许申请chunk的最大数量
    // 默认单个chunk的缓冲区大小
    _chunk_ctx_size = BUFFER_CHUNK;

    _front = _back = NULL;
}

Buffer::~Buffer()
{
    while (_front)
    {
        Chunk *tmp = _front;
        _front     = _front->_next;

        del_chunk(tmp);
    }

    _front = _back = NULL;
}

// 清空所有内容
// 删除多余的chunk只保留一个
void Buffer::clear()
{
    if (!_front) return;

    while (_front->_next)
    {
        Chunk *tmp = _front;
        _front     = _front->_next;

        del_chunk(tmp);
    }

    _front->clear();
    assert(_back == _front);
}

// 添加数据
void Buffer::append(const void *raw_data, const size_t len)
{
    const char *data = reinterpret_cast<const char *>(raw_data);
    size_t append_sz = 0;
    do
    {
        if (!reserved())
        {
            FATAL("buffer append reserved fail");
            return;
        }

        size_t space = _back->space_size();

        size_t size = std::min(space, len - append_sz);
        _back->append(data + append_sz, size);

        append_sz += size;

        // 大多数情况下，一次应该可以添加完数据
        // 如果不能，考虑调整单个chunk的大小，否则影响效率
    } while (EXPECT_FALSE(append_sz < len));
}

// 删除数据
void Buffer::remove(size_t len)
{
    do
    {
        size_t used = _front->used_size();

        // 这个chunk还有其他数据
        if (used > len)
        {
            _front->remove(len);
            break;
        }

        // 这个chunk只剩下这个数据包
        if (used == len)
        {
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

        // 这个数据包分布在多个chunk，一个个删
        Chunk *tmp = _front;

        _front = _front->_next;
        assert(_front && len > used);

        len -= used;
        del_chunk(tmp);
    } while (len > 0);
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
