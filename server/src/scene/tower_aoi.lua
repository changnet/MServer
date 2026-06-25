-- 灯塔aoi
local TowerAoi = oo.class("TowerAoi")

--[[
1. 坐标允许有负数。有负数时，需要指定一个offset，把负数转换为正数

2. 每个灯塔管理一个正方形范围，而不是圆形。灯塔的坐标以坐标原点为起点（即整个正方形中最小的坐标）
    视野不是正方形时，以最长的边为准即可

3. 灯塔的范围 = 管理范围 + 视野范围
    管理范围指纯粹以地图划分的范围，它决定地图上灯塔的数量
    视野范围指玩家在灯塔管理范围边缘时，能看到灯塔外的范围，因此需要那视野的这部分实体也归到灯塔管理中来

4. 理论上，灯塔的管理范围只需要>=1即可。但过小的管理范围会导致一个地图中灯塔数量太多，增删实体时需要遍历
    大量的灯塔，浪费性能。但过大的管理范围会导致一个灯塔管理的实体数量较多，遍历单个灯塔时浪费性能。

    灯塔的数量要根据地图的大小，实体的密集程序来调整。如果实体数量较少，一张地图一个灯塔都行。
    一般情况下，单个灯塔的管理范围应该>=视野范围，这样每次只遍历一个灯塔就行

5. 灯塔适合一些视野很大，不频繁移动（即不频繁进入、退出灯塔）的玩法，比如slg
]]

local assert = assert
local math_ceil = math.ceil

local version = -1 -- 实体去重版本号
local max_int = math.maxinteger

function TowerAoi:__init(tower_size, max_xy, view, offset)
    self.tower_size = tower_size -- 灯塔的管理范围大小
    self.max_xy = max_xy -- 地图的最大边长（以xy中最大值算，有负数则按绝对值算）
    self.view = view -- 视野大小（直径）
    self.offset = offset or 0 -- 地图坐标有负数时，需要偏移的量。>=整个地图最小值
    self.towers = {} -- [灯塔id] = 灯塔
    self.objects = {} -- [obj.uid] = object
    self.object_count = 0 -- 实体数量
    self.version = 0 -- 实体去重版本号
end

local function make_offset(self, x, y)
    local offset = self.offset

    x = offset + x
    y = offset + y

    assert(x >= 0 and y >= 0)
    return x, y
end

-- 遍历该坐标所涉及的灯塔
-- view为视野直径，内部转换为半径计算范围
local function iter_tower(self, view, x, y, func, ...)
    x, y = make_offset(self, x, y)
    local tower_size = self.tower_size
    local radius = math_ceil(view / 2) -- 视野半径

    -- 计算视野范围

    -- 左下角（笛卡尔坐标系），如果是屏幕坐标系则为左上角
    local min_x = x - radius
    local min_y = y - radius

    -- 右上角（笛卡尔坐标系），如果是屏幕坐标系则为右下角
    local max_x = x + radius
    local max_y = y + radius

    -- 对齐到灯塔边界（向下对齐min，向上对齐max）
    if min_x < 0 then min_x = 0 end
    if min_y < 0 then min_y = 0 end
    min_x = min_x // tower_size * tower_size
    min_y = min_y // tower_size * tower_size
    max_x = (max_x + tower_size - 1) // tower_size * tower_size
    max_y = (max_y + tower_size - 1) // tower_size * tower_size

    -- 不按九宫格的方式遍历，而是直接按范围遍历
    -- 这种方式允许灯塔管理范围比视野范围还小，但最好别这么做，性能很差
    local max_xy = self.max_xy
    for tx = min_x, max_x, tower_size do
        for ty = min_y, max_y, tower_size do
            local tid = ty * max_xy + tx -- tower id
            func(self, tid, ...)
        end
    end
end

local function add_obj_to_tower(self, tid, obj)
    local tower = self.towers[tid]
    if not tower then
        tower = {}
        self.towers[tid] = tower
    end

    tower[obj.uid] = obj
end

-- 添加实体
function TowerAoi:add(obj)
    obj.__v = nil
    self.objects[obj.uid] = obj
    self.object_count = self.object_count + 1
    return iter_tower(self, self.view, obj.x, obj.y, add_obj_to_tower, obj)
end

local function del_obj_from_tower(self, tid, uid)
    local tower = self.towers[tid]
    if not tower then return end

    tower[uid] = nil
end

-- 删除实体
function TowerAoi:del(obj)
    obj.__v = nil

    self.objects[obj.uid] = nil
    self.object_count = self.object_count - 1
    return iter_tower(self, self.view, obj.x, obj.y, del_obj_from_tower, obj.uid)
end

-- 获取指定坐标所在的灯塔
-- @return 该灯塔不存在（或者未创建）返回nil
function TowerAoi:get_tower(x, y)
    x, y = make_offset(self, x, y)
    local tower_size = self.tower_size

    local tx = x // tower_size * tower_size -- 对齐到灯塔坐标
    local ty = y // tower_size * tower_size

    local tid = ty * self.max_xy + tx

    return self.towers[tid]
end

local function iter_tower_object(tower, min_x, min_y, max_x, max_y, func, ...)
    for _, obj in pairs(tower) do
        -- 同一个obj会被多个tower管理，遍历时要去重。用table去掉太慢
        if obj.__v ~= version then
            obj.__v = version
            local x, y = obj.x, obj.y
            if x >= min_x and x <= max_x and y >= min_y and y <= max_y then
                func(obj, ...)
            end
        end
    end
end

local function iter_tid_object(self, tid, min_x, min_y, max_x, max_y, func, ...)
    local tower = self.towers[tid]
    if tower then
        iter_tower_object(tower, min_x, min_y, max_x, max_y, func, ...)
    end
end

local function setup_version(self)
    version = self.version + 1
    if version == max_int then
        version = 1
        for _, obj in pairs(self.objects) do obj.__v = nil end
    end
    self.version = version
end

-- 遍历指定坐标，指定视野范围内的所有实体
function TowerAoi:iter_object(x, y, view, func, ...)
    if not view then view = self.view end
    local radius = view / 2 -- 视野半径

    setup_version(self)

    local min_x = x - radius
    local min_y = y - radius
    local max_x = x + radius
    local max_y = y + radius
    if view < self.view then
        -- 只遍历单个灯塔
        local tower = self:get_tower(x, y)
        if tower then
            iter_tower_object(tower, min_x, min_y, max_x, max_y, func, ...)
        end
    else
        -- 视野超出单个灯塔，得遍历多个
        iter_tower(self, view, x, y, iter_tid_object,
            min_x, min_y, max_x, max_y, func, ...)
    end
end

local function collect_obj(obj, tbl, self_id)
    if obj.uid ~= self_id then
        local n = tbl.n + 1
        tbl[n] = obj.uid
        tbl.n = n
    end
end

-- 获取某个实体视野范围内的实体(不包含自己)
-- @param id 实体uid
-- @param view 视野大小（直径），nil则使用默认视野
-- @param tbl 存放结果的table，需要预先创建，结果数量存放在tbl.n
function TowerAoi:get_visual_entity(id, view, tbl)
    tbl.n = 0
    local obj = self.objects[id]
    if not obj then return end

    return self:iter_object(obj.x, obj.y, view or self.view, collect_obj, tbl, id)
end

-- 获取所有实体(这个仅是为了兼容C++接口，如果不需要兼容直接用self.objects)
-- @param tbl 存放结果的table，结果数量存放在tbl.n
function TowerAoi:get_all_entity(tbl)
    local n = 0
    for _, obj in pairs(self.objects) do
        n = n + 1
        tbl[n] = obj.uid
    end
    tbl.n = n
end

return TowerAoi
