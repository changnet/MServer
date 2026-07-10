---@diagnostic disable: missing-return

-- 导出模块: lua_rapidxml
lua_rapidxml = {}


--- 编码数据包
---@param schema protobuf的message名
---@param  pkt 待编码的数据，lua table格式
---@return 二进制string
function lua_rapidxml.encode(schema, pkt)
end

--- 解码数据包
---@param  schema protobuf的message名
---@param buffer 二进制string
---@return 包含解析出来数据的lua table
function lua_rapidxml.decode(schema, buffer)
end

--- encode a lua table to file. a lua error will throw if any error occur
---@param tbl a lua table to be encode
---@param file a file path that json string will be written in
---@param pretty boolean, format json string to pretty human readable or not
---@param opt number, option to set the table as array or object
---@return boolean
function lua_rapidxml.encode_to_file(tbl, file, pretty, opt)
end

--- decode a file content to a lua table.a lua error will throw if any error
--- occur
---@param a json file path
---@param comment boolean, is the file content containt comments
---@param opt number, enable integer key convertion if set
---@return a lua table
function lua_rapidxml.decode_from_file(a, comment, opt)
end
