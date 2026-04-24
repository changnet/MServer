---@diagnostic disable: missing-return

-- 导出类: MySql
MySql = {}
---@brief 测试连接是否完好，连接断开将自动重连
--- return 错误码
function MySql:ping()
end

---@brief 执行sql语句，不返回结果
---@param stmt sql语句
---@return 错误码
function MySql:exec(stmt)
end

---@brief 执行sql语句，返回结果
---@param stmt sql语句
---@return 错误码， 结果
function MySql:query(stmt)
end

---@brief 连接mysql数据库
--- return 错误码及错误信息
function MySql:error()
end

---@brief 对字符串进行MYSQL转义
---@param str 要转义的字符串
---@return 新字符串
function MySql:escape(str)
end

---@brief 连接mysql数据库
---@param host mysql地址
---@param port mysql端口
---@param usr 用户名
---@param pwd 密码
---@param dbname 数据库名
---@return 错误码
function MySql:connect(host, port, usr, pwd, dbname)
end

function MySql:disconnect()
end

---@brief 初始化线程
--- return 错误码
function MySql:thread_init()
end

---@brief 释放线程内存
--- return 错误码
function MySql:thread_end()
end

---@brief 追加字符串到sql语句
---@param ... 追加的多个字符串
function MySql:stmt_str()
end

---@brief 执行构建好的sql
---@param result 是否获取执行结果
---@return 错误码,结果集
function MySql:stmt_exec(result)
end

---@brief 追加数据库的值到sql，会进行转义
---@param type 数据库对应的字段类型
---@param value 字段值
function MySql:stmt_value(type, value)
end

---@brief 清空sql语句缓存
function MySql:stmt_clear()
end
