-- command_pre 指令预处理


local ss_map = {}
local sc_map = {}
local cs_map = {}

-- 使用ex来保证被热更
local ss_list  = require_ex "proto.ss_command"
local cmd_list = require_ex "proto.proto"

-- 协议使用太频繁，放到全局变量
-- 使用时只需要一个指令值就可以了，没必要暴露一个table给用户
SS = {}
for k,v in pairs( ss_list ) do
    SS[k] = v[1]
    ss_map[ v[1] ] = v
end

SC = {}
for k,v in pairs( cmd_list[1] ) do
    SC[k] = v[1]
    sc_map[ v[1] ] = v
end

CS = {}
for k,v in pairs( cmd_list[2] ) do
    CS[k] = v[1]
    cs_map[ v[1] ] = v
end

-- 对于CS、SS数据包，在注册时设置,因为要记录sesseion，实现自动转发
-- SC数据包只能全部设置
for _,v in pairs( cmd_list[1] ) do
    network_mgr:set_sc_cmd( v[1],v[2],v[3],0,Main.session )
end

local Command_pre = oo.singleton( nil,... )

function Command_pre:get_ss_cmd( cmd )
    return ss_map[cmd]
end

function Command_pre:get_sc_cmd( cmd )
    return sc_map[cmd]
end

function Command_pre:get_cs_cmd( cmd )
    return cs_map[cmd]
end

-- TODO 协议热更要重新处理

local preprocessor = Command_pre()
return preprocessor