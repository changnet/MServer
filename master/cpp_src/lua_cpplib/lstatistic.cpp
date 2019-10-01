#include "lstatistic.h"
#include "../system/static_global.h"

#define PUSH_STRING( name,val ) \
    do{\
        lua_pushstring( L,name );lua_pushstring( L,val );lua_rawset( L,-3 );\
    } while(0)

#define PUSH_INTEGER( name,val ) \
    do{\
        lua_pushstring( L,name );lua_pushinteger( L,val );lua_rawset( L,-3 );\
    } while(0)

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

    lua_pushstring( L,"traffic" );
    dump_total_traffic( L );
    lua_rawset( L,-3 );

    lua_pushstring( L,"socket" );
    dump_socket( L );
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

        PUSH_STRING( "name",itr->first.c_str() );

        PUSH_INTEGER( "max",bc._max );

        PUSH_INTEGER( "cur",bc._cur );

        lua_rawseti( L,-2,index );
        index ++;itr ++;
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

        PUSH_INTEGER( "id",thread->get_id() );

        PUSH_STRING( "name",thread->get_name() );

        PUSH_INTEGER( "finish",finished );

        PUSH_INTEGER( "unfinished",unfinished );

        lua_rawseti( L,-2,index );

        index ++;itr ++;
    }
}

void lstatistic::dump_lua_gc( lua_State *L )
{
    const statistic::time_counter &counter 
        = static_global::statistic()->get_lua_gc();

    lua_newtable( L );

    PUSH_INTEGER( "count",counter._count );

    PUSH_INTEGER( "msec",counter._msec );

    PUSH_INTEGER( "max",counter._max );

    PUSH_INTEGER( "min",counter._min );

    int64 count = counter._count > 0 ? counter._count : 1;
    PUSH_INTEGER( "avg",counter._msec / count );
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

        PUSH_STRING( "name",ps->get_name() );

        // 累计分配数量
        int64 max_new = ps->get_max_new();
        PUSH_INTEGER( "max",max_new );

        // 累计销毁数量
        int64 max_del = ps->get_max_del();
        PUSH_INTEGER( "del",max_del );

        // 当前还在内存池中数量
        int64 max_now = ps->get_max_now();
        PUSH_INTEGER( "now",max_now );

        // 单个对象内存大小
        size_t size = ps->get_sizeof();
        PUSH_INTEGER( "sizeof",size );

        // 当前在这个内存池分配的内存
        PUSH_INTEGER( "mem",(max_new - max_del) * size );

        // 当前还在池内的内存
        PUSH_INTEGER( "now_mem",max_now * size );

        lua_rawseti( L,-2,index++ );
    }
}

// 总的收发流量
void lstatistic::dump_total_traffic ( lua_State *L )
{
    const statistic *stat = static_global::statistic();
    const statistic::traffic_counter *list = stat->get_total_traffic();

    const time_t now = static_global::ev()->now();

    lua_newtable( L );
    for ( int32 type = 1;type < socket::CNT_MAX;type ++ )
    {
        const statistic::traffic_counter &counter = list[type];

        lua_newtable( L );
        int32 sec = now - counter._time;
        if (sec < 1) sec = 1;

        PUSH_INTEGER( "send",counter._send );

        PUSH_INTEGER( "recv",counter._recv );

        PUSH_INTEGER( "send_avg",counter._send / sec );

        PUSH_INTEGER( "recv_avg",counter._recv / sec );

        lua_rawseti( L,-2,type );
    }
}


// socket情况
void lstatistic::dump_socket( lua_State *L )
{
#define ST_T statistic::socket_traffic_t

    const statistic *stat = static_global::statistic();
    const ST_T &socket_traffic = stat->get_socket_traffic();

    const time_t now = static_global::ev()->now();

    class lnetwork_mgr *nm = static_global::network_mgr();

    int32 index = 1;
    lua_newtable( L );

    ST_T::const_iterator itr = socket_traffic.begin();
    while ( itr != socket_traffic.end() )
    {
        const statistic::traffic_counter &counter = itr->second;

        lua_newtable( L );

        uint32 conn_id = itr->first;
        int32 sec = now - counter._time;
        if (sec < 1) sec = 1;

        PUSH_INTEGER( "conn_id",conn_id );
        PUSH_INTEGER( "send",counter._send );
        PUSH_INTEGER( "recv",counter._recv );
        PUSH_INTEGER( "send_avg",counter._send / sec );
        PUSH_INTEGER( "recv_avg",counter._recv / sec );

        class socket *sk = nm->get_conn_by_conn_id( itr->first );
        if (!sk)
        {
            ERROR("dump_socket no such socket found:",conn_id);
        }
        else
        {
            uint32 smem = 0;
            uint32 rmem = 0;
            uint32 schunk = 0;
            uint32 rchunk = 0;
            uint32 spending = 0;
            uint32 rpending = 0;
            sk->get_stat( schunk,rchunk,smem,rmem,spending,rpending);

            // object_id，如果是客户端连接，则是玩家pid，其他情况没什么用
            PUSH_INTEGER( "object_id",sk->get_object_id() );
            PUSH_INTEGER( "send_mem",smem );
            PUSH_INTEGER( "recv_mem",rmem );
            PUSH_INTEGER( "send_chunk",schunk );
            PUSH_INTEGER( "recv_chunk",rchunk );
            PUSH_INTEGER( "send_pending",spending );
            PUSH_INTEGER( "recv_pending",rpending );
        }

        itr ++;
        lua_rawseti( L,-2,index++ );
    }
#undef ST_T
}

int32 lstatistic::dump_pkt( lua_State *L )
{
    const statistic *stat = static_global::statistic();

    lua_newtable( L );
    for (int32 type = SPKT_CSPK;type < SPKT_MAXT;type ++)
    {
        int32 index = 1;
        lua_newtable( L );
        const statistic::pkt_counter_t &pkts = stat->_pkt_count[type];

        statistic::pkt_counter_t::const_iterator itr = pkts.begin();
        while ( itr != pkts.end() )
        {
            lua_newtable( L );
            const statistic::pkt_counter &counter = itr->second;
            PUSH_INTEGER( "cmd",itr->first );
            PUSH_INTEGER( "max",counter._max );
            PUSH_INTEGER( "min",counter._min );
            PUSH_INTEGER( "msec",counter._msec );
            PUSH_INTEGER( "size",counter._size );
            PUSH_INTEGER( "count",counter._count );
            PUSH_INTEGER( "max_size",counter._max_size );
            PUSH_INTEGER( "min_size",counter._min_size );
            PUSH_INTEGER( "avg",counter._msec / counter._count );
            PUSH_INTEGER( "avg_size",counter._size / counter._count );

            itr ++;
            lua_rawseti( L,-2,index++ );
        }

        lua_rawseti( L,-2,type );
    }

    int32 index = 1;
    lua_newtable( L );

    statistic::rpc_counter_t::const_iterator itr = stat->_rpc_count.begin();
    while ( itr != stat->_rpc_count.end() )
    {
        lua_newtable( L );
        const statistic::pkt_counter &counter = itr->second;
        PUSH_STRING ( "cmd",itr->first.c_str() );
        PUSH_INTEGER( "max",counter._max );
        PUSH_INTEGER( "min",counter._min );
        PUSH_INTEGER( "msec",counter._msec );
        PUSH_INTEGER( "size",counter._size );
        PUSH_INTEGER( "count",counter._count );
        PUSH_INTEGER( "max_size",counter._max_size );
        PUSH_INTEGER( "min_size",counter._min_size );
        PUSH_INTEGER( "avg",counter._msec / counter._count );
        PUSH_INTEGER( "avg_size",counter._size / counter._count );

        itr ++;
        lua_rawseti( L,-2,index++ );
    }

    return 2;
}

////////////////////////////////////////////////////////////////////////////////
static const luaL_Reg statistic_lib[] =
{
    {"dump", lstatistic::dump},
    {"dump_pkt", lstatistic::dump_pkt},
    {NULL, NULL}
};

int32 luaopen_statistic( lua_State *L )
{
    luaL_newlib(L, statistic_lib);
    return 1;
}
