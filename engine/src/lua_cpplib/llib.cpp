#include <lparson.h>
#include <lrapidxml.hpp>

#include "llib.hpp"

#include "lacism.hpp"
#include "lgrid_aoi.hpp"
#include "lastar.hpp"
#include "lev.hpp"
#include "llog.hpp"
#include "lmap.hpp"
#include "lmongo.hpp"
#include "lsql.hpp"
#include "lutil.hpp"
#include "llist_aoi.hpp"

#include "net/socket.hpp"
#include "net/io/tls_ctx.hpp"
#include "net/codec/lua_codec.hpp"
#include "net/codec/pbc_codec.hpp"
#include "system/static_global.hpp"
#include "system/signal.hpp"
#include "thread/worker_thread.hpp"

#define LUA_LIB_OPEN(name, func)         \
    do                                   \
    {                                    \
        luaL_requiref(L, name, func, 1); \
        lua_pop(L, 1); /* remove lib */  \
    } while (0)

static void __dbg_break_hook(lua_State *L, lua_Debug *ar)
{
    UNUSED(ar);
    lua_sethook(L, nullptr, 0, 0);
    luaL_error(L, __FUNCTION__);
}

void __dbg_break()
{
    // 当程序死循环时，用gdb attach到进程
    // call __dbg_break()
    // 然后继续执行，应该能中断lua的执行
    // 注意：由于无法取得coroutine的L指针，无法中断coroutine中的逻辑（考虑把正在执行的
    // coroutine设置到一个global值，然后用getglobal取值）
    // 注意：对正在执行程序的影响未知
    lua_State *L = StaticGlobal::L;

    lua_sethook(L, __dbg_break_hook,
                LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT, 1);
}

const char *__dbg_traceback()
{
    // 当程序死循环时，用gdb attach到进程
    // call __dbg_traceback()
    // 然后继续执行，应该能打印出lua当前的堆栈
    // 注意：由于无法取得coroutine的L指针，无法打印coroutine中的堆栈
    // 注意：对正在执行程序的影响未知

    // worker线程的L不一样，这里得改一下
    lua_State *L = StaticGlobal::L;

    luaL_traceback(L, L, nullptr, 0);

    return lua_tostring(L, -1);
}

////////////////////////////////////////////////////////////////////////////////

// 全局函数放这里
static void luaopen_engine(lua_State *L)
{
    lcpp::module_begin(L, "Engine");
    lcpp::module_function<&signal_mask>(L, "signal_mask");
    lcpp::module_function<&signal_mask_once>(L, "signal_mask_once");
    lcpp::module_end(L);
}

static void luaopen_env(lua_State* L)
{
    lcpp::Class<Env> lc(L, "engine.Env");
    lc.def<&Env::get>("get");
    lc.def<&Env::set>("set");
}

static void luaopen_main_thread(lua_State *L)
{
    lcpp::Class<MainThread> lc(L, "engine.MainThread");
    lc.def<&MainThread::stop>("stop");
    lc.def<&MainThread::steady_clock>("steady_clock");
    lc.def<&MainThread::system_clock>("system_clock");
    lc.def<&MainThread::push_message>("push_message");
    lc.def<&MainThread::emplace_message_int>("emplace_message_int");
    lc.def<&MainThread::emplace_message_ptr>("emplace_message_ptr");
    /*
    lc.def<&LEV::now>("time");
    lc.def<&LEV::quit>("exit");
    lc.def<&LEV::signal>("signal");
    lc.def<&LEV::ms_now>("ms_time");
    lc.def<&LEV::backend>("backend");
    lc.def<&LEV::who_busy>("who_busy");
    lc.def<&LEV::set_app_ev>("set_app_ev");
    lc.def<&LEV::time_update>("time_update");
    lc.def<&LEV::steady_clock>("steady_clock");
    lc.def<&LEV::system_clock>("system_clock");
    lc.def<&LEV::timer_stop>("timer_stop");
    lc.def<&LEV::timer_start>("timer_start");
    lc.def<&LEV::periodic_stop>("periodic_stop");
    lc.def<&LEV::periodic_start>("periodic_start");
    lc.def<&LEV::set_critical_time>("set_critical_time");
    */
}

static void luaopen_worker_thread(lua_State *L)
{
    lcpp::Class<WorkerThread> lc(L, "engine.WorkerThread");
    lc.constructor<std::string>();
    lc.def<&WorkerThread::start>("start");
    lc.def<&WorkerThread::stop>("stop");
    lc.def<&WorkerThread::push_message>("push_message");
    lc.def<&WorkerThread::emplace_message_int>("emplace_message_int");
    lc.def<&WorkerThread::emplace_message_ptr>("emplace_message_ptr");
}

static void luaopen_tls(lua_State *L)
{
    lcpp::Class<TlsCtx> lc(L, "engine.TlsCtx");
    lc.def<&TlsCtx::init>("init");
}

static void luaopen_socket(lua_State *L)
{
    lcpp::Class<Socket> lc(L, "engine.Socket");

    lc.constructor<int32_t>();
    lc.def<&Socket::fd>("fd");
    lc.def<&Socket::start>("start");
    lc.def<&Socket::stop>("stop");
    lc.def<&Socket::listen>("listen");
    lc.def<&Socket::connect>("connect");
    lc.def<&Socket::set_io>("set_io");
    lc.def<&Socket::set_packet>("set_packet");
    lc.def<&Socket::set_buffer_params>("set_buffer_params");
    lc.def<&Socket::io_init_accept>("io_init_accept");
    lc.def<&Socket::io_init_connect>("io_init_connect");
    lc.def<&Socket::send_pkt>("send_pkt");
    lc.def<&Socket::send_clt>("send_clt");
    lc.def<&Socket::send_srv>("send_srv");
    lc.def<&Socket::send_ctrl>("send_ctrl");
    lc.def<&Socket::address>("address");
    lc.def<&Socket::get_http_header>("get_http_header");

    lc.set(Packet::PT_HTTP, "PT_HTTP");
    lc.set(Packet::PT_STREAM, "PT_STREAM");
    lc.set(Packet::PT_WEBSOCKET, "PT_WEBSOCKET");
    lc.set(Packet::PT_WSSTREAM, "PT_WSSTREAM");
}

static void luaopen_lua_codec(lua_State *L)
{
    lcpp::Class<LuaCodec> lc(L, "engine.LuaCodec");

    lc.def<&LuaCodec::encode>("encode");
    lc.def<&LuaCodec::decode>("decode");
}

static void luaopen_pbc_codec(lua_State *L)
{
    lcpp::Class<PbcCodec> lc(L, "engine.PbcCodec");

    lc.def<&PbcCodec::reset>("reset");
    lc.def<&PbcCodec::load>("load");
    lc.def<&PbcCodec::update>("update");
    lc.def<&PbcCodec::encode>("encode");
    lc.def<&PbcCodec::decode>("decode");
}


static void luaopen_sql(lua_State *L)
{

    //lcpp::Class<LSql> lc(L, "engine.Sql");
    //lc.def<&LSql::start>("start");
}

static void luaopen_mongo(lua_State *L)
{
    //lcpp::Class<LMongo> lc(L, "engine.Mongo");
    //lc.def<&LMongo::start>("start");
    //lc.def<&LMongo::stop>("stop");
}

static void luaopen_log(lua_State *L)
{
    lcpp::Class<LLog> lc(L, "engine.Log");
    lc.def<&LLog::stop>("stop");
    lc.def<&LLog::start>("start");

    lc.def<&LLog::plog>("plog");
    lc.def<&LLog::eprint>("eprint");

    lc.def<&LLog::append_file>("append_file");
    lc.def<&LLog::append_log_file>("append_log_file");

    lc.def<&LLog::set_name>("set_name");
    lc.def<&LLog::set_option>("set_option");
    lc.def<&LLog::set_std_option>("set_std_option");

    lc.set(AsyncLog::Policy::PT_NORMAL, "PT_NORMAL");
    lc.set(AsyncLog::Policy::PT_DAILY, "PT_DAILY");
    lc.set(AsyncLog::Policy::PT_SIZE, "PT_SIZE");
}

static void luaopen_acism(lua_State *L)
{
    lcpp::Class<LAcism> lc(L, "engine.Acism");

    lc.def<&LAcism::scan>("scan");
    lc.def<&LAcism::replace>("replace");
    lc.def<&LAcism::load_from_file>("load_from_file");
}

static void luaopen_grid_aoi(lua_State *L)
{
    lcpp::Class<LGridAoi> lc(L, "engine.GridAoi");

    lc.def<&LGridAoi::set_size>("set_size");
    lc.def<&LGridAoi::set_visual_range>("set_visual_range");

    lc.def<&LGridAoi::get_entity>("get_entity");
    lc.def<&LGridAoi::get_all_entity>("get_all_entity");
    lc.def<&LGridAoi::get_visual_entity>("get_visual_entity");
    lc.def<&LGridAoi::get_interest_me_entity>("get_interest_me_entity");

    lc.def<&LGridAoi::exit_entity>("exit_entity");
    lc.def<&LGridAoi::enter_entity>("enter_entity");
    lc.def<&LGridAoi::update_entity>("update_entity");

    lc.def<&LGridAoi::is_same_pos>("is_same_pos");
}

static void luaopen_list_aoi(lua_State *L)
{
    lcpp::Class<LListAoi> lc(L, "engine.ListAoi");

    lc.def<&LListAoi::valid_dump>("valid_dump");
    lc.def<&LListAoi::use_y>("use_y");
    lc.def<&LListAoi::set_index>("set_index");
    lc.def<&LListAoi::update_visual>("update_visual");

    lc.def<&LListAoi::get_entity>("get_entity");
    lc.def<&LListAoi::get_all_entity>("get_all_entity");
    lc.def<&LListAoi::get_visual_entity>("get_visual_entity");
    lc.def<&LListAoi::get_interest_me_entity>("get_interest_me_entity");

    lc.def<&LListAoi::exit_entity>("exit_entity");
    lc.def<&LListAoi::enter_entity>("enter_entity");
    lc.def<&LListAoi::update_entity>("update_entity");
}

static void luaopen_map(lua_State *L)
{
    lcpp::Class<LMap> lc(L, "engine.Map");

    lc.def<&LMap::set>("set");
    lc.def<&LMap::fill>("fill");
    lc.def<&LMap::load>("load");
    lc.def<&LMap::fork>("fork");
    lc.def<&LMap::get_size>("get_size");
    lc.def<&LMap::get_pass_cost>("get_pass_cost");
}

static void luaopen_astar(lua_State *L)
{
    lcpp::Class<LAstar> lc(L, "engine.Astar");

    lc.def<&LAstar::search>("search");
}

////////////////////////////////////////////////////////////////////////////////

namespace llib
{
void open_env(lua_State *L)
{
#define SET_ENV_BOOL(v)    \
    lua_pushboolean(L, 1); \
    lua_setglobal(L, v)

    // 在脚本中，设置LINUX、WINDOWS等系统名为true
    SET_ENV_BOOL(__OS_NAME__);
#undef SET_ENV_BOOL

    lcpp::Class<LLog>::push(L, StaticGlobal::LOG, false);
    lua_setglobal(L, "g_async_log");

    lcpp::Class<MainThread>::push(L, StaticGlobal::M, false);
    lua_setglobal(L, "g_engine");

    lcpp::Class<Env>::push(L, StaticGlobal::E, false);
    lua_setglobal(L, "g_env");
}

void open_cpp(lua_State *L)
{
    /* ============================库方式调用=============================== */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    LUA_LIB_OPEN("engine.util", luaopen_util);
    LUA_LIB_OPEN("engine.lua_parson", luaopen_lua_parson);
    LUA_LIB_OPEN("engine.lua_rapidxml", luaopen_lua_rapidxml);
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

    /* ============================对象方式调用============================= */
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
    luaopen_env(L);
    luaopen_engine(L);
    luaopen_main_thread(L);
    luaopen_worker_thread(L);
    luaopen_tls(L);
    luaopen_socket(L);
    luaopen_lua_codec(L);
    luaopen_pbc_codec(L);
    luaopen_sql(L);
    luaopen_log(L);
    luaopen_map(L);
    luaopen_astar(L);
    luaopen_acism(L);
    luaopen_mongo(L);
    luaopen_grid_aoi(L);
    luaopen_list_aoi(L);
    /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

    /* when debug,make sure lua stack clean after init */
    assert(0 == lua_gettop(L));
}

void open_libs(lua_State *L)
{
    luaL_openlibs(L);
    open_cpp(L);
    open_env(L); // env的变量需要C++的类型，因此需要先open_cpp
}

lua_State *new_state()
{
    lua_State *L = luaL_newstate();

    open_libs(L);

    return L;
}

lua_State* delete_state(lua_State* L)
{
    if (!L) return nullptr;

    assert(0 == lua_gettop(L));

    /* Destroys all objects in the given Lua state (calling the corresponding
     * garbage-collection metamethods, if any) and frees all dynamic memory used
     * by this state
     */
    lua_close(L);

    return nullptr;
}
} // namespace llib