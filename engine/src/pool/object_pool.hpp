#pragma once

#include <mutex>

#include "pool.hpp"
#include "../thread/spin_lock.hpp"

/**
 * 对象内存池
 * 用数组来管对象利用，解决频繁分配、释放问题。但无法解决多个小对象导致的内存碎片问题
 * @param msize 池中缓存保留最大空闲对象数量
 * @param nsize 每次分配的对象数量
 */
template <typename T, size_t msize = 1024, size_t nsize = 1024>
class ObjectPool : public Pool
{
public:
    /* @msize:允许池中分配的最大对象数量
     * @nsize:每次预分配的数量
     */
    explicit ObjectPool(const char *name) : Pool(name)
    {
        _objs.reserve(msize);
    }

    virtual ~ObjectPool()
    {
        clear();
    }

    // 清空内存，同clear。但这个是虚函数
    inline virtual void purge()
    {
        clear();
    }
    inline virtual size_t get_sizeof() const
    {
        return sizeof(Storage);
    }

    /**
     * @brief 构造对象，注意此函数不是virtual函数（模板不能是virtual）
     * @tparam ...Args 用于构造对象的构造函数
     * @param ...args 
     * @return 
    */
    template <typename... Args> T *construct(Args &&...args)
    {
        if (EXPECT_FALSE(_objs.empty()))
        {
            for (size_t i = 0; i < nsize; i++) _objs.push_back(new Storage);
        }

        Storage *storage = _objs.back();

        _objs.pop_back();
        return new (storage) T(std::forward<Args>(args)...);
    }

    /// 回收对象(当内存池已满时，对象会被直接销毁)
    void destroy(T *const object)
    {
        object->~T();

        if (_objs.size() > msize)
        {
            delete (Storage *)object;
        }
        else
        {
            _objs.push_back(reinterpret_cast<Storage *>(object));
        }
    }

private:
    /* 清空内存池 */
    inline void clear()
    {
        for (auto obj : _objs) delete obj;
        _objs.clear();
    }

private:
    // https://stackoverflow.com/questions/4583125/char-array-as-storage-for-placement-new
    // 不能在char[]上用placement new来创建对象，因为没有对齐
    using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    std::vector<Storage *> _objs;
};

/**
 * @brief 对象内存池，同ObjectPool，但加锁(注意不能用多态)
 * @tparam T 对象类型
 */
template <typename T, size_t msize = 1024, size_t nsize = 1024>
class ObjectPoolLock final : public ObjectPool<T, msize, nsize>
{
public:
    explicit ObjectPoolLock(const char *name)
        : ObjectPool<T, msize, nsize>(name)
    {
    }
    virtual ~ObjectPoolLock()
    {
        // 基类会释放，不需要加锁。如果释放的时候有其他线程访问，加锁也救不了
        // std::lock_guard<SpinLock> guard(_lock);
        // ObjectPool<T, msize, nsize>::purge();
    }

    inline virtual void purge() override
    {
        std::lock_guard<SpinLock> guard(_lock);
        ObjectPool<T, msize, nsize>::purge();
    }
    inline virtual size_t get_sizeof() const override
    {
        std::lock_guard<SpinLock> guard(_lock);
        return ObjectPool<T, msize, nsize>::get_sizeof();
    }

    /**
     * @brief 构造对象，注意此函数不是virtual函数（模板不能是virtual）
     * @tparam ...Args 
     * @param ...args 用于构造对象的参数
     * @return 
    */
    template <typename... Args> T *construct(Args &&...args)
    {
        std::lock_guard<SpinLock> guard(_lock);
        return ObjectPool<T, msize, nsize>::construct(args...);
    }

    void destroy(T *const object)
    {
        std::lock_guard<SpinLock> guard(_lock);
        ObjectPool<T, msize, nsize>::destroy(object);
    }

private:
    mutable SpinLock _lock;
};
