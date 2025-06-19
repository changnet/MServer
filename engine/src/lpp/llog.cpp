#include <lua.hpp>

#include "llog.hpp"
#include "ltools.hpp"
#include "system/static_global.hpp"
#include "ev/time.hpp"

struct ThreadBuffer
{
    ThreadBuffer()
    {
        used_   = 0;
        buffer_ = new char[10240];
        size_   = sizeof(buffer_);
    }

    ~ThreadBuffer()
    {
        delete[] buffer_;
    }

    void reserve(size_t need_size)
    {
        size_            = std::max(size_ * 2, need_size);
        char *new_buffer = new char[size_];
        memcpy(new_buffer, buffer_, used_);

        delete[] buffer_;
        buffer_ = new_buffer;
    }

    bool append_string(const char *str, size_t size)
    {
        // 单次日志打印的最大长度
        static const size_t MAX_BUFFER = 8 * 1024 * 1024;

        size_t need_size = used_ + size;
        if (unlikely(need_size >= MAX_BUFFER)) return false;

        if (unlikely(need_size > size_)) reserve(need_size);

        memcpy(buffer_ + used_, str, size);

        used_ = need_size;
        return true;
    }

    size_t used_;
    size_t size_;
    char *buffer_;
};

ThreadBuffer &get_thread_buffer()
{
    // print用的线程安全缓冲区。Thread-Local-Storage也是有大小的
    // 直接用thread_local char[102400]会占用太多，这只用struct包一层

    // windows下，程序启动时，会创建很多额外的ntdll等线程
    // 如果这个thread_local作用域放到整个cpp文件，则每个线程都会创建这个thread_local
    // 但是一些线程并不会释放这个thread_local
    thread_local ThreadBuffer buffer;

    return buffer;
}

LLog::LLog(const char *name) : AsyncLog(name ? name : "unknow")
{
}

LLog::~LLog()
{
}

void LLog::stop()
{
    if (stop_)
    {
        ELOG("log thread already stop");
        return;
    }

    AsyncLog::stop(true);
}

int32_t LLog::start(lua_State *L)
{
    if (!stop_)
    {
        luaL_error(L, "log thread already start");
        return 0;
    }

    /* 设定多久写入一次文件 */
    int32_t ms = luaL_optinteger32(L, 2, 10000);

    AsyncLog::start(ms);

    return 0;
}

int32_t LLog::append(lua_State *L)
{
    if (stop_)
    {
        return luaL_error(L, "log thread not start");
    }

    size_t len       = 0;
    const char *name = luaL_checkstring(L, 2);
    int32_t mask     = luaL_checkinteger32(L, 3);
    const char *str  = luaL_checklstring(L, 4, &len);
    int64_t time     = luaL_optinteger(L, 5, -1);
    if (time < 0) time = timing::try_frame_time();

    AsyncLog::append(name, mask, time, str, len);

    return 0;
}

int32_t LLog::print(lua_State *L)
{
    // 把栈里所有的参数都按lua的print函数打印出来
    // 不要在lua用table.concat把多个参数拼起来，效率很低
    // 一些基础类型，尽量不用tostring，那样会在lua创建一个str再gc掉

    int32_t n    = lua_gettop(L);
    int32_t mask = luaL_checkinteger32(L, 2);

    // 针对print("xxx")只打印一个str的情况优化
    if (n == 3 && LUA_TSTRING == lua_type(L, 3))
    {
        size_t len      = 0;
        const char *str = lua_tolstring(L, 3, &len);
        AsyncLog::append("info", mask, timing::try_frame_time(), str, len);
        return 0;
    }

    ThreadBuffer &buffer = get_thread_buffer();
    buffer.used_ = 0;
    for (int32_t i = 3; i <= n; i++)
    {
        if (i > 3) buffer.append_string(" ", 1);

        bool ok;
        switch (lua_type(L, i))
        {
        case LUA_TNIL: ok = buffer.append_string("nil", 3); break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, i))
            {
                ok = buffer.append_string("true", 4);
            }
            else
            {
                ok = buffer.append_string("false", 5);
            }
            break;
        case LUA_TNUMBER:
        {
            static const size_t MAX_NUM_SIZE = 64;
            if (buffer.used_ + MAX_NUM_SIZE > buffer.size_)
            {
                buffer.reserve(buffer.used_ + MAX_NUM_SIZE);
            }
            int32_t num_size =
                lua_isinteger(L, i)
                    ? snprintf(buffer.buffer_ + buffer.used_, MAX_NUM_SIZE,
                               LUA_INTEGER_FMT, lua_tointeger(L, i))
                    : snprintf(buffer.buffer_ + buffer.used_, MAX_NUM_SIZE,
                               "%f", lua_tonumber(L, i));
            if (num_size <= 0 || num_size > (int32_t)MAX_NUM_SIZE)
            {
                ELOG("print ERROR %u", buffer.used_);
                return 0;
            }

            ok = true;
            buffer.used_ += num_size;
            break;
        }
        case LUA_TSTRING:
        {
            size_t len      = 0;
            const char *str = lua_tolstring(L, i, &len);
            ok              = buffer.append_string(str, len);
            break;
        }
        default:
        {
            size_t len      = 0;
            const char *str = luaL_tolstring(L, i, &len);
            ok              = buffer.append_string(str, len);
            // luaL_tolstring会在堆栈上添加一个元素，而lua_tolstring不会
            lua_pop(L, 1);
            break;
        }
        }

        if (!ok)
        {
            ELOG("print buffer overflow");
            break;
        }
    }

    if (buffer.used_ <= 0) return 0;

    // TODO 这里能不能优化下，直接使用logger那边的buff，省去一次memcpy
    // 使用flexible_pool获取一个buffer，直接往logger push
    AsyncLog::append("info", mask, timing::try_frame_time(), buffer.buffer_,
                     buffer.used_);

    return 0;
}

int32_t LLog::error(lua_State *L)
{
    size_t len      = 0;
    int32_t mask    = luaL_checkinteger32(L, 2);
    const char *str = luaL_checklstring(L, 3, &len);

    // 错误日志不多，不用考虑像print那样优化
    AsyncLog::append("error", mask, timing::try_frame_time(), str, len);

    return 0;
}

int32_t LLog::add_device(lua_State *L)
{
    const char *name  = luaL_checkstring(L, 2);
    const char *path  = luaL_checkstring(L, 3);
    int32_t alive     = luaL_checkinteger32(L, 4);
    int32_t policy    = luaL_checkinteger32(L, 5);
    int64_t policy_u1 = luaL_optinteger(L, 6, 0);
    const char *multi = luaL_optstring(L, 7, nullptr);

    AsyncLog::add_device(name, path, alive, policy, policy_u1, multi);
    return 0;
}

// 设置日志参数
int32_t LLog::set_name(lua_State *L)
{
    const char *name = luaL_checkstring(L, 2);

    set_thread_name(name);
    return 0;
}
