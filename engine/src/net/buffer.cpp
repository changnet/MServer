#include "buffer.hpp"
#include "system/static_global.hpp"

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

    int32_t space = tail->space();
    if (space > 0)
    {
        int32_t write_len = std::min((int32_t)left, space);
        memcpy(tail->write_ptr(), ptr, write_len);

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
        memcpy(new_chk->write_ptr(), ptr, write_len);
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

void Buffer::remove(size_t len)
{
    if (0 == len) return;

    size_t remove_len = 0;
    Chunk *head       = head_.load(std::memory_order_acquire);

    while (head && remove_len < len)
    {
        int32_t chunk_len = head->pos_.load(std::memory_order_acquire);
        size_t used       = chunk_len - read_offset_;

        if (used > (len - remove_len))
        {
            // 当前chunk还有剩余数据
            read_offset_ += (len - remove_len);
            remove_len = len;
            break;
        }

        // 当前chunk数据已用完
        remove_len += used;

        // 检查是否有下一个chunk
        Chunk *next = head->next_.load(std::memory_order_acquire);
        if (next)
        {
            // 有下一个，删除当前 head，移动到 next
            read_offset_ = 0;
            StaticGlobal::C->destruct(head);

            head = next;
            head_.store(head, std::memory_order_release);
        }
        else
        {
            // 没有下一个，说明 head == tail，这是最后一个 chunk
            // 即便数据读完了也不删除，作为存根，保证 tail_ 有效
            // 只重置读取偏移
            // 注意：这里并不重置 pos_，因为 producer 可能还在往里写（并发）
            // 如果 producer 还在写，pos_ 会增加，我们下次还能读
            // 但如果 producer 写满了，分配了 next，我们下次循环会进 if(next) 分支

            // 为了简单，我们只更新 read_offset_
            // 等同于这个 chunk 变成了“空闲空间都在前面”的状态
            // 但我们的 append 只往后追加。所以这个 chunk 废了吗？
            // 只要 append 不复用前面的空间，这个 chunk 就只是占据内存。
            // OK, 我们的 append 逻辑是只往 tail 追加。
            // 当 tail 满了，producer 会 new chunk。
            // 所以，如果 head 满了(read_offset == capacity)，且 next 为空：
            // Producer 稍后会 new next。Consumer 下次会看到 next。
            read_offset_ += (len - remove_len); // 此时 remove_len 应该等于 len?
            // used = chunk_len - read_offset; remove_len += used; ==>
            // remove_len consumes all available used. Wait, logic: `used` is
            // amount available. `len` is amount req. If `used <= len -
            // remove_len`: we consume all used. So `read_offset_` becomes
            // `chunk_len`.
            read_offset_ = chunk_len;
            break;
        }
    }

    len_.fetch_sub(remove_len, std::memory_order_relaxed);
}

const char *Buffer::get_front(size_t &size)
{
    Chunk *head = head_.load(std::memory_order_relaxed);

    while (head)
    {
        int32_t chunk_len = head->pos_.load(std::memory_order_acquire);
        int32_t used      = chunk_len - (int32_t)read_offset_;

        if (used > 0)
        {
            size = (size_t)used;
            return head->data() + read_offset_;
        }

        // 当前chunk无数据，检查是否有下一个
        Chunk *next = head->next_.load(std::memory_order_acquire);
        if (next)
        {
            // 有下一个，删除当前耗尽的 head
            StaticGlobal::C->destruct(head);

            head = next;
            head_.store(head, std::memory_order_relaxed);
            read_offset_ = 0;

            // 继续循环检查新的 head
        }
        else
        {
            // 没有下一个，真的没数据了
            break;
        }
    }

    size = 0;
    return nullptr;
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
