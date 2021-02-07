#pragma once

#include "pool.hpp"

/**
 * 对象内存池
 * 用数组来管对象利用，解决频繁分配、释放问题。但无法解决多个小对象导致的内存碎片问题
 * @tparam msize 池中缓存最大对象数量
 * @tparam nsize 每次分配的对象数量
 */
template <typename T, uint32_t msize = 1024, uint32_t nsize = 1024>
class ObjectPool : public Pool
{
public:
    /* @msize:允许池中分配的最大对象数量
     * @nsize:每次预分配的数量
     */
    explicit ObjectPool(const char *name) : Pool(name) { _objs.reserve(msize); }

    ~ObjectPool() { clear(); }

    // 清空内存，同clear。但这个是虚函数
    inline virtual void purge() { clear(); }
    inline virtual size_t get_sizeof() const { return sizeof(Storage); }

    // 构造对象
    template <typename... Args> T *construct(Args &&... args)
    {
        if (EXPECT_FALSE(_objs.empty()))
        {
            for (uint32_t i = 0; i < nsize; i++) _objs.push_back(new Storage);
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
    }

private:
    // https://stackoverflow.com/questions/4583125/char-array-as-storage-for-placement-new
    // 不能在char[]上用placement new来创建对象，因为没有对齐
    using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    std::vector<Storage *> _objs;
};
