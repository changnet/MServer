#pragma once

#include "global/global.hpp"

/**
 * @brief 以二进制方式编码、解码Lua变量，目前仅用于RPC调用
 */
class LuaCodec final
{
public:
    LuaCodec();
    ~LuaCodec();

    /**
     * 解码数据包
     * @param buffer 待解码的char*
     * @param size 待解码的buff大小
     * @return <0 error,otherwise the number of parameter push to stack
     */
    int32_t decode(lua_State *L);
    /**
     * 编码数据包
     * @param ... 待编码的参数
     * @return buffer大小，buffer指针（该指针在下一次调用encode时有效）。出错时，buffer大小返回-1
     */
    int32_t encode(lua_State *L);

private:
    // 把基础类型写入缓冲区，不支持指针及自定义结构
    template <typename T> LuaCodec &operator<<(const T &v)
    {
        // *(reinterpret_cast<T *>(encode_buff_ + buff_len_)) = v; 对齐问题
        memcpy(encode_buff_ + buff_len_, &v, sizeof(T));
        buff_len_ += sizeof(T);
        return *this;
    }
    void append(const void *buff, const size_t len)
    {
        memcpy(encode_buff_ + buff_len_, buff, len);
        buff_len_ += len;
    }
    const char *subtract(const size_t len)
    {
        size_t pos = buff_pos_;
        buff_pos_ += len;

        return decode_buff_ + pos;
    }

    template <typename T> LuaCodec &operator>>(T &v)
    {
        v = *(reinterpret_cast<const T *>(decode_buff_ + buff_pos_));
        memcpy(&v, decode_buff_ + buff_pos_, sizeof(T));
        buff_pos_ += sizeof(T);
        return *this;
    }

    // 检测并扩展encode缓冲区
    void check_encode_buff(size_t size);
    // 把lua变量编码到缓冲区中
    int32_t encode_value(lua_State *L, int32_t index);
    // 把lua table编码到缓冲区中
    int32_t encode_table(lua_State *L, int32_t index);

    // 检测decode缓冲区是否有效
    void check_decode_buff(size_t size);
    // 从缓冲区解码数据到lua中
    int32_t decode_value(lua_State *L);
    // 从缓冲区解码table到lua中
    int32_t decode_table(lua_State *L);

private:
    size_t buff_len_;   // 已使用的缓冲区长度
    size_t buff_pos_;   // 已读取的缓冲区长度，仅decode时用到
    char *encode_buff_; // 用于序列化的缓冲区，避免内存分配
    size_t encode_buff_len_; // encode_buff_缓冲区的大小
    const char *decode_buff_; // 反序列化缓冲区
};
