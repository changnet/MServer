---@diagnostic disable: missing-return

-- 导出模块: Engine
Engine = {}


---@brief 设置信号的行为
---@param sig 信号id
---@param mask 0按默认行为处理信号 1忽略该信号 其他值则统一回调到脚本处理
function Engine.signal_mask(sig, mask)
end

--- 获取信号掩码并重置原有信号掩码
function Engine.signal_mask_once()
end

---@param addr number
---@param ctx any
function Engine.add_thread_ctx(addr, ctx)
end

---@param addr number
function Engine.del_thread_ctx(addr)
end

function Engine.get_thread_ctx()
end

function Engine.steady_clock()
end

--- 获取实时utc时间
---@return utc时间（毫秒）
function Engine.system_clock()
end

--- 获取帧utc时间戳(单位秒)
function Engine.time()
end

--- 获取帧时间（毫秒）
function Engine.clock()
end

--- 获取帧utc时间戳(单位毫秒)
function Engine.time_ms()
end

--- 更新全局pbc env到当前线程
function Engine.update()
end

---@brief 让当前线程睡眠指定时间
---@param ms 毫秒
function Engine.sleep(ms)
end

---@param name string
function Engine.set_thread_name(name)
end
