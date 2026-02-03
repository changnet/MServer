#include "buffer.hpp"
#include "system/static_global.hpp"

struct ThreadBuffer
{
    int64_t len_;
    char *buffer_;

    ThreadBuffer()
    {
        len_    = 0;
        buffer_ = nullptr;
    }
    ~ThreadBuffer()
    {
        delete[] buffer_;
    }
};

// @param rwflag 1读，2写，一个线程读写可能会同时存在
static char *get_thread_buffer(int64_t size, int32_t rwflag)
{
    thread_local ThreadBuffer tb[2];

    // 最小256kb，保证系统用mmap分配内存不会造成碎片
    // 256kb足够应付绝大多数情况，偶尔发送大数据时，后续要释放重新分配
    // 不然发个50M的数据，一台机子开20个服，200多线程，内存就直接没了

    static constexpr int64_t min_size = 256 * 1024;
    static constexpr int64_t max_size = 1024 * 1024;

    ThreadBuffer &b = 1 == rwflag ? tb[0] : tb[1];
    if (b.len_ < size || (size < max_size && b.len_ > max_size))
    {
        delete[] b.buffer_;
        b.len_    = size > min_size ? size : min_size;
        b.buffer_ = new char[b.len_];
    }
    return b.buffer_;
}

////////////////////////////////////////////////////////////////////////////////

Buffer::Buffer()
{
    // 预分配一个chunk，确保 head_、tail_ 永远有效
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
    // 保留最后一个chunk，避免 remove_head_data 修改 tail_
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

void Buffer::append(const void *data, int64_t len)
{
    const char *char_data = reinterpret_cast<const char *>(data);
    append_from_processor(char_data, len,
                          [](const char *wdata, char *wptr, int64_t space)
                          { memcpy(wptr, wdata, space); });
}

void Buffer::append_chunk(Chunk *chunk, int64_t len)
{
    assert(len > 0);
    chunk->pos_.store(len, std::memory_order_release);

    Chunk *curr_tail = tail_.load(std::memory_order_relaxed);
    curr_tail->next_.store(chunk, std::memory_order_release);
    tail_.store(chunk, std::memory_order_release);

    // 先设置好next_，再加上len_，避免消费者检查len_有数据，但却取不到数据
    len_.fetch_add(len, std::memory_order_acq_rel);
}

Buffer::Chunk *Buffer::allocate_chunk(int64_t alloc_size)
{
    int64_t cap = (int64_t)StaticGlobal::C->capacity<Buffer::Chunk>(alloc_size);
    return StaticGlobal::C->construct<Buffer::Chunk>(alloc_size, cap);
}

void Buffer::deallocate_chunk(Chunk *chunk)
{
    StaticGlobal::C->destruct(chunk);
}

char *Buffer::peek_buffer(int64_t size, int32_t rwflag)
{
    Chunk *head = head_.load(std::memory_order_relaxed);
    int64_t pos = head->pos_.load(std::memory_order_acquire);

    int64_t data_len = pos - read_offset_;

    // 异常情况检查：read_offset_ 不应该超过 pos
    assert(data_len >= 0);

    if (data_len >= size) return head->data() + read_offset_;

    // 在多个chunk中，需要拷贝到同一段连续的缓冲区
    char *buffer = get_thread_buffer(size, rwflag);

    char *wptr = buffer;
    if (data_len > 0)
    {
        memcpy(wptr, head->data() + read_offset_, data_len);
        size -= data_len;
        wptr += data_len;
    }

    while (size > 0)
    {
        Chunk *next = head->next_.load(std::memory_order_acquire);
        if (!next) return nullptr;

        int64_t pos = next->pos_.load(std::memory_order_acquire);
        // 只复制需要的数据量，避免越界写入
        int64_t copy_len = pos > size ? size : pos;
        memcpy(wptr, next->data(), copy_len);

        size -= copy_len;
        wptr += copy_len;
        head = next;
    }

    return 0 == size ? buffer : nullptr;
}

void Buffer::remove_head_data(int64_t len)
{
    if (0 == len) return;

    iterate_to_processor(
        [&len](bool &term, const char *rptr, int64_t size)
        {
            if (size >= len)
            {
                term = true;
                size = len;
            }
            len -= size;
            return size;
        });

    assert(0 == len);
}
