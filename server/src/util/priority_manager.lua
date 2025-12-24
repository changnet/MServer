-- 优先级管理器
local PriorityManager = oo.class("PriorityManager")

--[[
    每个优先级一组，前一个优先级的完成后才进行下一个优先级
]]

-- 构造函数，请勿手动调用
function PriorityManager:__init()
    self.sz = 0
    self.idx = 0
    self.map = {}
    self.list = {}
end

-- 获取元素的数量
function PriorityManager:size()
    return self.sz
end

-- 清空所有元素
function PriorityManager:clear()
    self.list = {}
end

-- 添加一个元素
--@param value 需要添加的任意数值
--@param pr 优先级，默认20。越小优先级越高
function PriorityManager:push(value, pr)
    if not pr then pr = 20 end

    local pr_list = self.map[pr]
    if not pr_list then
        table.insert(self.list, pr)
        table.sort(self.list)

        pr_list = {}
        self.map[pr] = pr_list
    end

    self.sz = self.sz + 1
    table.insert(pr_list, value)
end

-- 删除一个元素（效率较低）
function PriorityManager:remove(value)
    for _, pr_list in pairs(self.map) do
        for i, v in ipairs(pr_list) do
            if v == value then
                table.remove(pr_list, i)
                self.sz = self.sz - 1
                return true
            end
        end
    end

    return false
end

-- 获取当前正在执行的优先级
function PriorityManager:current()
    local pr = self.list[ self.idx ]
    return self.map[pr]
end

-- 切换到下一个优先级
function PriorityManager:next()
    local idx = self.idx + 1

    local pr = self.list[idx]
    if not pr then return nil end

    self.idx = idx
    return self.map[pr]
end

return PriorityManager
