-- luacheck setting
-- https://luacheck.readthedocs.io/en/stable/config.html

std = "lua53"

max_line_length = 120 -- 行长，包括注释
max_code_line_length = 80 -- 代码行长，不包括注释
max_string_line_length = 120 -- 暂时没生效，后面再看看为啥
max_comment_line_length = 256 -- 注释行长

-- 只读全局变量,一般是C++导出到Lua的对象
read_globals =
{
    "ev",
    "network_mgr",
    "__OS_NAME__",
    "__BACKEND__",
    "__COMPLIER_",
    "__VERSION__",
    "__TIMESTAMP__",
    "LINUX",
    "WINDOWS",
    "IPV4",
    "IPV6"
}

-- 可读可写全局变量
globals = require "__globals"

-- 一些做了扩展的内部库需要设置为global
table.insert(globals, "math")
table.insert(globals, "table")
table.insert(globals, "string")

exclude_files =
{
    "engine/*", -- 用于代码实例的引擎接口文件，不用检查
    "protto/proto_default.lua", -- 示例文件，不用检查
}

ignore =
{
    "212", -- (W212)unused argument
    "311", -- (W311)value assigned to variable 'player' is unused: player = nil
}
