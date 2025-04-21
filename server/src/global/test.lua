-- simple test facility
Test = {}

--[[
0. 此模块是参照jest来设计的，分为describe和it，describe相当于模块，it相当于具体功能
   每一个describe和it都有自己的名字，测试时可通过filter选择执行对应的测试
1. 所用测试的api都以Test.xxx形式命名，表示这是一个测试用的接口，请勿用于业务逻辑
2. 一些参数可通过Test.setup设置，具体查看Test.steup接口
3. 进行异步测试时，必须通过Test.setup接口设置定时器函数
4. 测试是同步进行的，当遇到异步测试时，会阻塞到测试完成或超时
5. Test.describe中的测试会先收集，再执行，故如果需要执行测试前后的动作，则需要Test.before、Test.after

用例：
```lua
Test.describe("测试套件名", function()
    it("测试逻辑", function()
        T.print("hello")
    end)

    it("测试逻辑2-异步", function()
        Test.async(2000)

        http.get("www.example.com", function())
            Test.done()
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

-- /////////////////////////////////////////////////////////////////////////////
-- data storage for test
_G.__test = {print = print}
local T = _G.__test

-- /////////////////////////////////////////////////////////////////////////////
-- describe a test case
local Describe = {}

-- describe constructor
-- @param title a custom test case tile
-- @param func test callback function
function Describe:__init(title, func)
    self.title = title
    self.func = func
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
    self.func = func

    assert(not T.now, "it block must NOT called inside another it block")
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

local OK   = "[  OK] "
local FAIL = "[FAIL] "
local PEND = "[PEND] "
-- Test.assert、Test.equal等函数失败时，需要中止测试，此时只能用error
-- 但并不希望把这个堆栈打印出来。这了和执行错误区分开了，需要一个特殊的标识
local TEST_FAIL = "__Test.fail"

local function append_msg(msg)
    if not T.now.msg then T.now.msg = {} end

    table.insert(T.now.msg, msg)
end

-- 打印obj的消息
-- @param obj 正在执行的Test.before、Test.it等对象
local function print_msg(obj)
    if not obj.msg then return end

    for _, msg in pairs(obj.msg) do
        for s in msg:gmatch("[^\r\n]+") do
            -- T.print(R(INDENT .. s))
            T.R(s)
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
    for k, v in pairs(o) do
        if type(k) ~= 'number' then k = '"' .. tostring(k) .. '"' end
        s = s .. '[' .. tostring(k) .. '] = ' .. dump(v) .. ','
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
        if not equal(got[k], v) then
            T.print("got value of key " .. tostring(k) .. " not match",
                got[k], v)
            return false
        end
    end
    for k, v in pairs(got) do
        if not equal(expect[k], v) then
            T.print("expect value of key " .. tostring(k) .. " not match",
                expect[k], v)
            return false
        end
    end

    return true
end

-- 执行一个before函数
local function test_one_before(b)
    local ok, msg = xpcall(b.func, error_msgh)
    if not ok then
        -- before函数失败，则整个describe就不用再执行了，全部失败
        T.fail = T.fail + #(T.d_now.i)
        T.R("%s%s before %s", FAIL, T.d_now.title, msg)
        return false
    end

    -- 异步超时
    if b.status == PEND then
        T.fail = T.fail + #(T.d_now.i)
        T.R("%s%s before (timeout)", FAIL, T.d_now.title)
        return false
    end

    -- 异步失败
    if b.status == FAIL then
        T.fail = T.fail + #(T.d_now.i)
        T.R(FAIL .. T.d_now.title)
        print_msg(b)
        return false
    end

    return true
end

local function test_one_it(i)
    local tm = T.clock()
    local ok, msg = xpcall(i.func, error_msgh)
    if not ok then
        T.fail = T.fail + 1
        if not msg:find(TEST_FAIL) then append_msg(msg) end
        T.R(FAIL .. i.title)
        print_msg(i)
        return
    end

    -- 异步超时
    if i.status == PEND then
        T.fail = T.fail + 1
        T.R("%s%s (timeout)", FAIL, i.title)
        return
    end

    -- 异步测试失败，唤醒协程会标记status为TAIL
    -- 执行wait的，只能是it，不可能为describe
    if i.status == FAIL then
        T.fail = T.fail + 1
        T.R(FAIL .. i.title)
        print_msg(i)
        return
    end

    tm = T.clock() - tm

    T.pass = T.pass + 1
    if tm > 1 then
        T.G("%s%s (%dms)", OK, i.title, tm)
    else
        T.G(OK .. i.title)
    end
end

-- 执行一个describe测试
-- @param d describe
local function test_one_describe(d)
    -- 被过滤掉，这个测试不需要执行
    if not d.should_run and 0 == #d.i then return end

    T.B(d.title)
    T.d_now = d -- 正在测试中的describe

    -- 执行before函数
    for _, b in pairs(d.before or {}) do
        T.now = b
        local ok = test_one_before(b)
        T.now = nil
        if not ok then
            T.d_now = nil
            return
        end
    end

    for _, i in pairs(d.i) do
        T.now = i
        test_one_it(i)
        T.now = nil
    end

    -- 执行after函数
    for _, func in pairs(d.after or {}) do
        local ok, msg = xpcall(func, error_msgh)
        if not ok then T.R("%s", msg) end
    end

    T.d_now = nil
end

local function run_one_describe(d)
    -- 如果一个describe包含在filter中，则无论it是否包含，都执行
    -- 如果一个describe不包含在filter中，则要检测it是否包含
    local should_run = true
    if T.filter then should_run = string.find_whole(d.title, T.filter) end
    if T.skip then
        for _, pattern in pairs(T.skip) do
            if string.find(d.title, pattern) then
                should_run = false
                break
            end
        end
    end

    d.should_run = should_run
    local ok, msg = xpcall(d.func, error_msgh)
    if not ok then T.R(msg) end
end

local function resume()
    local ok, msg = coroutine.resume(T.co)
    if not ok then error(msg) end
end

-- ///////////////// test interface ////////////////////////////////////////////
-- 打印测试信息
function Test.print(...)
    T.print(...)
end

local function on_fail(msg)
    -- 如果当前测试有定时器，出错时销毁定时器
    if T.now.timer then
        T.timer.del(T.now.timer)
        T.now.timer = nil
    end

    append_msg(msg)

    if "running" == coroutine.status(T.co) then
        -- 打断当前测试，回到xpcall(即describe或者it开始的地方)，继续执行下一个测试
        error(TEST_FAIL)
    else
        T.now.status = FAIL
        resume()
    end
end

-- 获取测试时间
function Test.clock()
    return T.clock()
end

-- test if two variable equal
function Test.equal(got, expect)
    assert(T.co, "test already finished or not begin yet")
    if equal(got, expect) then return end

    -- show msg late and abort current test
    local msg = debug.traceback(string.format("got: %s, expect: %s", dump(got),
                                              dump(expect)), 2)

    on_fail(msg)
end

-- test if expr is true
function Test.assert(expr, msg)
    assert(T.co, "test already finished or not begin yet")
    if expr then return end

    local fail_msg = "assertion failed"
    if msg then fail_msg = fail_msg .. ": " .. msg end

    local dbg_msg = debug.traceback(fail_msg, 2)

    on_fail(dbg_msg)
end

function Test.describe(title, func)
    local d = Describe(title, func)
    table.insert(T.d, d)

    -- valid_tesTest.ins(self, "Describe")
end

-- 创建一个具体的测试
-- @param title 测试名字
-- @param mask 可选参数，是否执行此测试
-- @param func 测试函数
function Test.it(title, mask, func)
    -- 根据条件判断是否执行此测试
    if not mask then return end

    -- 修正可选参数mask
    if not func then func = mask end
    assert(func)

    -- 如果一个descript被过滤掉，那么看下它的所有it是否被过滤掉
    local should_run = T.d_now.should_run
    if not should_run and T.filter then
        should_run = string.find(title, T.filter)
    end
    -- 如果该测试需要执行，那判断下是否需要跳过
    if should_run and T.skip then
        for _, pattern in pairs(T.skip) do
            if string.find(title, pattern) then
                should_run = false
                break
            end
        end
    end
    if not should_run then return end

    local i = It(title, func)

    -- 策略1：得到所有it block后再统一执行
    -- 那么运行describe中的代码将不按顺序顺序，需要使用Test.before、Test.after来执行
    table.insert(T.d_now.i, i)

    -- 策略2：直接执行所有it block
    -- 那么describe中的代码按顺序执行
    -- 但是，如果整个测试中有异步测试时，仍需要使用Test.before、Test.after来执行
    -- run_one_it(i)
end

-- 异步测试定时器超时，一般由定时器回调
function Test.timeout()
    -- Timer模块要求此函数需要为全局函数或者二级函数

    -- Timer.timeout超时会自动删除，并不需要手动删除
    -- if T.now.timer then T.timer.del(T.now.timer) end

    T.now.timer = nil
    T.now.status = PEND
    resume()
end

-- 等待当前异步测试完成，并设置超时时间(毫秒)
function Test.wait(timeout)
    assert(not T.now.timer, "call wait multi times")

    T.now.timer = T.timer.new(timeout or 2000, Test.timeout)
    coroutine.yield()

    if T.now.timer then T.timer.del(T.now.timer) end
    T.now.timer = nil
    -- T.now.status = nil -- 这里保留异步完成后的状态，需要根据状态统计成功、失败
end

-- 结束当前异步测试
function Test.done()
    if T.now.timer then T.timer.del(T.now.timer) end

    T.now.timer = nil
    T.now.status = nil
    resume()
end

-- 测试前运行的函数
function Test.before(func)
    assert(T.d_now, "MUST called inside describe block")

    if not T.d_now.before then T.d_now.before = {} end

    table.insert(T.d_now.before, {func = func})
end

-- 测试后运行的函数
function Test.after(func)
    assert(T.d_now, "MUST called inside describe block")

    if not T.d_now.after then T.d_now.after = {} end

    table.insert(T.d_now.after, func)
end

-- setup test parameters
function Test.setup(params)
    T.timer = params.timer

    T.print = params.print or print
    -- red green blue yellow颜色打印日志
    local printf = function(fmt, ...)
        return T.print(string.format(fmt, ...))
    end
    T.R = params.R or printf
    T.G = params.G or printf
    T.B = params.B or printf
    T.Y = params.Y or printf

    -- os.clock在linux下是不包含sleep时间的，并且可能会溢出
    T.clock = params.clock or function()
        return math.ceil(os.clock() * 1000)
    end

    -- 过滤器，允许只执行一部分测试
    -- ./start.sh test --filter=https 只执行名字包含https的测试
    T.filter = params.filter

    -- 跳过多个测试，以;分隔
    if params.skip then
        T.skip = {}
        string.gsub(params.skip, '[^;]+', function(w)
            table.insert(T.skip, w)
        end)
    end
end

-- reset the test session
function Test.reset()
    T.d = {}
    T.d_now = nil
    T.now = nil -- 正在执行的Test.before、Test.it等函数
    T.pass = 0
    T.fail = 0
    T.time = 0
end

-- run current test session
local function run()
    T.time = T.clock()

    -- 收集所有测试
    for _, d in pairs(T.d) do
        T.d_now = d
        run_one_describe(d)
        T.d_now = nil
    end

    -- 执行测试
    if T.filter then T.Y("filter: %s", T.filter) end
    if T.skip then T.Y("skip: %s", table.concat(T.skip, ';')) end

    for _, d in pairs(T.d) do test_one_describe(d) end

    local pass = T.pass
    local fail = T.fail
    local time = T.clock() - T.time
    if fail > 0 then
        T.R(string.format("%d passing, %d failing (%dms)", pass, fail, time))
    else
        T.G(string.format("%d passing, 0 failing (%dms)", pass, time))
    end

    T.co = nil
end

-- run current test session
function Test.run()
    T.co = coroutine.create(run)

    return resume()
end

-- /////////////////////////////////////////////////////////////////////////////
Test.reset() -- first reset
