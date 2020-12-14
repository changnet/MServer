-- scene.lua
-- xzc
-- 2018-11-21

-- 场景

local pix = 64 -- 一个格子边长64像素
local visual_width = 3 -- 视野宽度格子数
local visual_height = 4 -- 视野高度格子数

local Aoi = require "GridAoi"
local ET = require "modules.entity.entity_header"
local scene_conf = require_kv_conf("dungeon_scene","id")

local Scene = oo.class( ... )

-- 缓存一个table用于和底层交互，避免频繁创建table
local tmp_list = {}
local g_entity_mgr = g_entity_mgr

function Scene:__init(id,dungeon_id,dungeon_hdl)
    self.id = id
    self.dungeon_id  = dungeon_id
    self.dungeon_hdl = dungeon_hdl

    -- TODO:对于绝大多数地图，地图数据都是不变的，多个副本共用一份，则g_map_mgr管理就可以了
    -- 一些场景可能会有动态地图，比如根据帮派改变可行走区域，这时就要用map:fork来复制一份
    -- 地图数据根据程序处理了，这里暂不处理
    local map_id = scene_conf[id].map
    local map = g_map_mgr:get_map( map_id )

    local width,height = map:get_size()

    local pix_width = width * pix
    local pix_height = height * pix

    local aoi = Aoi()
    aoi:set_size(pix_width, pix_height, pix)
    aoi:set_visual_range(visual_width * pix,visual_height * pix)

    self.aoi = aoi
    self.map = map
    self.pix_width = pix_width
    self.pix_height = pix_height

    self.entity_count = {} -- 场景中各种实体的数量，在这里统计
    for _,et in pairs(ET) do self.entity_count[et] = 0 end
end

-- 该坐标能否通过
function Scene:can_pass(x,y,is_pix)
    return self.map:get_pass_cost(x,y,is_pix) >= 0
end

-- 广播实体出现
function Scene:broadcast_entity_appear(entity,eid_list,way)
    if eid_list.n <= 0 then return end


    local pid_list = {}
    local is_player = (ET.PLAYER == entity.et)

    for idx = 1,eid_list.n do
        local tmp_entity = g_entity_mgr:get_entity( eid_list[idx] )

        -- 如果在我周围的是一个玩家，告诉他我出现了
        if ET.PLAYER == tmp_entity.et then
            table.insert(pid_list,tmp_entity.pid)
        end

        -- 告诉我周围出现了这个实体(玩家或怪物、npc)
        if is_player then
            local other_pkt = tmp_entity:appear_pkt()
            entity:send_pkt(ENTITY.APPEAR,other_pkt)
        end
    end

    -- 告诉我周围的玩家，我出现了
    if not table.empty(pid_list) then
        -- 1表示底层按玩家pid广播
        local my_pkt = entity:appear_pkt()
        g_network_mgr:clt_multicast( 1,pid_list,ENTITY.APPEAR,my_pkt )
    end
end

-- 发送实体进入场景
local scene_pkt = {}
function Scene:send_enter_scene(entity,pix_x,pix_y)
    if ET.PLAYER ~= entity.et then return end

    scene_pkt.dungeon_id = self.dungeon_id
    scene_pkt.dungeon_hdl = self.dungeon_hdl
    scene_pkt.scene_id = self.id
    scene_pkt.pix_x = pix_x
    scene_pkt.pix_y = pix_y

    return entity:send_pkt(ENTITY.ENTERSCENE,scene_pkt)
end

-- 实体进入场景
function Scene:entity_enter(entity,pix_x,pix_y)
    if pix_x >= self.pix_width or pix_y >= self.pix_height then
        ERROR("cene:entity_enter invalid pos(%d, %d) for scene %d",
            pix_x, pix_y, self.id)
        return
    end

    -- 先退出旧场景
    local old_scene = entity:get_scene()
    if old_scene then
        ASSERT(old_scene ~= self,self.dungeon_id,self.id)
        old_scene:entity_exit(entity)
    end

    local et = entity.et
    self.aoi:enter_entity(entity.eid,pix_x,pix_y,et,tmp_list)

    self.entity_count[et] = 1 + self.entity_count[et]

    entity:set_pos(self.dungeon_hdl,self.dungeon_id,self.id,pix_x,pix_y)
    self:send_enter_scene(entity,pix_x,pix_y)

    return self:broadcast_entity_appear(entity,tmp_list)
end

-- 广播实体消失
-- @eid_list:eid列表，包含n字段表示数量
local exit_pkt = {}
function Scene:broadcast_entity_disappear(entity,eid_list,way)
    if eid_list.n <= 0 then return end

    -- 广播给周边的玩家该实体消失了
    local pid_list = {}
    local g_entity_mgr = g_entity_mgr

    for idx = 1,eid_list.n do
        local tmp_entity = g_entity_mgr:get_entity( eid_list[idx] )

        assert( ET.PLAYER == tmp_entity.et and tmp_entity.pid )
        table.insert( pid_list,tmp_entity.pid )
    end

    exit_pkt.way = 0 -- 如何退出
    exit_pkt.handle = entity.eid
    -- 1表示底层按玩家pid广播
    return g_network_mgr:clt_multicast(1,pid_list,ENTITY.DISAPPEAR,exit_pkt)
end

-- 广播数据给关注我的玩家
-- @to_me:是否也广播给自己
function Scene:broadcast_to_watch_me(entity,cmd,pkt,to_me)
    -- 广播给玩家我开始移动
    self.aoi:get_interest_me_entity(entity.eid,tmp_list)

    local pid_list = {}

    -- 如果自己是玩家，那么移动消息也发给自己
    if to_me and ET.PLAYER == entity.et then
        table.insert(pid_list,entity.pid)
    end

    for idx = 1,tmp_list.n do
        local tmp_entity = g_entity_mgr:get_entity( tmp_list[idx] )

        -- 如果在我周围的是一个玩家，告诉他我出现了
        if ET.PLAYER == tmp_entity.et then
            table.insert(pid_list,tmp_entity.pid)
        end
    end

    if not table.empty(pid_list) then
        -- 1表示底层按玩家pid广播
        g_network_mgr:clt_multicast( 1,pid_list,cmd,pkt )
    end
end

-- 实体退出场景
function Scene:entity_exit(entity)
    -- tmp_list只返回关注entity的实体列表，目前只有玩家列表
    -- TODO:aoi找不到这个实体会报错，这个要不要改成返回值
    self.aoi:exit_entity(entity.eid,tmp_list)

    local et = entity.et
    self.entity_count[et] = self.entity_count[et] - 1
    assert(self.entity_count[et] >= 0,"scene entity count fail")

    return self:broadcast_entity_disappear(entity,tmp_list)
end

-- 获取场景中的实体数据 移动
function Scene:get_entity_count( entity_type )
    return self.entity_count[entity_type]
end

-- 获取场景中的实体
-- 请不要保存或者修改返回的table
function Scene:get_entity( entity_type )
    self.aoi:get_all_entity(entity_type,tmp_list)

    return tmp_list
end

return Scene
