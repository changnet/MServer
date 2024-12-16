-- 用于测试的worker入口文件

local addr = ...

LOCAL_ADDR = tonumber(addr)

-- 设置lua文件搜索路径
package.path = "../?.lua;" .. "../src/?.lua;" .. package.path
-- 设置c库搜索路径，用于直接加载so或者dll的lua模块
if WINDOWS then
    package.cpath = "../c_module/?.dll;" .. package.cpath
else
    package.cpath = "../c_module/?.so;" .. package.cpath
end

require "global.oo" -- 这个文件不能热更
require "global.require" -- 需要热更的文件，必须放在这后面

-- 放require后面，是可以热更的
require "global.global"
require "engine.engine"

require "modules.system.define"
require "message.thread_message"
require "engine.signal"
require "engine.shutdown"
require "engine.bootstrap"
require "global.name"
require "rpc.rpc"

print("wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww", addr)
make_name()
