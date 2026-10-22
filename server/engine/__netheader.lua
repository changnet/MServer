---@diagnostic disable: missing-return

-- 导出类: NetHeader
NetHeader = {}
--- nh:define("i4i4i4i4") -> id；描述符语法见文件头注释
function NetHeader:define()
end

--- nh:size(id) -> 头的字节数
function NetHeader:size()
end

--- nh:encode(id, v1..vn) -> lightuserdata指针, 长度；指针下一次encode前有效
function NetHeader:encode()
end

--- nh:decode(id, ud, len) -> v1..vn, 载荷指针(lightuserdata|nil), 载荷长度
function NetHeader:decode()
end

--- nh:format(id) -> 描述符的规范形式（调试用，见实现）
function NetHeader:format()
end
