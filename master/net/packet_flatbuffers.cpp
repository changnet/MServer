#include <lflatbuffers.hpp>
#include "../global/assert.h"

/* 加载该目录下的有schema文件 */
int32 packet::load_schema( const char *path )
{
    return _lflatbuffers->load_bfbs_path( path );
}

/* 解析数据包
 * 返回push到栈上的参数数量
 */
template<class T>
int32 raw_parse( lflatbuffers *_lflatbuffers,
    lua_State *L,const char *schema,const char *object,const T *header )
{
    int32 size = PACKET_BUFFER_LEN( header );
    if ( size <= 0 || size > MAX_PACKET_LEN )
    {
        ERROR( "illegal packet buffer length" );
        return -1;
    }

    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );
    if ( _lflatbuffers->decode( L,schema,object,buffer,size ) < 0 )
    {
        ERROR( "decode cmd(%d):%s",header->_cmd,_lflatbuffers->last_error() );
        return -1;
    }

    return 1;
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const c2s_header *header )
{
    return raw_parse( _lflatbuffers,L,schema,object,header );
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const s2s_header *header )
{
    return raw_parse( _lflatbuffers,L,schema,object,header );
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const s2c_header *header )
{
    return raw_parse( _lflatbuffers,L,schema,object,header );
}

/* c2s打包接口 */
int32 packet::unparse_c2s( lua_State *L,int32 index,
    int32 cmd,const char *schema,const char *object,class buffer &send )
{
    if ( _lflatbuffers->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,_lflatbuffers->last_error() );
    }

    size_t size = 0;
    const char *buffer = _lflatbuffers->get_buffer( size );
    if ( size > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( size + sizeof(struct c2s_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    struct c2s_header hd;
    hd._length = PACKET_MAKE_LENGTH( struct c2s_header,size );
    hd._cmd    = static_cast<uint16>  ( cmd );

    send.__append( &hd,sizeof(struct c2s_header) );
    send.__append( buffer,size );

    return 0;
}

/* s2c打包接口 */
int32 packet::unparse_s2c( lua_State *L,int32 index,int32 cmd,
    int32 ecode,const char *schema,const char *object,class buffer &send )
{
    if ( _lflatbuffers->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,_lflatbuffers->last_error() );
    }

    size_t size = 0;
    const char *buffer = _lflatbuffers->get_buffer( size );
    if ( size > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( size + sizeof(struct s2c_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    struct s2c_header hd;
    hd._length = PACKET_MAKE_LENGTH( struct s2c_header,size );
    hd._cmd    = static_cast<uint16>  ( cmd );
    hd._errno  = ecode;

    send.__append( &hd,sizeof(struct s2c_header) );
    send.__append( buffer,size );

    return 0;
}


/* s2s打包接口 */
int32 packet::unparse_s2s( lua_State *L,int32 index,int32 session,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send )
{
    if ( _lflatbuffers->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,_lflatbuffers->last_error() );
    }

    size_t size = 0;
    const char *buffer = _lflatbuffers->get_buffer( size );
    if ( size > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( size + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    struct s2s_header hd;
    hd._length = PACKET_MAKE_LENGTH( struct s2s_header,size );
    hd._cmd    = static_cast<uint16>  ( cmd );
    hd._errno  = ecode;
    hd._owner  = session;
    hd._mask   = PKT_SSPK;

    send.__append( &hd,sizeof(struct s2s_header) );
    send.__append( buffer,size );

    return 0;
}

/* ssc打包接口 */
int32 packet::unparse_ssc( lua_State *L,int32 index,owner_t owner,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send )
{
    if ( _lflatbuffers->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,_lflatbuffers->last_error() );
    }

    size_t size = 0;
    const char *buffer = _lflatbuffers->get_buffer( size );
    if ( size > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( 
        size + sizeof(struct s2s_header) + sizeof(struct s2c_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    /* 先构造客户端收到的数据包 */
    struct s2c_header chd;
    chd._length = PACKET_MAKE_LENGTH( struct s2c_header,size );
    chd._cmd    = static_cast<uint16>  ( cmd );
    chd._errno  = ecode;

    /* 把客户端数据包放到服务器数据包 */
    struct s2s_header hd;
    hd._length = PACKET_MAKE_LENGTH( struct s2s_header,PACKET_LENGTH(&chd) );
    hd._cmd    = 0;
    hd._errno  = 0;
    hd._owner  = owner;
    hd._mask   = PKT_SCPK; /*指定数据包类型为服务器发送客户端 */

    send.__append( &hd ,sizeof(struct s2s_header) );
    send.__append( &chd,sizeof(struct s2c_header) );
    send.__append( buffer,size );

    return 0;
}