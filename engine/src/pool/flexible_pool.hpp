#pragma once

#include <mutex>
#include <cstddef> // std::max_align_t
#include "global/global.hpp"

struct FlexibleBuffer
{
    int32_t mask_; // 各种标记，1表示内存通过new分配
    int32_t size_; // 缓冲区大小
    // 获取缓冲区指针
    char *buffer() noexcept
    {
        // C++ 不支持Flexible Array Member，直接强转.C++ 20可用std::span
        return reinterpret_cast<char *>(this + 1);
    }
};

// 分配的buffer类型
enum FlexibleType
{
    MALLOC = 1, // 分配而来的内存
    POOL   = 2, // 从内存池分配的内存
};

/**
 * @brief 分配固定大小的buffer池，以减少内存碎片。请求的内存小于flexible_size，
 * 则按flexible_size分配，大于则采用new分配
 * @param flexible_size 单个buffer最小值，会自动对齐到alignof(std::max_align_t)
 * @param alloc_once 单个分配的buffer数量
 * @param Mutex 默认加锁，不加锁请用NullMutex
 */
template <size_t flexible_size, size_t alloc_once, typename Mutex = std::mutex>
class FlexiblePool
{
public:
    FlexiblePool()
    {
        // 单个chunk的大小，这个值必须大于new使用mmap的阈值（一般是128kb），
        // 避免分配的内存造成碎片
        static_assert(CHUNK_SIZE >= 128 * 1024);
    }
    ~FlexiblePool()
    {
        clear();
    }
    FlexiblePool(const FlexiblePool &)            = delete;
    FlexiblePool &operator=(const FlexiblePool &) = delete;

    /**
     * @brief Clears all allocated memory
     */
    void clear()
    {
        std::lock_guard<Mutex> lock(mutex_);
        for (char *chunk : chunks_)
        {
            delete[] chunk;
        }
        chunks_.clear();
        buffers_.clear();
    }
    /**
     * @param size 需要构建的缓冲区大小（不包含结构体本身）
     */
    template <typename T, typename... Args>
    T *construct(size_t size, Args &&...args)
    {
        T *ptr;
        uint16_t mask = size > 0 ? POOL : 0;
        if (size + sizeof(T) > SIZE)
        {
            ptr = new T(std::forward<Args>(args)...);
            mask |= MALLOC;
        }
        else
        {
            char *storage = pop();
            ptr           = new (storage) T(std::forward<Args>(args)...);
        }
        ptr->mask_ = mask;
        return ptr;
    }

    /**
     * @brief 释放对象
     */
    template <typename T> void destruct(T *ptr)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            ptr->~T();
        }

        if (ptr->mask_ & MALLOC)
        {
            delete ptr;
        }
        else
        {
            push(reinterpret_cast<char *>(ptr));
        }
    }

    // 释放不用的内存
    void gc()
    {
        std::lock_guard<Mutex> lg(mutex_);
        // <= 1个chunk不进行gc
        size_t buffer_size = buffers_.size();
        if (buffer_size <= CHUNK_SIZE / SIZE) return;

        // 对buffer进行排序，找出完全不用的chunk，进行释放
        std::sort(buffers_.begin(), buffers_.end(), std::less<char *>());

        for (int32_t i = (int32_t)chunks_.size() - 1; i >= 0; i--)
        {
            char *chunk = chunks_.at(i);
            auto b      = buffers_.begin();
            auto e      = buffers_.end();
            // lower_bound使用二分查找算法
            auto it = std::lower_bound(b, e, chunk);
            if (it != e && *it == chunk)
            {
                size_t ii        = it - b;
                size_t end_idx   = ii + CHUNK_SIZE / SIZE;
                char *end_buffer = chunk + CHUNK_SIZE - SIZE;
                if (end_idx <= buffer_size
                    && buffers_.at(end_idx - 1) == end_buffer)
                {
                    auto f = b + ii;
                    auto l = b + end_idx;

                    delete[] chunk;
                    buffers_.erase(f, l);
                    chunks_.erase(chunks_.begin() + i);
                }
            }
        }
    }

private:
    char *pop()
    {
        std::lock_guard<Mutex> lg(mutex_);
        if (buffers_.empty())
        {
            // 用mmap分配一块大内存，再拆成小内存，避免太多小内存形成碎片
            char *chunk = new char[CHUNK_SIZE];
            chunks_.push_back(chunk);

            for (size_t i = 0; i < CHUNK_SIZE / SIZE; i++)
            {
                buffers_.push_back(chunk + SIZE * i);
            }
        }

        char *buffer = buffers_.back();
        buffers_.pop_back();

        return buffer;
    }
    void push(char *ptr)
    {
        std::lock_guard<Mutex> lg(mutex_);
        buffers_.push_back(ptr);
    }

    /**
     * @brief 1024大小，应该能应对游戏中90%的前端协议包和rpc调用。这个值必须是
     * alignof(std::max_align_t)的倍数，以保证对齐问题。
     */
    static constexpr size_t ALIGNMENT = alignof(std::max_align_t);
    static constexpr size_t SIZE =
        ((flexible_size + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT;
    /**
     * 单个chunk的大小，这个值必须大于new使用mmap的阈值（一般是128kb），避免分配
     * 的内存造成碎片。
     */
    static constexpr size_t CHUNK_SIZE = alloc_once * SIZE;
    Mutex mutex_;
    std::vector<char *> chunks_;
    std::vector<char *> buffers_;
};
