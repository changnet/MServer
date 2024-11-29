#pragma once

#include "pool.hpp"

/**
 * 对象缓存池
 * 与object_pool的区别是这里不处理对象的构造、析构，允许缓存vector、list之类的对象。复用时
 * 需要手动处理对象重置
 * @tparam msize 池中缓存最大对象数量
 * @tparam nsize 每次分配的对象数量
 */
template <typename T, size_t msize = 1024, size_t nsize = 1024>
class CachePool : public Pool
{
public:
    /* @msize:允许池中分配的最大对象数量
     * @nsize:每次预分配的数量
     */
    explicit CachePool(const char *name) : Pool(name) { objs_.reserve(msize); }

    ~CachePool() { clear(); }

    // 清空内存，同clear。但这个是虚函数
    inline virtual void purge() { clear(); }
    inline virtual size_t get_sizeof() const { return sizeof(T); }

    // 构造对象
    T *construct()
    {
        if (unlikely(objs_.empty()))
        {
            for (size_t i = 0; i < nsize; i++) objs_.push_back(new T());
        }

        T *obj = objs_.back();

        objs_.pop_back();
        return obj;
    }

    /// 回收对象(当内存池已满时，对象会被直接销毁)
    void destroy(T *const object, bool del = false)
    {
        if (del || objs_.size() > msize)
        {
            delete object;
        }
        else
        {
            objs_.push_back(object);
        }
    }

private:
    /* 清空内存池 */
    inline void clear()
    {
        for (auto obj : objs_) delete obj;
    }

private:
    std::vector<T *> objs_;
};
