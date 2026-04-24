---@diagnostic disable: missing-return

-- 导出模块: lua_parson
lua_parson = {}


--- encode a lua table to json string. a lua error will throw if any error occur
---@param tbl a lua table to be encode
---@param pretty boolean, format json string to pretty human readable or not
---@param opt number, option to set the table as array or object
---@return json string
function lua_parson.encode(tbl, pretty, opt)
end

--- decode a json string to a lua table.a lua error will throw if any error occur
---@param a json string
---@param comment boolean, is the str containt comments
---@param opt number, enable integer key convertion if set
---@return a lua table
function lua_parson.decode(a, comment, opt)
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
