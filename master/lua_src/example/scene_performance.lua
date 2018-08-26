-- 场景相关性能测试

local map_mgr = map_mgr

local id = 9999
local width = 64
local height = 64

if not map_mgr:create_map(id,width,height) then
    PLOG("create map error")
    return
end

--[[

00: 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
01: 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
03: 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
03: 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
04: 0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
05: 0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
06: 0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
07: 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
08: 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
09: 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
10: 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,



]]

-- 地图的坐标原点在左上角
-- cost为负数表示不可行走，正数数值表示行走的难度，值越大，行走消耗越大，寻路优先级越低[1,127]
local grid = 
{
    { y = 1,cost = 1,x_min = 0,x_max = 31},
    { y = 2,cost = 1,x_min = 27,x_max = 27},
    { y = 3,cost = 1,x_min = 27,x_max = 27},
    { y = 4,cost = 1,x_min = 9,x_max = 27},
    { y = 5,cost = 1,x_min = 9,x_max = 26},
    { y = 6,cost = 1,x_min = 9,x_max = 26},
}

for _,range in pairs( grid ) do
    local y = range.y
    local cost = range.cost
    for x = range.x_min,range.x_max do
        if not map_mgr:fill_map(id,x,y,cost) then
            PLOG("fill map error")
        end
    end
end

local path = {} -- 路径

-- @dp: dump path
function test_path(id,x,y,dx,dy,path,dp)
    local cnt = map_mgr:find_path(id,x,y,dx,dy,path)
    if not cnt or cnt <= 0 then
        PFLOG("can not find path from (%d,%d) to (%d,%d)",x,y,dx,dy)
        return
    end

    if not dp then return end

    local path_set = {}
    for idx = 1,path.n,2 do
        local x = path[idx]
        local y = path[idx + 1]

        if not path_set[y] then path_set[y] = {} end
        path_set[y][x] = 1
    end

    local path_tbl = {}
    -- 把路径打印出来
    for cy = 0,height - 1 do
        for cx = 0,width - 1 do
            local val = 0
            if path_set[cy] then val = path_set[cy][cx] or 0 end
            table.insert(path_tbl,val)
        end

        table.insert(path_tbl,"\n");
    end

    vd(path)
    print(table.concat(path_tbl))
end

--test_path(id,1,1,1,1,path,false)
test_path(id,1,1,2,1,path,true)
test_path(id,0,1,10,1,path,false)

test_path(id,0,1,9,4,path,false)