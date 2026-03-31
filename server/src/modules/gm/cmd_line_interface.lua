-- CLI(Command Line Interface)交互

--[[
当配置cli为true时，表示需要启动交互模式，这时候会定时从终端（stdin）定时读取命令并执行

使用io.read()读取的话，当没有输入时程序会阻塞。独立开启一个线程太浪费，并且在lua也不好
处理。

用C++实现了一个read_stdin_nonblock()，这个函数实现得不太好。但这个功能仅开发、测试用，
就这样了

]]

local StdinReader = require "engine.StdinReader"

local function do_cmd(cmd)
    -- TODO 这里应该尝试找一个在线的玩家取pid，方便测试

    -- 如果没有以@开关，自动补充一个
    if not string.start_with(cmd, "@") then
        cmd = "@" .. cmd
    end

    GM.run_console_gm(cmd)
end

local function try_read()
    local line = _G.cli_reader:read()
    if not line then return end

    scall(do_cmd, line)
end

local function start_cli()
    print("start command line interface")
    local reader = StdinReader()
    reader:start()

    _G.cli_reader = reader
    Timer.interval(1000, 250, -1, try_read)
end

Rtti.name_func("cli.try_read", try_read)
Event.reg(EV.READY, start_cli)
