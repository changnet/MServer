#ifndef __STATISTIC_H__
#define __STATISTIC_H__

#include "../net/socket.h"

#define G_STAT    static_global::statistic()
#define C_OBJECT_ADD(what) do{G_STAT->add_c_obj(what,1);}while(0)
#define C_OBJECT_DEC(what) do{G_STAT->add_c_obj(what,-1);}while(0)

#define C_LUA_OBJECT_ADD(what) do{G_STAT->add_c_lua_obj(what,1);}while(0)
#define C_LUA_OBJECT_DEC(what) do{G_STAT->add_c_lua_obj(what,-1);}while(0)

#define C_SEND_TRAFFIC_ADD(type,val) \
    do{G_STAT->add_send_traffic(type,val);}while(0)
#define C_RECV_TRAFFIC_ADD(type,val) \
    do{G_STAT->add_recv_traffic(type,val);}while(0)

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

    // 时间计数器
    class time_counter
    {
    public:
        time_counter() { reset(); }
        inline void reset()
        {
            _count = 0;
            _msec  = 0;

            _max = 0;
            _min = -1;
        }
    public:
        int64 _count;
        int64 _msec;
        int64 _max;
        int64 _min;
    };

    // 流量计数器
    class traffic_counter
    {
    public:
    traffic_counter() { reset(); }
    inline void reset()
    {
        _recv = 0;_send = 0;
    }
    public:
        int64 _recv;
        int64 _send;
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

    void add_lua_gc(int32 msec)
    {
        _lua_gc._count += 1;
        _lua_gc._msec  += msec;
        if (_lua_gc._max < msec) _lua_gc._max = msec;
        if ( -1 == _lua_gc._min || _lua_gc._min > msec) _lua_gc._min = msec;
    }

    inline void add_send_traffic(socket::conn_t type,uint32 val)
    {
        assert( "connection type error",
            type > socket::CNT_NONE && type < socket::CNT_MAX );

        _socket_traffic[type]._send += val;
    }

    inline void add_recv_traffic(socket::conn_t type,uint32 val)
    {
        assert( "connection type error",
            type > socket::CNT_NONE && type < socket::CNT_MAX );

        _socket_traffic[type]._recv += val;
    }

    inline void reset_lua_gc() { _lua_gc.reset(); }
    void reset_socket_traffic();

    const time_t get_traffic_time() const { return _traffic_time; }
    const statistic::time_counter &get_lua_gc() const { return _lua_gc; }
    const statistic::base_counter_t &get_c_obj() const { return _c_obj; }
    const statistic::base_counter_t &get_c_lua_obj() const { return _c_lua_obj; }
    const statistic::traffic_counter *get_traffic() const { return _socket_traffic; }
private:
    time_counter _lua_gc; // lua gc时间统计
    base_counter_t _c_obj; // c对象计数器
    base_counter_t _c_lua_obj; // 从c push到lua对象

    time_t _traffic_time; // socket流量统计开始时间戳
    traffic_counter _socket_traffic[socket::CNT_MAX]; // socket流量统计
};

#endif /* __STATISTIC_H__ */
