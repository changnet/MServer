#include "statistic.h"
#include "../system/static_global.h"

statistic::~statistic()
{
}

statistic::statistic()
{
}

void statistic::add_c_obj(const char *what,int32 count)
{
    class base_counter &counter = _c_obj[what];

    counter._cur += count;
    if (count > 0)
    {
        counter._max += count;
    }
    else
    {
        assert("add_c_obj count < 0",counter._cur >= 0);
    }
}

void statistic::add_c_lua_obj(const char *what,int32 count)
{
    class base_counter &counter = _c_lua_obj[what];

    counter._cur += count;
    if (count > 0)
    {
        counter._max += count;
    }
    else
    {
        assert("add_c_lua_obj count < 0",counter._cur >= 0);
    }
}

void statistic::reset_trafic()
{
    const time_t now = static_global::ev()->now();
    for (int type = 0;type < socket::CNT_MAX;type ++)
    {
        _total_traffic[type].reset();
        _total_traffic[type]._time = now;
    }

    socket_traffic_t::iterator itr = _socket_traffic.begin();
    while ( itr != _socket_traffic.end() )
    {
        itr->second.reset();
        itr->second._time = now;

        itr ++;
    }
}

void statistic::remove_socket_traffic(uint32 conn_id)
{
    // 一个listen的sokcet，由于没有调用socket::start，是没发这里来的，但会调用stop
    // 一个connect失败的socket，也是没调用socket::start，但会调用stop
    // 目前暂时没法区分，也没必要加变量区分

    // assert("socket traffic del statistic corruption",
    //     _socket_traffic.end() != _socket_traffic.find(conn_id));

    _socket_traffic.erase( conn_id );
}

void statistic::insert_socket_traffic(uint32 conn_id)
{
    assert("socket traffic new statistic corruption",
        _socket_traffic.end() == _socket_traffic.find(conn_id));

    _socket_traffic[conn_id]._time = static_global::ev()->now();
}

void statistic::add_send_traffic(uint32 conn_id,socket::conn_t type,uint32 val)
{
    assert( "connection type error",
        type > socket::CNT_NONE && type < socket::CNT_MAX );

    _total_traffic[type]._send += val;
    _socket_traffic[conn_id]._send += val;
}

void statistic::add_recv_traffic(uint32 conn_id,socket::conn_t type,uint32 val)
{
    assert( "connection type error",
        type > socket::CNT_NONE && type < socket::CNT_MAX );

    _total_traffic[type]._recv += val;
    _socket_traffic[conn_id]._recv += val;
}