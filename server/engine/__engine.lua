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

--- sync log是在日志线程未启动或者已关闭情况下紧急使用的，所以不考虑颜色之类花里胡哨的东西
--- 在日志线程未关闭的情况下，直接往文件或者控制台写会引起多线程导致显示不正确
---@param nullptr any
function Engine.time(nullptr)
end

function Engine.clock()
end

function Engine.time_ms()
end

---@brief 合并更新数据
--- sharedata:update("user_list", 12345, {name = "abc"})
---@return 成功返回 true，失败返回 false, err_msg
function Engine.update()
end

---@param ms number
function Engine.sleep(ms)
end

---@param name string
function Engine.set_thread_name(name)
end
