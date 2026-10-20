-- 业务协议头的编解码
--
-- 传输层（Packet）只负责帧格式，帧里的业务字段（s2s的src/dst/mtype/usize、
-- c2s的协议号）由这里统一编解码。
--
-- 头格式属于业务，所以定义在lua这张表里，C++只提供通用编解码器(engine.NetHeader)。
-- 改协议头只要改DEFS，不需要重编译。
--
-- 每个lua_State(每个worker)各自require一次，实例天然隔离，不要跨线程共用。
-- 描述符语法见 engine/src/net/header/net_header.hpp

local EngineNetHeader = require "engine.NetHeader"

-- 名字 -> 描述符
--   '<' 小端(默认)  [bB]1B [hH]2B [iI]4B [qQ]8B  小写有符号
local DEFS = {
    none = "",
    ss = "i4i4i4i4", -- src, dst, mtype, usize
    ws = "H2" -- 协议号
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
-- @param data string或者lightuserdata
-- @param len 数据长度，data为lightuserdata时必填
-- @return 字段1..字段n, 载荷指针(lightuserdata), 载荷长度
--         数据不足一个头时返回nil，由调用方关连接
function NetHeader.decode(name, data, len)
    return hdr:decode(ids[name], data, len)
end

return NetHeader
