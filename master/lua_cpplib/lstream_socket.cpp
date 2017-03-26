#include <lbson.h>
#include <lflatbuffers.hpp>

#include "lstream_socket.h"

#include "ltools.h"
#include "../net/packet.h"
#include "../ev/ev_def.h"

lstream_socket::lstream_socket( lua_State *L )
    : lsocket(L)
{

}

lstream_socket::~lstream_socket()
{
}

/* check if a message packet complete
 * all packet should start with a packet length(uint16)
 */
int32 lstream_socket::is_message_complete()
{
    uint32 sz = _recv.data_size();
    if ( sz < sizeof(packet_length) ) return 0;
    return sz >= *(reinterpret_cast<packet_length *>(_recv.data())) ? 1 : 0;
}

const class lsocket *lstream_socket::accept_new( int32 fd )
{
    class lstream_socket *_s = new class lstream_socket( L );

    _s->socket::set<lsocket,&lsocket::message_cb>( _s );
    _s->socket::start( fd,EV_READ );  /* 这里会设置fd */

    return static_cast<class lsocket *>( _s );
}

/* get next server message */
int32 lstream_socket::srv_next()
{
    uint32 sz = _recv.data_size();
    if ( sz < sizeof(struct s2s_header) ) return 0;

    struct s2s_header *header =
        reinterpret_cast<struct s2s_header *>( _recv.data() );

    if ( sz < header->_length + sizeof( packet_length ) ) return 0;

    lua_pushinteger( L,header->_cmd );
    return 1;
}

/* get next client message */
int32 lstream_socket::clt_next()
{
    uint32 sz = _recv.data_size();
    if ( sz < sizeof(struct c2s_header) ) return 0;

    struct c2s_header *header =
        reinterpret_cast<struct c2s_header *>( _recv.data() );

    if ( sz < header->_length + sizeof( packet_length ) ) return 0;

    lua_pushinteger( L,header->_cmd );
    return 1;
}

/* get client to server to server cmd */
int32 lstream_socket::css_cmd()
{
    uint32 sz = _recv.data_size();
    if ( sz < sizeof(struct s2s_header) + sizeof(struct c2s_header) ) return 0;

    struct c2s_header *header = reinterpret_cast<struct c2s_header *>(
                             _recv.data() + sizeof(struct s2s_header) );

    if ( sz < header->_length + 
        sizeof(struct s2s_header) + sizeof( packet_length ) ) return 0;

    lua_pushinteger( L,header->_cmd );
    return 1;
}

/* ssc_flatbuffers_send( lfb,srv_cmd,clt_cmd,schema,object,tbl ) */
int32 lstream_socket::ssc_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_cmd = luaL_checkinteger( L,2 );
    int32 clt_cmd = luaL_checkinteger( L,3 );

    const char *schema = luaL_checkstring( L,4 );
    const char *object = luaL_checkstring( L,5 );

    if ( !lua_istable( L,6 ) )
    {
        return luaL_error( L,
            "argument #6 expect table,got %s",lua_typename( L,lua_type(L,6) ) );
    }

    if ( (*lfb)->encode( L,schema,object,6 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    if ( sz > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( sz + 
        sizeof(struct s2c_header) + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct s2c_header s2ch;
    s2ch._length = static_cast<packet_length>(
        sz + sizeof(struct s2c_header) - sizeof(packet_length) );
    s2ch._cmd    = static_cast<uint16>  ( clt_cmd );

    struct s2s_header s2sh;
    s2sh._length = static_cast<packet_length>( sz + sizeof(struct s2c_header) 
                        + sizeof(struct s2s_header) - sizeof(packet_length) );
    s2sh._cmd    = static_cast<uint16>  ( srv_cmd );

    _send.__append( &s2sh,sizeof(struct s2s_header) );
    _send.__append( &s2ch,sizeof(struct s2c_header) );
    _send.__append( buffer,sz );

    pending_send();
    return 0;
}

/* 发送协议到客户端
 * sc_flatbuffers_send( lfb,srv_cmd,schema,object,errno,pkt )
 */
int32 lstream_socket::sc_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 clt_cmd = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    int32 _errno = luaL_checkinteger( L,5 );

    if ( !lua_istable( L,6 ) )
    {
        return luaL_error( L,
            "argument #6 expect table,got %s",lua_typename( L,lua_type(L,6) ) );
    }

    if ( (*lfb)->encode( L,schema,object,6 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    if ( sz > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( sz + 
        sizeof(struct s2c_header) + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct s2c_header s2ch;
    s2ch._length = static_cast<packet_length>(
        sz + sizeof(struct s2c_header) - sizeof(packet_length) );
    s2ch._cmd    = static_cast<uint16>  ( clt_cmd );
    s2ch._errno  = static_cast<uint16>  ( _errno  );

    _send.__append( &s2ch,sizeof(struct s2c_header) );
    _send.__append( buffer,sz );

    pending_send();
    return 0;
}

 /* server to server:ss_flatbuffers_send( lfb,cmd,schema,object,tbl ) */
int32 lstream_socket::ss_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_cmd = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    if ( !lua_istable( L,5 ) )
    {
        return luaL_error( L,
            "argument #5 expect table,got %s",lua_typename( L,lua_type(L,5) ) );
    }

    if ( (*lfb)->encode( L,schema,object,5 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    if ( sz > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( sz + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct s2s_header s2sh;
    s2sh._length = static_cast<packet_length>(
        sz + sizeof(struct s2s_header) - sizeof(packet_length) );
    s2sh._cmd    = static_cast<uint16>  ( srv_cmd );

    _send.__append( &s2sh,sizeof(struct s2s_header) );
    _send.__append( buffer,sz );

    pending_send();
    return 0;
}

/* decode server to server packet
 * ss_flatbuffers_decode( lfb,srv_cmd,schema,object )
 */
int32 lstream_socket::ss_flatbuffers_decode()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_cmd = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    uint32 sz = _recv.data_size();
    if ( sz < sizeof(struct s2s_header) )
    {
        return luaL_error( L, "incomplete packet header" );
    }

    struct s2s_header *ph = reinterpret_cast<struct s2s_header *>(_recv.data());

    /* 验证包长度，_length并不包含本身 */
    size_t len = ph->_length + sizeof( packet_length );
    if ( sz < len )
    {
        return luaL_error( L, "packet header broken" );
    }

    /* 协议号是否匹配 */
    if ( srv_cmd != ph->_cmd )
    {
        return luaL_error( L,
            "cmd valid fail,expect %d,got %d",srv_cmd,ph->_cmd );
    }

    // 先取出数据指针，再subtract
    const char *buffer = _recv.data() + sizeof( struct s2s_header );

    /* 删除buffer,避免luaL_error longjump影响 */
    _recv.subtract( len );

    if ( (*lfb)->decode( L,schema,object,buffer,len ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    return 1;
}

/* decode client to  server packet
 * ss_flatbuffers_decode( lfb,clt_cmd,schema,object )
 */
int32 lstream_socket::cs_flatbuffers_decode()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 clt_cmd = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    uint32 sz = _recv.data_size();
    if ( sz < sizeof(struct c2s_header) )
    {
        return luaL_error( L, "incomplete message header" );
    }

    struct c2s_header *ph = reinterpret_cast<struct c2s_header *>(_recv.data());

    /* 验证包长度，_length并不包含本身 */
    size_t len = ph->_length + sizeof( packet_length );
    if ( sz < len )
    {
        return luaL_error( L, "packet header broken" );
    }

    /* 协议号是否匹配 */
    if ( clt_cmd != ph->_cmd )
    {
        return luaL_error( L,
            "cmd valid fail,expect %d,got %d",clt_cmd,ph->_cmd );
    }

    // 先取出数据指针，再subtract
    const char *buffer = _recv.data() + sizeof( struct c2s_header );

    /* 删除buffer,避免luaL_error longjump影响 */
    _recv.subtract( len );

    if ( (*lfb)->decode( L,schema,object,buffer,len ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    return 1;
}

/* cs_flatbuffers_send( lfb,srv_cmd,schema,object,pkt ) */
int32 lstream_socket::cs_flatbuffers_send()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_cmd = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    if ( !lua_istable( L,5 ) )
    {
        return luaL_error( L,
            "argument #5 expect table,got %s",lua_typename( L,lua_type(L,5) ) );
    }

    if ( (*lfb)->encode( L,schema,object,5 ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    size_t sz = 0;
    const char *buffer = (*lfb)->get_buffer( sz );
    if ( sz > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( sz + 
        sizeof(struct c2s_header) + sizeof(struct c2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct c2s_header c2sh;
    c2sh._length = static_cast<packet_length>(
        sz + sizeof(struct c2s_header) - sizeof(packet_length) );
    c2sh._cmd    = static_cast<uint16>  ( srv_cmd );

    _send.__append( &c2sh,sizeof(struct c2s_header) );
    _send.__append( buffer,sz );

    pending_send();
    return 0;
}

/* css_flatbuffers_send( srv_cmd,clt_conn ) */
int32 lstream_socket::css_flatbuffers_send()
{
    int32 srv_cmd = luaL_checkinteger( L,1 );

    class lstream_socket** clt_conn =
        (class lstream_socket**)luaL_checkudata( L, 2, "Stream_socket" );
    if ( clt_conn == NULL || *clt_conn == NULL )
    {
        return luaL_error( L, "argument #2 expect Stream_socket" );
    }

    class buffer &clt_recv = (*clt_conn)->_recv;
    uint32 sz = clt_recv.data_size();
    if ( sz < sizeof(struct c2s_header) )
    {
        return luaL_error( L, "incomplete packet header" );
    }

    struct c2s_header *ph = 
        reinterpret_cast<struct c2s_header *>(clt_recv.data());

    /* 验证包长度，_length并不包含本身 */
    size_t len = ph->_length + sizeof( packet_length );
    if ( sz < len )
    {
        return luaL_error( L, "packet header broken" );
    }

    if ( len + sizeof(struct s2s_header) > USHRT_MAX )
    {
        return luaL_error( L,"buffer size over USHRT_MAX" );
    }

    if ( !_send.reserved( len + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"out of socket buffer" );
    }

    struct s2s_header s2sh;
    s2sh._length = static_cast<packet_length>( len
                        + sizeof(struct s2s_header) - sizeof(packet_length) );
    s2sh._cmd    = static_cast<uint16>  ( srv_cmd );

    _send.__append( &s2sh,sizeof(struct s2s_header) );
    _send.__append( clt_recv.data(),len );

    pending_send();
    return 0;
}


/* decode server to client packet
 * sc_flatbuffers_decode( lfb,srv_cmd,schema,object )
 */
int32 lstream_socket::sc_flatbuffers_decode()
{
    class lflatbuffers** lfb =
        (class lflatbuffers**)luaL_checkudata( L, 1, "lua_flatbuffers" );
    if ( lfb == NULL || *lfb == NULL )
    {
        return luaL_error( L, "argument #1 expect lua_flatbuffers" );
    }

    int32 srv_cmd = luaL_checkinteger( L,2 );

    const char *schema = luaL_checkstring( L,3 );
    const char *object = luaL_checkstring( L,4 );

    uint32 sz = _recv.data_size();
    if ( sz < sizeof(struct s2c_header) )
    {
        return luaL_error( L, "incomplete message header" );
    }

    struct s2c_header *ph = reinterpret_cast<struct s2c_header *>(_recv.data());

    /* 验证包长度，_length并不包含本身 */
    size_t len = ph->_length + sizeof( packet_length );
    if ( sz < len )
    {
        return luaL_error( L, "packet header broken" );
    }

    /* 协议号是否匹配 */
    if ( srv_cmd != ph->_cmd )
    {
        return luaL_error( L,
            "cmd valid fail,expect %d,got %d",srv_cmd,ph->_cmd );
    }

    // 先取出数据指针，再subtract
    const char *buffer = _recv.data() + sizeof( struct s2c_header );

    /* 删除buffer,避免luaL_error longjump影响 */
    _recv.subtract( len );

    lua_pushinteger( L,ph->_errno );
    if ( (*lfb)->decode( L,schema,object,buffer,len ) < 0 )
    {
        return luaL_error( L,(*lfb)->last_error() );
    }

    return 2;
}

/* css_flatbuffers_decode( lfb,srv_cmd,clt_conn ) */
int32 lstream_socket::css_flatbuffers_decode()
{
    return 1;
}

int32 lstream_socket::rpc_send()
{
    int32 rpc_cmd = luaL_checkinteger( L,1 );

    struct error_collector ec;
    ec.what[0] = 0;

    bson_t *doc = bson_new();
    if ( 0 != lbs_do_encode_stack( L,doc,2,&ec ) ) // do not encode rpc_cmd
    {
        bson_destroy( doc );
        return luaL_error( L,ec.what );
    }

    const char *buffer = (const char *)bson_get_data( doc );

    struct s2s_header s2sh;
    s2sh._length = static_cast<packet_length>( doc->len +
                        sizeof(struct s2s_header) - sizeof(packet_length) );
    s2sh._cmd    = static_cast<uint16>  ( rpc_cmd );

    _send.__append( &s2sh,sizeof(struct s2s_header) );
    _send.__append( buffer,static_cast<uint32>(doc->len) );

    bson_destroy( doc );

    pending_send();
    return 0;
}

int32 lstream_socket::rpc_decode()
{
    int32 rpc_cmd = luaL_checkinteger( L,1 );
    int32 rpc_res = luaL_checkinteger( L,2 );
    /* the 3th one is a lua function to be invoked */
    if ( !lua_isfunction( L,3 ) )
    {
        return luaL_error( L,
            "argument #3 expect function,got %s",
            lua_typename( L,lua_type(L,3) ) );
    }
    /* the last one is self,if there is one. */
    int32 oldtop = lua_gettop( L );

    uint32 sz = _recv.data_size();
    if ( sz < sizeof(struct s2s_header) )
    {
        return luaL_error( L, "incomplete message header" );
    }

    struct s2s_header *ph = reinterpret_cast<struct s2s_header *>(_recv.data());

    /* 验证包长度，_length并不包含本身 */
    size_t len = ph->_length + sizeof( packet_length );
    if ( sz < len )
    {
        return luaL_error( L, "packet header broken" );
    }

    /* 协议号是否匹配 */
    if ( rpc_cmd != ph->_cmd )
    {
        return luaL_error( L,
            "cmd valid fail,expect %d,got %d",rpc_cmd,ph->_cmd );
    }

    // 先取出数据指针，再subtract
    const char *buffer = _recv.data() + sizeof( struct s2s_header );

    /* 删除buffer,避免luaL_error longjump影响 */
    _recv.subtract( len );

    bson_reader_t *reader = 
        bson_reader_new_from_data( (const uint8_t *)buffer,sz );

    const bson_t *doc = bson_reader_read( reader,NULL );
    if ( !doc )
    {
        bson_reader_destroy( reader );
        return luaL_error( L,"invalid bson buffer" );
    }

    struct error_collector ec;
    ec.what[0] = 0;
    int num = lbs_do_decode_stack( L,doc,&ec );

    bson_reader_destroy( reader );
    if ( num < 0 ) return luaL_error( L,ec.what );

    /* the rpc call should have function name and unique_id */
    if ( num < 2 )
    {
        return luaL_error( L,"rpc call miss function name and unique_id" );
    }

    if ( !lua_isinteger( L,oldtop + 2) )
    {
        return luaL_error( L,"rpc call miss unique_id should be integer" );
    }

    return rpc_call( 3,oldtop,rpc_res );
}

int32 lstream_socket::rpc_call( int32 index,int32 oldtop,int32 rpc_res )
{
    // index is the position of rpc invoke function
    assert( "rpc call,invoke function "
        "should below parameters in stack",index <= oldtop );

    int32 top = lua_gettop( L );
    assert( "rpc call:too many parameters",top < 32 );

    int32 unique_id = lua_tointeger( L,oldtop + 2 );

    /* set error call back function */
    lua_pushcfunction( L,traceback );
    lua_insert( L,index );

    int32 code = lua_pcall( L,top - index,LUA_MULTRET,index );

    /* have to response */
    if ( unique_id > 0 )
    {
        bson_t *doc = bson_new();
        BSON_APPEND_INT32( doc,"0",unique_id );

        if ( LUA_OK == code )
        {
            BSON_APPEND_INT32( doc,"1",0 );

            struct error_collector ec;
            ec.what[0] = 0;
            if ( 0 != lbs_do_encode_stack( L,doc,top + 1,&ec ) )
            {
                bson_destroy( doc );
                return luaL_error( L,ec.what );
            }

        }
        else
        {
            BSON_APPEND_INT32( doc,"1",-1 );
        }
        
        const char *buffer = (const char *)bson_get_data( doc );

        struct s2s_header s2sh;
        s2sh._length = static_cast<packet_length>( doc->len +
                        sizeof(struct s2s_header) - sizeof(packet_length) );
        s2sh._cmd    = static_cast<uint16>  ( rpc_res );

        _send.__append( &s2sh,sizeof(struct s2s_header) );
        _send.__append( buffer,doc->len );

        bson_destroy( doc );

        pending_send();
    }

    if ( LUA_OK != code )  return luaL_error( L,lua_tostring(L,-1) );

    return 0;
}