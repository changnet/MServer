---@diagnostic disable: missing-return

-- 导出类: LuaCodec
LuaCodec = {}
--- 编码数据包
---@param ... 待编码的参数
---@return 编码后的数据，lua string
function LuaCodec:encode()
end

--- 解码数据包
---@param buffer 待解码的char*
---@param size 待解码的buff大小
---@return <0 error,otherwise the number of parameter push to stack
function LuaCodec:decode(buffer, size)
end

--- 从二进制buffer指针解码数据包
---@param buffer 待解码的char*
---@param size 待解码的buff大小
---@return <0 error,otherwise the number of parameter push to stack
function LuaCodec:decode_from_buffer(buffer, size)
end

--- 编码数据包
---@param ... 待编码的参数
---@return buffer大小，buffer指针（该指针在下一次调用encode时有效）。出错时，buffer大小返回-1
function LuaCodec:encode_to_buffer()
end
