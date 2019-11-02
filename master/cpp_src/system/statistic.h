#pragma once

#include "../net/socket.h"
#include "../net/net_header.h"

#define G_STAT    static_global::statistic()

#define STAT_TIME_BEG() int64_t stat_time_beg = static_global::ev()->get_ms_time()
#define STAT_TIME_END() (static_global::ev()->get_ms_time() - stat_time_beg)

#define C_OBJECT_ADD(what) do{G_STAT->add_c_obj(what,1);}while(0)
#define C_OBJECT_DEC(what) do{G_STAT->add_c_obj(what,-1);}while(0)

#define C_LUA_OBJECT_ADD(what) do{G_STAT->add_c_lua_obj(what,1);}while(0)
#define C_LUA_OBJECT_DEC(what) do{G_STAT->add_c_lua_obj(what,-1);}while(0)

#define C_SOCKET_TRAFFIC_NEW( conn_id ) \
    do{G_STAT->insert_socket_traffic(conn_id);}while(0)
#define C_SOCKET_TRAFFIC_DEL( conn_id ) \
    do{G_STAT->remove_socket_traffic(conn_id);}while(0)
#define C_SEND_TRAFFIC_ADD(conn_id,type,val) \
    do{G_STAT->add_send_traffic(conn_id,type,val);}while(0)
#define C_RECV_TRAFFIC_ADD(conn_id,type,val) \
    do{G_STAT->add_recv_traffic(conn_id,type,val);}while(0)

#define PKT_STAT_ADD(type,cmd,size,msec) \
    do{G_STAT->add_pkt_count(type,cmd,size,msec);}while(0)
#define RPC_STAT_ADD(cmd,size,msec) \
    do{G_STAT->add_rpc_count(cmd,size,msec);}while(0)

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
        int64_t _max;
        int64_t _cur;
    };

    // 时间计数器
    class time_counter
    {
    public:
        time_counter() { reset(); }
        inline void reset()
        {
            _max = 0;_min = -1;_count = 0;_msec  = 0;
        }
    public:
        int64_t _max;
        int64_t _min;
        int64_t _msec;
        int64_t _count;
    };

    // 发包计数器(收包统计在脚本做)
    class pkt_counter
    {
    public:
        int64_t _max;
        int64_t _min;
        int64_t _msec;
        int64_t _size;
        int64_t _count;
        int32_t _max_size;
        int32_t _min_size;

        pkt_counter () { reset(); }
        inline void reset() 
        {
            _max   = 0;
            _min   = -1;
            _msec  = 0;
            _size  = 0;
            _count = 0;
            _max_size = 0;
            _min_size = -1;
        }
    };

    // 流量计数器
    class traffic_counter
    {
    public:
    traffic_counter() { reset(); }
    inline void reset()
    {
        _recv = 0;_send = 0;_time = 0;
    }
    public:
        int64_t _recv;
        int64_t _send;
        time_t _time; // 时间戳，各个socket时间不一样，要分开统计
    };

    typedef StdMap<int32_t,pkt_counter> pkt_counter_t;
    typedef StdMap<std::string,pkt_counter> rpc_counter_t;

    typedef StdMap<uint32_t,traffic_counter> socket_traffic_t;
    typedef StdMap<std::string,struct base_counter> base_counter_t;
public:
    ~statistic();
    explicit statistic();

    void reset_trafic();
    void add_c_obj(const char *what,int32_t count);
    void add_c_lua_obj(const char *what,int32_t count);

    void add_lua_gc(int32_t msec)
    {
        _lua_gc._count += 1;
        _lua_gc._msec  += msec;
        if (_lua_gc._max < msec) _lua_gc._max = msec;
        if ( -1 == _lua_gc._min || _lua_gc._min > msec) _lua_gc._min = msec;
    }

    void remove_socket_traffic(uint32_t conn_id);
    void insert_socket_traffic(uint32_t conn_id);

    void add_send_traffic(uint32_t conn_id,socket::conn_t type,uint32_t val);
    void add_recv_traffic(uint32_t conn_id,socket::conn_t type,uint32_t val);

    void add_rpc_count(const char *cmd,int32_t size,int64_t msec);
    void add_pkt_count(int32_t type,int32_t cmd,int32_t size,int64_t msec);

    inline void reset_lua_gc() { _lua_gc.reset(); }

    const statistic::time_counter &get_lua_gc() const { return _lua_gc; }
    const statistic::base_counter_t &get_c_obj() const { return _c_obj; }
    const statistic::base_counter_t &get_c_lua_obj() const { return _c_lua_obj; }
    const statistic::traffic_counter *get_total_traffic() const
    {
        return _total_traffic;
    }
    const statistic::socket_traffic_t &get_socket_traffic() const
    {
        return _socket_traffic;
    }
public:
    time_counter _lua_gc; // lua gc时间统计
    base_counter_t _c_obj; // c对象计数器
    base_counter_t _c_lua_obj; // 从c push到lua对象

    rpc_counter_t _rpc_count;
    pkt_counter_t _pkt_count[SPKT_MAXT]; // 发包时间、数量、大小统计
    socket_traffic_t _socket_traffic; // 各个socket单独流量统计
    traffic_counter _total_traffic[socket::CNT_MAX]; // socket总流量统计
};
