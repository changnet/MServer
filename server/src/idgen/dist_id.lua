-- distubuted id generator
-- xzc

-- 生成一个分布式的唯一id，字符串格式
DistId = oo.class("DistId")

--[[
如果是集群，节点数量有限，还可以将就把addr和自增拼在一起限制在64位内形成一个整数

但如果是小服模式，有多节点同时还要考虑跨服，想要id唯一就只能用字符串了

服务器id（20位） + 节点addr（23位） + 时间戳（32位） + 自增序列（16位）
]]

function DistId:__init()
end

-- 生成一个唯一id
function DistId:next_id()
end

return DistId
