#pragma once

/* 对象内存池
 * 1. 只提供基本的分配、缓存功能
 * 2. 会一次性申请大量对象缓存起来，但这些对象并不一定是连续的
 * 3. 对象申请出去后，内存池不再维护该对象，调用purge也不会释放该对象
 * 4. 回收对象时，不会检验该对象是否来自当前内存池
 * 5. TODO:construct和destory并不会调用构造、析构函数，以后有需求再加
 */

#include "pool.hpp"

template <typename T, uint32_t msize = 1024, uint32_t nsize = 1024>
class ObjectPool : public Pool
{
public:
    /* @msize:允许池中分配的最大对象数量
     * @nsize:每次预分配的数量
     */
    explicit ObjectPool(const char *name) : Pool(name)
    {
        _anpts    = NULL;
        _anptmax  = 0;
        _anptsize = 0;
    }

    ~ObjectPool() { clear(); }

    inline virtual void *construct_any() { return this->construct(); }

    inline virtual void destroy_any(void *const object, bool is_free = false)
    {
        this->destroy(static_cast<T *const>(object), is_free);
    }

    // 清空内存，同clear。但这个是虚函数
    inline virtual void purge() { clear(); }
    inline virtual size_t get_sizeof() const { return sizeof(T); }

    // 构造对象
    T *construct()
    {
        if (EXPECT_FALSE(0 == _anptsize))
        {
            ARRAY_RESIZE(T *, _anpts, _anptmax, nsize, ARRAY_NOINIT);
            // 暂时循环分配而不是一次分配一个数组
            // 因为要实现一个特殊的需求。像vector这种对象，如果分配太多内存，是不会
            // 回收的，低版本都没有shrink_to_fit,这里需要直接把这个对象删除掉
            for (; _anptsize < nsize; _anptsize++)
            {
                _max_new++;
                _max_now++;
                _anpts[_anptsize] = new T();
            }
        }

        _max_now--;
        return _anpts[--_anptsize];
    }

    /* 回收对象(当内存池已满时，对象会被直接销毁)
     * @free:是否直接释放对象内存
     */
    void destroy(T *const object, bool is_free = false)
    {
        if (is_free || _anptsize >= msize)
        {
            _max_del++;
            delete object;
        }
        else
        {
            if (EXPECT_FALSE(_anptsize >= _anptmax))
            {
                uint32_t mini_size = std::min(_anptmax + nsize, msize);
                ARRAY_RESIZE(T *, _anpts, _anptmax, mini_size, ARRAY_NOINIT);
            }
            _max_now++;
            _anpts[_anptsize++] = object;
        }
    }

private:
    /* 清空内存池 */
    inline void clear()
    {
        for (uint32_t idx = 0; idx < _anptsize; idx++) delete _anpts[idx];

        _anptsize = 0;
        delete[] _anpts;
        _anpts = NULL;
    }

private:
    T **_anpts;         /* 空闲对象数组 */
    uint32_t _anptmax;  /* 对象数组的最大长度 */
    uint32_t _anptsize; /* 对象数组的当前长度 */
};
