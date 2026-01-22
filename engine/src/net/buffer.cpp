#include "buffer.hpp"
#include "system/static_global.hpp"

struct ThreadBuffer
{
    size_t len_;
    char *buffer_;

    ThreadBuffer()
    {
        len_ = 0;
        buffer_ = nullptr;
    }
    ~ThreadBuffer()
    {
        delete[] buffer_;
    }
};

// @param rwflag 1读，2写，一个线程读写可能会同时存在
static char* get_thread_buffer(size_t size, int32_t rwflag)
{
    thread_local ThreadBuffer tb[2];

    // 最小256kb，保证系统用mmap分配内存不会造成碎片
    // 256kb足够应付绝大多数情况，偶尔发送大数据时，后续要释放重新分配
    // 不然发个50M的数据，一台机子开20个服，200多线程，内存就直接没了

    static constexpr size_t min_size = 256 * 1024;
    static constexpr size_t max_size = 1024 * 1024;

    ThreadBuffer &b = 1 == rwflag ? tb[0] : tb[1];
    if (b.len_ < size || (size < max_size && b.len_ > max_size))
    {
        delete[] b.buffer_;
        b.len_ = size > min_size ? size : min_size;
        b.buffer_ = new char[b.len_];
    }
    return b.buffer_;
}

////////////////////////////////////////////////////////////////////////////////

Buffer::Buffer()
{
    // 预分配一个chunk，确保 tail_ 永远有效，且只由 Producer 修改
    Chunk *chunk = allocate_chunk(CHUNK_SIZE);
    head_.store(chunk, std::memory_order_relaxed);
    tail_.store(chunk, std::memory_order_relaxed);

    len_.store(0, std::memory_order_relaxed);
    max_         = 64 * 1024;
    read_offset_ = 0;
}

Buffer::~Buffer()
{
    clear();
    // clear保留了一个，最后析构里彻底删除
    Chunk *chunk = head_.load(std::memory_order_relaxed);
    if (chunk)
    {
        StaticGlobal::C->destruct(chunk);
    }
}

void Buffer::clear()
{
    Chunk *chunk = head_.load(std::memory_order_relaxed);
    // 保留最后一个chunk，避免 remove 修改 tail_
    while (chunk)
    {
        Chunk *next = chunk->next_.load(std::memory_order_relaxed);
        if (!next) break; // 最后一个不删

        StaticGlobal::C->destruct(chunk);
        chunk = next;
    }

    // 重置状态
    if (chunk)
    {
        chunk->pos_.store(0, std::memory_order_relaxed);
        chunk->next_.store(nullptr, std::memory_order_relaxed);

        head_.store(chunk, std::memory_order_relaxed);
        tail_.store(chunk, std::memory_order_relaxed);
    }

    len_.store(0, std::memory_order_relaxed);
    read_offset_ = 0;
}

void Buffer::append(const void *data, size_t len)
{
    if (0 == len) return;

    const char *ptr = reinterpret_cast<const char *>(data);
    size_t left     = len;

    // 因为 tail_ 只有 producer 修改，relaxed 读取即可 (Read-Your-Own-Writes)
    // 发送那边只用next_，不用tail_的
    Chunk *tail = tail_.load(std::memory_order_relaxed);

    int32_t space = 0;
    char *buffer_ptr = tail->write_ptr(space);
    if (space > 0)
    {
        int32_t write_len = std::min((int32_t)left, space);
        memcpy(buffer_ptr, ptr, write_len);

        tail->pos_.fetch_add(write_len, std::memory_order_release);
        len_.fetch_add(write_len, std::memory_order_relaxed);

        left -= write_len;
        ptr += write_len;
    }

    if (0 == left) return;

    // 一般游戏的数据不会大于8kb，上面的缓冲区应该能应付完
    // 假如是服务器卡了，则left应该是比较小，这里一个循环也能搞定
    // 假如是发送超大数据，<= 8个chunk则用多个chunk装，否则一次性分配一个超大chunk
    static const size_t LARGE_SIZE = Buffer::CHUNK_SIZE * 8;
    while (left > 0)
    {
        size_t alloc_size = left > LARGE_SIZE ? left : Buffer::CHUNK_SIZE;

        Chunk *new_chk = allocate_chunk(alloc_size);

        int32_t write_len = std::min((int32_t)left, new_chk->capacity_);
        memcpy(new_chk->data(), ptr, write_len);
        new_chk->pos_.store(write_len, std::memory_order_release);

        len_.fetch_add(write_len, std::memory_order_relaxed);
        left -= write_len;
        ptr += write_len;

        // 当前线程是唯一修改 tail_ 的人，relaxed 读取也是安全的
        Chunk *curr_tail = tail_.load(std::memory_order_relaxed);
        curr_tail->next_.store(new_chk, std::memory_order_release);
        tail_.store(new_chk, std::memory_order_release);
    }
}

void Buffer::append_chunk(Chunk *chunk, int32_t len)
{
    assert(len > 0);
    chunk->pos_.store(len, std::memory_order_release);

    len_.fetch_add(len, std::memory_order_relaxed);

    Chunk *curr_tail = tail_.load(std::memory_order_relaxed);
    curr_tail->next_.store(chunk, std::memory_order_release);
    tail_.store(chunk, std::memory_order_release);
}

Buffer::Chunk *Buffer::allocate_chunk(size_t alloc_size)
{
    int32_t cap = StaticGlobal::C->capacity<Buffer::Chunk>(alloc_size);
    return StaticGlobal::C->construct<Buffer::Chunk>(alloc_size, cap);
}

void Buffer::deallocate_chunk(Chunk *chunk)
{
    StaticGlobal::C->destruct(chunk);
}

bool Buffer::peek_size(size_t size)
{
    // 不能用len_，len_是用做流量控制，并不精确
    // 后面看看要不要改，大部分情况应该最多检测两个chunk

    Chunk *head = head_.load(std::memory_order_acquire);
    int32_t pos = head->pos_.load(std::memory_order_acquire);

    size -= pos - read_offset_;
    while (size > 0)
    {
        Chunk *next = head->next_.load(std::memory_order_acquire);
        if (!next) return false;

        size -= head->pos_.load(std::memory_order_acquire);
        head = next;
    }

    return size <= 0;
}

char *Buffer::peek_buffer(size_t size, int32_t rwflag)
{
    Chunk *head = head_.load(std::memory_order_acquire);
    int32_t pos = head->pos_.load(std::memory_order_acquire);

    int32_t data_len = pos - read_offset_;
    if (data_len >= size) return head->data() + read_offset_;

    // 在多个chunk中，需要拷贝到同一段连续的缓冲区
    char *buffer = get_thread_buffer(size, rwflag);

    char *wptr = buffer;
    if (data_len > 0)
    {
        memcpy(wptr, head->data() + read_offset_, data_len);
        wptr += data_len;
    }
    size -= data_len;
    while (size > 0)
    {
        Chunk *next = head->next_.load(std::memory_order_acquire);
        if (!next) return nullptr;

        int32_t pos = next->pos_.load(std::memory_order_acquire);
        memcpy(wptr, next->data(), pos);

        size -= pos;
        wptr += pos;
        head = next;
    }

    return 0 == size ? buffer : nullptr;
}

void Buffer::remove(size_t len)
{
    if (0 == len) return;

    size_t left_len = len;
    Chunk *head     = head_.load(std::memory_order_acquire);

    while (left_len > 0)
    {
        int32_t pos = head->pos_.load(std::memory_order_acquire);
        // read_offset_在下面切换head时会重置，总是匹配当前head的
        size_t used       = pos - read_offset_;

        if (used > left_len)
        {
            // 当前chunk还有剩余数据
            read_offset_ += left_len;
            left_len = 0;
            break;
        }

        // 当前chunk数据已用完
        left_len -= used;

        // 检查是否有下一个chunk
        Chunk *next = head->next_.load(std::memory_order_acquire);
        if (next)
        {
            // 有下一个，删除当前 head，移动到 next
            read_offset_ = 0;
            deallocate_chunk(head);

            head = next;
            head_.store(head, std::memory_order_release);
        }
        else
        {
            // 没有下一个，说明 head == tail，这是最后一个 chunk
            // 即便数据读完了也不删除，保证 tail_ 有效
            // 注意：如果这个chunk还有空间，这里并不重置 pos_，因为 producer 可能还在往里写（并发）
            if (pos >= head->capacity_)
            {
                read_offset_ = 0;
                head->pos_.store(0, std::memory_order_release);
            }
            else
            {
                read_offset_ = pos;
            }
            break;
        }
    }

    assert(0 == left_len);
    len_.fetch_sub(len, std::memory_order_relaxed);
}

const char *Buffer::get_front_data(size_t &size)
{
    Chunk *head = head_.load(std::memory_order_relaxed);

    int32_t pos = head->pos_.load(std::memory_order_acquire);
    size        = (size_t)pos - read_offset_;

    return head->data() + read_offset_;
}

const char *Buffer::all_to_flat_ctx(size_t &len)
{
    // 这个函数会分配新内存，谨慎使用
    len = len_.load(std::memory_order_relaxed);
    if (0 == len) return nullptr;

    char *buf     = new char[len];
    size_t copied = 0;

    Chunk *curr     = head_.load(std::memory_order_acquire);
    size_t r_offset = read_offset_; // 第一个chunk的偏移

    while (curr && copied < len)
    {
        int32_t chunk_len = curr->pos_.load(std::memory_order_acquire);
        size_t used       = chunk_len - r_offset;

        memcpy(buf + copied, curr->data() + r_offset, used);

        copied += used;
        r_offset = 0; // 后续chunk从0开始

        curr = curr->next_.load(std::memory_order_acquire);
    }

    return buf;
}
