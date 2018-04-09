-- res.lua
-- 018-04-09
-- xzc

-- 资源管理模块
-- 资源在整个游戏配置中应该采用统一的配置(奖励、消耗等)，如
-- 资源数组{{id = 1,id = 0,count = 100},{id = 3,id = 1000,count = 1}}
-- 则基本上所有的资源操作都可以统一处理

local RES = require "modules.res.res_header"

local Res = oo.singleton( nil,... )

-- 添加资源
-- @res_list:资源数组{{id = 1,id = 0,count = 100},{id = 3,id = 1000,count = 1}}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:add_res( res_list,op,sub_op )
end

-- 扣除资源(这个函数并不检测资源是否足够)
-- @res_list:资源数组{{id = 1,id = 0,count = 100},{id = 3,id = 1000,count = 1}}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:dec_res( res_list,op,sub_op )
end

-- 增加一个资源
-- @res:资源{id = 1,id = 0,count = 100}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:add_one_res( res,op,sub_op )
end

-- 扣除一个资源
-- @res:资源{id = 1,id = 0,count = 100}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:dec_one_res( res,op,sub_op )
end

-- 检测资源是否足够
-- @res_list:资源数组{{id = 1,id = 0,count = 100},{id = 3,id = 1000,count = 1}}
function Res:check_res( res_list )
end

local res = Res()
return res
