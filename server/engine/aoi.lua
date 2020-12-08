-- Aoi
-- auto export by engine_api.lua do NOT modify!

-- AOI算法，目前用格子地图实现
Aoi = {}

-- 设置地图大小
-- @param width 地图像素宽度
-- @param height 地图像素高度
function Aoi:set_size(width, height)
end

-- 设置实体视野大小，目前所有实体视野一样
-- @param width 视野宽度(格子数)
-- @param height 视野高度(格子数)
function Aoi:set_visual_range(width, height)
end

-- 获取所有实体
-- @param mask 实体类型掩码，不同实体类型按位表示，只获取这些类型的实体
-- @param tbl 获取到的实体存放在此table中，数量设置在n字段
function Aoi:get_all_entity(mask, tbl)
end

-- 获取周围关注自己的实体列表,常用于自己释放技能、扣血、特效等广播给周围的人
-- @param id 自己的唯一实体id
-- @param tbl 获取到的实体存放在此table中，数量设置在n字段
-- @param mask 可选参数，实体类型掩码，不同实体类型按位表示，只获取这些类型的实体
-- @param event 可选参数，事件类型掩码，不同事件类型按位表示，只获取关注这些事件的实体
function Aoi:get_watch_me_entity(id, tbl, mask, event)
end

-- 获取某一范围内实体
-- 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
-- @param mask 实体类型掩码，不同实体类型按位表示，只获取这些类型的实体
-- @param tbl 获取到的实体存放在此table中，数量设置在n字段
-- @param src_x 矩形左上角x坐标
-- @param src_y 矩形左上角y坐标
-- @param dst_x 矩形右下角x坐标
-- @param dst_y 矩形右下角y坐标
function Aoi:get_entity(mask, tbl, src_x, src_y, dst_x, dst_y)
end

-- 实体退出场景
-- @param id 唯一实体id
-- @param tbl 可选参数，周围关注此实体退出事件的实体存放在此table中，数量设置在n字段
function Aoi:exit_entity(id, tbl)
end

-- 实体进入场景
-- @param id 唯一实体id
-- @param x 实体像素坐标x
-- @param y 实体像素坐标y
-- @param type 实体类型(怪物、npc等)
-- @param event 实体关注的事件(进入、退出场景等)
-- @param tbl 可选参数，周围关注此实体进入事件的实体存放在此table中，数量设置在n字段
function Aoi:enter_entity(id, x, y, type, event, tbl)
end

-- 实体更新，通常是位置变更
-- @param id 唯一实体id
-- @param x 实体像素坐标x
-- @param y 实体像素坐标y
-- @param tbl 可选参数，周围关注此实体更新事件的实体存放在此table中，数量设置在n字段
-- @param into 可选参数，周围关注此实体出现事件的实体存放在此table中，数量设置在n字段
-- @param out 可选参数，周围关注此实体消失事件的实体存放在此table中，数量设置在n字段
function Aoi:update_entity(id, x, y, tbl, into, out)
end

-- 判断两个像素坐标是否在同一个格子中
-- @param src_x 源位置x像素坐标
-- @param src_y 源位置y像素坐标
-- @param dst_x 目标位置x像素坐标
-- @param dst_y 目标位置y像素坐标
function Aoi:is_same_pos(src_x, src_y, dst_x, dst_y)
end

