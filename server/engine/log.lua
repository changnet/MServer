-- Log
-- auto export by engine_api.lua do NOT modify!

-- 异步日志
Log = {}

-- 停止日志线程，并把未写入文件的日志全部写入
function Log:stop()
end

-- 启动日志线程，sec和usec时间是叠加的
-- @param sec 可选参数，每隔多少秒数写入一次文件
-- @param usec 可选参数，每隔多少微秒写入一次文件
function Log:start(sec, usec)
end

-- 写入日志内容
-- @param path 日志文件路径
-- @param ctx 日志内容
-- @param out_type 日志类型，参考 LogOut 枚举
function Log:write(path, ctx, out_type)
end

-- stdout、文件双向输出日志打印函数
-- @param ctx 日志内容
function Log:plog(ctx)
end

-- stdout、文件双向输出错误日志函数
-- @param ctx 日志内容
function Log:elog(ctx)
end

-- 设置日志参数
-- @param is_daemon 是否后台进程，后台进程不在stdout打印日志
-- @param ppath print普通日志的输出路径
-- @param epath error错误日志的输出路径
-- @param mpath mongodb日志的输出路径
function Log:set_args(is_daemon, ppath, epath, mpath)
end

-- 设置进程名，比如gateway world，输出的日志会加上名字
-- @param name 进程名
function Log:set_name(name)
end

