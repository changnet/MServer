-- entity_mgr.lua
-- xzc
-- 2018-11-20

-- 实体管理
EntityMgr = {}

local ET = require "modules.entity.entity_header"
local TimeId = require "modules.system.time_id"

local Entity_npc = require "modules.entity.entity_npc"
local EntityPlayer = require "modules.entity.entity_player"
local Entity_monster = require "modules.entity.entity_monster"

local this = global_storage("EntityMgr", nil, function(storage)
    storage.entity = {} -- 以实体唯一id为key
    storage.entity_player = {} -- 以玩家id为key
    storage.id_generator = TimeId()
end)

-- 根据实体Id获取实体
function EntityMgr.get_entity(id)
    return this.entity[id]
end

-- 根据玩家Id获取玩家实体
function EntityMgr.get_player(pid)
    return this.entity_player[pid]
end

-- 创建实体
function EntityMgr.new_entity(et, ...)
    local entity = nil
    local eid = this.id_generator:next_id()

    assert(nil == this.entity[eid], "duplication entity id")

    if ET.NPC == et then
        entity = Entity_npc(eid, ...)
    elseif ET.PLAYER == et then
        entity = EntityPlayer(eid, ...)
        this.entity_player[entity.pid] = entity
    elseif ET.MONSTER == et then
        entity = Entity_monster(eid, ...)
    else
        assert(false, string.format("unknow entity type:%d", et))
    end

    this.entity[eid] = entity
    return entity
end

-- 删除实体
function EntityMgr.del_entity(eid)
    local entity = this.entity[eid]
    if not entity then return elog("del entity fail,no such entity") end

    this.entity[eid] = nil
    if ET.PLAYER == entity.et then this.entity_player[entity.pid] = nil end
end

-- 删除玩家实体
function EntityMgr.del_entity_player(pid)
    local entity = this.entity_player[pid]
    if not entity then return elog("del entity player fail,no such entity") end

    this.entity_player[pid] = nil
    this.entity[entity.eid] = nil
end

-- 事件主循环
local function routine(ms_now)
    for _, entity in pairs(this.entity) do entity:routine(ms_now) end
end

g_app:reg_ev(routine)

return EntityMgr
