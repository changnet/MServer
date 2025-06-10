#pragma once

#include <mutex>
#include "global/global.hpp"

struct FlexibleBuffer
{
    int32_t mask_; // 各种标记，1表示内存通过new分配
    int32_t size_; // 缓冲区大小
    // 获取缓冲区指针
    char* buffer() noexcept
    {
        // C++ 不支持Flexible Array Member，直接强转.C++ 20可用std::span
        return reinterpret_cast<char *>(this + 1);
    }
};

/**
 * @brief 任意大小的buffer池，用于线程内通信等。
 * 池只缓冲大小为1024的buffer，小于此大小会被修正为1024，大于1024会采用new分配。
 */
class FlexiblePool
{
public:
    enum
    {
        MALLOC = 1, // 分配而来的内存
        POOL   = 2, // buffer池中的内存
    };

public:
    FlexiblePool() = default;
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
        std::lock_guard<std::mutex> lock(mutex_);
        for (char *chunk : chunks_)
        {
            delete[] chunk;
        }
        chunks_.clear();
        buffers_.clear();
    }
    /**
     * @param size 需要构建的缓冲区大小（不包含结构体本身）
     * @param T 类似FlexibleBuffer，包含
     */
    template <typename T, typename... Args> T *construct(size_t size, Args &&...args)
    {
        if (size + sizeof(T) > SIZE)
        {
            T *ptr = new T(std::forward<Args>(args)...);
            ptr->mask_ |= MALLOC;
            return ptr;
        }
        else
        {
            char *storage = pop();
            T *ptr        = new(storage) T(std::forward<Args>(args)...);
            ptr->mask_ |= POOL;
            return ptr;
        }
    }

    /**
     * @brief 释放对象
     */
    template<typename T> void destruct(T* ptr)
    {
        ptr->~T();
        if (0 == (ptr->mask_ & POOL))
        {
            delete[] reinterpret_cast<char *>(ptr);
            return;
        }
        push(reinterpret_cast<char *>(ptr));
    }

    // 释放不用的内存
    void gc()
    {
        std::lock_guard<std::mutex> lg(mutex_);
        // <= 1个chunk不进行gc
        size_t buffer_size = buffers_.size();
        if (buffer_size <= CHUNK_SIZE / SIZE) return;

        // 对buffer进行排序，找出完全不用的chunk，进行释放
        std::sort(buffers_.begin(), buffers_.end(), std::less<char *>());

        for (size_t i = chunks_.size() - 1; i >= 0; i--)
        {
            char *chunk = chunks_.at(i);
            for (size_t ii = 0; ii < buffer_size; ii++)
            {
                if (buffers_.at(ii) != chunk) continue;

                size_t end_idx   = ii + CHUNK_SIZE / SIZE;
                char *end_buffer = chunk + CHUNK_SIZE - SIZE;
                if (end_idx <= buffer_size
                    && buffers_.at(end_idx - 1) == end_buffer)
                {
                    auto f = buffers_.begin() + ii;
                    auto l = buffers_.begin() + end_idx;

                    buffers_.erase(f, l);
                    chunks_.erase(chunks_.begin() + i);
                }
                break;
            }
        }
    }

private:
    char* pop()
    {
        std::lock_guard<std::mutex> lg(mutex_);
        if (buffers_.empty())
        {
            // 用mmap分配一块大内存，再拆成小内存，避免太多小内存形成碎片
            char *chunk = new char[CHUNK_SIZE];
            chunks_.push_back(chunk);

            for (int32_t i = 0; i < CHUNK_SIZE / SIZE; i++)
            {
                buffers_.push_back(chunk + SIZE * i);
            }
        }

        char *buffer = buffers_.back();
        buffers_.pop_back();

        return buffer;
    }
    void push(char* ptr)
    {
        std::lock_guard<std::mutex> lg(mutex_);
        buffers_.push_back(ptr);
    }

    /**
     * @brief 1024大小，应该能应对游戏中90%的前端协议包和rpc调用。这个值必须是
     * alignof(std::max_align_t)的倍数，以保证对齐问题。
     */
    static constexpr size_t SIZE = 1024;
    /**
     * 单个chunk的大小，这个值必须大于new使用mmap的阈值（一般是128kb），避免分配
     * 的内存造成碎片。
     */
    static constexpr size_t CHUNK_SIZE = 256 * 1024;
    std::mutex mutex_;
    std::vector<char *> chunks_;
    std::vector<char *> buffers_;
};
