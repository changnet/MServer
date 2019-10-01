#pragma once

#include <lua.hpp>
#include "../system/statistic.h"

class lstatistic
{
public:
    static int32 dump( lua_State *L );
    static int32 dump_pkt( lua_State *L );
private:
    static void dump_lua_gc  ( lua_State *L );
    static void dump_thread  ( lua_State *L );
    static void dump_mem_pool( lua_State *L );
    static void dump_socket  ( lua_State *L );
    static void dump_total_traffic ( lua_State *L );
    static void dump_base_counter(
        const statistic::base_counter_t &counter,lua_State *L );
};

extern int32 luaopen_statistic( lua_State *L );
