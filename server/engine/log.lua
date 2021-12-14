-- Log
-- auto export by engine_api.lua do NOT modify!

-- 异步日志
local Log = {}

-- 停止日志线程，并把未写入文件的日志全部写入
function Log:stop()
end

-- 启动日志线程
-- @param usec 可选参数，每隔多少微秒写入一次文件
function Log:start(usec)
end

-- 写入日志到指定文件
-- @param path 日志文件路径
-- @param ctx 日志内容
-- @param time 日志时间，不传则为当前主循环时间
function Log:append_log_file(path, ctx, time)
end

-- 写入字符串到指定文件，不加日志前缀，不自动换行
-- @param path 文件路径
-- @param ctx 内容
function Log:append_file(path, ctx)
end

-- stdout、文件双向输出日志打印函数
-- @param ctx 日志内容
function Log:plog(ctx)
end

-- stdout、文件双向输出错误日志函数
-- @param ctx 日志内容
function Log:elog(ctx)
end

-- 设置某个日志参数
-- @param path 日志的输出路径，用于区分日志
-- @param type 类型，详见PolicyType的定义
function Log:set_option(path, type)
end

-- 设置printf、error等基础日志参数
-- @param is_daemon 是否后台进程，后台进程不在stdout打印日志
-- @param ppath print普通日志的输出路径
-- @param epath error错误日志的输出路径
function Log:set_std_option(is_daemon, ppath, epath)
end

-- 设置进程名，比如gateway world，输出的日志会加上名字
-- @param name 进程名
function Log:set_name(name)
end

return Log
