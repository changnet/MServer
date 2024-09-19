#pragma once

#include <lua.hpp>
#include <string>
#include <cassert>

namespace lua
{
template <typename T> T lua_to_cpp(lua_State *L, int i)
{
    static_assert(std::is_pointer<T>::value, "type unknow");

    // 不是userdata这里会返回nullptr
    void *p = lua_touserdata(L, i);

    using T1 = typename std::remove_pointer_t<T>;
    if constexpr (std::is_void_v<T1>)
    {
        return p;
    }
    else
    {
        if (!p || lua_islightuserdata(L, i)) return (T)p;

        // 这里只能是full userdata了，如果定义过则是通过lclass push的指针
        const char *name = Class<T1>::template _class_name;
        if (luaL_testudata(L, i, name)) return *((T1 **)p);

        return nullptr;
    }
}

template <> inline bool lua_to_cpp<bool>(lua_State *L, int i)
{
    return lua_toboolean(L, i) != 0;
}

template <> inline char lua_to_cpp<char>(lua_State *L, int i)
{
    return (char)lua_tointeger(L, i);
}

template <> inline unsigned char lua_to_cpp<unsigned char>(lua_State *L, int i)
{
    return (unsigned char)lua_tointeger(L, i);
}

template <> inline short lua_to_cpp<short>(lua_State *L, int i)
{
    return (short)lua_tointeger(L, i);
}

template <>
inline unsigned short lua_to_cpp<unsigned short>(lua_State *L, int i)
{
    return (unsigned short)lua_tointeger(L, i);
}

template <> inline int lua_to_cpp<int>(lua_State *L, int i)
{
    return (int)lua_tointeger(L, i);
}

template <> inline unsigned int lua_to_cpp<unsigned int>(lua_State *L, int i)
{
    return (unsigned int)lua_tointeger(L, i);
}

template <> inline long lua_to_cpp<long>(lua_State *L, int i)
{
    return (long)lua_tointeger(L, i);
}

template <> inline unsigned long lua_to_cpp<unsigned long>(lua_State *L, int i)
{
    return (unsigned long)lua_tointeger(L, i);
}

template <> inline long long lua_to_cpp<long long>(lua_State *L, int i)
{
    return lua_tointeger(L, i);
}

template <>
inline unsigned long long lua_to_cpp<unsigned long long>(lua_State *L, int i)
{
    return (unsigned long long)lua_tointeger(L, i);
}

template <> inline float lua_to_cpp<float>(lua_State *L, int i)
{
    return (float)lua_tonumber(L, i);
}

template <> inline double lua_to_cpp<double>(lua_State *L, int i)
{
    return lua_tonumber(L, i);
}

template <> inline const char *lua_to_cpp<const char *>(lua_State *L, int i)
{
    // 这里要注意，无法转换为const char*会返回NULL
    // 这个类型对c++并不安全，比如 std::cout << NULL就会直接当掉，用时需要检测指针
    return lua_tostring(L, i);
}

template <> inline char *lua_to_cpp<char *>(lua_State *L, int i)
{
    // 这里要注意，无法转换为const char*会返回NULL
    // 这个类型对c++并不安全，比如 std::cout << NULL就会直接当掉，用时需要检测指针
    return const_cast<char *>(lua_tostring(L, i));
}

template <> inline std::string lua_to_cpp<std::string>(lua_State *L, int i)
{
    const char *str = lua_tostring(L, i);
    return str == nullptr ? "" : str;
}

template <typename T> void cpp_to_lua(lua_State *L, T v)
{
    static_assert(std::is_pointer<T>::value, "type unknow");

    // 如果声明过这个类，则以push方式推送到lua
    using T1 = typename std::remove_pointer_t<T>;
    if constexpr (std::is_void_v<T1>)
    {
        lua_pushlightuserdata(L, v);
    }
    else
    {
        const char *name = Class<T1>::template _class_name;
        if (name)
        {
            Class<T1>::push(L, v);
        }
        else
        {
            lua_pushlightuserdata(L, v);
        }
    }
}

inline void cpp_to_lua(lua_State *L, bool v)
{
    lua_pushboolean(L, v);
}

inline void cpp_to_lua(lua_State *L, char v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, unsigned char v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, short v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, unsigned short v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, int v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, unsigned int v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, long v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, unsigned long v)
{
    lua_pushinteger(L, v);
}

inline void cpp_to_lua(lua_State *L, long long v)
{
    lua_pushinteger(L, (lua_Integer)v);
}

inline void cpp_to_lua(lua_State *L, unsigned long long v)
{
    lua_pushinteger(L, (lua_Integer)v);
}

inline void cpp_to_lua(lua_State *L, float v)
{
    lua_pushnumber(L, v);
}

inline void cpp_to_lua(lua_State *L, double v)
{
    lua_pushnumber(L, v);
}

inline void cpp_to_lua(lua_State *L, const char *v)
{
    lua_pushstring(L, v);
}

inline void cpp_to_lua(lua_State *L, char *v)
{
    lua_pushstring(L, v);
}

inline void cpp_to_lua(lua_State *L, const std::string &v)
{
    lua_pushstring(L, v.c_str());
}

// C++ 20 std::remove_cvref
// TODO std::remove_volatile 需要吗？
template <typename T>
using remove_cvref = std::remove_cv<std::remove_reference_t<T>>::template type;

template <class T> class Register;
template <typename Ret, typename... Args> class Register<Ret (*)(Args...)>
{
private:
    static constexpr auto indices = std::make_index_sequence<sizeof...(Args)>{};

    template <size_t... I, typename = std::enable_if_t<!std::is_void<Ret>::value>>
    static int caller(lua_State *L, Ret (*fp)(Args...),
                      const std::index_sequence<I...> &)
    {
        cpp_to_lua(L, fp(lua_to_cpp<remove_cvref<Args>>(L, 1 + I)...));
        return 1;
    }
    template <size_t... I>
    static int caller(lua_State *L, void (*fp)(Args...),
                      const std::index_sequence<I...> &)
    {
        fp(lua_to_cpp<remove_cvref<Args>>(L, 1 + I)...);
        return 0;
    }

public:
    template <auto fp> static int reg(lua_State *L)
    {
        return caller(L, fp, indices);
    }
};

// 前置声明
template <typename T> struct class_remove;
// 特化为static函数或全局函数
template <typename Ret, typename... Args> struct class_remove<Ret (*)(Args...)>
{
    using type = Ret (*)(Args...);
};
// 特化为成员函数
template <typename T, typename Ret, typename... Args>
struct class_remove<Ret (T::*)(Args...)>
{
    using type = Ret (*)(Args...);
};
// 特化为const成员函数
template <typename T, typename Ret, typename... Args>
struct class_remove<Ret (T::*)(Args...) const>
    : public class_remove<Ret (T::*)(Args...)>
{
};

template <typename T>
inline constexpr bool is_lua_func =
    std::is_same<typename class_remove<T>::type, lua_CFunction>::value;

template <auto fp,
          typename = std::enable_if_t<!std::is_same_v<decltype(fp), lua_CFunction>>>
void reg_global_func(lua_State *L, const char *name)
{
    lua_register(L, name, Register<decltype(fp)>::template reg<fp>);
}

template <lua_CFunction fp> void reg_global_func(lua_State *L, const char *name)
{
    lua_register(L, name, fp);
}

template <class T> class Class
{
private:
    template <class C> class StaticRegister;
    template <typename Ret, typename... Args>
    class StaticRegister<Ret (*)(Args...)>
    {
    private:
        static constexpr auto indices =
            std::make_index_sequence<sizeof...(Args)>{};

        template <auto fp, size_t... I>
        static int caller(lua_State *L, const std::index_sequence<I...> &)
        {
            if constexpr (std::is_void_v<Ret>)
            {
                fp(lua_to_cpp<remove_cvref<Args>>(L, 2 + I)...);
                return 0;
            }
            else
            {
                cpp_to_lua(L, fp(lua_to_cpp<remove_cvref<Args>>(L, 2 + I)...));
                return 1;
            }
        }

    public:
        template <auto fp> static int reg(lua_State *L)
        {
            return caller<fp>(L, indices);
        }
    };

    template <typename C> class ClassRegister;
    template <typename C, typename Ret, typename... Args>
    class ClassRegister<Ret (C::*)(Args...)>
    {
    private:
        static constexpr auto indices =
            std::make_index_sequence<sizeof...(Args)>{};

        template <auto fp, size_t... I>
        static int caller(lua_State *L, const std::index_sequence<I...> &)
        {
            T **ptr = (T **)luaL_checkudata(L, 1, _class_name);
            if (ptr == nullptr || *ptr == nullptr)
            {
                return luaL_error(L, "%s calling method with null pointer",
                                  _class_name);
            }

            // 使用if constexpr替换多个模板好维护一些
            // template <size_t... I, typename = std::enable_if_t<!std::is_void<Ret>::value>>

            if constexpr (std::is_void_v<Ret>)
            {
                ((*ptr)->*fp)(lua_to_cpp<remove_cvref<Args>>(L, 2 + I)...);
                return 0;
            }
            else
            {
                cpp_to_lua(L, ((*ptr)->*fp)(
                                  lua_to_cpp<remove_cvref<Args>>(L, 2 + I)...));
                return 1;
            }
        }

    public:
        template <auto fp> static int reg(lua_State *L)
        {
            return caller<fp>(L, indices);
        }
    };

    template <typename C, typename Ret, typename... Args>
    class ClassRegister<Ret (C::*)(Args...) const>
        : public ClassRegister<Ret (C::*)(Args...)>
    {
    };

public:
    virtual ~Class()
    {
    }

    // 创建一个类的对象，但不向lua注册。仅用于注册后使用同样的对象并且虚拟机L应该和注册时一致
    explicit Class(lua_State *L) : L(L)
    {
        // 注册过后，必定存在类名
        // assert（_class_name);
    }

    // 注册一个类
    // @param L lua虚拟机指针
    explicit Class(lua_State *L, const char *classname) : L(L)
    {
        _class_name = classname;

        lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
        assert(lua_istable(L, -1));

        // 该类名已经注册过
        if (0 == luaL_newmetatable(L, _class_name))
        {
            assert(false);
            return;
        }

        /*
        使用Lua创建一个类，需要用到Lua元表中的__index机制。__index是元表的一个key，value
        为一个table，里面即这个类的各个函数。这说明，当访问一个类的函数时，需要判断获取metatable
        ，再获取__index这个table，最终才取得对应的函数去调用。

        这个机制的性能消耗其实是比较大的

        local tbl = {
            v1, v2, -- tbl对象的数据放tbl里

            -- 元表负责函数
            metatable = {
                __gc = xx,
                __tostring = xx,
                __index = {
                    func1 = xx,
                    func2 = xx,
                },
                metatable = {
                    __call = xx,
                }
            }
        }
         */

        lua_pushcfunction(L, gc);
        lua_setfield(L, -2, "__gc");

        lua_pushcfunction(L, tostring);
        lua_setfield(L, -2, "__tostring");

        lua_pushcfunction(L, toludata);
        lua_setfield(L, -2, "toludata");

        // 创建一个table作为metatable的元表，这样 metatable() 就能创建一个对象
        // 保持写法和C++一样
        // if constexpr 是编译时生成，所以没有默认构造函数（不包含任何参数）的类，是不会注册__call的
        if constexpr (std::is_constructible_v<T>)
        {
            lua_newtable(L);
            lua_pushcfunction(L, class_constructor<>);
            lua_setfield(L, -2, "__call");
            lua_setmetatable(L, -2);
        }

        /*
        __index还需要创建一个table来保存函数，但为了节省内存，让
        metatable.__index = metatable，
        这样func1和func2和__gc等函数放在一起，不需要额外创建一个table。

        但请注意，如果不是为了覆盖__gc、__tostring等内置函数，请不要用这种名字。这是一个feature也是一个坑
         */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        // 设置loaded，这样在Lua中可以像普通模块那样require "xxx"
        lua_setfield(L, -2, _class_name);

        lua_pop(L, 1); // drop _loaded table
    }

    // 指定构造函数的参数
    template <typename... Args> void constructor()
    {

        luaL_getmetatable(L, _class_name);
        assert(lua_istable(L, -1));

        // lua_getmetatable获取不到metatable的话，并不会往堆栈push一个nil
        if (!lua_getmetatable(L, -1))
        {
            lua_newtable(L);
        }
        lua_pushcfunction(L, class_constructor<Args...>);
        lua_setfield(L, -2, "__call");
        lua_setmetatable(L, -2);

        lua_pop(L, 1); /* drop class metatable */
    }

    /* 将c对象push栈,gc表示lua销毁userdata时，在gc函数中是否将当前指针delete
     * 由于此函数为static，但却依赖classname，而classname在构造函数中传入。
     * 因此，当调用类似lclass<lsocket>::push( L,_backend,false );的代码时，
     * 请保证你之前已注册对应的类，否则metatable将为nil
     */
    static int push(lua_State *L, const T *obj, bool gc = false)
    {
        assert(obj);
        assert(_class_name);

        /* 这里只是创建一个指针给lua管理
         */
        const T **ptr = (const T **)lua_newuserdata(L, sizeof(T *));
        *ptr          = obj;

        // 只有用lcalss定义了对应类的对象能push到lua，因此这里的metatable必须存在
        luaL_getmetatable(L, _class_name);
        if (!lua_istable(L, -1))
        {
            return -1;
        }

        /* 如果不自动gc，则需要在metatable中设置一张名为_notgc的表。以userdata
         * 为key的weaktable。当lua层调用gc时,userdata本身还存在，故这时判断是准确的
         */
        if (!gc)
        {
            subtable(L, 2, "_notgc", "k");

            lua_pushvalue(L, 1); /* 复制userdata到栈顶 */
            lua_pushboolean(L, 1);
            lua_settable(L, -3); /* _notgc[userdata] = true */

            lua_pop(L, 1); /* drop _notgc out of stack */
        }

        lua_setmetatable(L, -2);
        return 0;
    }

    // 把一个对象指针push到全局变量
    static int push_global(lua_State *L, const T *obj, const char *name,
                           bool gc = false)
    {
        if (0 != push(L, obj, gc)) return -1;

        lua_setglobal(L, name);
        return 0;
    }

    template <auto fp> void def(const char *name)
    {
        lua_CFunction cfp = nullptr;
        if constexpr (std::is_same_v<decltype(fp), lua_CFunction>)
        {
            cfp = fp;
        }
        else if constexpr (!std::is_member_function_pointer_v<decltype(fp)>)
        {
            cfp = StaticRegister<decltype(fp)>::template reg<fp>;
        }
        else if constexpr (is_lua_func<decltype(fp)>)
        {
            cfp = &fun_thunk<fp>;
        }
        else
        {
            cfp = ClassRegister<decltype(fp)>::template reg<fp>;
        }

        luaL_getmetatable(L, _class_name);

        lua_pushcfunction(L, cfp);
        lua_setfield(L, -2, name);

        lua_pop(L, 1); /* drop class metatable */
    }

    /* 注册变量,通常用于设置宏定义、枚举 */
    void set(int32_t val, const char *val_name)
    {
        luaL_getmetatable(L, _class_name);

        lua_pushinteger(L, val);
        lua_setfield(L, -2, val_name);

        lua_pop(L, 1); /* drop class metatable */
    }

private:
    template <typename... Args, size_t... I>
    static T *class_constructor_caller(lua_State *L,
                                       const std::index_sequence<I...> &)
    {
        return new T(lua_to_cpp<Args>(L, 2 + I)...);
    }

    template <typename... Args> static int class_constructor(lua_State *L)
    {
        T *obj = class_constructor_caller<Args...>(
            L, std::make_index_sequence<sizeof...(Args)>{});

        // lua调用__call,第一个参数是元表
        // 清除所有构造函数参数,只保留元表(TODO: 是否要清除)
        lua_settop(L, 1); /*  */

        T **ptr = (T **)lua_newuserdata(L, sizeof(T *));
        *ptr    = obj;

        /* 把新创建的userdata和元表交换堆栈位置 */
        lua_insert(L, 1);

        /* 把元表设置为userdata的元表，并弹出元表 */
        lua_setmetatable(L, -2);

        return 1;
    }

    // 把一个对象转换为一个light userdata
    static int toludata(lua_State *L)
    {
        T **ptr = (T **)luaL_checkudata(L, 1, _class_name);

        lua_pushlightuserdata(L, *ptr);
        return 1;
    }

    /* 元方法,__tostring */
    static int tostring(lua_State *L)
    {
        T **ptr = (T **)luaL_checkudata(L, 1, _class_name);
        if (ptr != nullptr)
        {
            lua_pushfstring(L, "%s: %p", _class_name, *ptr);
            return 1;
        }
        return 0;
    }

    /* gc函数 */
    static int gc(lua_State *L)
    {
        if (luaL_getmetafield(L, 1, "_notgc"))
        {
            /* 以userdata为key取值。如果未设置该userdata的_notgc值，则将会取得nil */
            lua_pushvalue(L, 1);
            lua_gettable(L, -2);
            /* gc = true表示执行gc函数 */
            if (lua_toboolean(L, -1)) return 0;
        }

        T **ptr = (T **)luaL_checkudata(L, 1, _class_name);
        if (*ptr != nullptr) delete *ptr;
        *ptr = nullptr;

        return 0;
    }

    // 创建弱表
    static void weaktable(lua_State *L, const char *mode)
    {
        lua_newtable(L);
        lua_pushvalue(L, -1); // table is its own metatable
        lua_setmetatable(L, -2);
        lua_pushliteral(L, "__mode");
        lua_pushstring(L, mode);
        lua_settable(L, -3); // metatable.__mode = mode
    }

    // 创建子弱表
    static void subtable(lua_State *L, int index, const char *name,
                         const char *mode)
    {
        lua_pushstring(L, name);
        lua_rawget(L, index); /* 判断是否已存在t[name] */

        if (lua_isnil(L, -1)) /* 不存在，则创建 */
        {
            lua_pop(L, 1); /* drop nil */
            weaktable(L, mode);
            lua_pushstring(L, name);
            lua_pushvalue(L, -2);
            lua_rawset(L, index); /* set t[name] */
        }
    }

    template <auto pf> static int fun_thunk(lua_State *L)
    {
        T **ptr = (T **)luaL_checkudata(L, 1, _class_name);
        if (ptr == nullptr || *ptr == nullptr)
        {
            return luaL_error(L, "%s calling method with null pointer",
                              _class_name);
        }

        return ((*ptr)->*pf)(L);
    }

public:
    static const char *_class_name;

private:
    lua_State *L;
};

class StackChecker
{
public:
    StackChecker(lua_State* L)
    {
        _L   = L;
        _top = lua_gettop(L);
        assert(0 == _top);
    }
    ~StackChecker()
    {
        assert(_top == lua_gettop(_L));
    }

private:
    int _top;
    lua_State *_L;
};

} // namespace lua
template <class T> const char *lua::Class<T>::_class_name = nullptr;
