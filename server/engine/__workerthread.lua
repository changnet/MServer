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

--- 是否运行中
function WorkerThread:is_start()
end

---@brief 把一个message并push到线程消息队列，同时唤醒线程。
--- 必须要保证message生命周期在当前线程处理前一直有效。
---@param message 消息指针，可用acquire_message获取或者construct_message构建
function WorkerThread:push_message(message)
end

---@brief 构造一个message并push到线程消息队列，同时唤醒线程
---@param udata 自定义数据，长度为usize。为nullptr时，usize可以用作数据字段
function WorkerThread:emplace_message(src, dst, type, udata, usize)
end

--- 夺取当前线程回调到Lua的消息（用于转发到其他线程），该消息在当前线程将不再销毁
function WorkerThread:acquire_message()
end

---@brief 启动定时器
---@param id 定时器唯一id
---@param after N毫秒秒后第一次执行
---@param interval 重复执行间隔，毫秒数
---@param policy 定时器重新规则时的策略
---@return 成功返回>=1,失败返回值<0
function WorkerThread:timer_start(id, after, interval, policy)
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
