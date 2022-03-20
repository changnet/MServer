#include "lnetwork_mgr.hpp"

#include "../system/static_global.hpp"
#include "ltools.hpp"

#include "../net/packet/http_packet.hpp"
#include "../net/packet/stream_packet.hpp"
#include "../net/packet/websocket_packet.hpp"

LNetworkMgr::~LNetworkMgr()
{
    clear();
}

LNetworkMgr::LNetworkMgr() : _conn_seed(0) {}

void LNetworkMgr::clear() /* 清除所有网络数据，不通知上层脚本 */
{
    socket_map_t::iterator itr = _socket_map.begin();
    for (; itr != _socket_map.end(); itr++)
    {
        class Socket *sk = itr->second;
        if (sk) sk->stop();
        delete sk;
    }

    _owner_map.clear();
    _socket_map.clear();
    _session_map.clear();
    _conn_session_map.clear();
}

/* 删除无效的连接 */
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

        if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 1, 0, 1)))
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

/* 产生一个唯一的连接id
 * 之所以不用系统的文件描述符fd，是因为fd对于上层逻辑不可控。比如一个fd被释放，可能在多个进程
 * 之间还未处理完，此fd就被重用了。当前的连接id不太可能会在短时间内重用。
 */
uint32_t LNetworkMgr::new_connect_id()
{
    do
    {
        if (0xFFFFFFFF <= _conn_seed) _conn_seed = 0;
        _conn_seed++;
    } while (_socket_map.end() != _socket_map.find(_conn_seed));

    return _conn_seed;
}

/* 设置某个客户端指令的参数
 * network_mgr:set_cs_cmd( cmd,schema,object[,mask,session] )
 */
int32_t LNetworkMgr::set_cs_cmd(lua_State *L)
{
    int32_t cmd        = luaL_checkinteger32(L, 1);
    const char *schema = luaL_checkstring(L, 2);
    const char *object = luaL_checkstring(L, 3);
    int32_t mask       = luaL_optinteger32(L, 4, 0);
    int32_t session    = luaL_optinteger32(L, 5, _session);

    CmdCfg &cfg  = _cs_cmd_map[cmd];
    cfg._cmd     = cmd;
    cfg._mask    = mask;
    cfg._session = session;

    snprintf(cfg._schema, MAX_SCHEMA_NAME, "%s", schema);
    snprintf(cfg._object, MAX_SCHEMA_NAME, "%s", object);

    return 0;
}

/* 设置某个服务器指令的参数
 * network_mgr:set_ss_cmd( cmd,schema,object[,mask,session] )
 */
int32_t LNetworkMgr::set_ss_cmd(lua_State *L)
{
    int32_t cmd        = luaL_checkinteger32(L, 1);
    const char *schema = luaL_checkstring(L, 2);
    const char *object = luaL_checkstring(L, 3);
    int32_t mask       = luaL_optinteger32(L, 4, 0);
    int32_t session    = luaL_optinteger32(L, 5, _session);

    CmdCfg &cfg  = _ss_cmd_map[cmd];
    cfg._cmd     = cmd;
    cfg._mask    = mask;
    cfg._session = session;

    snprintf(cfg._schema, MAX_SCHEMA_NAME, "%s", schema);
    snprintf(cfg._object, MAX_SCHEMA_NAME, "%s", object);

    return 0;
}

/* 设置某个sc指令的参数
 * network_mgr:set_ss_cmd( cmd,schema,object[,mask,session] )
 */
int32_t LNetworkMgr::set_sc_cmd(lua_State *L)
{
    int32_t cmd        = luaL_checkinteger32(L, 1);
    const char *schema = luaL_checkstring(L, 2);
    const char *object = luaL_checkstring(L, 3);
    int32_t mask       = luaL_optinteger32(L, 4, 0);
    int32_t session    = luaL_optinteger32(L, 5, _session);

    CmdCfg &cfg  = _sc_cmd_map[cmd];
    cfg._cmd     = cmd;
    cfg._mask    = mask;
    cfg._session = session;

    snprintf(cfg._schema, MAX_SCHEMA_NAME, "%s", schema);
    snprintf(cfg._object, MAX_SCHEMA_NAME, "%s", object);

    return 0;
}

/* 仅关闭socket，但不销毁内存
 * network_mgr:close( conn_id,false )
 */
int32_t LNetworkMgr::close(lua_State *L)
{
    uint32_t conn_id = luaL_checkinteger32(L, 1);
    bool flush       = lua_toboolean(L, 2);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    class Socket *_socket = itr->second;
    if (_socket->fd() < 0)
    {
        return luaL_error(L, "try to close a invalid socket");
    }

    /* stop尝试把缓冲区的数据直接发送 */
    _socket->stop(flush);

    /* 这里不能删除内存，因为脚本并不知道是否会再次访问此socket
     * 比如底层正在一个for循环里回调脚本时，就需要再次访问
     */
    _deleting.emplace(conn_id, 0);

    return 0;
}

/**
 * 获取某个连接的ip地址
 * @param conn_id 网关连接id
 */
int32_t LNetworkMgr::address(lua_State *L)
{
    uint32_t conn_id = luaL_checkinteger32(L, 1);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    int port = 0;
    char buf[128]; // INET6_ADDRSTRLEN
    if (!itr->second->address(buf, sizeof(buf), &port))
    {
        return luaL_error(L, Socket::str_error());
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

    uint32_t conn_id = new_connect_id();
    class Socket *_socket =
        new class Socket(conn_id, static_cast<Socket::ConnType>(conn_type));

    int32_t fd = _socket->listen(host, port);
    if (fd < 0)
    {
        delete _socket;
        lua_pushinteger(L, -1);
        lua_pushstring(L, Socket::str_error());
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

    uint32_t conn_id = new_connect_id();
    class Socket *_socket =
        new class Socket(conn_id, static_cast<Socket::ConnType>(conn_type));

    int32_t fd = _socket->connect(host, port);
    if (!Socket::fd_valid(fd))
    {
        delete _socket;
        luaL_error(L, Socket::str_error());
        return 0;
    }

    _socket_map[conn_id] = _socket;
    lua_pushinteger(L, conn_id);
    return 1;
}

int32_t LNetworkMgr::get_connect_type(lua_State *L)
{
    uint32_t conn_id = luaL_checkinteger32(L, 1);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    lua_pushinteger(L, itr->second->conn_type());
    return 1;
}

/* 获取客户端指令配置 */
const CmdCfg *LNetworkMgr::get_cs_cmd(int32_t cmd) const
{
    cmd_map_t::const_iterator itr = _cs_cmd_map.find(cmd);
    if (itr == _cs_cmd_map.end()) return nullptr;

    return &(itr->second);
}

/* 获取服务端指令配置 */
const CmdCfg *LNetworkMgr::get_ss_cmd(int32_t cmd) const
{
    cmd_map_t::const_iterator itr = _ss_cmd_map.find(cmd);
    if (itr == _ss_cmd_map.end()) return nullptr;

    return &(itr->second);
}

/* 获取sc指令配置 */
const CmdCfg *LNetworkMgr::get_sc_cmd(int32_t cmd) const
{
    cmd_map_t::const_iterator itr = _sc_cmd_map.find(cmd);
    if (itr == _sc_cmd_map.end()) return nullptr;

    return &(itr->second);
}

/* 通过所有者查找连接id */
uint32_t LNetworkMgr::get_conn_id_by_owner(Owner owner) const
{
    std::unordered_map<Owner, uint32_t>::const_iterator itr =
        _owner_map.find(owner);
    if (itr == _owner_map.end())
    {
        return 0;
    }

    return itr->second;
}

/* 通过session获取socket连接 */
class Socket *LNetworkMgr::get_conn_by_session(int32_t session) const
{
    std::unordered_map<int32_t, uint32_t>::const_iterator itr =
        _session_map.find(session);
    if (itr == _session_map.end()) return nullptr;

    socket_map_t::const_iterator sk_itr = _socket_map.find(itr->second);
    if (sk_itr == _socket_map.end()) return nullptr;

    return sk_itr->second;
}

/* 通过conn_id获取socket连接 */
class Socket *LNetworkMgr::get_conn_by_conn_id(uint32_t conn_id) const
{
    socket_map_t::const_iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end()) return nullptr;

    return itr->second;
}

/* 通过conn_id获取session */
int32_t LNetworkMgr::get_session_by_conn_id(uint32_t conn_id) const
{
    std::unordered_map<uint32_t, int32_t>::const_iterator itr =
        _conn_session_map.find(conn_id);
    if (itr == _conn_session_map.end()) return 0;

    return itr->second;
}

int32_t LNetworkMgr::reset_schema(lua_State *L)
{
    int32_t type = luaL_checkinteger32(L, 1);

    if (type < Codec::CT_NONE || type >= Codec::CT_MAX) return 0;

    StaticGlobal::codec_mgr()->reset(static_cast<Codec::CodecType>(type));

    return 0;
}

int32_t LNetworkMgr::load_one_schema(lua_State *L)
{
    int32_t type     = luaL_checkinteger32(L, 1);
    const char *path = luaL_checkstring(L, 2);

    if (type < Codec::CT_NONE || type >= Codec::CT_MAX) return 0;

    int32_t count = StaticGlobal::codec_mgr()->load_one_schema(
        static_cast<Codec::CodecType>(type), path);

    lua_pushinteger(L, count);
    return 1;
}

int32_t LNetworkMgr::load_one_schema_file(lua_State *L)
{
    int32_t type     = luaL_checkinteger32(L, 1);
    const char *path = luaL_checkstring(L, 2);

    if (type < Codec::CT_NONE || type >= Codec::CT_MAX) return -1;

    int32_t e = StaticGlobal::codec_mgr()->load_one_schema_file(
        static_cast<Codec::CodecType>(type), path);

    lua_pushinteger(L, e);
    return 1;
}

int32_t LNetworkMgr::set_conn_owner(lua_State *L)
{
    uint32_t conn_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    Owner owner      = static_cast<Owner>(luaL_checkinteger(L, 2));

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
    uint32_t conn_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    Owner owner      = static_cast<Owner>(luaL_checkinteger(L, 2));

    _owner_map.erase(owner);

    // 这个时候可能socket已被销毁
    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (sk) sk->set_object_id(0);

    return 0;
}

/* 设置(服务器)连接session */
int32_t LNetworkMgr::set_conn_session(lua_State *L)
{
    uint32_t conn_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int32_t session  = luaL_checkinteger32(L, 2);

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
    uint32_t conn_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));

    return raw_check_packet(L, conn_id, conn_ty);
}

class Packet *LNetworkMgr::raw_check_packet(lua_State *L, uint32_t conn_id,
                                            Socket::ConnType conn_ty)
{
    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        luaL_error(L, "invalid socket");
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

/* 发送c2s数据包
 * network_mgr:send_srv_packet( conn_id,cmd,pkt )
 */
int32_t LNetworkMgr::send_srv_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_NONE);
    pkt->pack_srv(L, 2);

    return 0;
}

/* 发送s2c数据包
 * network_mgr:send_clt_packet( conn_id,cmd,errno,pkt )
 */
int32_t LNetworkMgr::send_clt_packet(lua_State *L)
{
    class Packet *pkt = lua_check_packet(L, Socket::CT_NONE);
    pkt->pack_clt(L, 2);

    return 0;
}

/* 发送s2s数据包
 * network_mgr:send_s2s_packet( conn_id,cmd,errno,pkt )
 */
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

/* 在非网关发送客户端数据包
 * conn_id必须为网关连接
 * network_mgr:send_ssc_packet( conn_id,pid,cmd,errno,pkt )
 */
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

/* 获取http报文头数据 */
int32_t LNetworkMgr::get_http_header(lua_State *L)
{
    uint32_t conn_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));

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

/* 发送原始数据包
 * network_mgr:send_raw_packet( conn_id,content )
 */
int32_t LNetworkMgr::send_raw_packet(lua_State *L)
{
    uint32_t conn_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk || sk->fd() <= 0)
    {
        luaL_error(L, "invalid socket");
        return 0;
    }

    size_t size     = 0;
    const char *ctx = luaL_checklstring(L, 2, &size);
    if (!ctx) return 0;

    sk->append(ctx, size);

    return 0;
}

int32_t LNetworkMgr::set_buffer_params(lua_State *L)
{
    uint32_t conn_id    = luaL_checkinteger32(L, 1);
    int32_t send_max    = luaL_checkinteger32(L, 2);
    int32_t recv_max    = luaL_checkinteger32(L, 3);
    int32_t over_action = luaL_optinteger32(L, 4, Socket::OAT_NONE);

    socket_map_t::iterator itr = _socket_map.find(conn_id);
    if (itr == _socket_map.end())
    {
        return luaL_error(L, "no such socket found");
    }

    if (over_action < Socket::OAT_NONE || over_action >= Socket::OAT_MAX)
    {
        return luaL_error(L, "overflow action value illegal");
    }

    class Socket *_socket = itr->second;
    _socket->set_buffer_params(send_max, recv_max,
                           static_cast<Socket::OverActionType>(over_action));

    return 0;
}

/* 通过onwer获取socket连接 */
class Socket *LNetworkMgr::get_conn_by_owner(Owner owner) const
{
    uint32_t dest_conn = get_conn_id_by_owner(owner);
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
bool LNetworkMgr::accept_new(uint32_t conn_id, class Socket *new_sk)
{
    uint32_t new_conn_id = new_sk->conn_id();

    _socket_map[new_conn_id] = new_sk;

    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    lua_getglobal(L, "conn_accept");
    lua_pushinteger(L, conn_id);
    lua_pushinteger(L, new_conn_id);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 2, 0, 1)))
    {
        /* 出错后，无法得知脚本能否继续处理此连接
         * 为了防止死链，这里直接删除此连接
         */
        _deleting.emplace(new_conn_id, 0);
        ELOG("accept new socket:%s", lua_tostring(L, -1));

        lua_pop(L, 2); /* remove traceback and error object */
        return false;
    }
    lua_pop(L, 1); /* remove traceback */

    return true;
}

bool LNetworkMgr::connect_new(uint32_t conn_id, int32_t ecode)
{
    static lua_State *L = StaticGlobal::state();
    LUA_PUSHTRACEBACK(L);

    lua_getglobal(L, "conn_new");
    lua_pushinteger(L, conn_id);
    lua_pushinteger(L, ecode);

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 2, 0, 1)))
    {
        /* 出错后，无法得知脚本能否继续处理此连接
         * 为了防止死链，这里直接删除此连接
         */
        _deleting.emplace(conn_id, 0);
        ELOG("connect_new:%s", lua_tostring(L, -1));

        lua_pop(L, 2); /* remove traceback and error object */
        return false;
    }
    lua_pop(L, 1); /* remove traceback */

    if (0 != ecode) _deleting.emplace(conn_id, 0);

    return true;
}

bool LNetworkMgr::io_ok(uint32_t conn_id)
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

bool LNetworkMgr::connect_del(uint32_t conn_id)
{
    _deleting.emplace(conn_id, 1);

    // NOTE 这里暂时不触发脚本的on_disconnected事件
    // 等到异步真实删除时触发，有些地方删除socket时socket对象还在使用中
    // 也为了方便多次删除socket(比如http连接断开时回调到脚本，脚本又刚好判断这个socket超时了，删除了这socket)

    return true;
}

int32_t LNetworkMgr::set_conn_io(lua_State *L)
{
    uint32_t conn_id = luaL_checkinteger32(L, 1);
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

int32_t LNetworkMgr::set_conn_codec(lua_State *L) /* 设置socket的编译方式 */
{
    uint32_t conn_id   = luaL_checkinteger32(L, 1);
    int32_t codec_type = luaL_checkinteger32(L, 2);

    class Socket *sk = get_conn_by_conn_id(conn_id);
    if (!sk)
    {
        return luaL_error(L, "invalid conn id");
    }

    if (codec_type < Codec::CT_NONE || codec_type >= Codec::CT_MAX)
    {
        return luaL_error(L, "invalid codec type");
    }

    if (sk->set_codec_type(static_cast<Codec::CodecType>(codec_type)) < 0)
    {
        return luaL_error(L, "set conn codec error");
    }

    return 0;
}

int32_t LNetworkMgr::set_conn_packet(lua_State *L) /* 设置socket的打包方式 */
{
    uint32_t conn_id    = luaL_checkinteger32(L, 1);
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

    if (ssl_id <= 0)
    {
        return luaL_error(L, "new ssl ctx error");
    }

    lua_pushinteger(L, ssl_id);
    return 1;
}

// 查找该cmd在哪个进程处理
int32_t LNetworkMgr::get_cmd_session(int64_t object_id, int32_t cmd) const
{
    const CmdCfg *cmd_cfg = get_cs_cmd(cmd);
    if (EXPECT_FALSE(!cmd_cfg))
    {
        ELOG("get_cmd_session cmd(%d) no cmd cfg found", cmd);
        return -1;
    }

    // 是否需要动态转发
    if (0 == (cmd_cfg->_mask & CmdCfg::MK_DYNAMIC))
    {
        return cmd_cfg->_session;
    }

    /* 这个socket必须经过认证，归属某个对象后才能转发到其他服务器
     * 防止网关后面的服务器被攻击
     * 与客户端的连接，这个object_id就是玩家id
     */
    if (EXPECT_FALSE(!object_id))
    {
        ELOG("get_cmd_session cmd(%d) socket do NOT have object", cmd);
        return -1;
    }

    auto iter = _owner_session.find(static_cast<Owner>(object_id));
    if (iter == _owner_session.end())
    {
        ELOG("get_cmd_session cmd(%d) "
             "no session to forwarding:%d-" FMT64d,
             cmd, object_id);
        return -1;
    }

    return iter->second;
}

int32_t LNetworkMgr::cs_dispatch(uint16_t cmd, const class Socket *src_sk,
                                 const char *ctx, size_t size) const
{
    int64_t object_id = src_sk->get_object_id();
    int32_t session   = get_cmd_session(object_id, cmd);
    if (session < 0)
    {
        ELOG("cmd session error, packet abort:%d - %d", session, cmd);
        return -1;
    }

    // 在当前进程处理，不需要转发
    if (session == _session) return 0;

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    class Socket *dest_sk = get_conn_by_session(session);
    if (!dest_sk)
    {
        ELOG("client packet forwarding no destination found.cmd:%d", cmd);
        return 1; /* 如果转发失败，也相当于转发了 */
    }

    Codec::CodecType codec_ty = src_sk->get_codec_type();

    struct s2s_header s2sh;
    SET_HEADER_LENGTH(s2sh, size, cmd, SET_LENGTH_FAIL_BOOL);
    s2sh._cmd    = cmd;
    s2sh._packet = SPT_CSPK;
    s2sh._codec  = codec_ty;
    s2sh._errno  = 0;
    s2sh._owner  = static_cast<Owner>(object_id);

    dest_sk->append(&s2sh, sizeof(struct s2s_header));
    dest_sk->append(ctx, size);
    dest_sk->flush();

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

/* 广播到所有连接到当前进程的服务器
 * srv_multicast( conn_list,codec_type,cmd,errno,pkt )
 */
int32_t LNetworkMgr::srv_multicast(lua_State *L)
{
    STAT_TIME_BEG();
    if (!lua_istable(L, 1))
    {
        return luaL_error(L, "expect table,got %s",
                          lua_typename(L, lua_type(L, 1)));
    }
    int32_t codec_ty = luaL_checkinteger32(L, 2);
    uint16_t cmd     = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t ecode   = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    if (codec_ty < Codec::CT_NONE || codec_ty >= Codec::CT_MAX)
    {
        return luaL_error(L, "illegal codec type");
    }

    if (!lua_istable(L, 5))
    {
        return luaL_error(L, "expect table,got %s",
                          lua_typename(L, lua_type(L, 5)));
    }

    const CmdCfg *cfg = get_ss_cmd(cmd);
    if (!cfg)
    {
        return luaL_error(L, "no command conf found: %d", (int32_t)cmd);
    }

    Codec *encoder = StaticGlobal::codec_mgr()->get_codec(
        static_cast<Codec::CodecType>(codec_ty));
    if (!cfg)
    {
        return luaL_error(L, "no codec conf found: %d", (int32_t)cmd);
    }

    const char *buffer = nullptr;
    int32_t len        = encoder->encode(L, 5, &buffer, cfg);
    if (len < 0)
    {
        encoder->finalize();
        ELOG("srv_multicast encode error");
        return 0;
    }

    if (len > MAX_PACKET_LEN)
    {
        encoder->finalize();
        return luaL_error(L, "buffer size over MAX_PACKET_LEN");
    }

    lua_pushnil(L); /* first key */
    while (lua_next(L, 1) != 0)
    {
        if (!lua_isinteger(L, -1))
        {
            lua_pop(L, 1);
            encoder->finalize();
            return luaL_error(L, "conn list expect integer");
        }

        uint32_t conn_id = static_cast<uint32_t>(lua_tointeger(L, -1));

        lua_pop(L, 1);
        class Packet *pkt = raw_check_packet(L, conn_id, Socket::CT_SSCN);
        if (!pkt)
        {
            ELOG("srv_multicast conn not found:%ud", conn_id);
            continue;
        }

        if (pkt->raw_pack_ss(cmd, ecode, _session, buffer, len) < 0)
        {
            ELOG("srv_multicast can not raw_pack_ss:%ud", conn_id);
            continue;
        }
    }

    encoder->finalize();

    PKT_STAT_ADD(SPT_SSPK, cmd, int32_t(len + sizeof(struct s2s_header)),
                 STAT_TIME_END());
    return 0;
}

/* 网关进程广播数据到客户端
 * clt_multicast( conn_list,codec_type,cmd,errno,pkt )
 */
int32_t LNetworkMgr::clt_multicast(lua_State *L)
{
    STAT_TIME_BEG();
    if (!lua_istable(L, 1))
    {
        return luaL_error(L, "expect table,got %s",
                          lua_typename(L, lua_type(L, 1)));
    }
    int32_t codec_ty = luaL_checkinteger32(L, 2);
    uint16_t cmd     = static_cast<uint16_t>(luaL_checkinteger(L, 3));
    uint16_t ecode   = static_cast<uint16_t>(luaL_checkinteger(L, 4));
    if (codec_ty < Codec::CT_NONE || codec_ty >= Codec::CT_MAX)
    {
        return luaL_error(L, "illegal codec type");
    }

    if (!lua_istable(L, 5))
    {
        return luaL_error(L, "expect table,got %s",
                          lua_typename(L, lua_type(L, 5)));
    }

    const CmdCfg *cfg = get_sc_cmd(cmd);
    if (!cfg)
    {
        return luaL_error(L, "no command conf found: %d", (int32_t)cmd);
    }

    Codec *encoder = StaticGlobal::codec_mgr()->get_codec(
        static_cast<Codec::CodecType>(codec_ty));
    if (!cfg)
    {
        return luaL_error(L, "no codec conf found: %d", (int32_t)cmd);
    }

    const char *buffer = nullptr;
    int32_t len        = encoder->encode(L, 5, &buffer, cfg);
    if (len < 0)
    {
        encoder->finalize();
        ELOG("clt_multicast encode error");
        return 0;
    }

    if (len > MAX_PACKET_LEN)
    {
        encoder->finalize();
        return luaL_error(L, "buffer size over MAX_PACKET_LEN");
    }

    lua_pushnil(L); /* first key */
    while (lua_next(L, 1) != 0)
    {
        if (!lua_isinteger(L, -1))
        {
            lua_pop(L, 1);
            encoder->finalize();
            return luaL_error(L, "conn list expect integer");
        }

        uint32_t conn_id = static_cast<uint32_t>(lua_tointeger(L, -1));

        lua_pop(L, 1);
        class Packet *pkt = raw_check_packet(L, conn_id, Socket::CT_SSCN);
        if (!pkt)
        {
            ELOG("clt_multicast conn not found:%ud", conn_id);
            continue;
        }

        if (pkt->raw_pack_clt(cmd, ecode, buffer, len) < 0)
        {
            ELOG("clt_multicast can not raw_pack_ss:%ud", conn_id);
            continue;
        }
    }

    encoder->finalize();

    PKT_STAT_ADD(SPT_SCPK, cmd, int32_t(len + sizeof(struct s2c_header)),
                 STAT_TIME_END());

    return 0;
}

/* 非网关数据广播数据到客户端
 * ssc_multicast( conn_id,mask,conn_list or args_list,codec_type,cmd,errno,pkt )
 */
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
