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
    print("exec cmd:", cmd)
end

local function try_read()
    local line = _G.cli_reader:read()
    if not line then return end

    xpcall(do_cmd, __G__TRACKBACK, line)
end

local function start_cli()
    print("start command line interface")
    local reader = StdinReader()
    reader:start()

    _G.cli_reader = reader
    Timer.interval(1000, 250, -1, try_read)
end

Rtti.name_func("cli.try_read", try_read)
SE.reg(SE_READY, start_cli)
