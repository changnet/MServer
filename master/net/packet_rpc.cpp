
#include <lbson.h>

/* rpc打包接口 */
int32 packet::unparse_rpc( lua_State *L,
    int32 unique_id,int32 index,class buffer &send )
{
    struct error_collector ec;
    ec.what[0] = 0;

    bson_t *doc = bson_new();
    if ( 0 != lbs_do_encode_stack( L,doc,index,&ec ) )
    {
        bson_destroy( doc );
        return luaL_error( L,ec.what );
    }

    const char *buffer = (const char *)bson_get_data( doc );

    struct s2s_header s2sh;
    s2sh._length = PACKET_MAKE_LENGTH( struct s2s_header,doc->len );
    s2sh._cmd    = 0;
    s2sh._errno  = 0;
    s2sh._owner  = unique_id;
    s2sh._mask   = PKT_RPCS;

    send.__append( &s2sh,sizeof(struct s2s_header) );
    send.__append( buffer,static_cast<uint32>(doc->len) );

    bson_destroy( doc );

    return 0;
}

/* 解析rpc数据包 */
int32 packet::parse( lua_State *L,const struct s2s_header *header )
{
    size_t size = PACKET_BUFFER_LEN( header );

    const char *buffer = reinterpret_cast<const char *>( header + 1 );
    bson_reader_t *reader = 
        bson_reader_new_from_data( (const uint8_t *)buffer,size );

    const bson_t *doc = bson_reader_read( reader,NULL );
    if ( !doc )
    {
        bson_reader_destroy( reader );
        return luaL_error( L,"invalid bson buffer" );
    }

    struct error_collector ec;
    ec.what[0] = 0;
    int cnt = lbs_do_decode_stack( L,doc,&ec );

    bson_reader_destroy( reader );
    if ( cnt < 0 ) return luaL_error( L,ec.what );

    return cnt;
}

/* rpc返回打包接口 */
int32 packet::unparse_rpc( lua_State *L,
    int32 unique_id,int32 ecode,int32 index,class buffer &send )
{
    bson_t *doc = bson_new();
    if ( LUA_OK == ecode )
    {
        struct error_collector ec;
        ec.what[0] = 0;
        if ( 0 != lbs_do_encode_stack( L,doc,index,&ec ) )
        {
            bson_destroy( doc );
            ERROR( "unparse_rpc return packet:%s",ec.what );
            return -1;
        }
    }

    const char *buffer = (const char *)bson_get_data( doc );

    struct s2s_header s2sh;
    s2sh._length = PACKET_MAKE_LENGTH( struct s2s_header,doc->len );
    s2sh._cmd    = 0;
    s2sh._errno  = ecode;
    s2sh._owner  = unique_id;
    s2sh._mask   = PKT_RPCR;

    send.__append( &s2sh,sizeof(struct s2s_header) );
    send.__append( buffer,static_cast<uint32>(doc->len) );

    bson_destroy( doc );

    return 0;
}