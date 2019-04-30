#include "statistic.h"

statistic::~statistic()
{
}

statistic::statistic()
{
}

void statistic::add_c_obj(const char *what,int32 count)
{
    class base_counter &counter = _c_obj[what];

    counter._cur += count;
    if (count > 0)
    {
        counter._max += count;
    }
    else
    {
        assert("add_c_obj count < 0",counter._cur >= 0);
    }
}

void statistic::add_c_lua_obj(const char *what,int32 count)
{
    class base_counter &counter = _c_lua_obj[what];

    counter._cur += count;
    if (count > 0)
    {
        counter._max += count;
    }
    else
    {
        assert("add_c_lua_obj count < 0",counter._cur >= 0);
    }
}
