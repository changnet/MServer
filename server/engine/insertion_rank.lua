-- InsertionRank
-- auto export by engine_api.lua do NOT modify!

-- 插入法排序
InsertionRank = {}

-- 重置排行榜，但不释放内存
function InsertionRank:clear()
end

-- 删除某个元素
-- @param id 元素Id
-- @return 是否成功
function InsertionRank:remove(id)
end

-- 排入一个排序元素，如果失败，则抛出错误
-- @param id 元素id
-- @param factor 排序因子
-- @param ... 其他排序因子
function InsertionRank:insert(id, factor, ...)
end

-- 更新元素排序因子
-- @param id 元素id
-- @param factor 排序因子
-- @param index 可选参数，排序因子索引从1开始，不传则为第一个
-- @return 是否成功
function InsertionRank:update(id, factor, index)
end

-- 获取排行榜数量
function InsertionRank:get_count()
end

-- 设置排行榜最大数量，默认64
-- 排名在最大数量之后的元素将被丢弃并且排行榜数量减少后也不会回到排行榜
-- @param max_count 最大数量
function InsertionRank:set_max_count(max_count)
end

-- 获取当前排行榜用到的排序因子最大索引，从1开始
function InsertionRank:get_max_factor()
end

-- 获取某个元素的排序因子
-- @param id 元素id
-- @return 排序因子，排序因子1，...
function InsertionRank:get_factor(id)
end

-- 根据元素id获取排名
-- @param id 元素id
-- @return 返回排名(从1开始)，没有排名则返回-1
function InsertionRank:get_rank_by_id(id)
end

-- 根据排名获取元素id
-- @param rank 排名，从1开始
-- @return 元素id，无则返回-1
function InsertionRank:get_id_by_rank(rank)
end

