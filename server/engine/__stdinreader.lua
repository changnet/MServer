---@diagnostic disable: missing-return

-- 导出类: StdinReader
StdinReader = {}
---@brief 启动读取线程
function StdinReader:start()
end

---@brief 停止读取线程
function StdinReader:stop()
end

---@brief 读取从stdin获取到的数据
---@return 字符串，无则返回nil
function StdinReader:read()
end
