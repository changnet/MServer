-- simple test facility

--[[
1. 所用测试的api都以t_xx形式命名，表示这是一个测试用的接口，请勿用于业务逻辑
2. 一些参数可通过t_setup设置，具体查看t_steup接口
3. 进行异步测试时，必须通过t_setup接口设置定时器函数
4. 测试是同步进行的，当遇到异步测试时，会阻塞到测试完成或超时
5. t_describe中的测试会先收集，再执行，故如果需要执行测试前后的动作，则需要t_before、t_after

用例：
```lua
t_describe("测试套件名", function()
    it("测试逻辑", function()
        T.print("hello")
    end)

    it("测试逻辑2-异步", function()
        t_wait(2000)

        http.get("www.example.com", function())
            t_done()
        end
    end)
end)
```

PS： 曾考虑过异步执行测试，但这样一来，异步测试里的日志打印就及其混乱。js的mocha测试也
是同步测试
```lua
-- 参考js的promise实现的异步测试
it("async test", function(wait, done)
    wait(2000)

    http.get("www.example.com", function())
        done()
    end
end)
```
]]

-- https://stackoverflow.com/questions/1718403/enable-bash-output-color-with-lua-script
-- Note the \27. Whenever Lua sees a \ followed by a decimal number, it converts
-- this decimal number into its ASCII equivalent. I used \27 to obtain the bash
-- \033 ESC character

-- https://en.wikipedia.org/wiki/ANSI_escape_code
-- 1;32;41m 表示1加粗，32前景色为绿，41背景色为红。这些颜色可以组合使用，用;隔开
-- 0 - Normal Style
-- 1 - Bold
-- 2 - Dim
-- 3 - Italic
-- 4 - Underlined
-- 5 - Blinking
-- 7 - Reverse
-- 8 - Invisible

-- NONE RED BLUE GREEN YELLOW WHITE
local function N(fmt, ...)
    return string.format(fmt, ...)
end
local function R(fmt, ...)
    return "\27[31m" .. string.format(fmt, ...) .. "\27[0m"
end
local function B(fmt, ...)
    return "\27[34m" .. string.format(fmt, ...) .. "\27[0m"
end
local function G(fmt, ...)
    return "\27[32m" .. string.format(fmt, ...) .. "\27[0m"
end
local function Y(fmt, ...)
    return "\27[33m" .. string.format(fmt, ...) .. "\27[0m"
end
local function W(fmt, ...)
    return "\27[37m" .. string.format(fmt, ...) .. "\27[0m"
end
-- /////////////////////////////////////////////////////////////////////////////
-- data storage for test
_G.__test = {
    print = print
}
local T = _G.__test

-- /////////////////////////////////////////////////////////////////////////////
-- describe a test case
local Describe = {}

-- describe constructor
-- @param title a custom test case tile
-- @param func test callback function
function Describe:__init(title, func)
    self.title = title
    self.func  = func
    self.i = {} -- it block

    assert(not T.d_now, "recursive describe test not support now")
end

rawset(Describe, "__index", Describe)
rawset(Describe, "__name", "Describe")
setmetatable(Describe, {
    __call = function(self, ...)
        local ins = setmetatable({}, self)
        ins:__init(...)
        return ins;
    end
})

-- a specification or test-case with the given `title` and callback `fn`
local It = {}

-- describe constructor
-- @param title a custom test case tile
-- @param func test callback function
function It:__init(title, func)
    self.title = title
    self.func  = func

    assert(not T.i_now, "it block must NOT called inside another it block")
    assert(T.d_now, "it block MUST called inside describe describe block")
end

rawset(It, "__index", It)
rawset(It, "__name", "It")
setmetatable(It, {
    __call = function(self, ...)
        local ins = setmetatable({}, self)
        ins:__init(...)
        return ins;
    end
})

-- /////////////////// internal test function //////////////////////////////////

local OK     = "[  OK] "
local FAIL   = "[FAIL] "
local PEND   = "[PEND] "
local TEST_FAIL = "__test_fail__"

local function append_msg(msg)
    if not T.i_now.msg then T.i_now.msg = {} end

    table.insert(T.i_now.msg, msg)
end

local function print_msg(i)
    if not i.msg then return end

    for _, msg in pairs(i.msg) do
        for s in msg:gmatch("[^\r\n]+") do
            -- T.print(R(INDENT .. s))
            T.print(R(s))
        end
    end
end

-- error message handl
local function error_msgh(msg)
    return debug.traceback(msg, 2)
end

local function dump(o)
    if type(o) ~= 'table' then return tostring(o) end

    local s = '{ '
    for k,v in pairs(o) do
        if type(k) ~= 'number' then k = '"' .. k.. '"' end
        s = s .. '['..k..'] = ' .. dump(v) .. ','
    end
    return s .. '} '
end

-- 检测两个lua变量是否相等
local function equal(got, expect)
    if got == expect then return true end

    local t = type(got)
    if t ~= type(expect) or "table" ~= t then return false end

    -- test two table equal
    for k, v in pairs(expect) do
        if not equal(got[k], v) then return false end
    end
    for k, v in pairs(got) do
        if not equal(expect[k], v) then return false end
    end

    return true
end

local function run_one_it(i)
    local tm = os.clock()
    local ok, msg = xpcall(i.func, error_msgh)
    if not ok then
        T.fail = T.fail + 1
        if not msg:find(TEST_FAIL) then
            append_msg(msg)
        end
        T.print(R(FAIL .. i.title))
        print_msg(i)
        return
    end


    -- 进入异步等待
    if i.status == PEND then coroutine.yield() end

    -- 异步超时
    if i.status == PEND then
        T.fail = T.fail + 1
        T.print(R("%s%s (timeout)", FAIL, i.title))
        return
    end

    -- 异步失败
    if i.status == FAIL then
        T.fail = T.fail + 1
        T.print(R(FAIL .. i.title))
        print_msg(i)
        return
    end

    tm = math.ceil((os.clock() - tm) * 1000)

    T.pass = T.pass + 1
    if tm > 1 then
        T.print(G("%s%s (%dms)", OK, i.title, tm))
    else
        T.print(G(OK .. i.title))
    end
end

local function run_one_describe(d)
    local ok, msg = xpcall(d.func, error_msgh)
    if not ok then
        T.print(R(msg))
    end
end

-- ///////////////// test interface ////////////////////////////////////////////
-- 打印测试信息
function t_print(...)
    T.print(...)
end

-- test if two variable equal
function t_equal(got, expect)
    assert(T.co, "test already finished or not begin yet")
    if equal(got, expect) then return end

    -- show msg late and abort current test
    local msg = debug.traceback(string.format(
        "got: %s, expect: %s",dump(got), dump(expect)), 2)

    append_msg(msg)

    if "running" == coroutine.status(T.co) then
        assert(false, TEST_FAIL)
    else
        T.i_now.status = FAIL
        coroutine.resume(T.co)
    end
end

-- test if expr is true
function t_assert(expr)
    assert(T.co, "test already finished or not begin yet")
    if expr then return end

    local msg = debug.traceback("assertion failed!")

    append_msg(msg)
    if "running" == coroutine.status(T.co) then
        assert(false, TEST_FAIL)
    else
        T.i_now.status = FAIL
        coroutine.resume(T.co)
    end
end

function t_describe(title, func)
    local d = Describe(title, func)
    table.insert(T.d, d)

    -- valid_test_ins(self, "Describe")
end

-- 创建一个具体的测试
function t_it(title, func)
    local i = It(title, func)

    -- 策略1：得到所有it block后再统一执行
    -- 那么运行describe中的代码将不按顺序顺序，需要使用t_before、t_after来执行
    table.insert(T.d_now.i, i)

    -- 策略2：直接执行所有it block
    -- 那么describe中的代码按顺序执行
    -- 但是，如果整个测试中有异步测试时，仍需要使用t_before、t_after来执行
    -- run_one_it(i)
end

-- 设置当前测试异步超时时间(毫秒)
function t_wait(timeout)
    assert(not T.i_now.timer, "call wait multi times")

    T.i_now.status = PEND
    T.i_now.timer = T.timer.new(timeout or 2000, function()
        coroutine.resume(T.co)
    end)
end

-- 结束当前异步测试
function t_done()
    T.timer.del(T.i_now.timer)

    T.i_now.timer = nil
    T.i_now.status = nil
    coroutine.resume(T.co)
end

-- 测试前运行的函数
function t_before(func)
    assert(T.d_now, "MUST called inside describe block")

    if not T.d_now.before then T.d_now.before = {} end

    table.insert(T.d_now.before, func)
end

-- 测试后运行的函数
function t_after(func)
    assert(T.d_now, "MUST called inside describe block")

    if not T.d_now.after then T.d_now.after = {} end

    table.insert(T.d_now.after, func)
end

-- setup test parameters
function t_setup(params)
    T.timer = params.timer

    -- log outpout function if not using std print
    T.print = params.print or print
end

-- reset the test session
function t_reset()
    T.d = {}
    T.d_now = nil
    T.i_now = nil
    T.pass = 0
    T.fail = 0
    T.time = 0
end

-- run current test session
local function run()
    T.time = os.clock()

    -- 收集所有测试
    for _, d in pairs(T.d) do
        T.d_now = d
        run_one_describe(d)
        T.d_now = nil
    end

    -- 执行测试
    for _, d in pairs(T.d) do
        T.print(B(d.title))

        -- 执行before函数
        for _, func in pairs(d.before or {}) do
            local ok, msg = xpcall(func, error_msgh)
            if not ok then
                T.print(R("%s", msg))
            end
        end

        T.d_now = d
        for _, i in pairs(d.i) do
            T.i_now = i
            run_one_it(i)
            T.i_now = nil
        end

        -- 执行before函数
        for _, func in pairs(d.after or {}) do
            local ok, msg = xpcall(func, error_msgh)
            if not ok then
                T.print(R("%s", msg))
            end
        end

        T.d_now = nil
    end

    local pass = T.pass
    local fail = T.fail
    local time = math.ceil((os.clock() - T.time) * 1000)
    T.print(string.format("%s, %s (%dms)",
        G("%d passing", pass),
        fail > 0 and R("%d failing", fail) or G("0 failing"),
        time))

    T.co = nil
end

-- run current test session
function t_run()
    T.co = coroutine.create(run)

    coroutine.resume(T.co)
end

-- /////////////////////////////////////////////////////////////////////////////
t_reset() -- first reset
