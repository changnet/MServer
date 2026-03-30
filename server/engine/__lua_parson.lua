---@diagnostic disable: missing-return

-- 导出模块: lua_parson
lua_parson = {}

--- 编码数据包
---@param ... 待编码的参数
---@return 编码后的数据，lua string
function lua_parson.encode()
end

--- 解码数据包
---@param buffer 待解码的char*
---@param size 待解码的buff大小
---@return <0 error,otherwise the number of parameter push to stack
function lua_parson.decode(buffer, size)
end

--- encode a lua table to file. a lua error will throw if any error occur
---@param tbl a lua table to be encode
---@param file a file path that json string will be written in
---@param pretty boolean, format json string to pretty human readable or not
---@param opt number, option to set the table as array or object
---@return boolean
function lua_parson.encode_to_file(tbl, file, pretty, opt)
end

--- decode a file content to a lua table.a lua error will throw if any error
--- occur
---@param a json file path
---@param comment boolean, is the file content containt comments
---@param opt number, enable integer key convertion if set
---@return a lua table
function lua_parson.decode_from_file(a, comment, opt)
end
