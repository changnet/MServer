#include "lstatistic.h"


int32 lstatistic::dump( lua_State *L )
{
#define DUMP_BASE_COUNTER(what,counter)    \
    do{\
        lua_pushstring( L,what );\
        dump_base_counter( counter,L );\
        lua_rawset( L,-3 );\
    } while(0)
    const statistic *stat = statistic::instance();

    lua_newtable( L );

    DUMP_BASE_COUNTER( "c_obj",stat->get_c_obj() );
    DUMP_BASE_COUNTER( "c_lua_obj",stat->get_c_lua_obj() );

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
        lua_pushstring( L,itr->first );
        lua_rawset( L,-3 );

        lua_pushstring( L,"max" );
        lua_pushnumber( L,bc._max );
        lua_rawset( L,-3 );

        lua_pushstring( L,"cur" );
        lua_pushnumber( L,bc._cur );
        lua_rawset( L,-3 );

        lua_rawseti( L,-2,index );
        ++index;itr ++;
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
