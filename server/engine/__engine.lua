---@diagnostic disable: missing-return

-- 导出模块: Engine
Engine = {}


---@param sig number
---@param mask number
function Engine.signal_mask(sig, mask)
end

function Engine.signal_mask_once()
end

--- 把线程添加到管理器，用于消息转发
---@param addr 线程地址
---@param ctx 线程指针
function Engine.add_thread_ctx(addr, ctx)
end

--- 把线程从管理器删除
---@param addr 线程地址
function Engine.del_thread_ctx(addr)
end

--- 获取一个线程指针
---@param addr 线程地址
function Engine.get_thread_ctx(addr)
end

function Engine.steady_clock()
end

function Engine.system_clock()
end

function Engine.time()
end

function Engine.clock()
end

function Engine.time_ms()
end

function Engine.update()
end

---@brief 让当前线程睡眠指定时间
---@param ms 毫秒
function Engine.sleep(ms)
end

---@brief 设置当前线程的名字
---@param name 线程名字，最大16字符（包括\0结尾）
function Engine.set_thread_name(name)
end
