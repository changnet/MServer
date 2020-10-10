-- simple test facility

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

-- describe a test case
local Describe = {}

-- describe constructor
-- @param title a custom test case tile
-- @param func test callback function
function Describe:__init(title, func)
    self.title = title
    self.func  = func
    self.i = {} -- it block
end

rawset(Describe, "__index", Describe)
rawset(Describe, "__name", "Describe")
setmetatable(Describe, {
    __call = function(self, ...)
        local ins = setmetatable({}, Describe)
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
end

rawset(It, "__index", It)
rawset(It, "__name", "It")
setmetatable(It, {
    __call = function(self, ...)
        local ins = setmetatable({}, It)
        ins:__init(...)
        return ins;
    end
})

-- /////////////////// GLOBAL test function ////////////////////////////////////
-- data storage for test
_G.__test = {
    print = print
}
local T = _G.__test

local INDENT = "       "

local OK     = "[  OK] "
local FAIL   = "[FAIL] "
local TEST_FAIL = "__test_fail__"

-- check a variable is a test instance
local function valid_test_ins(ins, name)
    assert(type(ins) == "table", "using custom self in test function not allow")

    assert (ins.__name == name, "using custom self in test function not allow")
end

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

-- 打印测试信息
function t_print(...)
    T.print(...)
end

-- test if two variable equal
function t_equal(got, expect)
    if equal(got, expect) then return end

    -- show msg late and abort current test
    local msg = debug.traceback(string.format(
        "got: %s, expect: %s",dump(got), dump(expect)), 2)

    append_msg(msg)
    assert(fase, TEST_FAIL)
end

-- test if expr is true
function t_assert()
end

function t_describe(title, func)
    local d = Describe(title, func)
    table.insert(T.d, d)

    -- valid_test_ins(self, "Describe")
end

function t_it(title, func)
    local i = It(title, func)

    table.insert(T.d_now.i, i)
end

function t_wait(timeout)
end

function t_done()
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

-- first reset
t_reset()

local function run_one(d)
    T.print(B(d.title))
    T.d_now = d
    local ok, msg = xpcall(d.func, error_msgh)
    if not ok then
        T.print(R(msg))
        return
    end

    for _, i in pairs(d.i) do
        T.i_now = i
        local tm = os.clock()
        local ok, msg = xpcall(i.func, error_msgh)
        if ok then
            tm = math.ceil((os.clock() - tm) * 1000)

            T.pass = T.pass + 1
            if tm > 1 then
                T.print(G("%s%s (%dms)", OK, i.title, tm))
            else
                T.print(G(OK .. i.title))
            end
        else
            T.fail = T.fail + 1
            if not msg:find(TEST_FAIL) then
                append_msg(msg)
            end
            T.print(R(FAIL .. i.title))
            print_msg(i)
        end
    end
end

-- run current test session
function t_run()
    T.time = os.clock()

    for _, d in pairs(T.d) do
        run_one(d)
    end

    local pass = T.pass
    local fail = T.fail
    local time = math.ceil((os.clock() - T.time) * 1000)
    T.print(string.format("%s, %s (%dms)",
        G("%d passing", T.pass),
        fail > 0 and R("%d failing", fail) or G("0 failing"),
        time))
end
