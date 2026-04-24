---@diagnostic disable: missing-return

-- 导出类: PbcCodec
PbcCodec = {}
--- 重置全局pbc env
function PbcCodec:reset()
end

--- 加载单个pb文件
function PbcCodec:load()
end

--- 更新全局pbc env到当前线程
function PbcCodec:update()
end

--- 编码数据包
---@param schema protobuf的message名
---@param  pkt 待编码的数据，lua table格式
---@return 二进制string
function PbcCodec:encode(schema, pkt)
end

--- 解码数据包
---@param  schema protobuf的message名
---@param buffer 二进制string
---@return 包含解析出来数据的lua table
function PbcCodec:decode(schema, buffer)
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
