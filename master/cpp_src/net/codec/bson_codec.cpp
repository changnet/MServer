#include "bson_codec.h"

bson_codec::bson_codec()
{
    _bson_doc = NULL;
}

bson_codec::~bson_codec()
{
    finalize();
}

void bson_codec::finalize()
{
    if ( _bson_doc )
    {
        bson_destroy( _bson_doc );

        _bson_doc = NULL;
    }
}

/* 解码数据包
 * return: <0 error,otherwise the number of parameter push to stack
 */
int32 bson_codec::decode(
    lua_State *L,const char *buffer,int32 len,const cmd_cfg_t *cfg )
{
    UNUSED( cfg );

    bson_reader_t *reader = 
        bson_reader_new_from_data( (const uint8_t *)buffer,len );

    const bson_t *doc = bson_reader_read( reader,NULL );
    if ( !doc )
    {
        ERROR( "invalid bson buffer" );
        bson_reader_destroy( reader );
        return -1;
    }

    struct error_collector ec;
    ec.what[0] = 0;
    int32 args = lbs_do_decode_stack( L,doc,&ec );

    bson_reader_destroy( reader );
    if ( args < 0 )
    {
        ERROR( "bson decode:%s",ec.what );
        return -1;
    }

    return args;
}

/* 编码数据包
 * return: <0 error
 */
int32 bson_codec::encode(
    lua_State *L,int32 index,const char **buffer,const cmd_cfg_t *cfg )
{
    UNUSED( cfg );
    struct error_collector ec;
    ec.what[0] = 0;

    _bson_doc = bson_new();
    if ( 0 != lbs_do_encode_stack( L,_bson_doc,index,&ec ) )
    {
        finalize();
        ERROR("bson encode %s",ec.what );

        return -1;
    }

    *buffer = (const char *)bson_get_data( _bson_doc );

    return _bson_doc->len;
}
