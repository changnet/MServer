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
    virtual ~pool()
    {
        bool del_from_stat = false;
        class pool** pool_stat = get_pool_stat();
        for (int32 idx = 0;idx < MAX_POOL;idx ++)
        {
            if (this == pool_stat[idx])
            {
                del_from_stat = true;
                pool_stat[idx] = NULL;
                break;
            }
        }

        assert( "cant not del from stat",del_from_stat );
    }
    explicit pool(const char *name)
    {
        _max_new = 0;
        _max_del = 0;
        _max_now = 0;

        _name = name;

        bool add_to_stat = false;
        class pool** pool_stat = get_pool_stat();
        for (int32 idx = 0;idx < MAX_POOL;idx ++)
        {
            if (NULL == pool_stat[idx])
            {
                add_to_stat = true;
                pool_stat[idx] = this;
                break;
            }
        }

        // 目前C++的逻辑不涉及具体业务逻辑，内存池数量应该是可以预估的
        assert( "cant not add to stat",add_to_stat );
    }

    int64 get_max_new() const { return _max_new; }
    int64 get_max_del() const { return _max_del; }
    int64 get_max_now() const { return _max_now; }
    const char *get_name() const { return _name; }

    virtual void purge() = 0;
    virtual size_t get_object_size() const = 0;

    /* TODO
     * T *construct返回的类型和子类object_pool的模板参数有关。当实现多态时，无法预知子类
     * 的类型。如果把基类也做成模板T，子类中的typename T也是不一样的，无法实现多态。这个在
     * 日志中有用到。
     */
    virtual void *construct_any() = 0;
    virtual void destroy_any(void *const object,bool is_free = false) = 0;
public:
    static constexpr int32 MAX_POOL = 8;
    static class pool** get_pool_stat()
    {
        static class pool* pool_stat[MAX_POOL] = { 0 };
        return pool_stat;
    }
protected:
    const char *_name;
    int64 _max_new; // 总分配数量
    int64 _max_del; // 总删除数量
    int64 _max_now; // 当前缓存数量
};

template <typename T,uint32 msize = 1024,uint32 nsize = 1024>
class object_pool : public pool
{
public:
    /* @msize:允许池中分配的最大对象数量
     * @nsize:每次预分配的数量
     */
    explicit object_pool(const char *name) : pool(name)
    {
        _anpts = NULL;
        _anptmax = 0;
        _anptsize = 0;
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
    size_t get_object_size() const { return sizeof(T); }

    // 构造对象
    T *construct()
    {
        if (expect_false(0 == _anptsize))
        {
            array_resize( T*,_anpts,_anptmax,nsize,array_noinit );
            // 暂时循环分配而不是一次分配一个数组
            // 因为要实现一个特殊的需求。像vector这种对象，如果分配太多内存，是不会
            // 回收的，低版本都没有shrink_to_fit,这里需要直接把这个对象删除掉
            for (;_anptsize < nsize;_anptsize ++)
            {
                _max_new ++;
                _max_now ++;
                _anpts[_anptsize] = new T();
            }
        }

        _max_now --;
        return _anpts[--_anptsize];
    }

    /* 回收对象(当内存池已满时，对象会被直接销毁)
     * @free:是否直接释放对象内存
     */
    void destroy(T *const object,bool is_free = false)
    {
        if (is_free || _anptsize >= msize)
        {
            _max_del ++;
            delete object;
        }
        else
        {
            if (expect_false(_anptsize >= _anptmax))
            {
                uint32 mini_size = MATH_MIN(_anptmax + nsize,msize);
                array_resize( T*,_anpts,_anptmax,mini_size,array_noinit );
            }
            _max_now ++;
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
};

#endif /* __OBJECT_POOL_H__ */
