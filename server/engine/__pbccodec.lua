---@diagnostic disable: missing-return

-- 导出类: PbcCodec
PbcCodec = {}
function PbcCodec:reset()
end

function PbcCodec:load()
end

function PbcCodec:update()
end

function PbcCodec:encode()
end

function PbcCodec:decode()
end

--- 从二进制buffer指针解码数据包
---@param buffer 待解码的char*
---@param size 待解码的buff大小
---@return <0 error,otherwise the number of parameter push to stack
function PbcCodec:decode_from_buffer(buffer, size)
end

--- 编码数据包
---@param ... 待编码的参数
---@return buffer大小，buffer指针（该指针在下一次调用encode时有效）。出错时，buffer大小返回-1
function PbcCodec:encode_to_buffer()
end
