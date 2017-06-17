#include "pbc.h"
#include <lua.hpp>

/* linux open dir */
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h> /* for basename */

#include <fstream>

/* check if suffix match */
static int is_suffix_file( const char *path,const char *suffix )
{
    /* simply check,not consider file like ./subdir/.bfbs */
    size_t sz = strlen( suffix );
    size_t ps = strlen( path );

    /* file like .pb will be ignore */
    if ( ps <= sz + 2 ) return 0;

    if ( '.' == path[ps-sz-1] && 0 == strcmp( path + ps - sz,suffix ) )
    {
        return true;
    }

    return false;
}

class lprotobuf
{
public:
    ~lprotobuf();
    lprotobuf ();

    void del_message();

    int32 encode( lua_State *L,const char *object,int32 index );
    int32 decode( 
        lua_State *L,const char *object,const char *buffer,size_t size );

    const char *last_error();
    void get_buffer( struct pbc_slice &slice );

    int32 load_file( const char *paeth );
    int32 load_path( const char *path,const char *suffix = "pb" );
private:
    int32 raw_encode( lua_State *L,
        struct pbc_wmessage *wmsg,const char *object,int32 index );
    int32 encode_field( lua_State *L,
        struct pbc_wmessage *wmsg,int32 type,int32 index,const char *key );
    int32 raw_encode_field( lua_State *L,
        struct pbc_wmessage *wmsg,int32 type,int32 index,const char *key );

    int32 raw_decode( lua_State *L,struct pbc_rmessage *msg );
    int32 decode_field( lua_State *L,
        struct pbc_rmessage *msg,const char *key,int32 type,int32 idx );
private:
    struct pbc_env *_env;
    struct pbc_wmessage *_write_msg;
};

lprotobuf::lprotobuf()
{
    _env = pbc_new();
    _write_msg = NULL;
}

lprotobuf::~lprotobuf()
{
    if ( _write_msg ) pbc_wmessage_delete( _write_msg );
    _write_msg = NULL;

    if ( _env ) pbc_delete( _env );
    _env = NULL;
}

void lprotobuf::del_message()
{
    if ( _write_msg ) pbc_wmessage_delete( _write_msg );
    _write_msg = NULL;
}

int32 lprotobuf::load_file( const char *path )
{
    std::ifstream ifs( path,std::ifstream::binary|std::ifstream::in );
    if ( !ifs.good() )
    {
        ERROR( "can NOT open file(%s):%s",path,strerror(errno) );
        return -1;
    }


    // get length of file:
    ifs.seekg( 0, ifs.end );
    int32 len = ifs.tellg();
    ifs.seekg( 0, ifs.beg );

    if ( !ifs.good() or len <= 0 )
    {
        ifs.close();
        ERROR( "get file length error:%s",path );
        return -1;
    }

    struct pbc_slice slice;
    slice.len = len;
    slice.buffer = new char[slice.len];
    ifs.read( (char *)slice.buffer,slice.len );
    
    if ( !ifs.good() || ifs.gcount() != slice.len )
    {
        ifs.close();
        ERROR( "read file content error:%s",path );
        return -1;
    }
    ifs.close();

    if ( 0 != pbc_register( _env, &slice ) )
    {
        ERROR( "pbc register error(%s):%s",path,pbc_error( _env ) );
        return -1;
    }

    return 0;
}

int32 lprotobuf::load_path( const char *path,const char *suffix )
{
    char file_path[PATH_MAX];
    int sz = snprintf( file_path,PATH_MAX,"%s/",path );
    if ( sz <= 0 )
    {
        ERROR( "path too long:%s",path );

        return -1;
    }

    DIR *dir = opendir( path );
    if ( !dir )
    {
        ERROR( "can not open directory(%s):%s",path,strerror(errno) );

        return -1;
    }

    int count = 0;
    struct dirent *dt = NULL;
    while ( (dt = readdir( dir )) )
    {
        snprintf( file_path + sz,PATH_MAX,"%s",dt->d_name );

        struct stat path_stat;
        stat( file_path, &path_stat );

        if ( S_ISREG( path_stat.st_mode )
            && is_suffix_file( dt->d_name,suffix ) )
        {

            if ( load_file( file_path ) < 0 )
            {
                closedir( dir );
                return       -1;
            }
            ++ count;
        }
    }

    closedir( dir );
    return    count;
}

const char *lprotobuf::last_error()
{
    return pbc_error( _env );
}

void lprotobuf::get_buffer( struct pbc_slice &slice )
{
    assert( "no write message found",_write_msg );


    pbc_wmessage_buffer( _write_msg,&slice );
}

int32 lprotobuf::decode( 
    lua_State *L,const char *object,const char *buffer,size_t size )
{
    struct pbc_slice slice;
    slice.len = static_cast<int32>( size );
    slice.buffer = const_cast<char *>( buffer );

    struct pbc_rmessage * msg = pbc_rmessage_new( _env,object,&slice );
    if ( !msg )
    {
        ERROR( "protobuf decode:%s",last_error() );
        return -1;
    }

    int32 ecode = raw_decode( L,msg );
    pbc_rmessage_delete( msg );

    return ecode;
}

int32 lprotobuf::raw_decode( lua_State *L,struct pbc_rmessage *msg )
{
    int type = 0;
    const char *key = NULL;

    if ( !lua_checkstack( L,3 ) )
    {
        ERROR( "protobuf decode stack overflow" );
        return -1;
    }
    int32 top = lua_gettop( L );

    lua_newtable( L );

    while( true )
    {
        type = pbc_rmessage_next( msg, &key );
        if ( key == NULL ) break;

        lua_pushstring( L,key );
        if ( type & PBC_REPEATED )
        {
            lua_newtable( L );
            int32 raw_type = (type & ~PBC_REPEATED);
            int size = pbc_rmessage_size( msg, key );
            for ( int idx = 0;idx < size;idx ++ )
            {
                if ( decode_field( L,msg,key,raw_type,idx ) < 0 )
                {
                    lua_settop( L,top );
                    return           -1;
                }
                lua_rawseti( L,top + 3,idx + 1 );
            }
        }
        else
        {
            if ( decode_field( L,msg,key,type,0 ) < 0 )
            {
                lua_settop( L,top );
                return           -1;
            }
        }
        lua_rawset( L,top + 1 );
    }

    return 0;
}

int32 lprotobuf::decode_field( lua_State *L,
    struct pbc_rmessage *msg,const char *key,int32 type,int32 idx )
{
    switch( type )
    {
    case PBC_INT  : case PBC_FIXED32 :
    case PBC_UINT : case PBC_FIXED64 : case PBC_INT64 :
    {
        uint32_t hi,low;
        low = pbc_rmessage_integer( msg,key,idx,&hi );
        int64 val = (int64)((uint64)hi << 32 | (uint64)low);
        lua_pushinteger( L,val );
    }break;
    case PBC_REAL :
    {
        double val = pbc_rmessage_integer( msg, key, idx, NULL );
        lua_pushnumber( L,val );
    }break;
    case PBC_BOOL :
    {
        uint32 val = pbc_rmessage_integer( msg,key,idx,NULL );
        lua_pushboolean( L,val );
    }break;
    case PBC_ENUM :
    {
        uint32 val = pbc_rmessage_integer( msg,key,idx,NULL );
        lua_pushinteger( L,val );
    }break;
    case PBC_STRING : case PBC_BYTES :
    {
        int32 size = 0;
        const char *bytes = pbc_rmessage_string( msg,key,idx,&size );
        lua_pushlstring( L,bytes,size );
    }break;
    case PBC_MESSAGE :
    {
        struct pbc_rmessage *submsg = pbc_rmessage_message( msg,key,idx );
        if ( !submsg )
        {
            ERROR( "protobuf decode sub message not found:%s",key );
            return -1;
        }
        return raw_decode( L,msg );
    }break;
    default :
        ERROR( "protobuf decode unknow type" ); return -1;
    }

    return 0;
}

int32 lprotobuf::encode( lua_State *L,const char *object,int32 index )
{
    assert( "protobuf write message not clear",NULL == _write_msg );

    _write_msg = pbc_wmessage_new( _env,object );
    if ( !_write_msg )
    {
        ERROR( "no such protobuf message(%s)",object );
        return -1;
    }

    return raw_encode( L,_write_msg,object,index );
}

int32 lprotobuf::encode_field( lua_State *L,
    struct pbc_wmessage *wmsg,int32 type,int32 index,const char *key )
{
    if ( type & PBC_REPEATED )
    {
        if ( !lua_istable( L,index ) )
        {
            ERROR( "field(%s) expect a table",key );
            return -1;
        }

        int32 raw_type = (type & ~PBC_REPEATED);
        lua_pushnil( L );
        while( lua_next( L,index ) )
        {
            if ( raw_encode_field( L,wmsg,raw_type,index + 2,key ) < 0 )
            {
                return -1;
            }

            lua_pop( L, 1 );
        }

        return 0;
    }

    return raw_encode_field( L,wmsg,type,index,key );
}

int32 lprotobuf::raw_encode_field( lua_State *L,
    struct pbc_wmessage *wmsg,int32 type,int32 index,const char *key )
{
#define LUAL_CHECK( TYPE )    \
    if ( !lua_is##TYPE( L,index ) ){    \
        ERROR( "protobuf encode field(%s) expect "#TYPE",got %s",    \
                    key,lua_typename( L, lua_type( L, index ) ) );   \
        return -1;    \
    }

    switch( type )
    {
    case PBC_INT  : case PBC_FIXED32 :
    case PBC_UINT : case PBC_FIXED64 : case PBC_INT64 :
    {
        LUAL_CHECK( integer )
        int64 val = (int64)( lua_tointeger( L,index ) );
        uint32 hi = (uint32)( val >> 32 );
        pbc_wmessage_integer( wmsg, key, (uint32)val, hi );
    }break;
    case PBC_REAL    :
    {
        LUAL_CHECK( number )
        double val = lua_tonumber( L,index );
        pbc_wmessage_real( wmsg, key, val );
    }break;
    case PBC_BOOL :
    {
        int32 val = lua_toboolean( L,index );
        pbc_wmessage_integer( wmsg, key, (uint32)val, 0 );
    }break;
    case PBC_ENUM :
    {
        LUAL_CHECK( integer )
        int32 val = lua_tointeger( L,index );
        pbc_wmessage_integer( wmsg, key, (uint32)val, 0 );
    }break;
    case PBC_STRING  : case PBC_BYTES :
    {
        LUAL_CHECK( string )
        size_t len = 0;
        const char * val = lua_tolstring( L,index,&len );
        if ( pbc_wmessage_string( wmsg, key, val, (int32)len ) )
        {
            ERROR( "field(%s) write string error",key );
            return -1;
        }
    }break;
    case PBC_MESSAGE :
    {
        LUAL_CHECK( table )
        struct pbc_wmessage *submsg = pbc_wmessage_message( wmsg, key );
        return raw_encode( L,submsg,key,index );
    }break;
    default:
        ERROR( "unknow protobuf type" );return -1;
    }
    return 0;

#undef LUAL_CHECK
}

int32 lprotobuf::raw_encode( lua_State *L,
    struct pbc_wmessage *wmsg,const char *object,int32 index )
{
    if ( !lua_istable( L,index ) )
    {
        ERROR( "protobuf encode expect a table" );
        return -1;
    }

    if ( !lua_checkstack( L,2 ) )
    {
        ERROR( "protobuf encode stack overflow" );
        return -1;
    }

    int32 top = lua_gettop( L );

    lua_pushnil( L );
    while ( lua_next( L,index ) )
    {
        /* field name can only be string,if not,ignore */
        if ( LUA_TSTRING != lua_type( L,-2 ) ) continue;

        const char *key = lua_tostring( L,-2 );

        /* pbc并未提供遍历sdl的方法，只能反过来遍历tabale.
         * 如果table中包含较多的无效字段，hash消耗将会比较大
         */
        int32 val_type = pbc_type( _env,object,key,NULL );
        if ( val_type <= 0 )
        {
            lua_pop( L, 1 );
            continue;
        }

        if ( encode_field( L,wmsg,val_type,top + 2,key ) < 0 )
        {
            return -1;
        }
        lua_pop( L, 1 );
    }
    return 0;
}


/////////////////////////////////packet////////////////////////////////////////

#define DECODER ((lprotobuf *)_decoder)

/* 加载该目录下的有schema文件 */
int32 packet::load_schema( const char *path )
{
    return DECODER->load_path( path );
}

template<class T>
int32 raw_parse( 
    lprotobuf *_lprotobuf,lua_State *L,const char *object,const T *header )
{
    int32 size = PACKET_BUFFER_LEN( header );
    if ( size < 0 || size > MAX_PACKET_LEN )
    {
        ERROR( "illegal packet buffer length" );
        return -1;
    }

    /* 去掉header内容 */
    const char *buffer = reinterpret_cast<const char *>( header + 1 );
    if ( _lprotobuf->decode( L,object,buffer,size ) < 0 )
    {
        ERROR( "decode cmd(%d):%s",header->_cmd,_lprotobuf->last_error() );
        return -1;
    }

    return 1;
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const c2s_header *header )
{
    return raw_parse( DECODER,L,object,header );
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const s2s_header *header )
{
    return raw_parse( DECODER,L,object,header );
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const s2c_header *header )
{
    return raw_parse( DECODER,L,object,header );
}

/* c2s打包接口 */
int32 packet::unparse_c2s( lua_State *L,int32 index,
    int32 cmd,const char *schema,const char *object,class buffer &send )
{
    if ( DECODER->encode( L,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    struct pbc_slice slice;
    DECODER->get_buffer( slice );
    if ( slice.len > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( slice.len + sizeof(struct c2s_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    struct c2s_header hd;
    hd._length = PACKET_MAKE_LENGTH( struct c2s_header,slice.len );
    hd._cmd    = static_cast<uint16>  ( cmd );

    send.__append( &hd,sizeof(struct c2s_header) );
    if (slice.len > 0) send.__append( slice.buffer,slice.len );
    DECODER->del_message();

    return 0;
}

/* s2c打包接口 */
int32 packet::unparse_s2c( lua_State *L,int32 index,int32 cmd,
    int32 ecode,const char *schema,const char *object,class buffer &send )
{
    if ( DECODER->encode( L,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    struct pbc_slice slice;
    DECODER->get_buffer( slice );
    if ( slice.len > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( slice.len + sizeof(struct s2c_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    struct s2c_header hd;
    hd._length = PACKET_MAKE_LENGTH( struct s2c_header,slice.len );
    hd._cmd    = static_cast<uint16>  ( cmd );
    hd._errno  = ecode;

    send.__append( &hd,sizeof(struct s2c_header) );
    if (slice.len > 0) send.__append( slice.buffer,slice.len );
    DECODER->del_message();

    return 0;
}


/* s2s打包接口 */
int32 packet::unparse_s2s( lua_State *L,int32 index,int32 session,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send )
{
    if ( DECODER->encode( L,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    struct pbc_slice slice;
    DECODER->get_buffer( slice );
    if ( slice.len > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( slice.len + sizeof(struct s2s_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    struct s2s_header hd;
    hd._length = PACKET_MAKE_LENGTH( struct s2s_header,slice.len );
    hd._cmd    = static_cast<uint16>  ( cmd );
    hd._errno  = ecode;
    hd._owner  = session;
    hd._mask   = PKT_SSPK;

    send.__append( &hd,sizeof(struct s2s_header) );
    if (slice.len > 0) send.__append( slice.buffer,slice.len );
    DECODER->del_message();

    return 0;
}

/* ssc打包接口 */
int32 packet::unparse_ssc( lua_State *L,int32 index,owner_t owner,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send )
{
    if ( DECODER->encode( L,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    struct pbc_slice slice;
    DECODER->get_buffer( slice );
    if ( slice.len > MAX_PACKET_LEN )
    {
        return luaL_error( L,"buffer size over MAX_PACKET_LEN" );
    }

    if ( !send.reserved( 
        slice.len + sizeof(struct s2s_header) + sizeof(struct s2c_header) ) )
    {
        return luaL_error( L,"can not reserved buffer" );
    }

    /* 先构造客户端收到的数据包 */
    struct s2c_header chd;
    chd._length = PACKET_MAKE_LENGTH( struct s2c_header,slice.len );
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
    if (slice.len > 0) send.__append( slice.buffer,slice.len );
    DECODER->del_message();

    return 0;
}