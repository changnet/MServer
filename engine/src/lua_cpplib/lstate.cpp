#include <lparson.h>
#include <lrapidxml.hpp>
#include <lua.hpp>

#include "lacism.h"
#include "lgrid_aoi.h"
#include "lastar.h"
#include "lclass.h"
#include "lev.h"
#include "llog.h"
#include "lmap.h"
#include "lmongo.h"
#include "lnetwork_mgr.h"
#include "lrank.h"
#include "lsql.h"
#include "lstate.h"
#include "lstatistic.h"
#include "ltimer.h"
#include "lutil.h"

#include "../net/socket.h"

#define LUA_LIB_OPEN(name, func)         \
    do                                   \
    {                                    \
        luaL_requiref(L, name, func, 1); \
        lua_pop(L, 1); /* remove lib */  \
    } while (0)

LState::LState()
{
    /* 初始化lua */
    L = luaL_newstate();
    if (!L)
    {
        ERROR("lua new state fail\n");
        exit(1);
    }
    luaL_openlibs(L);
    open_cpp();
}

LState::~LState()
{
    ASSERT(0 == lua_gettop(L), "lua stack not clean at program exit");

    /* Destroys all objects in the given Lua state (calling the corresponding
     * garbage-collection metamethods, if any) and frees all dynamic memory used
     * by this state
     */
    lua_close(L);
    L = NULL;
}

int32_t luaopen_ev(lua_State *L);
int32_t luaopen_sql(lua_State *L);
int32_t luaopen_log(lua_State *L);
int32_t luaopen_aoi(lua_State *L);
int32_t luaopen_map(lua_State *L);
int32_t luaopen_rank(lua_State *L);
int32_t luaopen_timer(lua_State *L);
int32_t luaopen_astar(lua_State *L);
int32_t luaopen_acism(lua_State *L);
int32_t luaopen_mongo(lua_State *L);
int32_t luaopen_network_mgr(lua_State *L);

void LState::open_cpp()
{
    /* ============================库方式调用================================== */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    LUA_LIB_OPEN("util", luaopen_util);
    LUA_LIB_OPEN("statistic", luaopen_statistic);
    LUA_LIB_OPEN("lua_parson", luaopen_lua_parson);
    LUA_LIB_OPEN("lua_rapidxml", luaopen_lua_rapidxml);
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

    /* ============================对象方式调用================================ */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    luaopen_ev(L);
    luaopen_sql(L);
    luaopen_log(L);
    luaopen_aoi(L);
    luaopen_map(L);
    luaopen_rank(L);
    luaopen_timer(L);
    luaopen_astar(L);
    luaopen_acism(L);
    luaopen_mongo(L);
    luaopen_network_mgr(L);
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

    /* when debug,make sure lua stack clean after init */
    ASSERT(0 == lua_gettop(L), "lua stack not clean after init");
}

int32_t luaopen_ev(lua_State *L)
{
    LBaseClass<LEV> lc(L, "Ev");
    lc.def<&LEV::time>("time");
    lc.def<&LEV::exit>("exit");
    lc.def<&LEV::signal>("signal");
    lc.def<&LEV::ms_time>("ms_time");
    lc.def<&LEV::backend>("backend");
    lc.def<&LEV::who_busy>("who_busy");
    lc.def<&LEV::real_time>("real_time");
    lc.def<&LEV::set_app_ev>("set_app_ev");
    lc.def<&LEV::set_gc_stat>("set_gc_stat");
    lc.def<&LEV::real_ms_time>("real_ms_time");
    lc.def<&LEV::set_critical_time>("set_critical_time");

    return 0;
}

int32_t luaopen_timer(lua_State *L)
{
    LClass<LTimer> lc(L, "Timer");
    lc.def<&LTimer::set>("set");
    lc.def<&LTimer::stop>("stop");
    lc.def<&LTimer::start>("start");
    lc.def<&LTimer::active>("active");

    return 0;
}

int32_t luaopen_sql(lua_State *L)
{

    LClass<LSql> lc(L, "Sql");
    lc.def<&LSql::valid>("valid");
    lc.def<&LSql::start>("start");
    lc.def<&LSql::stop>("stop");

    lc.def<&LSql::do_sql>("do_sql");

    return 0;
}

int32_t luaopen_mongo(lua_State *L)
{
    LClass<LMongo> lc(L, "Mongo");
    lc.def<&LMongo::start>("start");
    lc.def<&LMongo::stop>("stop");
    lc.def<&LMongo::valid>("valid");

    lc.def<&LMongo::count>("count");
    lc.def<&LMongo::find>("find");
    lc.def<&LMongo::insert>("insert");
    lc.def<&LMongo::update>("update");
    lc.def<&LMongo::remove>("remove");
    lc.def<&LMongo::find_and_modify>("find_and_modify");

    return 0;
}

int32_t luaopen_log(lua_State *L)
{
    LClass<LLog> lc(L, "Log");
    lc.def<&LLog::stop>("stop");
    lc.def<&LLog::start>("start");
    lc.def<&LLog::write>("write");
    lc.def<&LLog::plog>("plog");
    lc.def<&LLog::elog>("elog");
    lc.def<&LLog::set_args>("set_args");
    lc.def<&LLog::set_name>("set_name");

    return 0;
}

int32_t luaopen_acism(lua_State *L)
{
    LClass<LAcism> lc(L, "Acism");

    lc.def<&LAcism::scan>("scan");
    lc.def<&LAcism::replace>("replace");
    lc.def<&LAcism::load_from_file>("load_from_file");

    return 0;
}

int32_t luaopen_network_mgr(lua_State *L)
{
    LBaseClass<LNetworkMgr> lc(L, "Network_mgr");

    lc.def<&LNetworkMgr::close>("close");
    lc.def<&LNetworkMgr::listen>("listen");
    lc.def<&LNetworkMgr::connect>("connect");
    lc.def<&LNetworkMgr::load_one_schema>("load_one_schema");
    lc.def<&LNetworkMgr::set_curr_session>("set_curr_session");

    lc.def<&LNetworkMgr::set_conn_session>("set_conn_session");
    lc.def<&LNetworkMgr::set_conn_owner>("set_conn_owner");
    lc.def<&LNetworkMgr::unset_conn_owner>("unset_conn_owner");

    lc.def<&LNetworkMgr::set_cs_cmd>("set_cs_cmd");
    lc.def<&LNetworkMgr::set_ss_cmd>("set_ss_cmd");
    lc.def<&LNetworkMgr::set_sc_cmd>("set_sc_cmd");

    lc.def<&LNetworkMgr::set_conn_io>("set_conn_io");
    lc.def<&LNetworkMgr::set_conn_codec>("set_conn_codec");
    lc.def<&LNetworkMgr::set_conn_packet>("set_conn_packet");

    lc.def<&LNetworkMgr::get_http_header>("get_http_header");

    lc.def<&LNetworkMgr::send_srv_packet>("send_srv_packet");
    lc.def<&LNetworkMgr::send_clt_packet>("send_clt_packet");
    lc.def<&LNetworkMgr::send_s2s_packet>("send_s2s_packet");
    lc.def<&LNetworkMgr::send_ssc_packet>("send_ssc_packet");
    lc.def<&LNetworkMgr::send_rpc_packet>("send_rpc_packet");
    lc.def<&LNetworkMgr::send_raw_packet>("send_raw_packet");
    lc.def<&LNetworkMgr::send_ctrl_packet>("send_ctrl_packet");

    lc.def<&LNetworkMgr::srv_multicast>("srv_multicast");
    lc.def<&LNetworkMgr::clt_multicast>("clt_multicast");
    lc.def<&LNetworkMgr::ssc_multicast>("ssc_multicast");

    lc.def<&LNetworkMgr::set_send_buffer_size>("set_send_buffer_size");
    lc.def<&LNetworkMgr::set_recv_buffer_size>("set_recv_buffer_size");

    lc.def<&LNetworkMgr::new_ssl_ctx>("new_ssl_ctx");

    lc.def<&LNetworkMgr::address>("address");

    lc.def<&LNetworkMgr::get_player_session>("get_player_session");
    lc.def<&LNetworkMgr::set_player_session>("set_player_session");

    lc.set(Socket::CT_NONE, "CNT_NONE");
    lc.set(Socket::CT_CSCN, "CNT_CSCN");
    lc.set(Socket::CT_SCCN, "CNT_SCCN");
    lc.set(Socket::CT_SSCN, "CNT_SSCN");

    lc.set(IO::IOT_NONE, "IOT_NONE");
    lc.set(IO::IOT_SSL, "IOT_SSL");

    lc.set(Packet::PT_NONE, "PKT_NONE");
    lc.set(Packet::PT_HTTP, "PKT_HTTP");
    lc.set(Packet::PT_STREAM, "PKT_STREAM");
    lc.set(Packet::PT_WEBSOCKET, "PKT_WEBSOCKET");
    lc.set(Packet::PT_WSSTREAM, "PKT_WSSTREAM");

    lc.set(Codec::CDC_NONE, "CDC_NONE");
    lc.set(Codec::CDC_BSON, "CDC_BSON");
    lc.set(Codec::CDC_STREAM, "CDC_STREAM");
    lc.set(Codec::CDC_FLATBUF, "CDC_FLATBUF");
    lc.set(Codec::CDC_PROTOBUF, "CDC_PROTOBUF");

    lc.set(SSLMgr::SSLVT_NONE, "SSLVT_NONE");
    lc.set(SSLMgr::SSLVT_TLS_GEN_AT, "SSLVT_TLS_GEN_AT");
    lc.set(SSLMgr::SSLVT_TLS_SRV_AT, "SSLVT_TLS_SRV_AT");
    lc.set(SSLMgr::SSLVT_TLS_CLT_AT, "SSLVT_TLS_CLT_AT");

    return 0;
}

int32_t luaopen_aoi(lua_State *L)
{
    LClass<LGridAoi> lc(L, "GridAoi");

    lc.def<&LGridAoi::set_size>("set_size");
    lc.def<&LGridAoi::set_visual_range>("set_visual_range");

    lc.def<&LGridAoi::get_entitys>("get_entitys");
    lc.def<&LGridAoi::get_all_entitys>("get_all_entitys");
    lc.def<&LGridAoi::get_watch_me_entitys>("get_watch_me_entitys");

    lc.def<&LGridAoi::exit_entity>("exit_entity");
    lc.def<&LGridAoi::enter_entity>("enter_entity");
    lc.def<&LGridAoi::update_entity>("update_entity");

    lc.def<&LGridAoi::is_same_pos>("is_same_pos");

    return 0;
}

int32_t luaopen_map(lua_State *L)
{
    LClass<LMap> lc(L, "Map");

    lc.def<&LMap::set>("set");
    lc.def<&LMap::fill>("fill");
    lc.def<&LMap::load>("load");
    lc.def<&LMap::fork>("fork");
    lc.def<&LMap::get_size>("get_size");
    lc.def<&LMap::get_pass_cost>("get_pass_cost");

    return 0;
}

int32_t luaopen_rank(lua_State *L)
{
    LClass<LInsertionRank> lc_insertion_rank(L, "InsertionRank");
    lc_insertion_rank.def<&LInsertionRank::clear>("clear");
    lc_insertion_rank.def<&LInsertionRank::remove>("remove");
    lc_insertion_rank.def<&LInsertionRank::insert>("insert");
    lc_insertion_rank.def<&LInsertionRank::update>("update");
    lc_insertion_rank.def<&LInsertionRank::get_count>("get_count");
    lc_insertion_rank.def<&LInsertionRank::set_max_count>("set_max_count");
    lc_insertion_rank.def<&LInsertionRank::get_max_factor>("get_max_factor");
    lc_insertion_rank.def<&LInsertionRank::get_factor>("get_factor");
    lc_insertion_rank.def<&LInsertionRank::get_rank_by_id>("get_rank_by_id");
    lc_insertion_rank.def<&LInsertionRank::get_id_by_rank>("get_id_by_rank");

    LClass<lBucketRank> lc_bucket_rank(L, "BucketRank");
    lc_bucket_rank.def<&lBucketRank::clear>("clear");
    lc_bucket_rank.def<&lBucketRank::insert>("insert");
    lc_bucket_rank.def<&lBucketRank::get_count>("get_count");
    lc_bucket_rank.def<&lBucketRank::get_top_n>("get_top_n");

    return 0;
}

int32_t luaopen_astar(lua_State *L)
{
    LClass<LAstar> lc(L, "Astar");

    lc.def<&LAstar::search>("search");

    return 0;
}
