-- 堆数据结构（即C++的priority_queue），通过不同的compare函数实现大小堆
local Heap = oo.class()

local function alloc_id(self)
    local id = self.next_id
    self.next_id = self.next_id + 1
    return id
end

-- 把指定位置的元素，移动到堆合适的位置
local function shift_up(comp, heap, i, e)
    -- 一直将当前元素和父节点对比，如果权重比父节点高，则交换
    local p = i // 2
    local e_value = e.value
    while (i > 1 and comp(e_value, heap[p].value)) do
        local tmp = heap[p]
        heap[i] = tmp
        tmp.index = i

        i = p
        p = i // 2
    end

    heap[i] = e
    e.index = i
end

local function shift_down(comp, heap, i, e)
    -- 左边子节点
    local c = 2 * i
    local count = #heap
    local e_value = e.value
    while (c <= count) do
        -- 将当前元素和左右子节点中权重大的那个比，如果当前元素权重小则交换
        local c_value = heap[c].value
        if c ~= count then
            local c1_value = heap[c + 1].value
            if comp(c1_value, c_value) then
                c = c + 1
                c_value = c1_value
            end
        end

        if comp(e_value, c_value) then break end

        local tmp = heap[c]
        heap[i] = tmp
        tmp.index = i

        i = c
        c = 2 * i
    end

    heap[i] = e
    e.index = i
end

-- 默认的权重对比函数，最大堆
local function default_comp(a, b)
    return a > b
end

-- 构造函数，请勿手动调用
function Heap:__init(comp)
    --[[
        用完全二叉树实现，用数组存储
        index从1开始时，节点的左子节点是 2*index，节点的右子节点是 2*index+1，
        每一个父节点权重大于（或等于）这个节点的子节点，但左右子节点的大小则不固定
    ]]
    self.heap = {}
    self.map = {}
    self.next_id = 1
    self.comp = comp or default_comp
end

-- 获取优先级最高的一个元素
function Heap:top()
    local e = self.heap[1]
    return e and e.value
end

-- 是否为空
function Heap:empty()
    return 0 == #self.heap
end

-- 获取元素的数量
function Heap:size()
    return #self.heap
end

-- 清空所有元素
function Heap:clear()
    self.heap = {}
    self.map = {}
end

-- 添加一个元素
--@param value 需要添加的数值
--@param return 唯一id，可根据该id从堆中查询、删除元素
function Heap:push(value)
    local id = alloc_id(self)
    local e = {
        id = id,
        index = 0,
        value = value,
    }

    local heap = self.heap
    local count = #heap + 1
    shift_up(self.comp, heap, count, e)

    self.map[id] = e
    return id
end

-- 弹出优先级最高的元素
-- @return 弹出的元素
function Heap:pop()
    local heap = self.heap
    local count = #heap
    if 0 == count then return end

    local v = heap[1]
    local last = heap[count]

    heap[count] = nil
    self.map[v.id] = nil

    -- 把最后一个元素交换到位置1，只是交换这个操作在shitft_down里操作
    -- 然后把这个元素移动到合适位置
    if count > 1 then shift_down(self.comp, heap, 1, last) end

    return v.value
end

-- 删除该元素
-- @param id push到堆时返回的id
-- @return 删除的元素
function Heap:erase(id)
    local e = self.map[id]
    if not e then return end

    local index = e.index
    local heap = self.heap
    local count = #heap
    assert(index > 0 and index <= count)

    -- 与最后一个节点交换，根据情况判断是往上移还是往下
    -- 交换操作在shift_up或者down里实现
    local p = index // 2
    local last = heap[count]
    local comp = self.comp

    self.map[id] = nil
    self.heap[count] = nil
    if index > 1 and comp(last.value, heap[p].value) then
        shift_up(comp, heap, index, last)
    else
        shift_down(comp, heap, index, last)
    end

    e.index = 0
    return e.value
end

return Heap
