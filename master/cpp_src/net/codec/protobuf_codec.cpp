#include "protobuf_codec.h"
#include "../net_header.h"

#include <pbc.h>
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

    int32_t encode( lua_State *L,const char *object,int32_t index );
    int32_t decode(
        lua_State *L,const char *object,const char *buffer,size_t size );

    const char *last_error();
    void get_buffer( struct pbc_slice &slice );

    int32_t load_file( const char *paeth );
    int32_t load_path( const char *path,const char *suffix = "pb" );
private:
    int32_t raw_encode( lua_State *L,
        struct pbc_wmessage *wmsg,const char *object,int32_t index );
    int32_t encode_field( lua_State *L,struct pbc_wmessage *wmsg,
        int32_t type,int32_t index,const char *key,const char *object );
    int32_t raw_encode_field( lua_State *L,struct pbc_wmessage *wmsg,
        int32_t type,int32_t index,const char *key,const char *object );

    int32_t raw_decode( lua_State *L,struct pbc_rmessage *msg );
    int32_t decode_field( lua_State *L,
        struct pbc_rmessage *msg,const char *key,int32_t type,int32_t idx );
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

int32_t lprotobuf::load_file( const char *path )
{
    std::ifstream ifs( path,std::ifstream::binary|std::ifstream::in );
    if ( !ifs.good() )
    {
        ERROR( "can NOT open file(%s):%s",path,strerror(errno) );
        return -1;
    }


    // get length of file:
    ifs.seekg( 0, ifs.end );
    int32_t len = ifs.tellg();
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
        delete (char *)slice.buffer;
        ERROR( "read file content error:%s",path );
        return -1;
    }
    ifs.close();

    if ( 0 != pbc_register( _env, &slice ) )
    {
        delete (char *)slice.buffer;
        ERROR( "pbc register error(%s):%s",path,pbc_error( _env ) );
        return -1;
    }

    delete [](char *)slice.buffer;
    return 0;
}

int32_t lprotobuf::load_path( const char *path,const char *suffix )
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
    ASSERT( _write_msg, "no write message found" );


    pbc_wmessage_buffer( _write_msg,&slice );
}

int32_t lprotobuf::decode(
    lua_State *L,const char *object,const char *buffer,size_t size )
{
    struct pbc_slice slice;
    slice.len = static_cast<int32_t>( size );
    slice.buffer = const_cast<char *>( buffer );

    struct pbc_rmessage * msg = pbc_rmessage_new( _env,object,&slice );
    if ( !msg )
    {
        ERROR( "protobuf decode:%s",last_error() );
        return -1;
    }

    int32_t ecode = raw_decode( L,msg );
    pbc_rmessage_delete( msg );

    return ecode;
}

int32_t lprotobuf::raw_decode( lua_State *L,struct pbc_rmessage *msg )
{
    int type = 0;
    const char *key = NULL;

    if ( !lua_checkstack( L,3 ) )
    {
        ERROR( "protobuf decode stack overflow" );
        return -1;
    }
    int32_t top = lua_gettop( L );

    lua_newtable( L );

    while( true )
    {
        type = pbc_rmessage_next( msg, &key );
        if ( key == NULL ) break;

        lua_pushstring( L,key );
        if ( type & PBC_REPEATED )
        {
            lua_newtable( L );
            int32_t raw_type = (type & ~PBC_REPEATED);
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

int32_t lprotobuf::decode_field( lua_State *L,
    struct pbc_rmessage *msg,const char *key,int32_t type,int32_t idx )
{
    switch( type )
    {
    case PBC_INT  : case PBC_FIXED32 :
    case PBC_UINT : case PBC_FIXED64 : case PBC_INT64 :
    {
        uint32_t hi,low;
        low = pbc_rmessage_integer( msg,key,idx,&hi );
        int64_t val = (int64_t)((uint64_t)hi << 32 | (uint64_t)low);
        lua_pushinteger( L,val );
    }break;
    case PBC_REAL :
    {
        double val = pbc_rmessage_integer( msg, key, idx, NULL );
        lua_pushnumber( L,val );
    }break;
    case PBC_BOOL :
    {
        uint32_t val = pbc_rmessage_integer( msg,key,idx,NULL );
        lua_pushboolean( L,val );
    }break;
    case PBC_ENUM :
    {
        uint32_t val = pbc_rmessage_integer( msg,key,idx,NULL );
        lua_pushinteger( L,val );
    }break;
    case PBC_STRING : case PBC_BYTES :
    {
        int32_t size = 0;
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
        return raw_decode( L,submsg );
    }break;
    default :
        ERROR( "protobuf decode unknow type" ); return -1;
    }

    return 0;
}

int32_t lprotobuf::encode( lua_State *L,const char *object,int32_t index )
{
    ASSERT( NULL == _write_msg, "protobuf write message not clear" );

    _write_msg = pbc_wmessage_new( _env,object );
    if ( !_write_msg )
    {
        ERROR( "no such protobuf message(%s)",object );
        return -1;
    }

    int32_t ecode = raw_encode( L,_write_msg,object,index );
    if ( 0 != ecode ) del_message();

    return ecode;
}

int32_t lprotobuf::encode_field(
    lua_State *L,struct pbc_wmessage *wmsg,
    int32_t type,int32_t index,const char *key,const char *object )
{
    if ( type & PBC_REPEATED )
    {
        if ( !lua_istable( L,index ) )
        {
            ERROR( "field(%s) expect a table",key );
            return -1;
        }

        int32_t raw_type = (type & ~PBC_REPEATED);
        lua_pushnil( L );
        while( lua_next( L,index ) )
        {
            if ( raw_encode_field( L,wmsg,raw_type,index + 2,key,object ) < 0 )
            {
                return -1;
            }

            lua_pop( L, 1 );
        }

        return 0;
    }

    return raw_encode_field( L,wmsg,type,index,key,object );
}

int32_t lprotobuf::raw_encode_field(
    lua_State *L,struct pbc_wmessage *wmsg,
    int32_t type,int32_t index,const char *key,const char *object )
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
        int64_t val = (int64_t)( lua_tointeger( L,index ) );
        uint32_t hi = (uint32_t)( val >> 32 );
        pbc_wmessage_integer( wmsg, key, (uint32_t)val, hi );
    }break;
    case PBC_REAL    :
    {
        LUAL_CHECK( number )
        double val = lua_tonumber( L,index );
        pbc_wmessage_real( wmsg, key, val );
    }break;
    case PBC_BOOL :
    {
        int32_t val = lua_toboolean( L,index );
        pbc_wmessage_integer( wmsg, key, (uint32_t)val, 0 );
    }break;
    case PBC_ENUM :
    {
        LUAL_CHECK( integer )
        int32_t val = lua_tointeger( L,index );
        pbc_wmessage_integer( wmsg, key, (uint32_t)val, 0 );
    }break;
    case PBC_STRING  : case PBC_BYTES :
    {
        LUAL_CHECK( string )
        size_t len = 0;
        const char * val = lua_tolstring( L,index,&len );
        if ( pbc_wmessage_string( wmsg, key, val, (int32_t)len ) )
        {
            ERROR( "field(%s) write string error",key );
            return -1;
        }
    }break;
    case PBC_MESSAGE :
    {
        LUAL_CHECK( table )
        struct pbc_wmessage *submsg = pbc_wmessage_message( wmsg, key );
        return raw_encode( L,submsg,object,index );
    }break;
    default:
        ERROR( "unknow protobuf type" );return -1;
    }
    return 0;

#undef LUAL_CHECK
}

int32_t lprotobuf::raw_encode( lua_State *L,
    struct pbc_wmessage *wmsg,const char *object,int32_t index )
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

    int32_t top = lua_gettop( L );

    /* pbc并未提供遍历sdl的方法，只能反过来遍历tabale.
     * 如果table中包含较多的无效字段，hash消耗将会比较大
     */
    lua_pushnil( L );
    while ( lua_next( L,index ) )
    {
        /* field name can only be string,if not,ignore */
        if ( LUA_TSTRING != lua_type( L,-2 ) ) continue;

        const char *key = lua_tostring( L,-2 );

        const char *sub_object = NULL;
        int32_t val_type = pbc_type( _env,object,key,&sub_object );
        if ( val_type <= 0 )
        {
            lua_pop( L, 1 );
            continue;
        }

        if ( encode_field( L,wmsg,val_type,top + 2,key,sub_object ) < 0 )
        {
            return -1;
        }
        lua_pop( L, 1 );
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
ProtobufCodec::ProtobufCodec()
{
    _is_proto_loaded = false;
    _lprotobuf = new class lprotobuf();
}

ProtobufCodec::~ProtobufCodec()
{
    finalize();

    delete _lprotobuf;
    _lprotobuf = NULL;
}

void ProtobufCodec::finalize()
{
    if ( _lprotobuf )
    {
        _lprotobuf->del_message();
    }
}

int32_t ProtobufCodec::load_path( const char *path )
{
    // pbc并没有什么unregister之类的函数，只能把整个对象销毁重建了
    if ( _is_proto_loaded )
    {
        finalize();

        delete _lprotobuf;
        _lprotobuf = new class lprotobuf();
    }

    _is_proto_loaded = true;
    return _lprotobuf->load_path( path );
}

/* 解码数据包
 * return: <0 error,otherwise the number of parameter push to stack
 */
int32_t ProtobufCodec::decode(
     lua_State *L,const char *buffer,int32_t len,const CmdCfg *cfg )
{
    if ( _lprotobuf->decode( L,cfg->_object,buffer,len ) < 0 )
    {
        ERROR( "protobuf decode:%s",_lprotobuf->last_error() );
        return -1;
    }

    // 默认情况下，所有内容解析到一个table
    return 1;
}

/* 编码数据包
 * return: <0 error,otherwise the length of buffer
 */
int32_t ProtobufCodec::encode(
    lua_State *L,int32_t index,const char **buffer,const CmdCfg *cfg )
{
    if ( _lprotobuf->encode( L,cfg->_object,index ) < 0 )
    {
        ERROR( "protobuf encode:%s",_lprotobuf->last_error() );
        return -1;
    }

    struct pbc_slice slice;
    _lprotobuf->get_buffer( slice );

    *buffer = static_cast<const char *>( slice.buffer );

    return slice.len;
}
