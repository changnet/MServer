---@diagnostic disable: missing-return

-- 导出类: EV
EV = {}
function EV:stop()
end

--- 解析一个thread message，非安全操作
---@return src,dst,mtype,udata,usize
function EV:unpack_message()
end

---@param 0 any
---@param 0 any
---@param NONE any
---@param nullptr any
---@param 0 any
function EV:emplace_message(0, 0, NONE, nullptr, 0)
end

--- 创建一个thread message
---@param size buffer的大小（不包含ThreadMessage本身）
---@return 消息指针，buffer指针
function EV:construct_message(size)
end

--- 销毁一个thread message，非安全操作
function EV:destruct_message()
end

---@brief 启动定时器
---@param id 定时器唯一id
---@param after N毫秒秒后第一次执行
---@param repeat 重复执行间隔，毫秒数
---@param policy 定时器重新规则时的策略
---@return 成功返回>=1,失败返回值<0
function EV:timer_start(id, after, repeat, policy)
end

---@brief 停止定时器并从管理器中删除
---@param id 定时器唯一id
---@return 成功返回0
function EV:timer_stop(id)
end

---@brief 启动utc定时器
---@param id 定时器唯一id
---@param after N毫秒秒后第一次执行
---@param repeat 重复执行间隔，毫秒数
---@param policy 定时器重新规则时的策略
---@return 成功返回>=1,失败返回值<0
function EV:periodic_start(id, after, repeat, policy)
end

---@brief 停止utc定时器并从管理器中删除
---@param id 定时器唯一id
---@return 成功返回0
function EV:periodic_stop(id)
end
