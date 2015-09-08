#ifndef __LCLASS_H__
#define __LCLASS_H__

/*
 * http://lua-users.org/wiki/CppBindingWithLunar
 * 原代码中gc控制在push每一个对象时设置，但其实所有对象共用一个metatable
 * (lua_pushvalue并不会复制table，只是在堆栈上复制一个指针)，而gc控制
 * 在metatable中，故是按类控制gc.
 * 本代码使用gc控制的原因是交给lua控制的只是一个对象指针，对象内存仍由c层管理。这样
 * 在lua层中的指针被销毁，对象仍能在c层继续使用。
 */

/* 注册struct、class到lua，适用于在lua创建c、c++对象 */

#include "../global/global.h"
#include <lua.hpp>

template<class T>
class lclass
{
public:
    /* gc = true表示执行gc函数 */
    lclass( lua_State *L,const char *super = NULL,bool _gc = true )
        :L(L)
    {
        /* lua 5.3的get函数基本返回类型，而5.1基本为void。需要另外调用check函数 */
        lua_getglobal( L,"oo" );
        if ( expect_false(!lua_istable( L,-1 ))  )
        {
            FATAL( "unable to get lua script oo\n" );
            return;
        }

        lua_getfield(L,-1,"cclass");
        if ( expect_false(!lua_isfunction( L,-1 )) )
        {
            FATAL( "unable to get function 'class' in lua script\n" );
            return;
        }
        
        lua_pushstring( L,super ); /* If s is NULL, pushes nil and returns NULL */
        lua_pushstring( L,T::classname ); /* third argument as class name */
        
        /* call oo.cclass( self,super,T::className ) */
        if ( expect_false( LUA_OK != lua_pcall(L,2,1,0) ) )
        {
            FATAL( "call oo.class fail:%s\n",lua_tostring(L,-1) );
            return;
        }
        std::cout << "====================" << T::classname << std::endl;
        /* oo.cclass should return a table on stack now,the original metatable */
        lua_pushcfunction(L, init);
        lua_setfield(L, -2, "__init");
        
        lua_pushcfunction(L, gc);
        lua_setfield(L, -2, "__gc");
        
        lua_pushboolean(L,_gc ? 1 : 0);
        lua_setfield(L, -2, "_notgc");
        
        lua_pushstring(L, T::classname);
        lua_setfield(L, -2, "_name_");
        
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");/* pop metatable */
        
        lua_pop(L,1);/* pop oo from stack */
    }
private:
    /* 初始化函数，在lua中的__init */
    static int init(lua_State* L)
    {
        T* obj = new T(L);
        /* 这里只是创建一个指针给lua管理，可以换用placement new把整个对象的
           内存都给lua管理。但会导致gc机制失效
        */
        T** ptr = (T**)lua_newuserdata(L, sizeof(T*));
        *ptr = obj;

        /* metatable交给oo.cnew */
        return 1;
    }

    /* gc函数 */
    static int gc(lua_State* L)
    {
        if(luaL_getmetafield(L, 1, "_notgc"))
        {
            /* gc = true表示执行gc函数 */
            if ( !lua_toboolean(L, -1) )
                return 0;
        }

        T** ptr = (T**)lua_touserdata(L, 1);
        if(*ptr != NULL)
            delete *ptr;
        *ptr = NULL;

        return 0;
    }

    /* 将c对象push栈,gc表示lua销毁userdata时，在gc函数中是否将当前指针delete */
    static int push(lua_State *L, T *obj, bool gc=false)
    {
        assert( "push null obj",obj );

        /* 这里只是创建一个指针给lua管理，可以换用placement new把整个对象的
           内存都给lua管理 
        */
        T** ptr = (T**)lua_newuserdata(L, sizeof(T*));
        *ptr = obj;

        int st = lua_gettop(L);
        lua_getglobal(L, "oo");
        if ( expect_false(!lua_istable( L,lua_gettop(L) ))  )
        {
            FATAL( "push obj unable to get lua script oo\n" );
            return;
        }

        lua_getfield(L, -1, "metatableof");
        if ( expect_false(!lua_isfunction( L,lua_gettop(L) ))  )
        {
            FATAL( "push obj can not get class metatable(metatableof)\n" );
            return;
        }

        lua_pushstring( L,T::classname ); /* third argument as class name */
        if ( expect_false( LUA_OK != lua_pcall(L,1,1,0) ) )
        {
            FATAL( "call oo.metatableof fail:%s\n",lua_tostring(L,-1) );
            return;
        }
        
        /* metatable on stack now,even it maybe nil */
        lua_setmetatable(L, st); /* 共享预先生成的元表 */

        lua_settop(L, st);

        return st; /* boj on the stack top */
    }
private:
    lua_State *L;
};

#endif /* __LCLASS_H__ */
