#pragma once

#include <mutex>

#include "pool.hpp"
#include "../thread/spin_lock.hpp"

/**
 * 等长内存池，参考了boost内存池(boolst/pool/pool.hpp).分配的内存只能是ordered_size
 * 的n倍。每一个n都形成一个空闲链表，利用率比boost低。
 * 1.分配出去的内存不再受池的管理
 * 2.所有内存在池销毁时会释放(包括未归还的)
 * 3.没有约束内存对齐。因此用的是系统默认对齐，在linux 32/64bit应该是OK的
 * 4.最小内存块不能小于一个指针长度(4/8 bytes)
 */
template <size_t ordered_size> class OrderedPool : public Pool
{
public:
    explicit OrderedPool(const char *name)
        : Pool(name), anpts(nullptr), anptmax(0), block_list(nullptr)
    {
        assert(ordered_size >= sizeof(void *));
    }

    virtual ~OrderedPool()
    {
        clear();
    }

    virtual void purge()
    {
        clear();
    }
    virtual size_t get_sizeof() const
    {
        return ordered_size;
    }

    void ordered_free(char *const ptr, size_t n)
    {
        assert(anptmax >= n && ptr);

        // 把ptr前几个字节指向anpts[n],然后设置的起点指针为ptr.
        // 相当于把ptr放到anpts[n]这个链表的首部
        nextof(ptr) = anpts[n];
        anpts[n]    = ptr;

        _max_now += n;
    }

    /**
     * 分配一块大小为n个ordered_size的内存
     * @param chunk_size 预分配的数量(因为内存大小不一，分配策略需要根据需求来定)
     */
    char *ordered_malloc(size_t n, size_t chunk_size)
    {
        assert(n > 0 && chunk_size > 0);
        if (EXPECT_FALSE(anptmax < n + 1))
        {
            size_t size = 16;
            while (size < n + 1) size *= 2;

            NODE *tmp = new NODE[size];
            memset(tmp, 0, sizeof(NODE) * size);

            if (anptmax > 0)
            {
                memcpy(tmp, anpts, sizeof(NODE) * anptmax);
                delete[] anpts;
            }
            anpts   = tmp;
            anptmax = size;
        }
        void *ptr = anpts[n];
        if (ptr)
        {
            _max_now -= n;
            anpts[n] = nextof(ptr);
            return static_cast<char *>(ptr);
        }

        /* 每次固定申请chunk_size块大小为(n*ordered_size)内存
         * 不用指数增长方式因为内存分配过大可能会失败
         */
        size_t partition_sz = n * ordered_size;
        assert(UINT_MAX / partition_sz > chunk_size);

        uint64_t block_size = sizeof(void *) + chunk_size * partition_sz;
        char *block         = new char[block_size];

        /* 分配出来的内存，预留一个指针的位置在首部，用作链表将所有从系统获取的
         * 内存串起来
         */
        nextof(block) = block_list;
        block_list    = block;

        // 统计的是最小单元的数量
        _max_new += n * chunk_size;
        _max_now += n * (chunk_size - 1);

        /* 第一块直接分配出去，其他的分成小块存到anpts对应的链接中 */
        segregate(block + sizeof(void *) + partition_sz, partition_sz,
                  chunk_size - 1, n);
        return block + sizeof(void *);
    }

private:
    /**
     * @brief 释放所有内存，包括已分配出去未归还的。慎用！
     */
    void clear()
    {
        if (anpts)
        {
            delete[] anpts;
            anpts = nullptr;
        }

        anptmax = 0;

        while (block_list)
        {
            char *_ptr = static_cast<char *>(block_list);
            block_list = nextof(block_list);

            delete[] _ptr;
        }

        block_list = nullptr;
    }

    /* 一块内存的指针是ptr,这块内存的前几个字节储存了下一块内存的指针地址
     * 即ptr可以看作是指针的指针
     * nextof返回这地址的引用
     */
    inline void *&nextof(void *const ptr)
    {
        return *(static_cast<void **>(ptr));
    }

    /* 把从系统获取的内存分成小块存到链表中
     * 这些内存块都是空的，故在首部创建一个指针，存放指向下一块空闲内存的地址
     * @ptr           内存块起始地址
     * @partition_sz  单块内存大小
     * @npartition    单块内存数量
     * @n             等长系数
     */
    inline void *segregate(void *const ptr, size_t partition_sz,
                           size_t npartition, size_t n)
    {
        assert(anptmax > n);
        /* in case ordered_malloc new only one chunk */
        if (npartition <= 0) return anpts[n];

        char *last = static_cast<char *>(ptr);
        for (size_t i = 1; i < npartition; i++)
        {
            char *next   = last + partition_sz;
            nextof(last) = next;
            last         = next;
        }

        nextof(last)    = anpts[n];
        return anpts[n] = ptr;
    }

private:
    typedef void *NODE;

    NODE *anpts; /* 空闲内存块链表数组,倍数n为下标 */
    size_t anptmax;

    void *block_list; /* 从系统分配的内存块链表 */
};

/**
 * @brief 等长内存池，同OrderedPool，但是加了SpinLock
 */
template <size_t ordered_size>
class OrderedPoolLock final : public OrderedPool<ordered_size>
{
public:
    explicit OrderedPoolLock(const char *name) : OrderedPool<ordered_size>(name)
    {
    }
    virtual ~OrderedPoolLock()
    {
    }

    virtual void purge() override
    {
        std::lock_guard<SpinLock> lg(_lock);
        OrderedPool<ordered_size>::purge();
    }
    virtual size_t get_sizeof() const override
    {
        std::lock_guard<SpinLock> lg(_lock);
        return OrderedPool<ordered_size>::get_sizeof();
    }

    void ordered_free(char *const ptr, size_t n) override
    {
        std::lock_guard<SpinLock> lg(_lock);
        OrderedPool<ordered_size>::ordered_free(ptr, n);
    }
    char *ordered_malloc(size_t n, size_t chunk_size) override
    {
        std::lock_guard<SpinLock> lg(_lock);
        return OrderedPool<ordered_size>::ordered_malloc(n, chunk_size);
    }

private:
    SpinLock _lock;
};
