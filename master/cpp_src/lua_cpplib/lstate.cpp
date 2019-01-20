#include <lua.hpp>
#include <lparson.h>
#include <lrapidxml.hpp>

#include "lev.h"
#include "llog.h"
#include "lsql.h"
#include "laoi.h"
#include "lmap.h"
#include "lrank.h"
#include "lutil.h"
#include "lastar.h"
#include "lmongo.h"
#include "lstate.h"
#include "lclass.h"
#include "ltimer.h"
#include "lacism.h"
#include "lobj_counter.h"
#include "lnetwork_mgr.h"

#include "../net/socket.h"

#define LUA_LIB_OPEN( name,func ) \
    do{luaL_requiref(L, name, func, 1);lua_pop(L, 1);  /* remove lib */}while(0)

class lstate *lstate::_state = NULL;
class lstate *lstate::instance()
{
    if ( !_state )
    {
        _state = new lstate();
    }

    return _state;
}

void lstate::uninstance()
{
    if ( _state ) delete _state;
    _state = NULL;
}

lstate::lstate()
{
    /* 初始化lua */
    L = luaL_newstate();
    if ( !L )
    {
        ERROR( "lua new state fail\n" );
        exit( 1 );
    }
    luaL_openlibs(L);
    open_cpp();
}

lstate::~lstate()
{
    assert( "lua stack not clean at program exit",0 == lua_gettop(L) );

    /* Destroys all objects in the given Lua state (calling the corresponding
     * garbage-collection metamethods, if any) and frees all dynamic memory used
     * by this state
     */
    lua_close(L);
    L = NULL;
}

int32 luaopen_ev    ( lua_State *L );
int32 luaopen_sql   ( lua_State *L );
int32 luaopen_log   ( lua_State *L );
int32 luaopen_aoi   ( lua_State *L );
int32 luaopen_map   ( lua_State *L );
int32 luaopen_rank  ( lua_State *L );
int32 luaopen_timer ( lua_State *L );
int32 luaopen_astar ( lua_State *L );
int32 luaopen_acism ( lua_State *L );
int32 luaopen_mongo ( lua_State *L );
int32 luaopen_network_mgr( lua_State *L );

void lstate::open_cpp()
{
    /* ============================库方式调用================================== */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    LUA_LIB_OPEN("util", luaopen_util);
    LUA_LIB_OPEN("lua_parson", luaopen_lua_parson);
    LUA_LIB_OPEN("lua_rapidxml", luaopen_lua_rapidxml);
    LUA_LIB_OPEN("obj_counter", luaopen_obj_counter);
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

    /* ============================对象方式调用================================ */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    luaopen_ev    (L);
    luaopen_sql   (L);
    luaopen_log   (L);
    luaopen_aoi   (L);
    luaopen_map   (L);
    luaopen_rank  (L);
    luaopen_timer (L);
    luaopen_astar (L);
    luaopen_acism (L);
    luaopen_mongo (L);
    luaopen_network_mgr(L);
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

    /* when debug,make sure lua stack clean after init */
    assert( "lua stack not clean after init", 0 == lua_gettop(L) );
}

int32 luaopen_ev( lua_State *L )
{
    lclass<lev> lc(L,"Ev");
    lc.def<&lev::time>     ("time"     );
    lc.def<&lev::exit>     ("exit"     );
    lc.def<&lev::signal>   ("signal"   );
    lc.def<&lev::ms_time>  ("ms_time"  );
    lc.def<&lev::backend>  ("backend"  );
    lc.def<&lev::real_time>("real_time");
    lc.def<&lev::set_app_ev>("set_app_ev");

    return 0;
}

int32 luaopen_timer ( lua_State *L )
{
    lclass<ltimer> lc(L,"Timer");
    lc.def<&ltimer::set>   ( "set"    );
    lc.def<&ltimer::stop>  ( "stop"   );
    lc.def<&ltimer::start> ( "start"  );
    lc.def<&ltimer::active>( "active" );

    return 0;
}

int32 luaopen_sql( lua_State *L )
{

    lclass<lsql> lc(L,"Sql");
    lc.def<&lsql::valid> ( "valid" );
    lc.def<&lsql::start> ( "start" );
    lc.def<&lsql::stop>  ( "stop"  );

    lc.def<&lsql::do_sql> ( "do_sql" );

    return 0;
}

int32 luaopen_mongo( lua_State *L )
{
    lclass<lmongo> lc(L,"Mongo");
    lc.def<&lmongo::start> ( "start" );
    lc.def<&lmongo::stop>  ( "stop"  );
    lc.def<&lmongo::valid> ( "valid" );

    lc.def<&lmongo::count>           ( "count"           );
    lc.def<&lmongo::find>            ( "find"            );
    lc.def<&lmongo::insert>          ( "insert"          );
    lc.def<&lmongo::update>          ( "update"          );
    lc.def<&lmongo::remove>          ( "remove"          );
    lc.def<&lmongo::find_and_modify> ( "find_and_modify" );

    return 0;
}

int32 luaopen_log( lua_State *L )
{
    lclass<llog> lc(L,"Log");
    lc.def<&llog::stop>  ("stop");
    lc.def<&llog::start> ("start");
    lc.def<&llog::write> ("write");
    lc.def<&llog::mkdir_p> ("mkdir_p");
    lc.def<&llog::plog> ("plog");
    lc.def<&llog::elog> ("elog");
    lc.def<&llog::set_args> ("set_args");
    lc.def<&llog::set_name> ("set_name");

    return 0;
}

int32 luaopen_acism( lua_State *L )
{
    lclass<lacism> lc(L,"Acism");

    lc.def<&lacism::scan> ( "scan" );
    lc.def<&lacism::replace> ( "replace" );
    lc.def<&lacism::load_from_file> ( "load_from_file" );

    return 0;
}

int32 luaopen_network_mgr( lua_State *L )
{
    lclass<lnetwork_mgr> lc(L,"Network_mgr");

    lc.def<&lnetwork_mgr::close> ( "close" );
    lc.def<&lnetwork_mgr::listen> ( "listen" );
    lc.def<&lnetwork_mgr::connect> ( "connect" );
    lc.def<&lnetwork_mgr::load_one_schema> ( "load_one_schema" );
    lc.def<&lnetwork_mgr::set_curr_session> ( "set_curr_session" );

    lc.def<&lnetwork_mgr::set_conn_session> ( "set_conn_session" );
    lc.def<&lnetwork_mgr::set_conn_owner>   ( "set_conn_owner"   );
    lc.def<&lnetwork_mgr::unset_conn_owner> ( "unset_conn_owner" );

    lc.def<&lnetwork_mgr::set_cs_cmd> ( "set_cs_cmd" );
    lc.def<&lnetwork_mgr::set_ss_cmd> ( "set_ss_cmd" );
    lc.def<&lnetwork_mgr::set_sc_cmd> ( "set_sc_cmd" );

    lc.def<&lnetwork_mgr::set_conn_io>     ( "set_conn_io"     );
    lc.def<&lnetwork_mgr::set_conn_codec>  ( "set_conn_codec"  );
    lc.def<&lnetwork_mgr::set_conn_packet> ( "set_conn_packet" );

    lc.def<&lnetwork_mgr::get_http_header> ( "get_http_header" );

    lc.def<&lnetwork_mgr::send_srv_packet > ( "send_srv_packet"  );
    lc.def<&lnetwork_mgr::send_clt_packet > ( "send_clt_packet"  );
    lc.def<&lnetwork_mgr::send_s2s_packet > ( "send_s2s_packet"  );
    lc.def<&lnetwork_mgr::send_ssc_packet > ( "send_ssc_packet"  );
    lc.def<&lnetwork_mgr::send_rpc_packet > ( "send_rpc_packet"  );
    lc.def<&lnetwork_mgr::send_raw_packet > ( "send_raw_packet"  );
    lc.def<&lnetwork_mgr::send_ctrl_packet> ( "send_ctrl_packet" );

    lc.def<&lnetwork_mgr::srv_multicast> ( "srv_multicast" );
    lc.def<&lnetwork_mgr::clt_multicast> ( "clt_multicast" );
    lc.def<&lnetwork_mgr::ssc_multicast> ( "ssc_multicast" );

    lc.def<&lnetwork_mgr::set_send_buffer_size> ( "set_send_buffer_size" );
    lc.def<&lnetwork_mgr::set_recv_buffer_size> ( "set_recv_buffer_size" );

    lc.def<&lnetwork_mgr::new_ssl_ctx> ( "new_ssl_ctx" );

    lc.set( "CNT_NONE",socket::CNT_NONE );
    lc.set( "CNT_CSCN",socket::CNT_CSCN );
    lc.set( "CNT_SCCN",socket::CNT_SCCN );
    lc.set( "CNT_SSCN",socket::CNT_SSCN );

    lc.set( "IOT_NONE",io::IOT_NONE );
    lc.set( "IOT_SSL" ,io::IOT_SSL  );

    lc.set( "PKT_NONE"     ,packet::PKT_NONE      );
    lc.set( "PKT_HTTP"     ,packet::PKT_HTTP      );
    lc.set( "PKT_STREAM"   ,packet::PKT_STREAM    );
    lc.set( "PKT_WEBSOCKET",packet::PKT_WEBSOCKET );
    lc.set( "PKT_WSSTREAM" ,packet::PKT_WSSTREAM  );

    lc.set( "CDC_NONE"    ,codec::CDC_NONE    );
    lc.set( "CDC_BSON"    ,codec::CDC_BSON    );
    lc.set( "CDC_STREAM"  ,codec::CDC_STREAM  );
    lc.set( "CDC_FLATBUF" ,codec::CDC_FLATBUF );
    lc.set( "CDC_PROTOBUF",codec::CDC_PROTOBUF);

    return 0;
}

int32 luaopen_aoi( lua_State *L )
{
    lclass<laoi> lc(L,"Aoi");

    lc.def<&laoi::set_size> ( "set_size" );
    lc.def<&laoi::set_visual_range> ( "set_visual_range" );

    lc.def<&laoi::get_entitys> ( "get_entitys" );
    lc.def<&laoi::get_all_entitys> ( "get_all_entitys" );
    lc.def<&laoi::get_watch_me_entitys> ( "get_watch_me_entitys" );

    lc.def<&laoi::exit_entity> ( "exit_entity" );
    lc.def<&laoi::enter_entity> ( "enter_entity" );
    lc.def<&laoi::update_entity> ( "update_entity" );
    return 0;
}

int32 luaopen_map( lua_State *L )
{
    lclass<lmap> lc(L,"Map");

    lc.def<&lmap::set>  ( "set" );
    lc.def<&lmap::fill> ( "fill" );
    lc.def<&lmap::load> ( "load" );
    lc.def<&lmap::fork> ( "fork" );
    lc.def<&lmap::get_size> ( "get_size" );
    lc.def<&lmap::get_pass_cost> ( "get_pass_cost" );

    return 0;
}

int32 luaopen_rank( lua_State *L )
{
    lclass< linsertion_rank > lc_insertion_rank(L,"Insertion_rank");
    lc_insertion_rank.def< &linsertion_rank::clear > ("clear");
    lc_insertion_rank.def< &linsertion_rank::remove > ("remove");
    lc_insertion_rank.def< &linsertion_rank::insert > ("insert");
    lc_insertion_rank.def< &linsertion_rank::update > ("update");
    lc_insertion_rank.def< &linsertion_rank::get_count > ("get_count");
    lc_insertion_rank.def< &linsertion_rank::set_max_count > ("set_max_count");
    lc_insertion_rank.def< &linsertion_rank::get_max_factor > ("get_max_factor");
    lc_insertion_rank.def< &linsertion_rank::get_factor > ("get_factor");
    lc_insertion_rank.def< &linsertion_rank::get_rank_by_id > ("get_rank_by_id");
    lc_insertion_rank.def< &linsertion_rank::get_id_by_rank > ("get_id_by_rank");

    lclass< lbucket_rank > lc_bucket_rank(L,"Bucket_rank");
    lc_bucket_rank.def< &lbucket_rank::clear > ("clear");
    lc_bucket_rank.def< &lbucket_rank::insert > ("insert");
    lc_bucket_rank.def< &lbucket_rank::get_count > ("get_count");
    lc_bucket_rank.def< &lbucket_rank::get_top_n > ("get_top_n");

    return 0;
}

int32 luaopen_astar( lua_State *L )
{
    lclass<lastar> lc(L,"Astar");

    lc.def<&lastar::search>  ( "search" );

    return 0;
}
