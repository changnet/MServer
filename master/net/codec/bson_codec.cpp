#include <lbson.h>
#include "bson_codec.h"

/* bson_t在bson库中是typedef声明方式，不能直接前置声明 */
struct x_bson_t : public bson_t {};

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

        delete _bson_doc;
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
    return 0;
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

    bson_t *_bson_doc = bson_new();
    if ( 0 != lbs_do_encode_stack( L,_bson_doc,index,&ec ) )
    {
        finalize();
        ERROR("bson encode %s",ec.what );

        return -1;
    }

    *buffer = (const char *)bson_get_data( _bson_doc );

    return _bson_doc->len;
}
