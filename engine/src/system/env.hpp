#pragma once

#include <mutex>
#include <unordered_map>
#include "global/platform.hpp"

/**
 * 设置自定义环境变量
 * 
 * cstdlib里，有setenv、getenv等一套环境变量的接口：https://man7.org/linux/man-pages/man3/setenv.3.html
 * 对应的lua接口是os.getenv，但没有os.setenv，而且不是线程安全的
 * 不同系统的环境变量总大小是有限制的（虽然应该也不可能用完）
 * 
 * 不过既然线程不安全，干脆自己用一个map来实现了
 */
class Env
{
public:
    /**
     * @brief 获取环境变量
     * @param k 不允许为nullptr或者空字符串
     * @param v 如果为nullptr，则删除该变量
     */
    bool set(const char *k, const char *v)
    {
        if (!k || *k == '\0') return false;

        std::lock_guard<std::mutex> lg(mutex_);
        if (!v)
        {
            env_.erase(k);
        }
        else
        {
            env_[k] = v;
        }

        return true;
    }

    /**
     * @brief 获取环境变量
     * @return 字符串，无则返回nil
     */
    const char* get(const char* k) const
    {
        if (!k || *k == '\0') return nullptr;

        std::lock_guard<std::mutex> lg(mutex_);

        auto iter = env_.find(k);
        return iter == env_.end() ? nullptr : iter->second.c_str();
    }

    void init(const char *source);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> env_;
};
