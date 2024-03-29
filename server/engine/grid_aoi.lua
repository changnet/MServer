-- GridAoi
-- auto export by engine_api.lua do NOT modify!

-- AOI算法，目前用格子地图实现
local GridAoi = {}

-- 设置地图大小
-- @param width 地图像素宽度
-- @param height 地图像素高度
-- @param pix_grid 单个格子边长(像素)
function GridAoi:set_size(width, height, pix_grid)
end

-- 设置实体视野大小，目前所有实体视野一样
-- @param width 视野宽度(像素)
-- @param height 视野高度(像素)
function GridAoi:set_visual_range(width, height)
end

-- 获取所有实体
-- @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
-- @param tbl 获取到的实体存放在此table中，数量设置在n字段
function GridAoi:get_all_entity(mask, tbl)
end

-- 获取周围关注自己的实体列表,常用于自己释放技能、扣血、特效等广播给周围的人
-- @param id 自己的唯一实体id
-- @param tbl 获取到的实体存放在此table中，数量设置在n字段
function GridAoi:get_interest_me_entity(id, tbl)
end

-- 获取某一范围内实体
-- 底层这里只支持矩形，如果是其他形状的，上层根据实体位置再筛选即可
-- @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
-- @param tbl 获取到的实体存放在此table中，数量设置在n字段
-- @param src_x 矩形左上角x坐标
-- @param src_y 矩形左上角y坐标
-- @param dst_x 矩形右下角x坐标
-- @param dst_y 矩形右下角y坐标
function GridAoi:get_entity(mask, tbl, src_x, src_y, dst_x, dst_y)
end

-- 获取某个实体视野范围内的实体
-- @param id 实体id
-- @param mask 掩码，根据mask获取特定类型的实体(0xF表示所有)
-- @param tbl 获取到的实体存放在此table中，数量设置在n字段
function GridAoi:get_visual_entity(id, mask, tbl)
end

-- 实体退出场景
-- @param id 唯一实体id
-- @param tbl [optional]interest_me存放在此table中，数量设置在n字段
function GridAoi:exit_entity(id, tbl)
end

-- 实体进入场景
-- @param id 唯一实体id
-- @param x 实体像素坐标x
-- @param y 实体像素坐标y
-- @param mask 实体掩码
-- @param tbl [optional]视野内新增实体存放在此table中，数量设置在n字段
function GridAoi:enter_entity(id, x, y, mask, tbl)
end

-- 实体更新，通常是位置变更
-- @param id 唯一实体id
-- @param x 实体像素坐标x
-- @param y 实体像素坐标y
-- @param into [optional]视野内新增实体存放在此table中，数量设置在n字段
-- @param out [optional]视野内消失实体存放在此table中，数量设置在n字段
function GridAoi:update_entity(id, x, y, into, out)
end

-- 判断两个像素坐标是否在同一个格子中
-- @param src_x 源位置x像素坐标
-- @param src_y 源位置y像素坐标
-- @param dst_x 目标位置x像素坐标
-- @param dst_y 目标位置y像素坐标
function GridAoi:is_same_pos(src_x, src_y, dst_x, dst_y)
end

return GridAoi
