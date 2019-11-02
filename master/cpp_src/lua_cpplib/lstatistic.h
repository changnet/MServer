#pragma once

#include <lua.hpp>
#include "../system/statistic.h"

class LStatistic
{
public:
    static int32_t dump( lua_State *L );
    static int32_t dump_pkt( lua_State *L );
private:
    static void dump_lua_gc  ( lua_State *L );
    static void dump_thread  ( lua_State *L );
    static void dump_mem_pool( lua_State *L );
    static void dump_socket  ( lua_State *L );
    static void dump_total_traffic ( lua_State *L );
    static void dump_base_counter(
        const Statistic::BaseCounterType &counter,lua_State *L );
};

extern int32_t luaopen_statistic( lua_State *L );
