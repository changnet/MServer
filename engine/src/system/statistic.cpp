#include "statistic.hpp"
#include "../system/static_global.hpp"

Statistic::~Statistic() {}

Statistic::Statistic() {}

void Statistic::add_c_obj(const char *what, int32_t count)
{
    class BaseCounter &counter = _c_obj[what];

    counter._cur += count;
    if (count > 0)
    {
        counter._max += count;
    }
    else
    {
        ASSERT(counter._cur >= 0, "add_c_obj count < 0");
    }
}

void Statistic::add_c_lua_obj(const char *what, int32_t count)
{
    class BaseCounter &counter = _c_lua_obj[what];

    counter._cur += count;
    if (count > 0)
    {
        counter._max += count;
    }
    else
    {
        ASSERT(counter._cur >= 0, "add_c_lua_obj count < 0");
    }
}

void Statistic::reset_trafic()
{
    const time_t now = StaticGlobal::ev()->now();
    for (int type = 0; type < Socket::CT_MAX; type++)
    {
        _total_traffic[type].reset();
        _total_traffic[type]._time = now;
    }

    SocketTrafficType::iterator itr = _socket_traffic.begin();
    while (itr != _socket_traffic.end())
    {
        itr->second.reset();
        itr->second._time = now;

        itr++;
    }
}

void Statistic::remove_socket_traffic(uint32_t conn_id)
{
    // 一个listen的sokcet，由于没有调用socket::start，不在_socket_traffic
    // 一个connect失败的socket，也是没调用socket::start
    // 但它们会调用stop，会尝试从_socket_traffic中删除。目前暂时没法区分，也没必要区分

    // ASSERT(_socket_traffic.end() != _socket_traffic.find(conn_id),
    //    "socket traffic del statistic corruption",);

    _socket_traffic.erase(conn_id);
}

void Statistic::insert_socket_traffic(uint32_t conn_id)
{
    ASSERT(_socket_traffic.end() == _socket_traffic.find(conn_id),
           "socket traffic new statistic corruption");

    _socket_traffic[conn_id]._time = StaticGlobal::ev()->now();
}

void Statistic::add_send_traffic(uint32_t conn_id, Socket::ConnType type,
                                 uint32_t val)
{
    ASSERT(type > Socket::CT_NONE && type < Socket::CT_MAX);

    _total_traffic[type]._send += val;
    _socket_traffic[conn_id]._send += val;
}

void Statistic::add_recv_traffic(uint32_t conn_id, Socket::ConnType type,
                                 uint32_t val)
{
    ASSERT(type > Socket::CT_NONE && type < Socket::CT_MAX);

    _total_traffic[type]._recv += val;
    _socket_traffic[conn_id]._recv += val;
}

void Statistic::add_rpc_count(const char *cmd, int32_t size, int64_t msec)
{
    if (!cmd)
    {
        ERROR("rpc cmd not found");
        return;
    }

    PktCounter &pkt = _rpc_count[cmd];
    pkt._size += size;
    pkt._msec += msec;
    pkt._count += 1;

    if (msec > pkt._max) pkt._max = msec;
    if (-1 == pkt._min || msec < pkt._min) pkt._min = msec;

    if (size > pkt._max_size) pkt._max_size = size;
    if (-1 == pkt._min_size || size < pkt._min_size) pkt._min_size = size;
}

void Statistic::add_pkt_count(int32_t type, int32_t cmd, int32_t size,
                              int64_t msec)
{
    ASSERT(type > SPT_NONE && type < SPT_MAXT, "cmd type error");

    PktCounter &pkt = _pkt_count[type][cmd];
    pkt._size += size;
    pkt._msec += msec;
    pkt._count += 1;

    if (msec > pkt._max) pkt._max = msec;
    if (-1 == pkt._min || msec < pkt._min) pkt._min = msec;

    if (size > pkt._max_size) pkt._max_size = size;
    if (-1 == pkt._min_size || size < pkt._min_size) pkt._min_size = size;
}
