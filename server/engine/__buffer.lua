---@diagnostic disable: missing-return

-- 导出类: Buffer
Buffer = {}
---@brief 获取数据
--- sharedata:get("user_list", 12345)
function Buffer:get()
end

---@brief 设置数据
--- sharedata:set("user_list", 12345, {name = "abc", level = 123})
---@return 成功返回 true，失败返回 false, err_msg
function Buffer:set()
end

--- 从buffer中读取一个整形（int8, int32，int64等等）
---@param  buffer C++的buffer指针
---@param size 读取的字节大小(int32=4字节，int64=8字节)
---@param offset 指针的偏移量（可选）
---@return 读取到的数值，读取后的buffer指针
function Buffer:read_int(buffer, size, offset)
end

--- 往buffer中写入一个整形（int8, int32，int64等等）
---@param  buffer C++的buffer指针
---@param size 读取的字节大小(int32=4字节，int64=8字节)
---@param offset 指针的偏移量（可选）
---@return 读取到的数值，读取后的buffer指针
function Buffer:write_int(buffer, size, offset)
end

--- 往buffer中写入一个另一段buffer
---@param  buffer C++的buffer指针
---@param wbuffer 要写入的buffer
---@param size 要写入的长度
---@param offset 指针的偏移量（可选）
---@return 读取到的数值，读取后的buffer指针
function Buffer:write_buffer(buffer, wbuffer, size, offset)
end

--- 把C++中void *类型指针的buffer转换为Lua的string
---@param buffer C++中void *类型指针的buffer
---@param size buffer的大小
---@return string
function Buffer:lightud_tostring(buffer, size)
end

--- 把lua string保存到当前buffer中
---@return buffer指针
function Buffer:fromstring()
end

--- 把当前buffer中的数据转换为lua string
---@return buffer指针，buffer大小
function Buffer:tostring()
end
