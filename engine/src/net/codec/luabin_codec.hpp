#pragma once

#include "global/global.hpp"

/**
 * @brief 以二进制方式编码、解码Lua变量，目前仅用于RPC调用
 */
class LuaBinCodec final
{
public:
    LuaBinCodec();
    ~LuaBinCodec();

    /**
     * 解码数据包
     * @return <0 error,otherwise the number of parameter push to stack
     */
    int32_t decode(lua_State *L, const char *buffer, size_t len);
    /**
     * 编码数据包
     * @return <0 error,otherwise the length of buffer
     */
    int32_t encode(lua_State *L, int32_t index, const char **buffer);

private:
    //< 把基础类型写入缓冲区，不支持指针及自定义结构
    template <typename T> LuaBinCodec &operator<<(const T &v)
    {
        *(reinterpret_cast<T *>(_encode_buff + _buff_len)) = v;
        _buff_len += sizeof(T);
        return *this;
    }
    void append(const void *buff, const size_t len)
    {
        memcpy(_encode_buff + _buff_len, buff, len);
        _buff_len += len;
    }
    const char *subtract(const size_t len)
    {
        size_t pos = _buff_pos;
        _buff_pos += len;

        return _decode_buff + pos;
    }

    template <typename T> LuaBinCodec &operator>>(T &v)
    {
        v = *(reinterpret_cast<const T *>(_decode_buff + _buff_pos));
        _buff_pos += sizeof(T);
        return *this;
    }

    //< 把lua变量编码到缓冲区中
    int32_t encode_value(lua_State *L, int32_t index);
    //< 把lua table编码到缓冲区中
    int32_t encode_table(lua_State *L, int32_t index);

    //< 从缓冲区解码数据到lua中
    int32_t decode_value(lua_State *L);
    //< 从缓冲区解码table到lua中
    int32_t decode_table(lua_State *L);

private:
    size_t _buff_len;   // 已使用的缓冲区长度
    size_t _buff_pos;   // 已读取的缓冲区长度，仅decode时用到
    char *_encode_buff; // 用于序列化的缓冲区，避免内存分配
    const char *_decode_buff; // 反序列化缓冲区
};
