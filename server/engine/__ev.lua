---@diagnostic disable: missing-return

-- 导出类: EV
EV = {}
--- 退出主循环
function EV:stop()
end

---@brief 把一个message并push到线程消息队列，同时唤醒线程。
--- 必须要保证message生命周期在当前线程处理前一直有效。
---@param message 消息指针，可用acquire_message获取或者construct_message构建
function EV:push_message(message)
end

--- 解析一个thread message，非安全操作
---@return src,dst,mtype,udata,usize
function EV:unpack_message()
end

---@brief 构造一个message并push到线程消息队列，同时唤醒线程
---@param udata 自定义数据，长度为usize。为nullptr时，usize可以用作数据字段
function EV:emplace_message(src, dst, type, udata, usize)
end

--- 创建一个thread message
---@param size buffer的大小（不包含ThreadMessage本身）
---@return 消息指针，buffer指针
function EV:construct_message(size)
end

--- 销毁一个thread message，非安全操作
function EV:destruct_message()
end

--- 夺取当前线程回调到Lua的消息（用于转发到其他线程），该消息在当前线程将不再销毁
function EV:acquire_message()
end

---@brief 启动定时器
---@param id 定时器唯一id
---@param after N毫秒秒后第一次执行
---@param interval 重复执行间隔，毫秒数
---@param policy 定时器重新规则时的策略
---@return 成功返回>=1,失败返回值<0
function EV:timer_start(id, after, interval, policy)
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
