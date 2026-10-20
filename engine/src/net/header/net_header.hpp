#pragma once

#include "lua.hpp"
#include "global/global.hpp"

/*
 * 业务协议头编解码器
 *
 * 传输层（Packet）只管帧格式，协议里的业务字段（如 s2s 的 src/dst/type/usize、
 * c2s 的协议号）统一由这里编解码。
 *
 * 设计要点：
 *   - 头格式用描述符字符串定义，不在 C++ 里写死。改协议头只改 lua 里的一张表
 *   - define 时把 "i4i4i4i4" 预编译成 offset/width 表，encode 只做写字节
 *   - encode 写进实例内的 scratch_，返回 lightuserdata；契约与
 *     PbcCodec::encode_to_buffer 一致：**下一次 encode 之前有效**
 *   - 不持有 lua_State，不注册 GC 对象；一个 lua_State 一个实例（每 worker 各一份，
 *     天然线程隔离，不要跨线程共用）
 *
 * 描述符语法（定长字段，够用即止）：
 *   '<' | '>'    字节序，默认小端（现有协议头全部是小端）
 *   [bB] 1B  [hH] 2B  [iI] 4B  [qQ] 8B
 *   小写有符号、大写无符号；数字后缀（i4、H2）只是可读性，会被忽略
 *
 * 线上字节变化会影响两端互通，编解码一律走显式移位写字节，不 memcpy 结构体，
 * 保证与端序、对齐无关。
 */
class NetHeader final
{
public:
    static constexpr int32_t MAX_DESC  = 16; // 最多定义多少种头
    static constexpr int32_t MAX_FIELD = 8;  // 单个头最多几个字段
    static constexpr int32_t MAX_HEAD  = 64; // 单个头最大字节数

public:
    NetHeader();
    ~NetHeader() = default;

    // nh:define("i4i4i4i4") -> id，id 是本实例内的下标
    int32_t define(lua_State *L);
    // nh:size(id) -> 头的字节数
    int32_t size(lua_State *L) const;
    // nh:encode(id, v1..vn) -> ptr(lightuserdata), len
    int32_t encode(lua_State *L);
    // nh:decode(id, data[, len]) -> v1..vn, body_ptr, body_len；长度不足返回 nil
    int32_t decode(lua_State *L);
    // nh:format(id) -> 描述符字符串（调试用）
    int32_t format(lua_State *L) const;

private:
    struct Field
    {
        uint8_t offset;
        uint8_t width;
        bool is_signed;
    };

    struct Desc
    {
        Field fields[MAX_FIELD];
        uint8_t count;
        uint8_t total;
        bool big_endian;
    };

    const Desc *get_desc(lua_State *L, int32_t &id) const;

private:
    Desc descs_[MAX_DESC];
    int32_t desc_count_;

    // 头最长 64 字节，现有最大的是 ss 的 16 字节
    alignas(8) char scratch_[MAX_HEAD];
};
