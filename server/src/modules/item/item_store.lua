-- 负责道具数据管理的基类
local ItemStore = oo.class("ItemStore")

--@param pid 角色id
--@param id number 背包id
function ItemStore:__init(pid, id, max_size)
    self.pid = pid
    self.id = id
    self.max_size = max_size
end

-- 从数据库加载数据
--@param is_new boolean 是否新号，新号不从数据库加载数据
--@return boolean
function ItemStore:load(is_new)
    if is_new then
        self.list = {}
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
    -- TODO 其他道具属性，比如 强化等级、星级 等
    return {
        grid = grid,
        uuid = item_obj.uuid,
        id = item_obj.id,
        count = item_obj.count,
    }
end

-- 发送整个背包的数据给前端
function ItemStore:send_info(player)
    local items = {}
    for grid, item_obj in pairs(self.list) do
        table.insert(items, self:pack_item(item_obj, grid))
    end
    NetMsg.send(player, M.BagInfo, {id = self.id, items = items})
end

-- 记录道具变动
--@param item_obj ItemObj
--@param op OP 日志操作id
--@param str string 额外的日志数据
function ItemStore:log(item_obj, op, str)
end

return ItemStore
