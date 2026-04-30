-- 负责道具数据管理的基类
local ItemStore = oo.class("ItemStore")

--@class ItemObj 单个道具对象
--@field id integer 道具配置id
--@field uid string 唯一id
--@field num integer 数量
--@field attr table<integer,integer> kv属性

--@class ItemStore 道具数据管理
--@field id integer 背包id
--@field pid integer 角色id
--@field max_size integer 背包最大格子数
--@field size integer 当前已使用格子数
--@field grid table<integer, ItemOjb> [格子索引]=道具，仅旧的固定格子背包才有
--@field list table<string, ItemOjb> [uid]=道具
--@field ihash table<integer, ItemOjb[]> [道具id]=道具列表，仅背包启用，装备不用

local log_tbl = {}

--@param pid 角色id
--@param bid number 背包id
function ItemStore:__init(pid, bid, max_size)
    self.pid = pid
    self.id = bid
    self.size = 0
    self.max_size = max_size
end

-- 从数据库加载数据
--@param is_new boolean 是否新号，新号不从数据库加载数据
--@return boolean
function ItemStore:load(player, is_new)
    if is_new then
        self.list = {}
        if self.is_hash then self.ihash = {} end
        return true
    end

    local e, rows = Call[DATA_ADDR].DataCache.get(
        "player_item", {"pid", player.pid, "bid", self.id})

    if 0 ~= e then
        perror(player, "load item failed", e, self.id)
        return false
    end

    local size = 0
    local list = {}
    if self.is_hash then
        local ihash = {}
        for _, obj in pairs(rows[1].list) do
            size = size + 1
            list[obj.uid] = obj

            local id = obj.id
            local hash = ihash[id]
            if not hash then
                hash = {}
                ihash[id] = hash
            end
            table.insert(hash, obj)
        end
        self.ihash = ihash
    else
        for _, obj in pairs(rows[1].list) do
            size = size + 1
            list[obj.uid] = obj
        end
    end

    self.list = list
    self.size = size
    table.set_array(list, 1)

    return true
end

-- 保存数据到数据库
function ItemStore:save(player)
    if not self:modify() then return true end

    Send[DATA_ADDR].DataCache.update("player_item",
        {"pid", player.pid, "bid", self.id},
        {pid = player.pid, bid = self.id, list = self.list}
    )

    self:set_modify(false)
    return true
end

-- 数据是否有变动，有才需要存库
function ItemStore:modify()
    -- 默认不需要标记，全部存库
    -- 如果是仓库这种不经常变化的，可以手动标记
    return true
end

-- 设置变动标记
function ItemStore:set_modify(flag)
end

-- 打包单个道具数据
function ItemStore:pack_item(item_obj, grid)
    local attr = item_obj.attr
    if not attr then return item_obj end

    -- TODO 其他道具属性，比如 强化等级、星级 等，这里要加个判断是否前端属性
    local clt_attr = {}
    for k, v in pairs(attr) do
        table.insert(clt_attr, {k = k, v = v})
    end

    return {
        grid = grid,
        uid = item_obj.uid,
        id = item_obj.id,
        num = item_obj.num,
        attr = clt_attr,
    }
end

-- 发送整个背包的数据给前端
function ItemStore:send_info(player)
    local items = {}
    for grid, item_obj in pairs(self.list) do
        table.insert(items, self:pack_item(item_obj, grid))
    end
    NetMsg.send(player, M.BagInfo, {bid = self.id, items = items})
end

-- 记录道具变动
--@param change_num integer 变动的数量，负数表示扣除
--@param item_obj ItemObj
--@param op OP 日志操作id
--@param ext string 额外的日志数据
function ItemStore:log(item_obj, change_num, op, ext)
    log_tbl.id = item_obj.id
    log_tbl.uid = item_obj.uid
    log_tbl.pid = self.pid
    log_tbl.num = item_obj.num
    log_tbl.change_num = change_num
    log_tbl.op = op
    log_tbl.ext = ext
    log_tbl.bid = self.id

    Log.db("item", log_tbl)
end

-- 添加某个道具
function ItemStore:add(item_obj, op, log_ext)
    if not item_obj.uid then
        item_obj.uid = BagMgr.next_id()
    end

    self.list[item_obj.uid] = item_obj
    self.size = (self.size or 0) + 1

    if self.is_hash then
        local id = item_obj.id
        if not self.ihash[id] then self.ihash[id] = {} end
        table.insert(self.ihash[id], item_obj)
    end

    self:set_modify(true)
    self:log(item_obj, item_obj.num, op, log_ext)

    return item_obj.num
end

-- 删除某个道具
function ItemStore:dec(item_obj, op, log_ext)
    local uid = item_obj
    if not self.list[uid] then
        eprint("item dec obj not in store", uid, item_obj.id, op)
        return 0
    end

    local removed = item_obj.num or 0

    self.list[uid] = nil
    self.size = self.size - 1

    if self.is_ihash then
        local id = item_obj.id
        local arr = self.ihash[id]

        for i, obj in ipairs(arr) do
            if obj.uid == item_obj.uid then
                table.remove(arr, i)
                break
            end
        end
        if #arr == 0 then self.ihash[id] = nil end
    end

    self:set_modify(true)
    self:log(item_obj, -removed, op, log_ext)

    return removed
end

-- 删除某个道具指定的数量
function ItemStore:dec_count(item_obj, change_num, op, log_ext)
    if change_num >= item_obj.num then
        return self:dec(item_obj, op, log_ext)
    end

    item_obj.num = item_obj.num - change_num
    self:set_modify(true)
    self:log(item_obj, -change_num, op, log_ext)

    return change_num
end

-- 获取某个id的道具对象列表
--@return ItemObj[]|nil 如果不存在则返回nil
function ItemStore:get_id_objs(id)
    return self.ihash[id]
end

return ItemStore
