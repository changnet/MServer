---@diagnostic disable: missing-return

-- 导出类: ShareData
ShareData = {}
---@brief 获取数据
--- sharedata:get("user_list", 12345)
function ShareData:get()
end

---@brief 设置数据
--- sharedata:set("user_list", 12345, {name = "abc", level = 123})
---@return 成功返回 true，失败返回 false, err_msg
function ShareData:set()
end

---@brief 获取多个字段到cache中
--- sharedata:fetch_into("user_list", 12345, {name = 1, level = 1}, cache)
function ShareData:fetch_into()
end

---@brief 原子加法，只支持对整数操作
--- sharedata:fetch_add("user_list", 12345, "level", 1)
function ShareData:fetch_add()
end

---@brief 获取所有key
--- sharedata:keys("user_list")
function ShareData:keys()
end

---@brief 合并更新数据
--- sharedata:update("user_list", 12345, {name = "abc"})
---@return 成功返回 true，失败返回 false, err_msg
function ShareData:update()
end

---@brief 获取当前对象占用的内存大小
function ShareData:memory_size()
end

---@brief 移除数据
--- sharedata:unset("user_list", 12345)
---@return 成功返回 true
function ShareData:unset()
end
