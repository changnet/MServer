-- 业务协议头的编解码
--
-- 传输层（Packet）只负责帧格式，帧里的业务字段（s2s的src/dst/mtype/usize、
-- c2s的协议号）由这里统一编解码。
--
-- 头格式属于业务，所以定义在lua这张表里，C++只提供通用编解码器(engine.NetHeader)。
-- 改协议头只要改DEFS，不需要重编译。
--
-- 每个lua_State(每个worker)各自require一次，实例天然隔离，不要跨线程共用。
--
-- ---------------------------------------------------------------------------
-- 描述符语法：string.pack 的定长整数子集（完整说明见 net/header/net_header.hpp）
--
--   '<' | '>'     字节序，默认小端（'<'可省）
--   b / B         1字节  有符号 / 无符号
--   h / H         2字节  有符号 / 无符号
--   i / I         4字节  有符号 / 无符号
--   q / Q         8字节  有符号 / 无符号
--
-- 小写有符号，大写无符号。数字后缀规则同 string.pack：
--   i / I   后缀就是宽度（1~8），省略时为4，所以 i == i4，i8 就是8字节
--   b h q   固有宽度固定，后缀必须与之一致（H2 合法，H4 会报错）
-- 空格可作分隔符。不支持 l L j J T n s c z x X f d。
--
-- 值是二进制补码；encode 的参数是 Lua integer(64位)，所以 8 字节无符号字段
-- 在 Lua 侧表现为有符号（-1 即 0xFFFF...FF），需要 >2^63 的值不要用。
-- debug 构建下 encode 会做范围校验，超范围直接报错（release 不校验，按低位截断）。
-- 线上字节等价于 string.pack("<" .. fmt, ...)，没有对齐 padding。
-- ---------------------------------------------------------------------------

local EngineNetHeader = require "engine.NetHeader"

-- 名字 -> 描述符
local DEFS = {
    none = "",
    ss = "i4i4i4i4", -- s2s 头：4 个 4 字节有符号小端整数，共 16 字节，src, dst, mtype, usize
    ws = "H2" -- c2s 头：1 个 2 字节无符号小端整数，共 2 字节，表示协议号
}

local hdr = EngineNetHeader()
local ids = {}

for name, fmt in pairs(DEFS) do
    ids[name] = hdr:define(fmt)
end

local NetHeader = {}

-- 某个头的字节数
function NetHeader.size(name)
    return hdr:size(ids[name])
end

-- 编码业务头
-- @param ... 字段值，个数必须与描述符一致，否则报错
-- @return lightuserdata指针, 长度
--         指针写在本实例的scratch上，**下一次encode之前有效**，与pbc的契约一致
function NetHeader.encode(name, ...)
    return hdr:encode(ids[name], ...)
end

-- 解码业务头
-- 与encode对称：只收 lightuserdata，不收 Lua string（保证接收侧零拷贝）
-- @param ud 数据指针(lightuserdata)，必须是接收缓冲区里的地址
-- @param len 数据长度
-- @return 字段1..字段n, 载荷指针(lightuserdata), 载荷长度
--         载荷长度为0时指针给nil（不是指向帧尾的指针）
--         数据不足一个头时整体返回nil，由调用方关连接
function NetHeader.decode(name, ud, len)
    return hdr:decode(ids[name], ud, len)
end

return NetHeader
