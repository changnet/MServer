-- heap.lua
-- xzc
-- 2018-11-14

-- the lua version of c++ STL algorithm heap implements,using a vector as
-- container,not a Binary Heap.

local Heap = oo.class( nil,... )

local function default_comp(src,dest) return src < dest end

function Heap:__init( comp )
    self.comp = comp or default_comp
end

function Heap:make_heap( tbl )
end

function Heap:push_head( object )
end

-- pop a object
-- @object: if object is nil,pop the head object
function Heap:pop_head( object )
end

function Heap:size()
end

function Heap:to_list()
end

return Heap
