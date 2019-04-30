#ifndef __STATISTIC_H__
#define __STATISTIC_H__

#include "../global/global.h"

#define C_OBJECT_ADD(what) \
    do{static_global::statistic()->add_c_obj(what,1);}while(0)
#define C_OBJECT_DEC(what) \
    do{static_global::statistic()->add_c_obj(what,-1);}while(0)

#define C_LUA_OBJECT_ADD(what) \
    do{static_global::statistic()->add_c_lua_obj(what,1);}while(0)
#define C_LUA_OBJECT_DEC(what) \
    do{static_global::statistic()->add_c_lua_obj(what,-1);}while(0)

// 统计对象数量、内存、socket流量等...
class statistic
{
public:

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
    };

    /* 所有统计的名称都是static字符串,不要传入一个临时字符串
     * 低版本的C++用std::string做key会每次申请内存都构造字符串
     */
    typedef const_char_map_t(struct base_counter) base_counter_t;
public:
    ~statistic();
    explicit statistic();

    void add_c_obj(const char *what,int32 count);
    void add_c_lua_obj(const char *what,int32 count);

    const statistic::base_counter_t &get_c_obj() const { return _c_obj; }
    const statistic::base_counter_t &get_c_lua_obj() const { return _c_lua_obj; }
private:
private:
    base_counter_t _c_obj; // c对象计数器
    base_counter_t _c_lua_obj; // 从c push到lua对象
};

#endif /* __STATISTIC_H__ */
