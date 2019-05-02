-- 排行榜算法测试

local Bucket_rank = require "util.bucket_rank"
local Insertion_rank = require "util.insertion_rank"
-- local Insertion_rank = require "Insertion_rank"

local function dump(rank)
    local count = rank:get_count()
    for idx = 1,count do
        local id = rank:get_id_by_rank(idx)
        local ft1,ft2,ft3 = rank:get_factor(id)
        PRINT(idx,id,ft1,ft2,ft3)
    end
end

local irank = Insertion_rank()


irank:insert(1,999,999,999)
irank:update(1,9999,3)
irank:insert(2,999,998,999)
irank:insert(3,991,998,999)

irank:update(2,1000)
irank:remove(1)
irank:update(3,992)

dump(irank)

irank:clear()

-- 随机性能测试
f_tm_start()

local max_object = 1000
local max_random = 200000000

irank:set_max_count(96)

for idx = 1,max_object do
    local ft1 = math.random(1,max_random)
    local ft2 = math.random(1,max_random)
    local ft3 = math.random(1,max_random)

    local object = irank:insert(idx,ft1,ft2,ft3)
    object.name = "ee[ee\"hh"
    object.level = idx
end

for idx = 1,max_object do
    local idx = math.random(1,3)
    local ft1 = math.random(1,max_random)

    irank:update(idx,ft1,idx)
end

for idx = 1,max_object/2 do
    irank:remove(math.random(1,max_object))
end

f_tm_stop("rank cost") -- 1000 max_object,rank cost	5789	microsecond

dump(irank)

irank:save("runtime/rank/test.rank")

irank:clear()
irank:load("runtime/rank/test.rank")
dump(irank)
-- vd(irank.object)


local random_f = {}
for idx = 1,1000000 do
    table.insert(random_f,math.random(1,100))
end

irank:clear()
brank = Bucket_rank()

f_tm_start()
for idx = 1,#random_f do
    irank:insert(idx,random_f[idx])
end
f_tm_stop("insert rank cost")

f_tm_start()
for idx = 1,#random_f do
    brank:insert(idx,random_f[idx])
end
f_tm_stop("bucket rank cost")
