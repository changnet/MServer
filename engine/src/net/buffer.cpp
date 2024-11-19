#include "buffer.hpp"
#include "system/static_global.hpp"


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

char *Buffer::LargeBuffer::get(size_t len)
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

size_t Buffer::reserve()
{
    size_t len = 0;
    if (unlikely(!_back || 0 == (len = _back->get_free_size())))
    {
        Chunk *tmp = new_chunk();
        if (_back)
        {
            _back->_next = tmp;
            _back        = tmp;
        }
        else
        {
            _back = _front = tmp;
        }
        len = tmp->get_free_size();
    }

    return len;
}

char *Buffer::get_large_buffer(size_t len)
{
    // 原本想在LargeBuffer里加个锁，所有线程共用缓冲区
    // 但缓冲区通常要持有一段时间，这导致这个锁非常难管理，需要手动加锁解锁
    // 那干脆用thread_local，多耗点内存，但不用考虑锁的问题
    thread_local LargeBuffer buffer;

    return buffer.get(len);
}

Buffer::ChunkPool *Buffer::get_chunk_pool()
{
    return StaticGlobal::buffer_chunk_pool();
}

void Buffer::clear()
{
    std::lock_guard<SpinLock> guard(_lock);

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

void Buffer::__append(const void *data, const size_t len)
{
    // 已经追加的数据大小
    size_t append_sz = 0;
    const char *append_data = reinterpret_cast<const char *>(data);

    do
    {
        size_t free_sz = reserve();

        size_t size = std::min(free_sz, len - append_sz);
        // void *类型不能和数字运算，要转成char *
        _back->append(append_data + append_sz, size);

        append_sz += size;

        // 大多数情况下，一次应该可以添加完数据
        // 如果不能，考虑调整单个chunk的大小，否则影响效率
    } while (unlikely(append_sz < len));
}

int32_t Buffer::append(const void *data, const size_t len)
{
    std::lock_guard<SpinLock> guard(_lock);
    __append(data, len);

    return _chunk_size > _chunk_max ? 1 : 0;
}

bool Buffer::remove(size_t len)
{
    bool next_used = false;
    std::lock_guard<SpinLock> guard(_lock);
    do
    {
        size_t used = _front->get_used_size();

        if (used > len)
        {
            // 这个chunk还有其他数据
            next_used = true;
            _front->remove_used(len);
            break;
        }
        else if (used == len)
        {
            // 这个chunk只剩下这个数据包
            Chunk *next = _front->_next;
            if (next)
            {
                // 还有下一个chunk，则指向下一个chunk
                next_used = true;
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

    return next_used;
}

Buffer::Transaction Buffer::any_seserve(bool no_overflow)
{
    Transaction ts(_lock);

    if (likely(!no_overflow || _chunk_size <= _chunk_max))
    {
        ts._internal = true;
        ts._len = (int)reserve();
        ts._ctx = _back->get_free_ctx();
    }

    return ts;
}

Buffer::Transaction Buffer::flat_reserve(size_t len)
{
    /**
     * 打包数据时(例如protobuf、websocket)需要预先分配一块连续的缓冲区
     * 但是buffer连续的缓冲区可能不够大。方案一是由打包那边自己分配再
     * append到buffer，这样的拷贝消耗比较大。方案二是当连续缓冲区不够
     * 大时，buffer额外申请一块临时内存，再append到，这样大部分情况下
     * 都不需要拷贝
     */

    Transaction ts(_lock);

    size_t free_size = reserve();
    if (free_size >= len)
    {
        ts._internal = true;
        ts._len      = (int)reserve();
        ts._ctx      = _back->get_free_ctx();
    }
    else
    {
        // 这里用了外部缓存，是可以解锁的。不过提升不了多少
        // 如果这里解锁commit那里要改一下，保证只锁一次
        // ts._ul.unlock()
        ts._len = static_cast<int32_t>(len);
        ts._ctx = get_large_buffer(len);
    }

    return ts;
}

const char *Buffer::to_flat_ctx(size_t len)
{
    std::lock_guard<SpinLock> guard(_lock);

    // 解析数据时，要求在同一个chunk才能解析。大多数情况下，是在同一个chunk的。
    // 如果不是，建议调整下chunk定义的缓冲区大小，否则影响效率
    size_t used = _front->get_used_size();
    if (0 == used) return nullptr;

    if (likely(used >= len))
    {
        return _front->get_used_ctx();
    }

    /**
     * 这个函数可能会调用很频繁。例如，判断包是否完整时需要一个16位长度
     * 假如前8位在一个chunk，后8位在另一个chunk，那每次判断包是否完整都
     * 会产生一次memcpy，好在这种情况拷贝的数据比较少，效率还可以接受
     */

    size_t copy_len   = 0; // 已拷贝长度
    const Chunk *next = _front;
    char *buf         = get_large_buffer(len);
    do
    {
        // 本次要拷贝的长度
        size_t size = std::min(len - copy_len, next->get_used_size());

        memcpy(buf + copy_len, next->get_used_ctx(), size);

        copy_len += size;
        next = next->_next;
    } while (next && copy_len < len);

    assert(copy_len <= len);

    // 用来检测二次拷贝出现的频率，确认没问题这个可以去掉
    PLOG("using continuous buffer:%d ok = %d", len, copy_len);

    // 返回nullptr表示当前的数据小于该长度
    return copy_len == len ? buf : nullptr;
}

const char *Buffer::all_to_flat_ctx(size_t &len)
{
    len = 0;
    std::lock_guard<SpinLock> guard(_lock);

    // 从未写入过数据，就不会分配_front
    // websocket的控制帧不需要写入数据就会尝试获取有没有收到数据
    if (unlikely(!_front)) return nullptr;

    if (likely(!_front->_next))
    {
        len = _front->get_used_size();
        return _front->get_used_ctx();
    }

    size_t copy_len       = 0;
    const Chunk *next = _front;

    // 用get_all_used_size效率有点底，这里粗略估计下大小即可
    size_t max_ctx = _chunk_size * Chunk::MAX_CTX;
    char *buf = get_large_buffer(max_ctx);
    do
    {
        size_t used_size = next->get_used_size();

        assert(copy_len + used_size <= max_ctx);
        memcpy(buf + copy_len, next->get_used_ctx(), used_size);

        copy_len += used_size;
        next = next->_next;
    } while (next);

    len = copy_len;
    assert(copy_len > 0);

    // 用来检测二次拷贝出现的频率，确认没问题这个可以去掉
    PLOG("using all continuous buffer:%d", len);

    return buf;
}

void Buffer::commit(const Transaction &ts, int32_t len)
{
    // 事务自带锁，这里不用处理锁

    // len来自read等函数，可能为0，可能为负
    if (len <= 0) return;

    if (ts._internal)
    {
        assert(ts._ctx == _back->get_free_ctx());

        _back->add_used(len);
    }
    else
    {
        assert(ts._ctx == get_large_buffer(0));

        __append(ts._ctx, len);
    }
}

bool Buffer::check_used_size(size_t len) const
{
    std::lock_guard<SpinLock> guard(_lock);

    size_t used       = 0;
    const Chunk *next = _front;

    while (next && used < len)
    {
        used += next->get_used_size();

        next = next->_next;
    };

    return used >= len;
}

const char *Buffer::get_front_used(size_t &size, bool &next) const
{
    std::lock_guard<SpinLock> guard(_lock);
    if (!_front)
    {
        size = 0;
        next = false;
        return nullptr;
    }

    next = _chunk_size > 1 ? true : false;
    size = _front->get_used_size();
    return _front->get_used_ctx();
}

size_t Buffer::get_all_used_size() const
{
    std::lock_guard<SpinLock> guard(_lock);
    size_t used       = 0;
    const Chunk *next = _front;

    while (next)
    {
        used += next->get_used_size();

        next = next->_next;
    }

    return used;
}

void Buffer::set_chunk_size(int32_t max)
{
    std::lock_guard<SpinLock> guard(_lock);
    _chunk_max = max;
}
