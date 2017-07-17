#include "lobj_counter.h"

class obj_counter *obj_counter::_obj_counter = NULL;

obj_counter::obj_counter()
{
}

obj_counter::~obj_counter()
{
}

void obj_counter::uninstance()
{
    if ( _obj_counter ) delete _obj_counter;

    _obj_counter = NULL;
}

class obj_counter *obj_counter::instance()
{
    if ( !_obj_counter ) _obj_counter = new obj_counter();

    return _obj_counter;
}

int32 obj_counter::add_count( const char *name )
{
    struct count_info &_info = _counter[name];

    if ( _info._max >= ULLONG_MAX )
    {
        ERROR( "lobj_counter overflow" );
        _info._max = 0;
    }

    ++(_info._max);
    ++(_info._cur);
    return _info._cur;
}

int32 obj_counter::dec_count( const char *name )
{
    counter::iterator itr = _counter.find( name );
    if ( itr == _counter.end() ) return -1;

    struct count_info &_info = itr->second;
    return --(_info._cur);
}

/* 在程序结束时，检测是否所有对象都销毁了
 * 由于lua虚拟机被销毁，所有对象都被释放，这个检测没什么意义
 * 除非lclass.h写得有问题
 */
int32 obj_counter::final_check()
{
    counter::iterator itr = _counter.begin();
    while ( itr != _counter.end() )
    {
        const struct count_info &_info = itr->second;

        /* > 0 || < 0 error */
        if ( _info._cur != 0 ) return 0;

        ++itr;
    }

    return 1;
}

/////////////////////////////lua function///////////////////////////////////////

/* 返回一个包含统计信息的lua table
 * {
 *   { name = "xxx",max = 99,cur = 99 }
 * }
 */
int32 lobj_counter::dump( lua_State *L )
{
    class obj_counter *_obj_counter = obj_counter::instance();

    int32 index = 1;
    lua_newtable( L );
    obj_counter::counter::iterator itr = (_obj_counter->_counter).begin();
    while ( itr != (_obj_counter->_counter).end() )
    {
        const struct obj_counter::count_info &_info = itr->second;

        lua_createtable( L,0,3 );
        lua_pushstring( L,"name" );
        lua_pushstring( L,itr->first );
        lua_rawset( L,-3 );

        lua_pushstring( L,"max" );
        lua_pushnumber( L,_info._max );
        lua_rawset( L,-3 );

        lua_pushstring( L,"cur" );
        lua_pushnumber( L,_info._cur );
        lua_rawset( L,-3 );

        lua_rawseti( L,-2,index );
        ++index;

        ++itr;
    }

    return 1;
}

int32 lobj_counter::obj_count( lua_State *L )
{
    const char *name = luaL_checkstring( L,1 );
    class obj_counter *_obj_counter = obj_counter::instance();

    obj_counter::counter::iterator itr = (_obj_counter->_counter).find( name );
    if ( itr == (_obj_counter->_counter).end() ) return 0;

    const struct obj_counter::count_info &_info = itr->second;

    lua_pushnumber( L,_info._max );
    lua_pushnumber( L,_info._cur );
    return 2;
}

static const luaL_Reg obj_counter_lib[] =
{
    {"dump", lobj_counter::dump},
    {"obj_count", lobj_counter::obj_count},
    {NULL, NULL}
};

int32 luaopen_obj_counter( lua_State *L )
{
    luaL_newlib(L, obj_counter_lib);
    return 1;
}
