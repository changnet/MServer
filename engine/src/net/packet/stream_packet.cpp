#include "stream_packet.hpp"

#include "../../lua_cpplib/ltools.hpp"
#include "../../system/static_global.hpp"
#include "../socket.hpp"
#include "../codec/luabin_codec.hpp"

StreamPacket::StreamPacket(class Socket *sk) : Packet(sk)
{
}

StreamPacket::~StreamPacket()
{
}

int32_t StreamPacket::unpack(Buffer &buffer)
{
    // 检测包头是否完整
    const char *buf = buffer.to_flat_ctx(sizeof(struct base_header));
    if (!buf) return 0;

    const struct base_header *header =
        reinterpret_cast<const struct base_header *>(buf);

    // 检测包内容是否完整
    uint32_t length = header->_length;
    if (!buffer.check_used_size(length)) return 0;

    header =
        reinterpret_cast<const struct base_header *>(buffer.to_flat_ctx(length));

    dispatch(header);      // 数据包完整，派发处理
    bool next = buffer.remove(length); // 无论成功或失败，都移除该数据包

    return next ? 1 : 0;
}

void StreamPacket::dispatch(const struct base_header *header)
{
    static lua_State *L = StaticGlobal::state();

    assert(0 == lua_gettop(L));

    int32_t conn_id = _socket->conn_id();

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "command_new");
    lua_pushinteger(L, conn_id);
    lua_pushlightuserdata(L, const_cast<struct base_header *>(header));

    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, 2, 0, 1)))
    {
        ELOG("%s", lua_tostring(L, -1));

        lua_settop(L, 0); /* remove traceback and error object */
        return;
    }
    lua_settop(L, 0); /* remove traceback */
}

#ifdef OLDNET
void StreamPacket::rpc_command(const s2s_header *header)
{
    /**
     * TODO protobuf的codec是在lua做的，一是方便更换编码方案，二是外部的protobuf库接口是lua的
     * rpc要处理返回值，如果在lua做需要使用table.pack和unpack处理返回值，效率比较低
    */
    static lua_State *L = StaticGlobal::state();
    assert(0 == lua_gettop(L));

    size_t size = PACKET_BUFFER_LEN(header);
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>(header + 1);

    LUA_PUSHTRACEBACK(L);
    int32_t top = lua_gettop(L); // pcall后，下面的栈都会被弹出

    lua_getglobal(L, "rpc_command_new");
    lua_pushinteger(L, _socket->conn_id());
    lua_pushinteger(L, header->_owner);

    LuaBinCodec *decoder = StaticGlobal::lua_bin_codec();
    int32_t cnt    = decoder->decode(L, buffer, size, nullptr);
    if (cnt < 1) // rpc调用至少要带函数名
    {
        lua_settop(L, 0);
        ELOG("rpc command miss function name");
        return;
    }

    int32_t unique_id = static_cast<int32_t>(header->_owner);
    int32_t ecode     = lua_pcall(L, 2 + cnt, LUA_MULTRET, 1);
    // unique_id是rpc调用的唯一标识，如果不为0，则需要返回结果
    if (unique_id > 0)
    {
        do_pack_rpc(L, unique_id, LUA_OK == ecode ? 0 : 0xFFFF, SPT_RPCR,
                    top + 1);
    }

    if (LUA_OK != ecode)
    {
        ELOG("rpc command:%s", lua_tostring(L, -1));
        lua_settop(L, 0); /* remove error object and traceback */
        return;
    }

    lua_settop(L, 0); /* remove trace back */
}

void StreamPacket::rpc_return(const s2s_header *header)
{
    static lua_State *L = StaticGlobal::state();
    assert(0 == lua_gettop(L));

    size_t size        = PACKET_BUFFER_LEN(header);
    const char *buffer = reinterpret_cast<const char *>(header + 1);

    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "rpc_command_return");
    lua_pushinteger(L, _socket->conn_id());
    lua_pushinteger(L, header->_owner);
    lua_pushinteger(L, header->_errno);

    // rpc在出错的情况下仍返回，这时buff可能无法成功打包
    int32_t cnt = 0;
    if (size > 0)
    {
        cnt = StaticGlobal::lua_bin_codec()->decode(L, buffer, size, nullptr);
    }
    if (LUA_OK != lua_pcall(L, 3 + cnt, 0, 1))
    {
        ELOG("rpc_return:%s", lua_tostring(L, -1));

        lua_settop(L, 0); /* remove traceback and error object */
        return;
    }
    lua_settop(L, 0); /* remove traceback */

    return;
}

/* 打包rpc数据包 */
int32_t StreamPacket::do_pack_rpc(lua_State *L, int32_t unique_id,
                                  uint16_t ecode, uint16_t pkt, int32_t index)
{
    STAT_TIME_BEG();

    int32_t len        = 0;
    const char *buffer = nullptr;
    LuaBinCodec *encoder = StaticGlobal::lua_bin_codec();

    if (LUA_OK == ecode)
    {
        len = encoder->encode(L, index, &buffer, nullptr);
        if (len < 0)
        {
            // 发送时，出错就不发了
            // 返回时，出错也应该告知另一进程出错了
            if (SPT_RPCS == pkt) return -1;
            len   = 0;
            ecode = 0xFFFF - 1;
        }
    }

    struct s2s_header s2sh;
    SET_HEADER_LENGTH(s2sh, len, 0, SET_LENGTH_FAIL_RETURN);
    s2sh._cmd    = ecode;
    s2sh._packet = pkt;
    s2sh._owner = unique_id;

    _socket->append(&s2sh, sizeof(struct s2s_header));
    if (len > 0)
    {
        _socket->append(buffer, static_cast<uint32_t>(len));
    }
    _socket->flush();

    encoder->finalize();

    // rcp返回结果也是走这里，但是返回是不包含rpc函数名的
    if (SPT_RPCS == pkt && lua_isstring(L, index))
    {
        RPC_STAT_ADD(lua_tostring(L, index), (int32_t)s2sh._length,
                     STAT_TIME_END());
    }

    return 0;
}

/* 打包服务器发往客户端数据包 */
int32_t StreamPacket::pack_clt(lua_State *L, int32_t index)
{
    STAT_TIME_BEG();
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    int32_t cmd = luaL_checkinteger32(L, index);



    const char *buffer = luaL_checkludata(L, index + 1);
    int32_t len        = luaL_checkinteger(L, index + 2);
    if (len < 0) return -1;

    if (len > MAX_PACKET_LEN)
    {
        return luaL_error(L, "buffer size over MAX_PACKET_LEN");
    }

    if (raw_pack_clt(cmd, ecode, buffer, len) < 0)
    {
        return luaL_error(L, "can not raw pack clt");
    }

    PKT_STAT_ADD(SPT_SCPK, cmd, int32_t(len + sizeof(struct s2c_header)),
                 STAT_TIME_END());
    return 0;
}


/* 打包客户端发往服务器数据包 */
int32_t StreamPacket::pack_srv(lua_State *L, int32_t index)
{
    STAT_TIME_BEG();

    int32_t cmd = luaL_checkinteger32(L, index);

    const char *buffer = luaL_checkludata(L, index + 1);
    int32_t len        = luaL_checkinteger32(L, index + 2);
    if (len < 0) return -1;

    struct c2s_header c2sh;
    SET_HEADER_LENGTH(c2sh, len, cmd, SET_LENGTH_FAIL_ENCODE);
    c2sh._cmd = static_cast<uint16_t>(cmd);

    _socket->append(&c2sh, sizeof(c2sh));
    if (len > 0) _socket->append(buffer, len);

    _socket->flush();

    PKT_STAT_ADD(SPT_CSPK, cmd, int32_t(c2sh._length), STAT_TIME_END());

    return 0;
}

int32_t StreamPacket::pack_ss(lua_State *L, int32_t index)
{
    STAT_TIME_BEG();

    int32_t cmd = luaL_checkinteger32(L, index);

    const char *buffer = luaL_checkludata(L, index + 1);
    int32_t len        = luaL_checkinteger(L, index + 2);
    if (len < 0) return -1;

    if (len > MAX_PACKET_LEN)
    {
        encoder->finalize();
        return luaL_error(L, "buffer size over MAX_PACKET_LEN");
    }

    int32_t session = network_mgr->get_curr_session();
    if (raw_pack_ss(cmd, ecode, session, buffer, len) < 0)
    {
        return luaL_error(L, "can not raw_pack_ss");
    }

    PKT_STAT_ADD(SPT_SSPK, cmd, int32_t(len + sizeof(struct s2s_header)),
                 STAT_TIME_END());
    return 0;
}

int32_t StreamPacket::pack_rpc(lua_State *L, int32_t index)
{
    int32_t unique_id = luaL_checkinteger32(L, index);
    // ecode默认0
    return do_pack_rpc(L, unique_id, 0, SPT_RPCS, index + 1);
}

int32_t StreamPacket::pack_ssc(lua_State *L, int32_t index)
{
    STAT_TIME_BEG();

    Owner owner      = static_cast<Owner>(luaL_checkinteger(L, index));
    int32_t cmd      = luaL_checkinteger32(L, index + 1);

    const char *buffer = luaL_checkludata(L, index + 2);
    int32_t len        = luaL_checkinteger(L, index + 3);
    if (len < 0) return -1;

    /* 把客户端数据包放到服务器数据包 */
    struct s2s_header s2sh;
    SET_HEADER_LENGTH(s2sh, len, cmd, SET_LENGTH_FAIL_ENCODE);
    s2sh._cmd   = static_cast<uint16_t>(cmd);
    s2sh._errno = ecode;
    s2sh._owner = owner;
    s2sh._codec = Codec::CT_NONE; /* 避免valgrind警告内存未初始化 */
    s2sh._packet = SPT_SCPK; /*指定数据包类型为服务器发送客户端 */

    _socket->append(&s2sh, sizeof(s2sh));
    if (len > 0) _socket->append(buffer, len);

    _socket->flush();

    PKT_STAT_ADD(SPT_SSPK, cmd, int32_t(s2sh._length), STAT_TIME_END());

    return 0;
}

int32_t StreamPacket::raw_pack_clt(int32_t cmd, uint16_t ecode, const char *ctx,
                                   size_t size)
{
    /* 先构造客户端收到的数据包 */
    struct s2c_header s2ch;
    SET_HEADER_LENGTH(s2ch, size, cmd, SET_LENGTH_FAIL_RETURN);
    s2ch._cmd   = static_cast<uint16_t>(cmd);
    s2ch._errno = ecode;

    _socket->append(&s2ch, sizeof(s2ch));
    if (size > 0) _socket->append(ctx, size);

    _socket->flush();
    return 0;
}

int32_t StreamPacket::raw_pack_ss(int32_t cmd, uint16_t ecode, int32_t session,
                                  const char *ctx, size_t size)
{
    struct s2s_header s2sh;
    SET_HEADER_LENGTH(s2sh, size, cmd, SET_LENGTH_FAIL_RETURN);
    s2sh._cmd    = static_cast<uint16_t>(cmd);
    s2sh._owner  = session;
    s2sh._packet = SPT_SSPK;

    _socket->append(&s2sh, sizeof(s2sh));
    if (size > 0) _socket->append(ctx, size);

    _socket->flush();
    return 0;
}

// 打包客户端广播数据
// ssc_multicast( conn_id,mask,args_list,codec_type,cmd,errno,pkt )
int32_t StreamPacket::pack_ssc_multicast(lua_State *L, int32_t index)
{
    STAT_TIME_BEG();

    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    Owner list[MAX_CLT_CAST] = {0};
    int32_t mask             = luaL_checkinteger32(L, index);
    int32_t cmd              = luaL_checkinteger32(L, index + 2);
    const char *buffer = luaL_checkludata(L, index + 3);
    int32_t len        = luaL_checkinteger(L, index + 4);
    if (len < 0) return -1;

    lUAL_CHECKTABLE(L, index + 1);
    lUAL_CHECKTABLE(L, index + 5);

    // 占用list的两个位置，这样写入socket缓存区时不用另外处理
    list[0] = mask;
    // list[1] = 0; 数量
    int32_t idx = 2;
    lua_pushnil(L); /* first key */
    while (lua_next(L, index + 1) != 0)
    {
        if (!lua_isinteger(L, -1))
        {
            lua_pop(L, 1);
            return luaL_error(L, "ssc_multicast list expect integer");
        }

        if (idx >= MAX_CLT_CAST)
        {
            ELOG("pack_ssc_multicast too many id in list:%d", idx);
            lua_pop(L, 2);
            break;
        }
        // 以后可能定义owner_t为unsigned类型，当传参数而不是owner时，不要传负数
        Owner owner = static_cast<Owner>(lua_tointeger(L, -1));
        list[idx++] = owner;
        lua_pop(L, 1);
    }

    list[1]         = idx - 2; // 数量
    size_t list_len = sizeof(Owner) * idx;

    /* 把客户端数据包放到服务器数据包 */
    struct s2s_header s2sh;
    SET_HEADER_LENGTH(s2sh, list_len + len, cmd, SET_LENGTH_FAIL_ENCODE);
    s2sh._cmd   = static_cast<uint16_t>(cmd);
    s2sh._owner = 0;
    s2sh._packet = SPT_CBCP; /*指定数据包类型为服务器发送客户端 */

    _socket->append(&s2sh, sizeof(s2sh));
    _socket->append(list, list_len);
    if (len > 0) _socket->append(buffer, len);

    encoder->finalize();
    _socket->flush();

    PKT_STAT_ADD(SPT_CBCP, cmd, int32_t(s2sh._length), STAT_TIME_END());

    return 0;
}

// 转发到一个客户端
void StreamPacket::ssc_one_multicast(Owner owner, int32_t cmd, uint16_t ecode,
                                     const char *ctx, size_t size)
{
    static const class LNetworkMgr *network_mgr = StaticGlobal::network_mgr();

    class Socket *sk = network_mgr->get_conn_by_owner(owner);
    if (!sk)
    {
        ELOG("ssc_one_multicast no clt connect found");
        return;
    }

    // 这个客户端已经断开连接
    if (sk->is_closed()) return;

    if (Socket::CT_SCCN != sk->conn_type())
    {
        ELOG("ssc_one_multicast destination conn is not a clt");
        return;
    }
    class Packet *sk_packet = sk->get_packet();
    if (!sk_packet)
    {
        ELOG("ssc_one_multicast no packet found");
        return;
    }
    sk_packet->raw_pack_clt(cmd, ecode, ctx, size);
}

// 处理其他进程发过来的客户端广播
void StreamPacket::ssc_multicast(const s2s_header *header)
{
    const Owner *raw_list = reinterpret_cast<const Owner *>(header + 1);
    int32_t mask          = static_cast<int32_t>(*raw_list);
    int32_t count         = static_cast<int32_t>(*(raw_list + 1));
    if (MAX_CLT_CAST < count)
    {
        ELOG("ssc_multicast too many to cast:%d", count);
        return;
    }

    // 长度记得包含mask和count本身这两个变量
    size_t size         = PACKET_BUFFER_LEN(header);
    size_t raw_list_len = sizeof(Owner) * (count + 2);
    const char *ctx     = reinterpret_cast<const char *>(header + 1);

    if (size < raw_list_len)
    {
        ELOG("ssc_multicast packet length broken");
        return;
    }

    ctx += raw_list_len;
    size -= raw_list_len;

    // 根据玩家pid广播，底层直接处理
    if (CLT_MC_OWNER == mask)
    {
        for (int32_t idx = 0; idx < count; idx++)
        {
            ssc_one_multicast(*(raw_list + idx + 2), header->_cmd,
                              header->_errno, ctx, size);
        }
        return;
    }

    // 如果不是根据pid广播，那么这个list就是自定义参数，参数不能太多
    if (count > 16) // 限制一下参数，防止lua栈溢出
    {
        ELOG("ssc_multicast too many argument");
        return;
    }

    static lua_State *L = StaticGlobal::state();
    assert(0 == lua_gettop(L));

    // 根据参数从lua获取对应的玩家id
    LUA_PUSHTRACEBACK(L);
    lua_getglobal(L, "clt_multicast_new");
    lua_pushinteger(L, mask);
    for (int32_t idx = 2; idx < count + 2; idx++)
    {
        lua_pushinteger(L, *(raw_list + idx));
    }
    if (EXPECT_FALSE(LUA_OK != lua_pcall(L, count + 1, 1, 1)))
    {
        ELOG("clt_multicast_new:%s", lua_tostring(L, -1));

        lua_settop(L, 0); /* remove traceback and error object */
        return;
    }
    if (!lua_istable(L, -1))
    {
        ELOG("clt_multicast_new do NOT return a table");
        lua_settop(L, 0);
        return;
    }
    lua_pushnil(L); /* first key */
    while (lua_next(L, -2) != 0)
    {
        if (!lua_isinteger(L, -1))
        {
            lua_settop(L, 0);
            ELOG("ssc_multicast list expect integer");
            return;
        }
        Owner owner = static_cast<Owner>(lua_tointeger(L, -1));
        ssc_one_multicast(owner, header->_cmd, header->_errno, ctx, size);

        lua_pop(L, 1);
    }
    lua_settop(L, 0); /* remove traceback */
}
#endif
