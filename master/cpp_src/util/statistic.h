#ifndef __STATISTIC_H__
#define __STATISTIC_H__

#include "../global/global.h"

// 只记录数量的计数器
class base_counter
{
public:
    base_counter()
    {
        _max = 0;
        _cur = 0;
    }
public:
    int64 _max;
    int64 _cur;
};

// 按时间间隔的计数器
class interval_counter : public base_counter
{
public:
    interval_counter()
    {
        _min = 0;
    }
public:
    int64 _min;

    int64 _int_max; // 时间间隔内最大值
    int64 _int_min; // 时间间隔内最小值

    int64 _int_time; // 时间间隔结束时间戳
    int32 _interval; // 时间间隔大小(秒)
    int64 _int_total; // 时间间隔内总数
}

class statistic
{
public:
    static void uninstance();
    static class statistic *instance();
private:
    ~statistic();
    explicit statistic();
private:
    map_t<std::string,struct base_counter> _c_obj; // c对象计数器
    map_t<std::string,struct base_counter> _c_lua_obj; // 从c push到lua对象
};

#endif /* __STATISTIC_H__ */
