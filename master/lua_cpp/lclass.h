#ifndef __LCLASS_H__
#define __LCLASS_H__

/*
 * http://lua-users.org/wiki/CppBindingWithLunar
 * 原代码中gc控制在push每一个对象时设置，但其实所有对象共用一个metatable
 * (lua_pushvalue并不会复制table，只是在堆栈上复制一个指针)，而gc控制
 * 在metatable中，故是按类控制gc.
 * 本代码使用gc控制的原因是交给lua控制的只是一个对象指针，对象内存仍由c层管理。这样
 * 在lua层中的指针被销毁，对象仍能在c层继续使用。又或者push的对象不是new出来的。
 */

/* 注册struct、class到lua，适用于在lua创建c、c++对象 */

#include "../global/global.h"
#include <lua.hpp>

template<class T>
class lclass
{
public:
    lclass( lua_State *L,const char *super = NULL )
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
        /* oo.cclass should return a table on stack now,the original metatable */
        lua_pushcfunction(L, init);
        lua_setfield(L, -2, "__init");
        
        lua_pushcfunction(L, gc);
        lua_setfield(L, -2, "__gc");
        
        lua_pushstring(L, T::classname);
        lua_setfield(L, -2, "_name_");
        
        lua_pushcfunction(L, tostring);
        lua_setfield(L, -2, "__tostring");
        
        /* metatable as value and pop metatable */
        lua_setfield(L, -2, "__index");
        
        lua_pop(L,1);/* pop oo from stack */
    }
    
    /* 将c对象push栈,gc表示lua销毁userdata时，在gc函数中是否将当前指针delete */
    static int push(lua_State *L, T *obj, bool gc = false)
    {
        assert( "push null obj",obj );

        /* 这里只是创建一个指针给lua管理，可以换用placement new把整个对象的
           内存都给lua管理 
        */
        T** ptr = (T**)lua_newuserdata(L, sizeof(T*));
        *ptr = obj;

        get_classtable(L);
        /* metatable on stack now,even it maybe nil */

        return set_classtable(L,gc);
    }
    
    typedef int32 (T::*pf_t)(lua_State*);

    /* 注册函数,const char* func_name 就是注册到lua中的函数名字 */
    template <pf_t pf>
    lclass<T>& def(const char* func_name)
    {
        get_classtable(L);

        lua_getfield(L, -1, func_name);
        if ( !lua_isnil(L, -1) )
        {
            ERROR( "dumplicate def function %s:%s\n",T::classname,func_name );
        }
        lua_pop(L, 1); /* drop field */

        lua_pushcfunction(L, &fun_thunk<pf>);
        lua_setfield(L, -2, func_name);

        lua_pop(L, 1); /* drop class metatable */

        return *this;
    }
    
    /* 注册变量,通常用于设置宏定义、枚举 */
    lclass<T>& set(const char* val_name, int32 val)
    {
        get_classtable(L);

        lua_getfield(L, -1, val_name);
        if(!lua_isnil(L, -1))
        {
            ERROR( "dumplicate set variable %s:%s\n",T::classname,val_name );
        }
        lua_pop(L, 1);/* drop field */

        lua_pushinteger(L, val);
        lua_setfield(L, -2, val_name);

        lua_pop(L, 1); /* drop class metatable */

        return *this;
    }
private:
    /* 初始化函数，在lua中的__init */
    static int init(lua_State* L)
    {
        T* obj = new T();
        
        T** ptr = (T**)lua_newuserdata(L, sizeof(T*));
        *ptr = obj;
        
        /* 把新创建的userdata和元表交换堆栈位置 */
        lua_insert(L,1);

        /* lua无法给userdata设metatable */
        return set_classtable( L,true );
    }
    
    /* 元方法,__tostring */
    static int tostring(lua_State* L)
    {
        std::cout << lua_gettop(L) << lua_type(L,1) << std::endl;
        if( !lua_isuserdata(L, 1) )
        {
            luaL_error( L,"unable to call %s::tostring,userdata expected",T::classname );
            ERROR( "unable to call %s:tostring,userdata expected\n",T::classname );
            return 0;
        }

        T** ptr = (T**)lua_touserdata(L, -1);
        if(ptr != NULL)
        {
            lua_pushfstring(L, "%s: %p", T::classname, *ptr);
            return 1;
        }
        return 0;
    }

    /* gc函数 */
    static int gc(lua_State* L)
    {
        if ( luaL_getmetafield(L, 1, "_notgc") )
        {
            /* 以userdata为key取值。如果未设置该userdata的_notgc值，则将会取得nil */
            lua_pushvalue(L, 1);
            lua_gettable(L, -2);
            /* gc = true表示执行gc函数 */
            if ( !lua_toboolean(L, -1) )
                return 0;
        }

        T** ptr = (T**)lua_touserdata(L, 1);
        if ( *ptr != NULL )
            delete *ptr;
        *ptr = NULL;

        return 0;
    }
    
    //创建弱表
    static void weaktable(lua_State *L, const char *mode)
    {
        lua_newtable(L);
        lua_pushvalue(L, -1);  // table is its own metatable
        lua_setmetatable(L, -2);
        lua_pushliteral(L, "__mode");
        lua_pushstring(L, mode);
        lua_settable(L, -3);   // metatable.__mode = mode
    }

    //创建子弱表
    static void subtable(lua_State *L, int tindex, const char *name, const char *mode)
    {
        lua_pushstring(L, name);
        lua_gettable(L, tindex);/* 判断是否已存在t[name] */

        if ( lua_isnil(L, -1) ) /* 不存在，则创建 */
        {std::cout << lua_gettop(L) << std::endl;
            lua_pop(L, 1);               /* drop nil */
            weaktable(L, mode);
            lua_pushstring(L, name);
            lua_pushvalue(L, -2);
            lua_settable(L, tindex);   /* set t[name] */
        }
    }
    
    /* 将当前类的元表压栈 */
    static int get_classtable(lua_State* L)
    {
        lua_getglobal(L, "oo");
        if ( expect_false(!lua_istable( L,lua_gettop(L) ))  )
        {
            FATAL( "get_classtable unable to get lua script oo\n" );
            return 0;
        }

        lua_getfield(L, -1, "metatableof");
        if ( expect_false(!lua_isfunction( L,lua_gettop(L) ))  )
        {
            FATAL( "get_classtable can not get class metatable(metatableof)\n" );
            return 0;
        }

        lua_pushstring( L,T::classname ); /* third argument as class name */
        if ( expect_false( LUA_OK != lua_pcall(L,1,1,0) ) )
        {
            FATAL( "call oo.metatableof fail:%s\n",lua_tostring(L,-1) );
            return 0;
        }
        
        lua_remove( L,-2 ); /* drop oo */
        return 1;
    }
    
    /* 设置压栈对象的metatable */
    static int set_classtable(lua_State *L,bool gc)
    {
        /* 如果不自动gc，则需要在metatable中设置一张名为_notgc的表。以userdata为key的
        weaktable。当lua层调用gc时,userdata本身还存在，故这时判断是准确的 */
        if ( !gc )
        {
            subtable(L, 2, "_notgc", "k");

            lua_pushvalue(L,1);  /* 复制userdata到栈顶 */
            lua_pushboolean(L,0); /* do not delete memory */
            lua_settable(L,-3);  /* _notgc[userdata] = 1 and pop _notgc table*/
            
            lua_pop(L, 1); /* drop _notgc out of stack */
        }

        /* 此时又是metatable在栈顶 */
        lua_setmetatable(L, 1); /* 共享预先生成的元表并弹出metatable */

        return 1;
    }
    
    template <pf_t pf>
    static int fun_thunk(lua_State* L)
    {
        if ( expect_false(!lua_isuserdata(L, 1)) ) return 0;

        T** ptr = (T**)lua_touserdata(L, -1);/* get 'self', or if you prefer, 'this' */
        if ( expect_false(ptr == NULL || *ptr == NULL) )
        {
            luaL_error(L, "%s calling method with null pointer", T::classname);
            FATAL( "%s calling method with null pointer", T::classname );
            return 0;
        }
        
        /* remove self so member function args start at index 1 */
        lua_pop(L, 1);

        return ((*ptr)->*pf)(L);
    }
private:
    lua_State *L;
};

#endif /* __LCLASS_H__ */
