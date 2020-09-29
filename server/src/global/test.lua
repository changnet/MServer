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

-- RED BLUE GREEN YELLOW WHITE
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
end

rawset(Describe, "__index", Describe)
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
end

rawset(It, "__index", It)
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

-- 打印测试信息
function t_print()
end

function t_equal()
end

function t_assert()
end

function t_describe(title, func)
end

function t_it(title, func)
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
end

-- run current test session
function t_run()

    local pass = 0
    local fail = 0
    local time = 0
    T.print(string.format("%s, %s (%dms)",
        G("%d passing", pass), R("%d failing", fail), time))
end
