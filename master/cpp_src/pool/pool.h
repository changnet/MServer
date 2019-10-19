#pragma once

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

        ASSERT( del_from_stat, "cant not del from stat" );
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
        ASSERT( add_to_stat, "cant not add to stat" );
    }

    int64 get_max_new() const { return _max_new; }
    int64 get_max_del() const { return _max_del; }
    int64 get_max_now() const { return _max_now; }
    const char *get_name() const { return _name; }

    virtual void purge() = 0;
    virtual size_t get_sizeof() const = 0;

    /* TODO
     * T *construct返回的类型和子类object_pool的模板参数有关。当实现多态时，无法预知子类
     * 的类型。如果把基类也做成模板T，子类中的typename T也是不一样的，无法实现多态。这个在
     * 日志中有用到。
     */
    virtual void *construct_any()
    {
        ASSERT(false, "can NOT call base construct_any"); return NULL;
    }
    virtual void destroy_any(void *const object,bool is_free = false)
    {
        ASSERT(false, "can NOT call base destroy_any");
    }
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
