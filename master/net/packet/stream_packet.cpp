#include "stream_packet.h"


/* 网络通信消息包头格式定义
 */

/* 根据一个header指针获取整个packet的长度(包括_length本身) */
#define PACKET_LENGTH( h ) ((h)->_length + sizeof(packet_length))

/* 根据一个header和buff长度获取header的_length字段值 */
#define PACKET_MAKE_LENGTH( h,l )   \
    static_cast<packet_length>(sizeof(h) + l - sizeof(packet_length))

/* 根据一个header指针获取header后buffer的长度 */
#define PACKET_BUFFER_LEN( h )    \
    ((h)->_length - sizeof(*h) + sizeof(packet_length))


typedef enum
{
    PKT_NONE = 0,  // invalid
    PKT_CSPK = 1,  // c2s packet
    PKT_SCPK = 2,  // s2c packet
    PKT_SSPK = 3,  // s2s packet
    PKT_RPCS = 4,  // rpc send packet
    PKT_RPCR = 5,  // rpc return packet
    PKT_CBCP = 6,  // client broadcast packet
    PKT_SBCP = 7,  // server broadcast packet

    PKT_MAXT       // max packet type
} packet_t;

#pragma pack (push, 1)

typedef uint16 array_header;
typedef uint16 packet_length;
typedef uint16 string_header;
typedef int32  owner_t;

/* !!!!!!!!! 这里的数据结构必须符合POD结构 !!!!!!!!! */

struct base_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint16  _cmd  ; /* 8bit模块号,8bit功能号 */
}

/* 客户端发往服务器 */
struct c2s_header : public base_header
{
};

/* 服务器发往客户端 */
struct s2c_header : public base_header
{
    uint16 _errno ; /* 错误码 */
};

/* 服务器发往服务器 */
struct s2s_header : public base_header
{
    uint16  _errno ; /* 错误码 */
    uint16  _packet; /* 数据包类型，见packet_t */
    uint16  _codec ; /* 解码类型，见codec::codec_t */
    owner_t _owner ; /* 当前数据包所属id，通常为玩家id */
};

#pragma pack(pop)


stream_packet::stream_packet( class socket *sk )
    : packet( sk )
{
}

stream_packet::~stream_packet()
{
}


int32 stream_packet::pack()
{
    return 0;
}

/* 解析二进制流数据包 */
int32 stream_packet::unpack()
{
    class buffer &recv = _socket->recv_buffer();

    uint32 size = _recv.data_size();
    if ( size < sizeof( struct base_header ) ) return 0;

    const struct base_header *header = 
        reinterpret_cast<const struct base_header *>( _recv.data() );
    if ( size < header->_length ) return 0;

    // 数据包完整，派发处理

    return header->_length;
}

void dispatch( const struct base_header *header )
{
    switch( _socket->conn_type() )
    {
        case socket::CNT_CSCN: /* 解析服务器发往客户端的包 */
            sc_command( reinterpret_cast<struct s2c_header *>( header ) );
            break;
        case socket::CNT_SCCN: /* 解析客户端发往服务器的包 */
            cs_dispatch( reinterpret_cast<struct c2s_header *>( header ) );
            break;
        case socket::CNT_SSCN: /* 解析服务器发往服务器的包 */
            break;
        default :
            ERROR("stream_packet dispatch "
                "unknow connect type:%d",_socket->conn_type());
            return;
    }
}

/* 服务器发往客户端数据包回调脚本 */
void stream_packet::sc_command( const struct base_header *s2c_header )
{
    assert( "lua stack dirty",0 == lua_gettop(L) );

    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    const cmd_cfg_t *cmd_cfg = network_mgr->get_sc_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "s2c cmd(%d) cfg found",header->_cmd );
        return;
    }

    int32 size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    uint32 conn_id = _socket->conn_id();

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"sc_command_new" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    int32 cnt = codec::decode( L,_socket->codec_type(),buffer,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_pop( L,5 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "sc_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 派发客户端发给服务器数据包 */
void stream_packet::cs_dispatch( const struct c2s_header *header )
{
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    const cmd_cfg_t *cmd_cfg = network_mgr->get_cs_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "c2s cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    if ( cmd_cfg->_session != network_mgr->curr_session() )
    {
        clt_forwarding( header,cmd_cfg->_session );
        return;
    }

    cs_command( header,cmd_cfg );/* 在当前进程处理 */
}

/* 转客户端数据包 */
void stream_packet::clt_forwarding( const c2s_header *header,int32 session )
{
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    class socket *dest_sk  = network_mgr->get_connection( session );
    if ( !dest_sk )
    {
        ERROR( "client "
            "packet forwarding no destination found.cmd:%d",header->_cmd );
        return;
    }

    size_t size = PACKET_LENGTH( header );
    class buffer &send = dest_sk->send_buffer();
    if ( !send.reserved( size + sizeof(struct s2s_header) ) )
    {
        ERROR( "client packet forwarding,can not "
            "reserved memory:%ld",int64(size + sizeof(struct s2s_header)) );
        return;
    }

    struct s2s_header s2sh;
    s2sh._length = PACKET_MAKE_LENGTH( struct s2s_header,size );
    s2sh._cmd    = 0;
    s2sh._packet = packet::PKT_CSPK;
    s2sh._codec  = _socket->codec_type();
    s2sh._owner  = get_owner( conn_id );

    send.__append( &s2sh,sizeof(struct s2s_header) );
    send.__append( header,size );

    dest_sk->pending_send();
}

/* 客户端发往服务器数据包回调脚本 */
void stream_packet::cs_command(
    const c2s_header *header,const cmd_cfg_t *cmd_cfg )
{
    assert( "lua stack dirty",0 == lua_gettop(L) );
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    int32 size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    int32 conn_id = _socket->conn_id();
    owner_t owner = network_mgr->get_owner( conn_id )

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"cs_command_new" );
    lua_pushinteger  ( L,conn_id );
    lua_pushinteger  ( L,owner   );
    lua_pushinteger  ( L,header->_cmd );

    int32 cnt = codec::decode( L,_socket->codec_type(),buffer,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_pop( L,5 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "cs_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}


/* 处理服务器之间数据包 */
void stream_packet::process_ss_command( const s2s_header *header )
{
    /* 先判断数据包类型 */
    switch ( header->_packet )
    {
        case packet::PKT_SSPK : ss_dispatch( header );break;
        case packet::PKT_CSPK : css_command( header );return;
        case packet::PKT_SCPK : ssc_command( header );return;
        case packet::PKT_RPCS : rpc_command( header );return;
        case packet::PKT_RPCR : rpc_return ( header );return;
        default :
        {
            ERROR( "unknow server "
                "packet:cmd %d,packet type %d",header->_cmd,header->_packet );
            return;
        }
    }
}

/* 派发服务器之间的数据包 */
void stream_packet::ss_dispatch( const s2s_header *header )
{
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    const cmd_cfg_t *cmd_cfg = network_mgr->get_ss_cmd( header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "s2s cmd(%d) no cmd cfg found",header->_cmd );
        return;
    }

    /* 这个指令不是在当前进程处理，自动转发到对应进程 */
    if ( cmd_cfg->_session != network_mgr->curr_session() )
    {
        class socket *dest_sk  = network_mgr->get_connection( cmd_cfg->_session );
        if ( !dest_sk )
        {
            ERROR( "server packet forwarding "
                "no destination found.cmd:%d",header->_cmd );
            return;
        }

        bool is_ok = dest_sk->append( header,PACKET_LENGTH( header ) );
        if ( !is_ok )
        {
            ERROR( "server packet forwrding "
                "can not reserved memory:%ld",int64(PACKET_LENGTH( header )) );
        }
        return;
    }

    ss_command( header,cmd_cfg );
}

/* 服务器发往服务器数据包回调脚本 */
void stream_packet::ss_command(
    const s2s_header *header,const cmd_cfg_t *cmd_cfg )
{
    assert( "lua stack dirty",0 == lua_gettop( L ) );

    int32 size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"ss_command_new" );
    lua_pushinteger( L,_socket->conn_id() );
    lua_pushinteger( L,header->_owner );
    lua_pushinteger( L,header->_cmd );
    lua_pushinteger( L,header->_errno );

    int32 cnt = codec::decode( L,_socket->codec_type(),buffer,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_pop( L,6 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,4 + cnt,0,1 ) ) )
    {
        ERROR( "ss_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}

/* 客户端发往服务器，由网关转发的数据包回调脚本 */
void stream_packet::css_command( const c2s_header *header )
{
    assert( "lua stack dirty",0 == lua_gettop(L) );
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    const struct c2s_header *clt_header = 
        reinterpret_cast<const struct c2s_header *>( header + 1 );
    const cmd_cfg_t *cmd_cfg = network_mgr->get_cs_cmd( clt_header->_cmd );
    if ( !cmd_cfg )
    {
        ERROR( "c2s cmd(%d) no cmd cfg found",clt_header->_cmd );
        return;
    }

    int32 size = PACKET_BUFFER_LEN( clt_header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( clt_header + 1 );

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"css_command_new" );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushinteger  ( L,header->_owner   );
    lua_pushinteger  ( L,clt_header->_cmd );

    int32 cnt = codec::decode( L,header->_codec,buffer,size,cmd_cfg );
    if ( cnt < 0 )
    {
        lua_pop( L,5 );
        return;
    }

    if ( expect_false( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) ) )
    {
        ERROR( "css_command:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */
}


/* 解析其他服务器转发到网关的客户端数据包 */
void stream_packet::ssc_command( const s2s_header *header )
{
    static const class lnetwork_mgr *network_mgr = lnetwork_mgr::instance();

    class socket *sk = network_mgr->get_connection_by_owner( header->_owner );
    if ( !sk )
    {
        ERROR( "ssc packet no clt connect found" );
        return;
    }

    if ( socket::CNT_SCCN != sk->conn_type() )
    {
        ERROR( "ssc packet destination conn is not a clt" );
        return;
    }

    class buffer &send = sk->send_buffer();
    bool is_ok = send.append( header + 1,PACKET_BUFFER_LEN(header) );
    if ( !is_ok )
    {
        ERROR( "ssc packet can not append buffer" );
        return;
    }
    sk->pending_send();
}


/* 处理rpc调用 */
void stream_packet::rpc_command( const s2s_header *header )
{
    assert( "lua stack dirty",0 == lua_gettop(L) );

    int32 size = PACKET_BUFFER_LEN( header );
    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );

    lua_pushcfunction( L,traceback );
    lua_getglobal    ( L,"rpc_command_new"  );
    lua_pushinteger  ( L,_socket->conn_id() );
    lua_pushinteger  ( L,header->_owner     );

    // rpc解析方式目前固定为bson
    int32 cnt = codec::decode( L,codec::CDC_BSON,buffer,size,NULL );
    if ( cnt < 1 ) // rpc调用至少要带参数名
    {
        lua_pop( L,4 + cnt );
        ERROR( "rpc command miss function name" );
        return;
    }

    int32 top = lua_gettop( L );
    int32 unique_id = static_cast<int32>( header->_owner );
    int32 ecode = lua_pcall( L,4 + cnt,LUA_MULTRET,1 );
    // unique_id是rpc调用的唯一标识，如果不为0，则需要返回结果
    if ( unique_id > 0 )
    {
        int32 ret = codec::encode( 
            L,unique_id,ecode,top,sk->send_buffer() );
        _socket->pending_send();
    }

    if ( LUA_OK != ecode )
    {
        ERROR( "rpc command:%s",lua_tostring(L,-1) );
        lua_pop( L,2 ); /* remove error object and traceback */
        return;
    }

    lua_pop( L,1 ); /* remove trace back */
}

/* 处理rpc返回 */
void stream_packet::rpc_return( const s2s_header *header )
{
    lua_State *L = lstate::instance()->state();
    assert( "lua stack dirty",0 == lua_gettop(L) );

    lua_pushcfunction( L,traceback );
    lua_getglobal( L,"rpc_command_return" );
    lua_pushinteger( L,conn_id );
    lua_pushinteger( L,header->_owner );
    lua_pushinteger( L,header->_errno );

    int32 cnt = packet::instance()->parse( L,header );
    if ( LUA_OK != lua_pcall( L,3 + cnt,0,1 ) )
    {
        ERROR( "process_rpc_return:%s",lua_tostring( L,-1 ) );

        lua_pop( L,2 ); /* remove traceback and error object */
        return;
    }
    lua_pop( L,1 ); /* remove traceback */

    return;
}
