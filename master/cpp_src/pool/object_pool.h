#ifndef __OBJECT_POOL_H__
#define __OBJECT_POOL_H__

/* 对象内存池
 * 1. 只提供基本的分配、缓存功能
 * 2. 会一次性申请大量对象缓存起来，但这些对象并不一定是连续的
 * 3. 对象申请出去后，内存池不再维护该对象，调用purge也不会释放该对象
 * 4. 回收对象时，不会检验该对象是否来自当前内存池
 * 5. TODO:construct和destory并不会调用构造、析构函数，以后有需求再加
 */

#include "../global/global.h"

// 内存池基类

class pool
{
public:
    pool() {}
    virtual ~pool() {}

    virtual void purge() = 0;

    /* TODO
     * T *construct返回的类型和子类object_pool的模板参数有关。当实现多态时，无法预知子类
     * 的类型。如果把基类也做成模板T，子类中的typename T也是不一样的，无法实现多态。这个在
     * 日志中有用到。
     */
    virtual void *construct_any() = 0;
    virtual void destroy_any(void *const object,bool is_free = false) = 0;
};

template <typename T,uint32 msize = 1024,uint32 nsize = 1024>
class object_pool : public pool
{
public:
    /* @msize:允许池中分配的最大对象数量
     * @nsize:每次预分配的数量
     */
    object_pool() : pool()
    {
        _anpts = NULL;
        _anptmax = 0;
        _anptsize = 0;

        _max_size = msize;
        _next_size = nsize; // 不设置此值时，默认每次分配对象数量
    }

    ~object_pool()
    {
        clear();
    }

    inline void *construct_any()
    {
        return this->construct();
    }

    inline void destroy_any(void *const object,bool is_free = false)
    {
        this->destroy(static_cast<T *const>(object),is_free);
    }

    // 清空内存，同clear。但这个是虚函数
    inline void purge() { clear(); }

    // 构造对象
    T *construct()
    {
        if (expect_false(0 == _anptsize))
        {
            array_resize( T*,_anpts,_anptmax,_next_size,array_noinit );
            // 暂时循环分配而不是一次分配一个数组
            // 因为要实现一个特殊的需求。像vector这种对象，如果分配太多内存，是不会
            // 回收的，低版本都没有shrink_to_fit,这里需要直接把这个对象删除掉
            for (;_anptsize < _next_size;_anptsize ++)
            {
                _anpts[_anptsize] = new T();
            }
        }

        return _anpts[--_anptsize];
    }

    /* 回收对象(当内存池已满时，对象会被直接销毁)
     * @free:是否直接释放对象内存
     */
    void destroy(T *const object,bool is_free = false)
    {
        if (is_free || _anptsize >= _max_size)
        {
            delete object;
        }
        else
        {
            if (expect_false(_anptsize >= _anptmax))
            {
                uint32 mini_size = MATH_MIN(_anptmax + _next_size,_max_size);
                array_resize( T*,_anpts,_anptmax,mini_size,array_noinit );
            }
            _anpts[_anptsize++] = object;
        }
    }
private:
    /* 清空内存池 */
    inline void clear()
    {
        for (uint32 idx = 0;idx < _anptsize;idx ++) delete _anpts[idx];

        _anptsize = 0;
        delete []_anpts;
        _anpts = NULL;
    }
private:
    T **_anpts;    /* 空闲对象数组 */
    uint32 _anptmax; /* 对象数组的最大长度 */
    uint32 _anptsize; /* 对象数组的当前长度 */
    uint32 _next_size; /* 下一次申请对象的数量 */
    uint32 _max_size; /* 允许池中缓存的最大对象数量 */
};

#endif /* __OBJECT_POOL_H__ */
