---@diagnostic disable: missing-return

-- 导出类: NetHeader
NetHeader = {}
--- nh:define("i4i4i4i4") -> id，id 是本实例内的下标
function NetHeader:define()
end

--- nh:size(id) -> 头的字节数
function NetHeader:size()
end

--- nh:encode(id, v1..vn) -> ptr(lightuserdata), len
function NetHeader:encode()
end

--- nh:decode(id, data[, len]) -> v1..vn, body_ptr, body_len；长度不足返回 nil
function NetHeader:decode()
end

--- nh:format(id) -> 描述符字符串（调试用）
function NetHeader:format()
end
