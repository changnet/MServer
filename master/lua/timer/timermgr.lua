-- timermgr.lua
-- 2015-10-04
-- xzc

-- 定时器管理

local Timer_mgr = oo.singleton( nil,... )
local Timer = require "lua.timer.timer"

-- 构造函数
function Timer_mgr:__init()
    ev:set_timer_ref( self,self.do_timer )
    self.timer = {}
end
 
-- 开启新定时器
function Timer_mgr:start( _repeat,cb,... )
    local id = ev:timer_start( _repeat )
    self.timer[id] = Timer( cb,... )
    
    return id
end

-- 关闭定时器
function Timer_mgr:stop( id )
    ev:timer_kill( id )
    self.timer[id] = nil
end

-- timer总回调函数
function Timer_mgr:do_timer( id )
    local timer = self.timer[id]

    xpcall( timer.invoke,__G__TRACKBACK__,timer )
end

local timer_mgr = Timer_mgr()

return timer_mgr
