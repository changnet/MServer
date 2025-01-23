#pragma once

#include "global/global.hpp"
#include <lua.hpp>

// lua对C++ buffer的操作
class LuaBuffer
{
    // TODO 后续可以把这个类扩展成直接操作C++的buffer，以提高lua对string以及原始内存的操作能力
    // 但是现在用到啥接口就先写啥接口
public:
    LuaBuffer()
    {
        size_   = 0;
        buffer_ = nullptr;
    }
    ~LuaBuffer()
    {
        if (buffer_) delete[] buffer_;
    }

    void *set(void *buffer, size_t size)
    {
        if (!buffer || 0 == size) return nullptr;

        if (0 == size_)
        {
            buffer_ = new char[size];
        }
        else if (size_ < size)
        {
            delete[] buffer_;
            buffer_ = new char[size];
        }

        size_ = size;
        memcpy(buffer_, buffer, size_);

        return buffer_;
    }

    int32_t get(lua_State *L)
    {
        if (!buffer_) return 0;

        lua_pushlightuserdata(L, buffer_);
        lua_pushinteger(L, size_);

        return 2;
    }

    /**
     * 从buffer中读取一个整形（int8, int32，int64等等）
     * @param  buffer C++的buffer指针
     * @param size 读取的字节大小(int32=4字节，int64=8字节)
     * @param offset 指针的偏移量（可选）
     * @return 读取到的数值，读取后的buffer指针
     */
    static int32_t read_int(lua_State *L)
    {
        const char *buffer = (const char *)lua_touserdata(L, 1);
        if (!buffer) return luaL_error(L, "invalid buffer");

        size_t size   = luaL_checkinteger(L, 2);
        size_t offset = luaL_optinteger(L, 3, 0);

        if (size < 1 || size > sizeof(int64_t))
        {
            return luaL_error(L, "invalid read size");
        }

        int64_t i = 0;
        memcpy(&i, buffer + offset, size);

        lua_pushinteger(L, i);
        lua_pushlightuserdata(L, (void *)(buffer + offset + size));
        return 2;
    }

private:
    size_t size_;
    char *buffer_;
};
