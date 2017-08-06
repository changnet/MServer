-- ai管理

-- ai名字以aiN来命名，方便策划配置怪物AI逻辑。例如：
-- ai1是benchmark测试
-- ai2是静止不主动攻击怪物
-- ai3是静止主动攻击怪物
-- ai4是巡逻主动攻击怪物
-- ai5是护送怪物，走特定路线

-- ai的加载通过ai_mgr的load函数来加载

-- ai调用比较频繁，但比较少改动。是否有必要使用class?
-- 通用的ai action写在哪?要考虑复用
-- 通用ai action也要和数字对应，方便策划配置怪物action
-- 事件返馈如何处理，如:怪物被攻击、血量减少、血量增加、移动到指定位置、定时、死亡
-- 要求不高的怪物，使用时间轮片(1秒分成4片，每次调用1/4的怪物)，部分事件驱动的怪物，不需要激活时间事件

-- ai通用接口
-- __init:初始化接口，在加载AI时调用
-- execute:定时器接口
-- event:事件接口，根据事件不同

-- AI精度，每N秒运行一次time_poll
local TIME_SLICE = 4

local Ai_mgr = oo.singleton( nil,... )

function Ai_mgr:__init()
    self.load_path = {}
    self.time_poll = {}
    self.time_index = 1
    self.insert_idx = 1

    for index = 1,TIME_SLICE do
        self.time_poll[index] = {}
        setmetatable( self.time_poll[index], {["__mode"]='k'} )
    end

    self.timer = g_timer_mgr:new_timer( self,10,1/TIME_SLICE/TIME_SLICE )

    g_timer_mgr:start_timer( self.timer )
end

function Ai_mgr:load( entity,index,conf )
    local load_path = self.load_path[index]
    if not load_path then
        load_path = string.format( "ai.ai%d",index )
        self.load_path[index] = load_path
    end

    local Ai = require( load_path )

    entity.ai = Ai
    entity.ai_conf  = conf
    entity.ai_index = index

    if Ai.__init then Ai.__init( entity,conf ) end

    entity.insert_idx = self.insert_idx
    self.time_poll[self.insert_idx][entity] = true
    self.insert_idx = self.insert_idx + 1
    if self.insert_idx > TIME_SLICE then self.insert_idx = 1 end
end

function Ai_mgr:unload( entity )
    local idx = entity.insert_idx
    self.time_poll[idx][entity] = nil
end

function Ai_mgr:do_timer()
    -- print("ai do time now index is %d",self.time_index)
    -- 大部分AI精度要求不高，通过一个定时器来分时间片来处理。
    for entity,_ in pairs( self.time_poll[self.time_index] ) do
        entity.ai.execute( entity )
    end

    self.time_index = self.time_index + 1
    if self.time_index > TIME_SLICE then self.time_index = 1 end
end

local ai_mgr = Ai_mgr()

return ai_mgr