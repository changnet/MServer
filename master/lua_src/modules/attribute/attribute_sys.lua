-- attribute_sys.lua
-- xzc
-- 2018-12-02

-- 玩家属性系统

require "modules.attribute.attribute_header"
local Attribute = require "modules.attribute.attribute"

local ABTSYS = ABTSYS
local Attribute_sys = oo.class( ... )

function Attribute_sys:__init( pid )
    self.pid = pid
    self.sys = {} -- attribute_header.lua中定义的系统id为key

    -- 各个系统的属性，最终会收集到这个集合里
    self.final_abt = Attribute()
end

-- 获取某个系统的属性集
function Attribute_sys:raw_get_sys(id)
    return selef.sys[id]
end

-- 获取某个系统的属性集(不存在则创建)
function Attribute_sys:get_sys(id)
    if id <= ABTSYS.NONE or id >= ABTSYS.MAX then return nil end

    if not self.sys[id] then self.sys[id] = Attribute() end

    return self.sys[id]
end

-- 获取最终属性
function Attribute_sys:get_final_attr()
    return self.final_abt.attribute
end

-- 获取一个最终属性
function Attribute_sys:get_one_final_attr(id)
    return self.final_abt.attribute[id]
end

-- 添加属性到某个系统
function Attribute_sys:add_sys_abt( id,abt_list )
    local sys = self:get_sys(id)
    return sys:push( abt_list )
end

-- 设置某个系统属性
function Attribute_sys:set_sys_abt( id,abt_list )
    local sys = self:get_sys(id)

    sys:clear()
    return sys:push( abt_list )
end

-- 标识是否变动
function Attribute_sys:set_modify(md)
    self.modify = md
end

-- 定时检测属性变动
function Attribute_sys:update_modify()
    if not self.modify then return end

    self.modify = false
    return self:calc_final_abt()
end

-- 计算总的属性
function Attribute_sys:calc_final_abt()
    self.final_abt:clear()

    for _,sys in pairs( self.sys ) do
        self.final_abt:merge( sys )
    end

    -- 最终总战力
    self.final_fv = self.final_abt:calc_fight_value()

    return self.final_fv
end

-- 把属性同步到场景
function Attribute_sys:update_battle_abt()
    -- TODO: 这里需要优化一下，看下是不是要改成protobuf同步
    local abt_list = {}
    for k,v in pairs( self.final_abt.attribute ) do
        table.insert(abt_list,k)
        table.insert(abt_list,v)
    end

    g_rpc:player_update_battle_abt( self.pid,abt_list )
end

return Attribute_sys
