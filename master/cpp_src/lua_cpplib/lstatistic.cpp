#include "lstatistic.h"
#include "../system/static_global.h"

int32 lstatistic::dump( lua_State *L )
{
#define DUMP_BASE_COUNTER(what,counter)    \
    do{\
        lua_pushstring( L,what );\
        dump_base_counter( counter,L );\
        lua_rawset( L,-3 );\
    } while(0)

    const statistic *stat = static_global::statistic();

    lua_newtable( L );

    DUMP_BASE_COUNTER( "c_obj",stat->get_c_obj() );
    DUMP_BASE_COUNTER( "c_lua_obj",stat->get_c_lua_obj() );

    lua_pushstring( L,"thread" );
    dump_thread( L );
    lua_rawset( L,-3 );

    lua_pushstring( L,"lua_gc" );
    dump_lua_gc( L );
    lua_rawset( L,-3 );

    lua_pushstring( L,"mem_pool" );
    dump_mem_pool( L );
    lua_rawset( L,-3 );

    return 1;

#undef DUMP_BASE_COUNTER
}

void lstatistic::dump_base_counter( 
    const statistic::base_counter_t &counter,lua_State *L )
{
    int32 index = 1;

    lua_newtable( L );
    statistic::base_counter_t::const_iterator itr = counter.begin();
    while ( itr != counter.end() )
    {
        const struct statistic::base_counter &bc = itr->second;
        lua_createtable( L,0,3 );
        lua_pushstring( L,"name" );
        lua_pushstring( L,itr->first._raw_ctx );
        lua_rawset( L,-3 );

        lua_pushstring( L,"max" );
        lua_pushinteger( L,bc._max );
        lua_rawset( L,-3 );

        lua_pushstring( L,"cur" );
        lua_pushinteger( L,bc._cur );
        lua_rawset( L,-3 );

        lua_rawseti( L,-2,index );
        ++index;itr ++;
    }
}

void lstatistic::dump_thread( lua_State *L )
{
    const thread_mgr::thread_mpt_t &threads =
        static_global::thread_mgr()->get_threads();

    int32 index = 1;
    lua_newtable( L );
    thread_mgr::thread_mpt_t::const_iterator itr = threads.begin();
    while ( itr != threads.end() )
    {
        class thread *thread = itr->second;

        size_t finished = 0;
        size_t unfinished = 0;
        thread->busy_job( &finished,&unfinished );

        lua_createtable( L,0,3 );

        lua_pushstring( L,"id" );
        lua_pushinteger( L,thread->get_id() );
        lua_rawset( L,-3 );

        lua_pushstring( L,"name" );
        lua_pushstring( L,thread->get_name() );
        lua_rawset( L,-3 );

        lua_pushstring( L,"finish" );
        lua_pushinteger( L,finished );
        lua_rawset( L,-3 );

        lua_pushstring( L,"unfinished" );
        lua_pushinteger( L,unfinished );
        lua_rawset( L,-3 );

        lua_rawseti( L,-2,index );

        ++index;itr ++;
    }
}

void lstatistic::dump_lua_gc( lua_State *L )
{
    const statistic::time_counter &counter 
        = static_global::statistic()->get_lua_gc();

    lua_newtable( L );

    lua_pushstring( L,"count" );
    lua_pushinteger( L,counter._count );
    lua_rawset( L,-3 );

    lua_pushstring( L,"msec" );
    lua_pushinteger( L,counter._msec );
    lua_rawset( L,-3 );

    lua_pushstring( L,"max" );
    lua_pushinteger( L,counter._max );
    lua_rawset( L,-3 );

    lua_pushstring( L,"min" );
    lua_pushinteger( L,counter._min );
    lua_rawset( L,-3 );

    lua_pushstring( L,"avg" );
    lua_pushinteger( L,counter._msec/counter._count );
    lua_rawset( L,-3 );
}

void lstatistic::dump_mem_pool( lua_State *L )
{
    int32 index = 1;
    lua_newtable( L );

    class pool** pool_stat = pool::get_pool_stat();
    for (int32 idx = 0;idx < pool::MAX_POOL;idx ++)
    {
        if (NULL == pool_stat[idx]) continue;

        class pool *ps = pool_stat[idx];

        lua_newtable( L );

        lua_pushstring( L,"name" );
        lua_pushstring( L,ps->get_name() );
        lua_rawset( L,-3 );

        // 累计分配数量
        int64 max_new = ps->get_max_new();
        lua_pushstring( L,"max" );
        lua_pushinteger( L, max_new);
        lua_rawset( L,-3 );

        // 累计销毁数量
        int64 max_del = ps->get_max_del();
        lua_pushstring( L,"del" );
        lua_pushinteger( L,max_del );
        lua_rawset( L,-3 );

        // 当前还在内存池中数量
        int64 max_now = ps->get_max_now();
        lua_pushstring( L,"now" );
        lua_pushinteger( L, max_now);
        lua_rawset( L,-3 );

        // 单个对象内存大小
        size_t size = ps->get_sizeof();
        lua_pushstring( L,"sizeof" );
        lua_pushinteger( L,size );
        lua_rawset( L,-3 );

        // 当前在这个内存池分配的内存
        lua_pushstring( L,"mem" );
        lua_pushinteger( L,(max_new - max_del) * size );
        lua_rawset( L,-3 );

        // 当前还在池内的内存
        lua_pushstring( L,"now_mem" );
        lua_pushinteger( L,max_now * size );
        lua_rawset( L,-3 );

        lua_rawseti( L,-2,index++ );
    }
}

////////////////////////////////////////////////////////////////////////////////
static const luaL_Reg statistic_lib[] =
{
    {"dump", lstatistic::dump},
    {NULL, NULL}
};

int32 luaopen_statistic( lua_State *L )
{
    luaL_newlib(L, statistic_lib);
    return 1;
}
