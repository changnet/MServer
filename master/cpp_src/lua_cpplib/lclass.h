#pragma once

/*
 * http://lua-users.org/wiki/CppBindingWithLunar
 *
 * 本代码使用gc控制的原因是交给lua控制的只是一个对象指针，对象内存仍由c层管理。这样
 * 在lua层中的指针被销毁，对象仍能在c层继续使用。又或者push的对象不是new出来的。
 *
 * light userdata是有metatable的，但是全局共用一个。所以，如果需要把light userdata
 * 传到lua层，是没法确认其类型的。执行强转时容易coredump。需要设计一个全局metatable，
 * 以light userdata为key,v为类型来确定类型。
 */

/* 注册struct、class到lua，适用于在lua创建c、c++对象 */

#include <lua.hpp>
#include "../system/static_global.h"

template <class T> class LBaseClass
{
protected:
    /* 提供两种不同的注册函数,其返回值均为返回lua层的值数量 */
    typedef int32_t (T::*lua_CppFunction)(lua_State *);
    // typedef int32 (*pf_st_t)(lua_State*); //lua_CFunction
public:
    virtual ~LBaseClass() {}

    /* 在构造函数中不要调用virtual函数取c_new_func，只能当参数传进进来了 */
    explicit LBaseClass(lua_State *L, const char *classname,
                        lua_CFunction c_new_func = NULL)
        : L(L)
    {
        _class_name = classname;
        /* lua 5.3的get函数基本返回类型，而5.1基本为void。需要另外调用is函数 */

        if (0 == luaL_newmetatable(L, _class_name))
        {
            FATAL("dumplicate define class %s\n", _class_name);
            return;
        }

        lua_pushcfunction(L, gc);
        lua_setfield(L, -2, "__gc");

        lua_pushcfunction(L, tostring);
        lua_setfield(L, -2, "__tostring");

        /* metatable as value and pop metatable */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_newtable(L);
        lua_pushcfunction(L, c_new_func);
        lua_setfield(L, -2, "__call");
        lua_setmetatable(L, -2);

        lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
        if (!lua_istable(L, -1))
        {
            FATAL("define class before lua openlibs");
            return;
        }

        lua_pushvalue(L, 1);
        lua_setfield(L, -2, _class_name);

        lua_settop(L, 0);
    }

    /* 将c对象push栈,gc表示lua销毁userdata时，在gc函数中是否将当前指针delete
     * 由于此函数为static，但却依赖classname，而classname在构造函数中传入。
     * 因此，当调用类似lclass<lsocket>::push( L,_backend,false );的代码时，
     * 请保证你之前已注册对应的类，否则metatable将为nil
     */
    static int push(lua_State *L, const T *obj, bool gc = false)
    {
        ASSERT(obj, "push null obj");
        ASSERT(_class_name, "class not regist yet");

        /* 这里只是创建一个指针给lua管理，可以换用placement new把整个对象的
           内存都给lua管理
        */
        const T **ptr = (const T **)lua_newuserdata(L, sizeof(T *));
        *ptr          = obj;

        C_LUA_OBJECT_ADD(_class_name);

        luaL_getmetatable(L, _class_name);
        /* metatable on stack now,can not be nil */

        /* 如果不自动gc，则需要在metatable中设置一张名为_notgc的表。以userdata
           为key的weaktable。当lua层调用gc时,userdata本身还存在，故这时判断是准确的
        */
        if (!gc)
        {
            subtable(L, 2, "_notgc", "k");

            lua_pushvalue(L, 1);   /* 复制userdata到栈顶 */
            lua_pushboolean(L, 0); /* do not delete memory */
            lua_settable(L, -3); /* _notgc[userdata] = 1 and pop _notgc table*/

            lua_pop(L, 1); /* drop _notgc out of stack */
        }

        return lua_setmetatable(L, -2);
    }

    /* 注册函数,const char* func_name 就是注册到lua中的函数名字 */
    template <lua_CppFunction pf> LBaseClass<T> &def(const char *func_name)
    {
        luaL_getmetatable(L, _class_name);

        lua_getfield(L, -1, func_name);
        if (!lua_isnil(L, -1))
        {
            ERROR("dumplicate def function %s:%s", _class_name, func_name);
        }
        lua_pop(L, 1); /* drop field */

        lua_pushcfunction(L, &fun_thunk<pf>);
        lua_setfield(L, -2, func_name);

        lua_pop(L, 1); /* drop class metatable */

        return *this;
    }

    /* 用于定义类的static函数 */
    template <lua_CFunction pf> LBaseClass<T> &def(const char *func_name)
    {
        luaL_getmetatable(L, _class_name);

        lua_getfield(L, -1, func_name);
        if (!lua_isnil(L, -1))
        {
            ERROR("dumplicate def function %s:%s", _class_name, func_name);
        }
        lua_pop(L, 1); /* drop field */

        lua_pushcfunction(L, pf);
        lua_setfield(L, -2, func_name);

        lua_pop(L, 1); /* drop class metatable */

        return *this;
    }

    /* 注册变量,通常用于设置宏定义、枚举 */
    LBaseClass<T> &set(const char *val_name, int32_t val)
    {
        luaL_getmetatable(L, _class_name);

        lua_getfield(L, -1, val_name);
        if (!lua_isnil(L, -1))
        {
            ERROR("dumplicate set variable %s:%s", _class_name, val_name);
        }
        lua_pop(L, 1); /* drop field */

        lua_pushinteger(L, val);
        lua_setfield(L, -2, val_name);

        lua_pop(L, 1); /* drop class metatable */

        return *this;
    }

private:
    /* 单例，不能创建c对象。可以直接在C中push对象到lua */
    static int32_t cnew(lua_State *L)
    {
        ASSERT(false, "base class,cant NOT cteate object");
        return 0;
    }

    /* 元方法,__tostring */
    static int tostring(lua_State *L)
    {
        T **ptr = (T **)luaL_checkudata(L, 1, _class_name);
        if (ptr != NULL)
        {
            lua_pushfstring(L, "%s: %p", _class_name, *ptr);
            return 1;
        }
        return 0;
    }

    /* gc函数 */
    static int gc(lua_State *L)
    {
        C_LUA_OBJECT_DEC(_class_name);

        if (luaL_getmetafield(L, 1, "_notgc"))
        {
            /* 以userdata为key取值。如果未设置该userdata的_notgc值，则将会取得nil */
            lua_pushvalue(L, 1);
            lua_gettable(L, -2);
            /* gc = true表示执行gc函数 */
            if (!lua_toboolean(L, -1)) return 0;
        }

        T **ptr = (T **)luaL_checkudata(L, 1, _class_name);
        if (*ptr != NULL) delete *ptr;
        *ptr = NULL;

        return 0;
    }

    //创建弱表
    static void weaktable(lua_State *L, const char *mode)
    {
        lua_newtable(L);
        lua_pushvalue(L, -1); // table is its own metatable
        lua_setmetatable(L, -2);
        lua_pushliteral(L, "__mode");
        lua_pushstring(L, mode);
        lua_settable(L, -3); // metatable.__mode = mode
    }

    //创建子弱表
    static void subtable(lua_State *L, int tindex, const char *name,
                         const char *mode)
    {
        lua_pushstring(L, name);
        lua_gettable(L, tindex); /* 判断是否已存在t[name] */

        if (lua_isnil(L, -1)) /* 不存在，则创建 */
        {
            lua_pop(L, 1); /* drop nil */
            weaktable(L, mode);
            lua_pushstring(L, name);
            lua_pushvalue(L, -2);
            lua_settable(L, tindex); /* set t[name] */
        }
    }

    template <lua_CppFunction pf> static int fun_thunk(lua_State *L)
    {
        T **ptr = (T **)luaL_checkudata(
            L, 1, _class_name); /* get 'self', or if you prefer, 'this' */
        if (EXPECT_FALSE(ptr == NULL || *ptr == NULL))
        {
            return luaL_error(L, "%s calling method with null pointer",
                              _class_name);
        }

        /* remove self so member function args start at index 1 */
        lua_remove(L, 1);

        return ((*ptr)->*pf)(L);
    }

protected:
    lua_State *L;
    static const char *_class_name;
};

template <class T> const char *LBaseClass<T>::_class_name = NULL;

template <class T> class LClass : public LBaseClass<T>
{
public:
    LClass(lua_State *L, const char *classname)
        : LBaseClass<T>(L, classname, cnew)
    {
    }

private:
    using LBaseClass<T>::_class_name;

    /* 创建c对象 */
    static int cnew(lua_State *L)
    {
        /* 优先计数，在构造函数调用luaL_error执行longjump导致内存泄漏
         * 这里至少能统计到
         */
        C_LUA_OBJECT_ADD(_class_name);

        /* lua调用__call,第一个参数是该元表所属的table.取构造函数参数要注意 */
        T *obj = new T(L);

        lua_settop(L, 1); /* 清除所有构造函数参数,只保留元表 */

        T **ptr = (T **)lua_newuserdata(L, sizeof(T *));
        *ptr    = obj;

        /* 把新创建的userdata和元表交换堆栈位置 */
        lua_insert(L, 1);

        /* 弹出元表,并把元表设置为userdata的元表 */
        lua_setmetatable(L, -2);

        return 1;
    }
};
