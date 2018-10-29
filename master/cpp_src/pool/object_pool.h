#ifndef __OBJECT_POOL_H__
#define __OBJECT_POOL_H__

/* 对象内存池
 * 1. 只提供基本的分配、缓存功能
 * 2. 会一次性申请大量对象缓存起来，但这些对象并不一定是连续的
 * 3. 对象申请出去后，内存池不再维护该对象，调用purge也不会释放该对象
 * 4. 回收对象时，不会检验该对象是否来自当前内存池
 */

#include "../global/global.h"

template <typename T>
class object_pool
{
public:
    explicit object_pool(uint32 msize = 1024,uint32 nsize = 1024)
    {
        _anpts = NULL;
        _anptmax = 0;
        _anptsize = 0;

        _max_size = msize;
        _next_size = nsize; // 不设置此值时，默认每次分配对象数量
    }

    ~object_pool()
    {
        purge();
    }

    /* 清空内存池 */
    void purge()
    {
        for (uint32 idx = 0;idx < _anptsize;idx ++)
        {
            delete _anpts[idx];
        }
        _anptsize = 0;
        delete []_anpts;
        _anpts = NULL;
    }

    void set_max_size(const uint32 size) { _max_size = size; }
    // 当对象池为空时，一次申请的对象数量
    void set_next_size(const uint32 size){ _next_size = size; };

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
    void destroy(T *const obj,bool free = false)
    {
        if (free || _anptsize >= _max_size)
        {
            delete obj;
        }
        else
        {
            if (expect_false(_anptsize >= _anptmax))
            {
                uint32 msize = MATH_MIN(_anptmax + _next_size,_max_size);
                array_resize( T*,_anpts,_anptmax,msize,array_noinit );
            }
            _anpts[_anptsize++] = obj;
        }
    }
private:
    T **_anpts;    /* 空闲对象数组 */
    uint32 _anptmax; /* 对象数组的最大长度 */
    uint32 _anptsize; /* 对象数组的当前长度 */
    uint32 _next_size; /* 下一次申请对象的数量 */
    uint32 _max_size; /* 允许池中缓存的最大对象数量 */
};

#endif /* __OBJECT_POOL_H__ */
