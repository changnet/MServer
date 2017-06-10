#include "pbc.h"

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

    int32 load_file( const char *paeth );
    int32 load_path( const char *path,const char *suffix = "pb" );
private:
    struct pbc_env *_env;
};

lprotobuf::lprotobuf()
{
    _env = pbc_new();
}

lprotobuf::~lprotobuf()
{
    if ( _env ) pbc_delete( _env );
    _env = NULL;
}

int32 load_file( const char *path )
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
    ifs.read( slice.buffer,slice.len );
    
    if ( !ifs.good() || ifs.gcount() != slice.len )
    {
        ifs.close();
        ERROR( "read file content error:%s",path );
        return -1;
    }
    ifs.close();

    if ( 0 != pbc_register( _env, &slice ) )
    {
        ERROR( "pbc register error(%s):%s",path,_evn)
        return -1;
    }

    return 0;
}

int32 load_path( const char *path )
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

            if ( !load_file( file_path ) )
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

/////////////////////////////////packet////////////////////////////////////////

#define DECODER ((lprotobuf *)_decoder)

/* 加载该目录下的有schema文件 */
int32 packet::load_schema( const char *path )
{
    return DECODER->load_path( path );
}


int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const c2s_header *header )
{
    return raw_parse( DECODER,L,schema,object,header );
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const s2s_header *header )
{
    return raw_parse( DECODER,L,schema,object,header );
}

int32 packet::parse( lua_State *L,
    const char *schema,const char *object,const s2c_header *header )
{
    return raw_parse( DECODER,L,schema,object,header );
}

/* c2s打包接口 */
int32 packet::unparse_c2s( lua_State *L,int32 index,
    int32 cmd,const char *schema,const char *object,class buffer &send )
{
    if ( DECODER->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    size_t size = 0;
    const char *buffer = DECODER->get_buffer( size );
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
    if ( DECODER->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    size_t size = 0;
    const char *buffer = DECODER->get_buffer( size );
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
    if ( DECODER->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    size_t size = 0;
    const char *buffer = DECODER->get_buffer( size );
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
    if ( DECODER->encode( L,schema,object,index ) < 0 )
    {
        return luaL_error( L,DECODER->last_error() );
    }

    size_t size = 0;
    const char *buffer = DECODER->get_buffer( size );
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