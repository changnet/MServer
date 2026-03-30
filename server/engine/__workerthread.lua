---@diagnostic disable: missing-return

-- 导出类: WorkerThread
WorkerThread = {}
--- 启动worker线程
---@param path 入口文件lua脚本
---@param addr 当前worker的地址
---@param ... 其他参数
function WorkerThread:start(path, addr)
end

---@brief 停止线程
---@param join 如果从主线程停止，可join此子线程。子线程主动停止不可join
function WorkerThread:stop(join)
end

function WorkerThread:is_start()
end

---@param 0 any
---@param 0 any
---@param NONE any
---@param nullptr any
---@param 0 any
function WorkerThread:emplace_message(0, 0, NONE, nullptr, 0)
end

---@brief 启动定时器
---@param id 定时器唯一id
---@param after N毫秒秒后第一次执行
---@param repeat 重复执行间隔，毫秒数
---@param policy 定时器重新规则时的策略
---@return 成功返回>=1,失败返回值<0
function WorkerThread:timer_start(id, after, repeat, policy)
end

---@brief 停止定时器并从管理器中删除
---@param id 定时器唯一id
---@return 成功返回0
function WorkerThread:timer_stop(id)
end

---@brief 启动utc定时器
---@param id 定时器唯一id
---@param after N毫秒秒后第一次执行
---@param repeat 重复执行间隔，毫秒数
---@param policy 定时器重新规则时的策略
---@return 成功返回>=1,失败返回值<0
function WorkerThread:periodic_start(id, after, repeat, policy)
end

---@brief 停止utc定时器并从管理器中删除
---@param id 定时器唯一id
---@return 成功返回0
function WorkerThread:periodic_stop(id)
end
