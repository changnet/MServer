-- entity.lua
-- xzc
-- 2019-01-01
-- 实体基类
-- 由于实体在场景中使用比较频繁，一开始不想用继承的，不过考虑下还是用了。
-- 到时测试得到结果后再做处理
local Entity = oo.class(...)

-- @eid:实体唯一id
-- @et :实体类型
function Entity:__init(eid, et)
    self.eid = eid -- 实体唯一id
    self.et = et
end

-- 设置位置信息
function Entity:set_pos(dungeon_hdl, dungeon_id, scene_id, pix_x, pix_y)
    self.dungeon_hdl = dungeon_hdl
    self.dungeon_id = dungeon_id
    self.scene_id = scene_id
    self.pix_x = pix_x
    self.pix_y = pix_y
end

-- 获取所有副本对象
function Entity:get_dungeon()
    return g_dungeon_mgr:get_dungeon(self.dungeon_hdl)
end

-- 获取所在场景对象
function Entity:get_scene()
    local dungeon = g_dungeon_mgr:get_dungeon(self.dungeon_hdl)
    if not dungeon then return nil end

    return dungeon.scene[self.scene_id]
end

-- 创建实体出现的数据包
-- @pkt：不要频繁创建table，必须由上层传进入，因为不同的实体使用的数据包格式不一样
-- 由基类缓存是无法覆盖所有字段的
function Entity:appear_pkt(pkt)
    pkt.handle = self.eid
    pkt.way = 0
    pkt.type = self.et
    pkt.pix_x = self.pix_x or 0
    pkt.pix_y = self.pix_y or 0
    pkt.name = self.name
end

-- 退出场景
function Entity:exit_scene()
    local dungeon = g_dungeon_mgr:get_dungeon(self.dungeon_hdl)
    if not dungeon then
        eprint("Entity::exit_scene not in scene:%s", self.name)
        return
    end

    dungeon:exit(self, self.scene_id)
end

return Entity
