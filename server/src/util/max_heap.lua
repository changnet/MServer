--max_heap.lua
--Jan 30, 2015
--cxd
--最大堆排序算法

-- 用完全二叉树实现，用数组存储，
-- 节点的左子节点是 2*index，
-- 节点的右子节点是 2*index+1，
-- 每一个节点大于（或等于）这个节点的子节点，但左右子节点的大小则不固定

local MaxHeap = oo.class( ... )

function MaxHeap:__init()
    self.heap       = {}
    self.map_list   = {}
    self.count      = 0
    self.next_id    = 1
end

function MaxHeap:alloc_id()
    local id = self.next_id
    self.next_id = self.next_id + 1
    return id
end

function MaxHeap:top()
    return self.heap[1]
end

function MaxHeap:empty()
    return 0 == self.count
end

function MaxHeap:size()
    return self.count
end

function MaxHeap:clear()
    self.heap       = {}
    self.map_list   = {}
    self.count      = 0
end

function MaxHeap:shift_up(i, e)
    local p = math.floor(i / 2)
    while (i > 1 and self.heap[p].key < e.key) do
        local o = self.heap[p]
        self.heap[i] = o
        o.index = i
        i = p
        p = math.floor(i / 2)
    end
    self.heap[i] = e
    e.index = i
end

function MaxHeap:shift_down(i, e)
    local c = 2 * i
    local count = self.count
    while (c <= count) do
        if c ~= count and self.heap[c].key < self.heap[c + 1].key then
            c = c + 1
        end

        if e.key > self.heap[c].key then
            break
        end

        local o = self.heap[c]
        self.heap[i] = o
        o.index = i
        i = c
        c = 2 * i
    end

    self.heap[i] = e
    e.index = i
end

function MaxHeap:push(key, value)
    local id = self:alloc_id()
    local e = {}
    e.key = key
    e.value = value
    e.id = id
    e.index = 0
    self.count = self.count + 1
    self:shift_up(self.count, e)
    self.map_list[id] = e
    return id
end

function MaxHeap:pop()
    local count = self.count
    if 0 ~= count then
        self.map_list[self.heap[1].id] = nil
        self.count = count - 1
        self:shift_down(1, self.heap[count])
        self.heap[count] = nil
    end
end

function MaxHeap:erase(id)
    local count = self.count
    if 0 ~= count then
        local e = self.map_list[id]
        if e and 0 ~= e.index then
            assert(e.index <= count, string.format("%d, %d", e.index, count))
            local p = math.floor(e.index / 2)
            local last = self.heap[count]
            self.count = count - 1
            if e.index > 1 and self.heap[p].key < last.key then
                self:shift_up(e.index, last)
            else
                self:shift_down(e.index, last)
            end

            self.map_list[id] = nil
            self.heap[count] = nil
            e.index = 0
        end
    end
end

return MaxHeap
