#include "lnetwork_mgr.hpp"

#include "../system/static_global.hpp"
#include "ltools.hpp"
#include "../net/net_compat.hpp"

#include "../net/packet/http_packet.hpp"
#include "../net/packet/stream_packet.hpp"
#include "../net/packet/websocket_packet.hpp"

LNetworkMgr::~LNetworkMgr()
{
    clear();
}

LNetworkMgr::LNetworkMgr() : _conn_seed(0)
{
}

void LNetworkMgr::clear()
{
    socket_map_t::iterator itr = _socket_map.begin();
    for (; itr != _socket_map.end(); itr++)
    {
        class Socket *sk = itr->second;
        if (sk) sk->stop(false, true);
        delete sk;
    }

    _owner_map.clear();
    _socket_map.clear();
    _session_map.clear();
    _conn_session_map.clear();
}

void LNetworkMgr::invoke_delete()
{
    if (_deleting.empty()) return;

    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    for (const auto &iter : _deleting)
    {
        socket_map_t::iterator sk_itr = _socket_map.find(iter.first);
        if (sk_itr == _socket_map.end())
        {
            ELOG("no socket to delete: conn_id = %d", iter.first);
            continue;
        }

        const class Socket *sk = sk_itr->second;
        assert(nullptr != sk && sk->is_closed());

        lua_getglobal(L, "conn_del");
        lua_pushinteger(L, iter.first);
        lua_pushinteger(L, iter.second);

        if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 2, 0, 1)))
        {
            ELOG("conn_del:%s", lua_tostring(L, -1));

            lua_pop(L, 1); /* remove error message */
        }

        delete sk;
        _socket_map.erase(sk_itr);
    }

    lua_pop(L, 1); /* remove traceback */

    _deleting.clear();
}

int32_t LNetworkMgr::new_connect_id()
{
    do
    {
        if (INT32_MAX <= _conn_seed) _conn_seed = 0;
        _conn_seed++;
    } while (_socket_map.end() != _socket_map.find(_conn_seed));

    return _conn_seed;
}

int32_t LNetworkMgr::close(lua_State *L)
{
    int32_t conn_id = luaL_checkinteger32(L, 1);
    bool flush      = lua_toboolean(L, 2);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    class Socket *_socket = itr->second;

    /* stop尝试把缓冲区的数据直接发送 */
    _socket->stop(flush);

    /* 这里不能删除内存，因为脚本并不知道是否会再次访问此socket
     * 比如底层正在一个for循环里回调脚本时，就需要再次访问
     * 同时需要等待io线程关闭对应的socket后再删除
     */

    return 0;
}

int32_t LNetworkMgr::address(lua_State *L)
{
    int32_t conn_id = luaL_checkinteger32(L, 1);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    int port = 0;
    char buf[128]; // INET6_ADDRSTRLEN
    if (!itr->second->address(buf, sizeof(buf), &port))
    {
        return luaL_error(L, netcompat::strerror(netcompat::noerror()));
    }

    lua_pushstring(L, buf);
    lua_pushinteger(L, port);
    return 2;
}

int32_t LNetworkMgr::listen(lua_State *L)
{
    const char *host = luaL_checkstring(L, 1);
    if (!host)
    {
        lua_pushinteger(L, -1);
        lua_pushstring(L, "host not specify");
        return 2;
    }

    int32_t port      = luaL_checkinteger32(L, 2);
    int32_t conn_type = luaL_checkinteger32(L, 3);
    if (conn_type <= Socket::CT_NONE || conn_type >= Socket::CT_MAX)
    {
        lua_pushinteger(L, -1);
        lua_pushstring(L, "illegal connection type");
        return 2;
    }

    int32_t conn_id = new_connect_id();
    class Socket *_socket =
        new class Socket(conn_id, static_cast<Socket::ConnType>(conn_type));

    int32_t fd = _socket->listen(host, port);
    if (fd < 0)
    {
        delete _socket;
        lua_pushinteger(L, -1);
        lua_pushstring(L, netcompat::strerror(netcompat::noerror()));
        return 2;
    }

    _socket_map[conn_id] = _socket;

    lua_pushinteger(L, conn_id);
    return 1;
}

int32_t LNetworkMgr::connect(lua_State *L)
{
    const char *host = luaL_checkstring(L, 1);
    if (!host)
    {
        return luaL_error(L, "host not specify");
    }

    int32_t port      = luaL_checkinteger32(L, 2);
    int32_t conn_type = luaL_checkinteger32(L, 3);
    if (conn_type <= Socket::CT_NONE || conn_type >= Socket::CT_MAX)
    {
        return luaL_error(L, "illegal connection type");
    }

    int32_t conn_id = new_connect_id();
    class Socket *_socket =
        new class Socket(conn_id, static_cast<Socket::ConnType>(conn_type));

    int32_t fd = _socket->connect(host, port);
    if (fd == netcompat::INVALID)
    {
        delete _socket;
        luaL_error(L, netcompat::strerror(netcompat::noerror()));
        return 0;
    }

    _socket_map[conn_id] = _socket;
    lua_pushinteger(L, conn_id);
    return 1;
}

int32_t LNetworkMgr::get_connect_type(lua_State *L)
{
    int32_t conn_id = luaL_checkinteger32(L, 1);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    lua_pushinteger(L, itr->second->conn_type());
    return 1;
}

/* 通过所有者查找连接id */
int32_t LNetworkMgr::get_conn_id_by_owner(Owner owner) const
{
    const auto itr = _owner_map.find(owner);
    if (itr == _owner_map.end())
    {
        return 0;
    }

    return itr->second;
}

/* 通过session获取socket连接 */
class Socket *LNetworkMgr::get_conn_by_session(int32_t session) const
{
    const auto itr = _session_map.find(session);
    if (itr == _session_map.end()) return nullptr;

    socket_map_t::const_iterator sk_itr = _socket_map.find(itr->second);
    if (sk_itr == _socket_map.end()) return nullptr;

    return sk_itr->second;
}

/* 通过conn_id获取socket连接 */
class Socket *LNetworkMgr::get_conn_by_conn_id(int32_t conn_id) const
{
    socket_map_t::const_iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end()) return nullptr;

    return itr->second;
}

/* 通过conn_id获取session */
int32_t LNetworkMgr::get_session_by_conn_id(int32_t conn_id) const
{
    const auto itr = _conn_session_map.find(conn_id);
    if (itr == _conn_session_map.end()) return 0;

    return itr->second;
}

int32_t LNetworkMgr::set_conn_owner(lua_State *L)
{
    int32_t conn_id = static_cast<int32_t>(luaL_checkinteger(L, 1));
    Owner owner     = static_cast<Owner>(luaL_checkinteger(L, 2));

    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        return luaL_error(L, "invalid connection");
    }

    if (Socket::CT_SCCN != sk->conn_type())
    {
        return luaL_error(L, "illegal connection type");
    }

    sk->set_object_id(owner);
    _owner_map[owner] = conn_id;

    return 0;
}

/* 解除(客户端)连接所有者 */
int32_t LNetworkMgr::unset_conn_owner(lua_State *L)
{
    int32_t conn_id = static_cast<int32_t>(luaL_checkinteger(L, 1));
    Owner owner     = static_cast<Owner>(luaL_checkinteger(L, 2));

    _owner_map.erase(owner);

    // 这个时候可能socket已被销毁
    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (sk) sk->set_object_id(0);

    return 0;
}

/* 设置(服务器)连接session */
int32_t LNetworkMgr::set_conn_session(lua_State *L)
{
    int32_t conn_id = static_cast<int32_t>(luaL_checkinteger(L, 1));
    int32_t session = luaL_checkinteger32(L, 2);

    const class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        return luaL_error(L, "invalid connection");
    }

    if (Socket::CT_SSCN != sk->conn_type())
    {
        return luaL_error(L, "illegal connection type");
    }

    _session_map[session]      = conn_id;
    _conn_session_map[conn_id] = session;

    return 0;
}

/* 设置当前进程的session */
int32_t LNetworkMgr::set_curr_session(lua_State *L)
{
    _session = luaL_checkinteger32(L, 1);
    return 0;
}

class Packet *LNetworkMgr::lua_check_packet(lua_State *L, Socket::ConnType conn_ty)
{
    int32_t conn_id = static_cast<int32_t>(luaL_checkinteger(L, 1));

    return raw_check_packet(L, conn_id, conn_ty);
}

class Packet *LNetworkMgr::raw_check_packet(lua_State *L, int32_t conn_id,
                                            Socket::ConnType conn_ty)
{
    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        luaL_error(L, "invalid socket");
        return nullptr;
    }
    if (sk->is_closed())
    {
        luaL_error(L, "socket already closed");
        return nullptr;
    }

    // CNT_NONE表示不需要检测连接类型
    if (Socket::CT_NONE != conn_ty && conn_ty != sk->conn_type())
    {
        luaL_error(L, "illegal socket connecte type");
        return nullptr;
    }

    class Packet *pkt = sk->get_packet();
    if (!pkt)
    {
        luaL_error(L, "no packet found");
        return nullptr;
    }

    return pkt;
}

int32_t LNetworkMgr::send_srv_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_NONE);
    pkt->pack_srv(L, 2);

    return 0;
}

int32_t LNetworkMgr::send_clt_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_NONE);
    pkt->pack_clt(L, 2);

    return 0;
}

int32_t LNetworkMgr::send_s2s_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_SSCN);

    // s2s数据包只有stream_packet能打包
    if (Packet::PT_STREAM != pkt->type())
    {
        return luaL_error(L, "illegal packet type");
    }

    // maybe to slower
    // class stream_packet *stream_pkt = dynamic_cast<class stream_packet
    // *>(pkt); if ( !stream_pkt )
    // {
    //     return luaL_error( L,"not a stream packet" );
    // }
    (reinterpret_cast<StreamPacket *>(pkt))->pack_ss(L, 2);

    return 0;
}

int32_t LNetworkMgr::send_ssc_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_SSCN);

    // ssc数据包只有stream_packet能打包
    if (Packet::PT_STREAM != pkt->type())
    {
        return luaL_error(L, "illegal packet type");
    }

    (reinterpret_cast<StreamPacket *>(pkt))->pack_ssc(L, 2);

    return 0;
}

int32_t LNetworkMgr::get_http_header(lua_State *L)
{
    int32_t conn_id = static_cast<int32_t>(luaL_checkinteger(L, 1));

    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        return luaL_error(L, "invalid socket");
    }

    const class Packet *pkt = sk->get_packet();
    if (!pkt || Packet::PT_HTTP != pkt->type())
    {
        return luaL_error(L, "illegal socket packet type");
    }

    int32_t size =
        (reinterpret_cast<const class HttpPacket *>(pkt))->unpack_header(L);
    if (size < 0)
    {
        return luaL_error(L, "http unpack header error");
    }

    return size;
}

int32_t LNetworkMgr::send_rpc_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_SSCN);

    // s2s数据包只有stream_packet能打包
    if (Packet::PT_STREAM != pkt->type())
    {
        return luaL_error(L, "illegal packet type");
    }

    int32_t ecode = (reinterpret_cast<StreamPacket *>(pkt))->pack_rpc(L, 2);
    if (0 != ecode)
    {
        return luaL_error(L, "send rpc packet error");
    }

    return 0;
}

int32_t LNetworkMgr::send_raw_packet(lua_State *L)
{
    int32_t conn_id  = static_cast<int32_t>(luaL_checkinteger(L, 1));
    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk || sk->is_closed())
    {
        luaL_error(L, "invalid socket");
        return 0;
    }

    size_t size     = 0;
    const char *ctx = luaL_checklstring(L, 2, &size);
    if (!ctx) return 0;

    sk->send(ctx, size);

    return 0;
}

int32_t LNetworkMgr::set_buffer_params(lua_State *L)
{
    int32_t conn_id  = luaL_checkinteger32(L, 1);
    int32_t send_max = luaL_checkinteger32(L, 2);
    int32_t recv_max = luaL_checkinteger32(L, 3);
    int32_t mask     = luaL_optinteger32(L, 4, 0);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    class Socket *_socket = itr->second;
    _socket->set_buffer_params(send_max, recv_max, mask);

    return 0;
}

/* 通过onwer获取socket连接 */
class Socket *LNetworkMgr::get_conn_by_owner(Owner owner) const
{
    int32_t dest_conn = get_conn_id_by_owner(owner);
    if (!dest_conn) // 客户端刚好断开或者当前进程不是网关 ?
    {
        return nullptr;
    }
    socket_map_t::const_iterator itr = _socket_map.find(dest_conn);
    if (itr == _socket_map.end())
    {
        return nullptr;
    }

    /* 由网关转发给客户的数据包，是根据_socket_map来映射连接的。
     * 但是上层逻辑是脚本，万一发生错误则可能导致_socket_map的映射出现错误
     * 造成玩家数据发送到另一个玩家去了
     */
    Socket *sk = itr->second;
    if (sk->get_object_id() != owner)
    {
        ELOG("get_conn_by_owner not match:%d,conn = %d", owner, sk->conn_id());
        return nullptr;
    }

    return sk;
}

/* 新增连接 */
bool LNetworkMgr::accept_new(int32_t conn_id, class Socket *new_sk)
{
    int32_t new_conn_id = new_sk->conn_id();

    _socket_map[new_conn_id] = new_sk;

    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    lua_getglobal(L, "conn_accept");
    lua_pushinteger(L, conn_id);
    lua_pushinteger(L, new_conn_id);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 2, 0, 1)))
    {
        ELOG("accept new socket:%s", lua_tostring(L, -1));

        lua_pop(L, 2); /* remove traceback and error object */
        return false;
    }
    lua_pop(L, 1); /* remove traceback */

    return true;
}

bool LNetworkMgr::connect_new(int32_t conn_id, int32_t ecode)
{
    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    lua_getglobal(L, "conn_new");
    lua_pushinteger(L, conn_id);
    lua_pushinteger(L, ecode);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 2, 0, 1)))
    {
        ELOG("connect_new:%s", lua_tostring(L, -1));

        lua_pop(L, 2); /* remove traceback and error object */
        return false;
    }
    lua_pop(L, 1); /* remove traceback */

    return true;
}

bool LNetworkMgr::io_ok(int32_t conn_id)
{
    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    lua_getglobal(L, "io_ok");
    lua_pushinteger(L, conn_id);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 1, 0, 1)))
    {
        ELOG("io_ok:%s", lua_tostring(L, -1));

        lua_pop(L, 2); /* remove traceback and error object */
        return false;
    }
    lua_pop(L, 1); /* remove traceback */

    return true;
}

bool LNetworkMgr::connect_del(int32_t conn_id, int32_t e)
{
    _deleting.emplace(conn_id, e);

    // NOTE 这里暂时不触发脚本的on_disconnected事件
    // 等到异步真实删除时触发，有些地方删除socket时socket对象还在使用中
    // 也为了方便多次删除socket(比如http连接断开时回调到脚本，脚本又刚好判断这个socket超时了，删除了这socket)

    // 需要在下一次循环中删除，不允许线程进入sleep
    StaticGlobal::ev()->set_job(true);

    return true;
}

int32_t LNetworkMgr::set_conn_io(lua_State *L)
{
    int32_t conn_id  = luaL_checkinteger32(L, 1);
    int32_t io_type  = luaL_checkinteger32(L, 2);
    int32_t io_param = luaL_optinteger32(L, 3, 0);

    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        return luaL_error(L, "invalid conn id");
    }

    if (io_type < IO::IOT_NONE || io_type >= IO::IOT_MAX)
    {
        return luaL_error(L, "invalid io type");
    }

    if (sk->set_io(static_cast<IO::IOType>(io_type), io_param) < 0)
    {
        return luaL_error(L, "set conn io error");
    }

    return 0;
}

int32_t LNetworkMgr::set_conn_packet(lua_State *L) /* 设置socket的打包方式 */
{
    int32_t conn_id     = luaL_checkinteger32(L, 1);
    int32_t packet_type = luaL_checkinteger32(L, 2);

    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        return luaL_error(L, "invalid conn id");
    }

    if (packet_type < Packet::PT_NONE || packet_type >= Packet::PKT_MAX)
    {
        return luaL_error(L, "invalid packet type");
    }

    if (sk->set_packet(static_cast<Packet::PacketType>(packet_type)) < 0)
    {
        return luaL_error(L, "set conn packet error");
    }
    return 0;
}

int32_t LNetworkMgr::new_ssl_ctx(lua_State *L) /* 创建一个ssl上下文 */
{
    int32_t sslv          = luaL_checkinteger32(L, 1);
    const char *cert_file = lua_tostring(L, 2);

    const char *key_file = lua_tostring(L, 3);
    const char *passwd   = lua_tostring(L, 4);

    const char *ca = lua_tostring(L, 5);

    int32_t ssl_id = StaticGlobal::ssl_mgr()->new_ssl_ctx(
        static_cast<SSLMgr::SSLVT>(sslv), cert_file, key_file, passwd, ca);

    if (ssl_id < 0)
    {
        return luaL_error(L, "new ssl ctx error");
    }

    lua_pushinteger(L, ssl_id);
    return 1;
}

/* 发送ping-pong等数据包 */
int32_t LNetworkMgr::send_ctrl_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_NONE);

    // 检测packet类型，基类是websocket_packet才能发送控制帧
    Packet::PacketType type = pkt->type();
    if ((Packet::PT_WSSTREAM != type) && (Packet::PT_WEBSOCKET != type))
    {
        return luaL_error(L, "illegal packet type");
    }

    (reinterpret_cast<class WebsocketPacket *>(pkt))->pack_ctrl(L, 2);

    return 0;
}

int32_t LNetworkMgr::ssc_multicast(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_SSCN);

    // ssc广播数据包只有stream_packet能打包
    if (Packet::PT_STREAM != pkt->type())
    {
        return luaL_error(L, "illegal packet type");
    }

    (reinterpret_cast<StreamPacket *>(pkt))->pack_ssc_multicast(L, 2);

    return 0;
}

// 设置玩家当前所在的session
int32_t LNetworkMgr::set_player_session(lua_State *L)
{
    Owner owner     = static_cast<Owner>(luaL_checkinteger(L, 1));
    int32_t session = luaL_checkinteger32(L, 2);

    if (session <= 0)
    {
        _owner_session.erase(owner);
    }
    else
    {
        _owner_session[owner] = session;
    }

    return 0;
}

// 设置玩家当前所在的session
int32_t LNetworkMgr::get_player_session(lua_State *L)
{
    Owner owner = (Owner)luaL_checkinteger(L, 1);

    std::unordered_map<Owner, int32_t>::const_iterator iter =
        _owner_session.find(owner);

    lua_pushinteger(L, iter == _owner_session.end() ? -1 : iter->second);

    return 1;
}
