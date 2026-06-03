-- 函数调用队列
FuncQueue = oo.class("FuncQueue")

local max = math.maxinteger

function FuncQueue:__init(use_co)
    self.queue = {} -- 环型队列
    self.beg_idx = 0 -- 队头索引
    self.end_idx = 0 -- 队尾索引
    self.use_co = use_co -- 是否使用协程执行函数
    self.invoking = false -- 是否正在执行函数中
end

local function invoke_func(self, func, ...)
    -- 调用的函数中可能用协程挂起

    self.invoking = true

    if self.use_co then
        CoPool.invoke(func, ...)
    else
        func(...)
    end

    self.invoking = false

    -- 执行队列中的下一个函数
    local beg_idx = self.beg_idx
    if beg_idx >= self.end_idx then
        return
    end

    beg_idx = beg_idx + 1
    local cb = self.queue[beg_idx]
    self.queue[beg_idx] = nil -- 及时释放内存

    return invoke_func(self, cb)
end

-- 把函数放入队列按顺序执行，前一个函数执行完后才会执行下一个函数
-- @param func 需要执行的函数
-- @param ... 需要传递给函数的参数
function FuncQueue:push(func, ...)
    local end_idx = self.end_idx

    if self.closed then error("queue closed") end

    if not self.invoking then
        return invoke_func(self, func, ...)
    end

    -- 就算一个函数每次执行1ms，不停地往队列里push，end_idx也不可能溢出
    if end_idx >= max then error("out of index") end

    end_idx = end_idx + 1
    self.queue[end_idx] = Rtti.make_method_cb(func, ...)

    self.end_idx = end_idx
end

-- 当前队列中长度（包含已经发起但未完成的调用）
function FuncQueue:len()
    local l = self.end_idx - self.beg_idx
    if self.invoking then l = l + 1 end

    return l
end

-- 裁减队列，保留前n个元素（n包含已经发起但未完成的调用）
function FuncQueue:trim(len)
    local end_idx = self.end_idx
    local beg_idx = self.beg_idx

    if self.invoking then len = len - 1 end
    if end_idx - beg_idx <= len then return end

    local new_end_idx = beg_idx + len
    for i = new_end_idx + 1, end_idx do
        self.queue[i] = nil
    end

    self.end_idx = new_end_idx
end

-- 关闭队列，不允许push，但允许执行完队列中剩余的函数
function FuncQueue:close()
    self.closed = true
end

return FuncQueue
