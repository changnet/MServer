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
    self.bid = bid
    self.size = 0
    self.max_size = max_size
end

-- 从数据库加载数据
--@param is_new boolean 是否新号，新号不从数据库加载数据
--@return boolean
function ItemStore:load(is_new)
    if is_new then
        self.list = {}
        self.grid = {}
        self.list = {}
        self.ihash = {}
        return true
    end

    return true
end

-- 保存数据到数据库
function ItemStore:save()
    if not self:modify() then return true end

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
end

-- 删除某个道具
function ItemStore:dec(item_obj, op, log_ext)
end

-- 删除某个道具指定的数量
function ItemStore:dec_count(item_obj, change_num, op, log_ext)
end

-- 获取某个id的道具对象列表
--@return ItemObj[]|nil 如果不存在则返回nil
function ItemStore:get_id_objs(id)
    return self.ihash[id]
end

return ItemStore
