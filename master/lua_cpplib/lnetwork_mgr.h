#ifndef __LNETWORK_MGR_H__
#define __LNETWORK_MGR_H__

#include <lua.hpp>

#include "../global/global.h"

#if __cplusplus < 201103L    /* -std=gnu99 */
    #include <map>
    #define map_t    std::map
#else                       /* if support C++ 2011 */
    #include <unordered_map>
    #define map_t    std::unordered_map
#endif

typedef enum
{
    PKT_NONE = 0,  // invalid
    PKT_CSPK = 1,  // c2s packet
    PKT_SCPK = 2,  // s2c packet
    PKT_SSPK = 3,  // s2s packet
    PKT_RPCP = 4,  // rpc packet

    PKT_MAXT       // max packet type
} packet_t;


#define MAX_SCHEMA_NAME  64

/* _maks 设定掩码，按位
 * 1bit: 解码方式:0 普通解码，1 unpack解码
 * 2bit: TODO:广播方式 ？？这个放到脚本
 */
typedef struct
{
    int32 _cmd;
    int32 _mask;
    int32 _session;
    char _schema[MAX_SCHEMA_NAME];
    char _object[MAX_SCHEMA_NAME];
} cmd_cfg_t;

class socket;
class lnetwork_mgr
{
private:
    typedef map_t<int32,cmd_cfg_t> cmd_map_t;
    typedef map_t<uint32,class socket*> socket_map_t;
public:
    static void uninstance();
    static class lnetwork_mgr *instance();

    ~lnetwork_mgr();
    explicit lnetwork_mgr( lua_State *L );

    /* 设置指令参数 */
    int32 set_cmd();

    int32 send ();
    int32 close();
    int32 address ();
    int32 listen  ();
    int32 connect ();
private:
    /* 获取connect_id */
    uint32 connect_id();
private:
    lua_State *L;

    int32 _session; /* 当前进程的session */
    uint32 _conn_seed; /* connect_id种子 */

    cmd_map_t _cmd_map;
    socket_map_t _socket_map;

    int32 _deletecnt; /* 当前需要删除的socket数量 */
    static class lnetwork_mgr *_network_mgr;
};

#endif /* __LNETWORK_MGR_H__ */