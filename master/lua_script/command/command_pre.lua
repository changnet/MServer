-- command_pre 指令预处理


local ss  = require "command.ss_command"
local cmd = require "command.sc_command"

-- 协议使用太频繁，放到全局变量
-- 使用时只需要一个指令值就可以了，没必要暴露一个table给用户
SS = {}
for k,v in pairs( ss ) do
    SS[k] = v[1]
end

SC = {}
for k,v in pairs( cmd[1] ) do
    SC[k] = v[1]
end

CS = {}
for k,v in pairs( cmd[2] ) do
    CS[k] = v[1]
end

local Command_pre = oo.singleton( nil,... )

-- TODO 协议热更要重新处理

return command_pre