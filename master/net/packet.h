#ifndef __PACKET_H__
#define __PACKET_H__

struct lua_State;

class packet
{
public:
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
public:
    static void uninstance();
    static class packet *instance();

    /* 加载path目录下所有schema文件 */
    int32 load_schema( const char *path );

    /* 外部解析接口 */
    int32 parse( lua_State *L,
        const char *schema,const char *object,const c2s_header *header );
    int32 parse( lua_State *L,
        const char *schema,const char *object,const s2s_header *header );
    int32 parse( lua_State *L,
        const char *schema,const char *object,const s2c_header *header );
    int32 parse( lua_State *L,const s2s_header *header );

    /* c2s打包接口 */
    int32 unparse_c2s( lua_State *L,int32 index,
        int32 cmd,const char *schema,const char *object,class buffer &send );
    /* s2c打包接口 */
    int32 unparse_s2c( lua_State *L,int32 index,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send );
    /* s2s打包接口 */
    int32 unparse_s2s( lua_State *L,int32 index,int32 session,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send );
    /* ssc打包接口 */
    int32 unparse_ssc( lua_State *L,int32 index,owner_t owner,int32 cmd,
        int32 ecode,const char *schema,const char *object,class buffer &send );
    /* rpc调用打包接口 */
    int32 unparse_rpc( lua_State *L,
        int32 unique_id,int32 index,class buffer &send );
    /* rpc返回打包接口 */
    int32 unparse_rpc( lua_State *L,
        int32 unique_id,int32 ecode,int32 index,class buffer &send );
private:
    packet();
    ~packet();
private:
    void  *_decoder;
    static class packet *_packet;
};

#endif /* __PACKET_H__ */
