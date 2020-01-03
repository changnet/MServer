-- luacheck setting
-- https://luacheck.readthedocs.io/en/stable/config.html

std = "lua53"

max_line_length = 80
max_code_line_length = 80

-- 只读全局变量,一般是C++导出到Lua的对象
read_globals =
{
    "ev",
    "network_mgr"
}

-- 可读可写全局变量
globals =
{
    "table",
    "__g_async_log"
}

ignore =
{
    "212", -- (W212)unused argument
}
