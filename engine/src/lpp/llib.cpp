#include <lparson.h>
#include <lrapidxml.hpp>

#include "llib.hpp"

#include "lacism.hpp"
#include "lgrid_aoi.hpp"
#include "lastar.hpp"
#include "llog.hpp"
#include "lmap.hpp"
#include "lutil.hpp"
#include "llist_aoi.hpp"

#include "net/socket.hpp"
#include "net/io/tls_ctx.hpp"
#include "net/codec/lua_codec.hpp"
#include "net/codec/pbc_codec.hpp"
#include "system/static_global.hpp"
#include "system/signal.hpp"
#include "thread/worker_thread.hpp"
#include "mysql/mysql.hpp"
#include "mongo/mongo.hpp"
#include "ev/time.hpp"
#include "lbuffer.hpp"

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
    //lua_State *L = StaticGlobal::L;

    //lua_sethook(L, __dbg_break_hook,
    //            LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT, 1);
}

const char *__dbg_traceback()
{
    // 当程序死循环时，用gdb attach到进程
    // call __dbg_traceback()
    // 然后继续执行，应该能打印出lua当前的堆栈
    // 注意：由于无法取得coroutine的L指针，无法打印coroutine中的堆栈
    // 注意：对正在执行程序的影响未知

    // worker线程的L不一样，这里得改一下
    lua_State *L = nullptr; // StaticGlobal::L;

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
    lcpp::module_function<&ThreadContextMgr::add_thread_ctx>(L, "add_thread_ctx");
    lcpp::module_function<&ThreadContextMgr::del_thread_ctx>(L, "del_thread_ctx");
    lcpp::module_function<&timing::steady_clock>(L, "steady_clock");
    lcpp::module_function<&timing::system_clock>(L, "system_clock");
    lcpp::module_function<&timing::time>(L, "time");
    lcpp::module_function<&timing::clock>(L, "clock");
    lcpp::module_function<&timing::time_ms>(L, "time_ms");
    lcpp::module_function<&timing::update>(L, "update");
    lcpp::module_end(L);
}

static void luaopen_env(lua_State* L)
{
    lcpp::Class<Env> lc(L, "engine.Env");
    lc.def<&Env::get>("get");
    lc.def<&Env::set>("set");
}

static void luaopen_ev(lua_State *L)
{
    lcpp::Class<EV> lc(L, "engine.EV");
    lc.def<&EV::stop>("stop");
    lc.def<&EV::push_message>("push_message");
    lc.def<&EV::emplace_message>("emplace_message");
    lc.def<&EV::construct_message>("construct_message");
    lc.def<&EV::destruct_message>("destruct_message");
    lc.def<&EV::acquire_message>("acquire_message");
    lc.def<&EV::timer_start>("timer_start");
    lc.def<&EV::timer_stop>("timer_stop");
    lc.def<&EV::periodic_start>("periodic_start");
    lc.def<&EV::periodic_stop>("periodic_stop");
}

static void luaopen_worker_thread(lua_State *L)
{
    lcpp::Class<WorkerThread> lc(L, "engine.WorkerThread");
    lc.constructor<std::string>();
    lc.def<&WorkerThread::start>("start");
    lc.def<&WorkerThread::stop>("stop");
    lc.def<&WorkerThread::is_start>("is_start");
    lc.def<&WorkerThread::push_message>("push_message");
    lc.def<&WorkerThread::emplace_message>("emplace_message");
    lc.def<&WorkerThread::acquire_message>("acquire_message");
    lc.def<&WorkerThread::timer_start>("timer_start");
    lc.def<&WorkerThread::timer_stop>("timer_stop");
    lc.def<&WorkerThread::periodic_start>("periodic_start");
    lc.def<&WorkerThread::periodic_stop>("periodic_stop");
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
    lc.def<&Socket::accept>("accept");
    lc.def<&Socket::get_event>("get_event");
    lc.def<&Socket::set_event>("set_event");
    lc.def<&Socket::set_io>("set_io");
    lc.def<&Socket::get_io>("get_io");
    lc.def<&Socket::set_packet>("set_packet");
    lc.def<&Socket::is_connect_success>("is_connect_success");
    lc.def<&Socket::set_buffer_params>("set_buffer_params");
    lc.def<&Socket::io_init_accept>("io_init_accept");
    lc.def<&Socket::io_init_connect>("io_init_connect");
    lc.def<&Socket::send_pkt>("send_pkt");
    lc.def<&Socket::send_clt>("send_clt");
    lc.def<&Socket::send_srv>("send_srv");
    lc.def<&Socket::send_ctrl>("send_ctrl");
    lc.def<&Socket::address>("address");
    lc.def<&Socket::get_http_header>("get_http_header");
    lc.def<&Socket::unpack_on_closed>("unpack_on_closed");
    lc.def<&Socket::get_errno>("get_errno");
    lc.def<&Socket::unpack>("unpack");
    lc.def<&Socket::close>("close");
    lc.def<&Socket::set_keep_alive>("set_keep_alive");
    lc.def<&Socket::set_user_timeout>("set_user_timeout");
    lc.def<&Socket::set_nodelay>("set_nodelay");
    lc.def<&Socket::set_watcher_event>("set_watcher_event");
    lc.def<&Socket::set_ipv6only>("set_ipv6only");

    lc.set(Packet::PT_HTTP, "PT_HTTP");
    lc.set(Packet::PT_SSSTREAM, "PT_SSSTREAM");
    lc.set(Packet::PT_WEBSOCKET, "PT_WEBSOCKET");
    lc.set(Packet::PT_WSSTREAM, "PT_WSSTREAM");
}

static void luaopen_socket_io(lua_State *L)
{
    lcpp::Class<IO> lc(L, "engine.IO");

    lc.def_pointer_call<&IO::set_ssl_sni>("set_ssl_sni");
    lc.def_pointer_call<&IO::set_ssl_alpn>("set_ssl_alpn");
    lc.def_pointer_call<&IO::set_ssl_cert_host>("set_ssl_cert_host");
    lc.def_pointer_call<&IO::set_ssl_verify_mode>("set_ssl_verify_mode");
}

static void luaopen_lua_codec(lua_State *L)
{
    lcpp::Class<LuaCodec> lc(L, "engine.LuaCodec");

    lc.def<&LuaCodec::encode>("encode");
    lc.def<&LuaCodec::decode>("decode");
    lc.def<&LuaCodec::decode_from_buffer>("decode_from_buffer");
    lc.def<&LuaCodec::encode_to_buffer>("encode_to_buffer");
}

static void luaopen_pbc_codec(lua_State *L)
{
    lcpp::Class<PbcCodec> lc(L, "engine.PbcCodec");

    lc.def<&PbcCodec::reset>("reset");
    lc.def<&PbcCodec::load>("load");
    lc.def<&PbcCodec::update>("update");
    lc.def<&PbcCodec::encode>("encode");
    lc.def<&PbcCodec::decode>("decode");
    lc.def<&PbcCodec::decode_from_buffer>("decode_from_buffer");
    lc.def<&PbcCodec::encode_to_buffer>("encode_to_buffer");
}


static void luaopen_mysql(lua_State *L)
{
    lcpp::Class<MySql> lc(L, "engine.MySql");

    lc.def<&MySql::ping>("ping");
    lc.def<&MySql::exec>("exec");
    lc.def<&MySql::query>("query");
    lc.def<&MySql::error>("error");
    lc.def<&MySql::escape>("escape");
    lc.def<&MySql::connect>("connect");
    lc.def<&MySql::disconnect>("disconnect");

    lc.def<&MySql::thread_init>("thread_init");
    lc.def<&MySql::thread_end>("thread_end");

    lc.def<&MySql::stmt_str>("stmt_str");
    lc.def<&MySql::stmt_exec>("stmt_exec");
    lc.def<&MySql::stmt_value>("stmt_value");
    lc.def<&MySql::stmt_clear>("stmt_clear");
}

static void luaopen_mongo(lua_State *L)
{
    lcpp::Class<Mongo> lc(L, "engine.Mongo");
    lc.def<&Mongo::uriconnect>("uriconnect");
    lc.def<&Mongo::disconnect>("disconnect");
    lc.def<&Mongo::set_array_opt>("set_array_opt");

    lc.def<&Mongo::ping>("ping");
    lc.def<&Mongo::error>("error");
    lc.def<&Mongo::insert>("insert");
    lc.def<&Mongo::update>("update");
    lc.def<&Mongo::count>("count");
    lc.def<&Mongo::find>("find");
    lc.def<&Mongo::remove>("remove");
    lc.def<&Mongo::find_and_modify>("find_and_modify");
    lc.def<&Mongo::drop_collection>("drop_collection");
    lc.def<&Mongo::drop_index>("drop_index");
    lc.def<&Mongo::create_index>("create_index");
    lc.def<&Mongo::aggregate>("aggregate");
    lc.def<&Mongo::insert_many>("insert_many");
}

static void luaopen_log(lua_State *L)
{
    lcpp::Class<LLog> lc(L, "engine.Log");
    lc.constructor<const char *>();
    lc.def<&LLog::stop>("stop");
    lc.def<&LLog::start>("start");

    lc.def<&LLog::print>("print");
    lc.def<&LLog::error>("error");

    lc.def<&LLog::append>("append");

    lc.def<&LLog::set_name>("set_name");
    lc.def<&LLog::add_device>("add_device");
    lc.def<&LLog::del_device>("del_device");
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

static void luaopen_buffer(lua_State *L)
{
    lcpp::Class<LuaBuffer> lc(L, "engine.Buffer");
    lc.def<&LuaBuffer::get>("get");
    lc.def<&LuaBuffer::set>("set");
    lc.def<&LuaBuffer::read_int>("read_int");
    lc.def<&LuaBuffer::write_int>("write_int");
    lc.def<&LuaBuffer::write_buffer>("write_buffer");
    lc.def<&LuaBuffer::lightud_tostring>("lightud_tostring");
    lc.def<&LuaBuffer::fromstring>("fromstring");
    lc.def<&LuaBuffer::tostring>("tostring");
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

    lcpp::Class<EV>::push(L, StaticGlobal::E, false);
    lua_setglobal(L, "g_mthread");

    lcpp::Class<Env>::push(L, StaticGlobal::V, false);
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
    luaopen_ev(L);
    luaopen_worker_thread(L);
    luaopen_tls(L);
    luaopen_socket(L);
    luaopen_socket_io(L);
    luaopen_lua_codec(L);
    luaopen_pbc_codec(L);
    luaopen_mysql(L);
    luaopen_log(L);
    luaopen_map(L);
    luaopen_astar(L);
    luaopen_acism(L);
    luaopen_mongo(L);
    luaopen_grid_aoi(L);
    luaopen_list_aoi(L);
    luaopen_buffer(L);
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