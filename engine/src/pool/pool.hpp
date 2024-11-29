#pragma once

#include "global/global.hpp"

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
        max_new_ = 0;
        max_del_ = 0;
        max_now_ = 0;

        name_ = name;

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

    int64_t get_max_new() const { return max_new_; }
    int64_t get_max_del() const { return max_del_; }
    int64_t get_max_now() const { return max_now_; }
    const char *get_name() const { return name_; }

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
    const char *name_;
    int64_t max_new_; // 总分配数量
    int64_t max_del_; // 总删除数量
    int64_t max_now_; // 当前缓存数量
};
