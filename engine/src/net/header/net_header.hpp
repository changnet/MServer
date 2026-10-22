#pragma once

#include "lua.hpp"
#include "global/global.hpp"

/*
 * 业务协议头编解码器
 *
 * =====================================================================
 * 一、编码规则
 * =====================================================================
 *
 * 用描述符字符串声明头的布局，语法参考 Lua string.pack：
 *
 *   '<' | '>'     字节序，默认小端（'<' 可省略，'>' 大端，当前无人使用）
 *   b / B         1 字节   有符号 / 无符号
 *   h / H         2 字节   有符号 / 无符号
 *   i / I         4 字节   有符号 / 无符号
 *   q / Q         8 字节   有符号 / 无符号
 *
 * 小写有符号，大写无符号。数字后缀决定宽度：
 *   i / I  后缀就是宽度，取值 1~8，省略时为 4（i == i4，i8 就是 8 字节）
 *   b h q 等固有宽度固定的，后缀必须等于固有宽度（H2 合法，H4 报错）
 * 空格 / tab 可作分隔符，纯可读性。
 *
 * 例子：
 *
 *   "i4i4i4i4"  4 个 4 字节有符号小端整数，共 16 字节
 *   "H2"        1 个 2 字节无符号小端整数，共 2 字节
 *   ""          空头，共 0 字节（定义出来占个 id，便于代码里统一走 encode）
 *
 * =====================================================================
 * 二、与 Lua 的接口
 * =====================================================================
 *
 *   - encode 写进实例内的 scratch_，返回 lightuserdata + 长度；
 *     生命周期在下一次 encode 之前有效
 *
 * =====================================================================
 * 三、实现要点
 * =====================================================================
 *
 *   - define 时把描述符预编译成 offset/width 表，encode 只做写字节
 *   - 一律走显式移位读写，不 memcpy 结构体：线上字节与机器端序、对齐无关
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

    // nh:define("i4i4i4i4") -> id；描述符语法见文件头注释
    int32_t define(lua_State *L);
    // nh:size(id) -> 头的字节数
    int32_t size(lua_State *L) const;
    // nh:encode(id, v1..vn) -> lightuserdata指针, 长度；指针下一次encode前有效
    int32_t encode(lua_State *L);
    // nh:decode(id, ud, len) -> v1..vn, 载荷指针(lightuserdata|nil), 载荷长度
    int32_t decode(lua_State *L);
    // nh:format(id) -> 描述符的规范形式（调试用，见实现）
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
