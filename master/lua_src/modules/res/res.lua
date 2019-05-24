-- res.lua
-- 018-04-09
-- xzc

-- 资源管理模块
-- 资源在整个游戏配置中应该采用统一的配置(奖励、消耗等)，如
-- 资源数组{{res = 1,id = 0,count = 100},{res = 3,id = 1000,count = 1}}
-- 则基本上所有的资源操作都可以统一处理

local res_get_map = {} -- 获取
local res_add_map = {} -- 增加
local res_dec_map = {} -- 扣除

local RES = require "modules.res.res_header"

local Res = oo.singleton( ... )

-- 添加资源
-- @res_list:资源数组{{res = 1,id = 0,count = 100},{res = 3,id = 1000,count = 1}}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:add_res( player,res_list,op,sub_op )
    for _,one in pairs( res_list ) do
        self:add_one_res( player,one,op,sub_op )
    end
end

-- 扣除资源(这个函数并不检测资源是否足够)
-- @res_list:资源数组{{res = 1,id = 0,count = 100},{res = 3,id = 1000,count = 1}}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:dec_res( player,res_list,op,sub_op )
    for _,one in pairs( res_list ) do
        self:dec_one_res( player,one,op,sub_op )
    end
end

-- 增加一个资源
-- @res:资源{res = 1,id = 0,count = 100}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:add_one_res( player,res,op,sub_op )
    local add_func = res_dec_map[res.res]
    if not add_func then
        ERROR("Res:add_one_res no function found:%s,op %d",tostring(res.res),op)
        return
    end

    -- 把res也传过去，有时候某些资源会有特殊属性，如强化、升星...
    return add_func( player,res.id,res.count,op,sub_op,res )
end

-- 扣除一个资源
-- @res:资源{res = 1,id = 0,count = 100}
-- @op:操作代码，用于记录日志
-- @sub_op:子操作代码，用于记录日志。比如op为邮件，那么sub_op可以表示哪个功能发的
function Res:dec_one_res( player,res,op,sub_op )
    local dec_func = res_dec_map[res.res]
    if not dec_func then
        ERROR("Res:dec_one_res no function found:%s,op %d",tostring(res.res),op)
        return
    end

    -- 把res也传过去，有时候某些资源会有特殊属性，如强化、升星...
    return dec_func( player,res.id,res.count,op,sub_op,res )
end

-- 检查某个资源是否足够
function Res:check_one_res( player,res )
    local get_func = res_get_map[res.res]
    if not get_func then
        ERROR("Res:check_one_res not found:%s,op %d",tostring(res.res),op)
        return false
    end

    -- 把res也传过去，有时候某些资源会有特殊属性，如强化、升星...
    return get_func( player,res.id,res.count,res )
end

-- 检测资源是否足够
-- @res_list:资源数组{{res = 1,id = 0,count = 100},{res = 3,id = 1000,count = 1}}
function Res:check_res( player,res_list )
    for _,one in pairs( res_list ) do
        if not self:dec_one_res( player,one,op,sub_op ) then
            return false,one
        end
    end

    return true
end

-- 注册处理函数(不能放在__init里，因为要热更。热更不会重新调用__init)
function Res:reg_player_res( res_type,get,add,dec )
    local res_op = function( player,... ) return func( player,... ) end
    res_get_map[res_type] = function( player,... ) return get( player,... ) end
    res_add_map[res_type] = function( player,... ) return add( player,... ) end
    res_dec_map[res_type] = function( player,... ) return dec( player,... ) end
end

local function res_op_factory( module,func )
    return function( player,... )
        return func( player:get_module(module),... )
    end
end

-- 注册玩家子模块资源处理函数(不能放在__init里，因为要热更。热更不会重新调用__init)
function Res:reg_module_res( module,res_type,get,add,dec )
    res_get_map[res_type] = res_op_factory( module,get)
    res_add_map[res_type] = res_op_factory( module,add)
    res_dec_map[res_type] = res_op_factory( module,dec)
end

local res = Res()
return res
