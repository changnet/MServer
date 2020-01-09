-- BucketRank
-- auto export by engine_api.lua do NOT modify!

-- 桶排序
BucketRank = {}

-- 更新元素排序因子
-- @param id 元素id
-- @param factor 排序因子
-- @param index 可选参数，排序因子索引从1开始，不传则为第一个
-- @return 是否成功
function BucketRank:insert(id, factor, index)
end

-- 获取排行榜前N个元素id
-- @param n 前N个元素
-- @param tbl 获取的元素id存放在该表中，数量在n字段
function BucketRank:get_top_n(n, tbl)
end

-- 重置排行榜，但不释放内存
function BucketRank:clear()
end

-- 获取排行榜数量
function BucketRank:get_count()
end

