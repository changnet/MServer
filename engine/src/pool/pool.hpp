#pragma once

#include "../global/global.hpp"

/// 内存池、对象池基类
class Pool
{
public:
    virtual ~Pool()
    {
        class Pool **pool_stat = get_pool_stat();
        for (int32_t idx = 0; idx < MAX_POOL; idx++)
        {
            if (this == pool_stat[idx])
            {
                pool_stat[idx] = NULL;
                return;
            }
        }

        assert(false);
    }
    explicit Pool(const char *name)
    {
        _max_new = 0;
        _max_del = 0;
        _max_now = 0;

        _name = name;

        class Pool **pool_stat = get_pool_stat();
        for (int32_t idx = 0; idx < MAX_POOL; idx++)
        {
            if (NULL == pool_stat[idx])
            {
                pool_stat[idx] = this;
                return;
            }
        }

        // 目前C++的逻辑不涉及具体业务逻辑，内存池数量应该是可以预估的
        assert(false);
    }

    int64_t get_max_new() const { return _max_new; }
    int64_t get_max_del() const { return _max_del; }
    int64_t get_max_now() const { return _max_now; }
    const char *get_name() const { return _name; }

    virtual void purge()              = 0;
    virtual size_t get_sizeof() const = 0;

public:
    static constexpr int32_t MAX_POOL = 16;
    static class Pool **get_pool_stat()
    {
        static class Pool *pool_stat[MAX_POOL] = {0};
        return pool_stat;
    }

protected:
    const char *_name;
    int64_t _max_new; // 总分配数量
    int64_t _max_del; // 总删除数量
    int64_t _max_now; // 当前缓存数量
};
