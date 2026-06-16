-- Lua内存分析
MemProfile = {}

--[[
Lua的内存由虚拟机管理，不存在泄漏（悬空指针）的情况，但有可能因为逻辑bug未清除变量引用而
导致内存使用一直涨。这种情况有3种方法可以解决

1. 把所有table所占内存打印出来，进行对比
    但有些数据可能并不是存在可访问的table，而是upvalue，甚至metable中

2. 遍历所有Lua变量（包括注册表、_G、upvalue），进行快照
    类似https://github.com/changnet/lua_memdump，但实测这个遍历在真实项目中太慢，几乎无法执行

3. 类似于profile.lua中的timing，记录每一条协议、每一个rpc调用内存的变化
    collectgarbage("count")速度很快，可直接在线上项目执行
    无法预知gc在哪里、在什么时候触发，单次获得的值可能是不正确的
    但多次的结果统计出来，应该是能看出问题，总体是可行的
]]

local TVALUE_SIZE = 16

local type = type
local tostring = tostring
local string_len = string.len
local table_insert = table.insert

local util = require("engine.util")

--@class MemNode
--@field key string 节点名字
--@field total_size number 总大小
--@field count number key数量
--@field children_vec table<MemNode> 子节点向量
--@field children_map table<string, MemNode> 子节点哈希表

-- MemNode构造函数
-- @param key string 节点名字
-- @return MemNode 节点对象
local function new_node(key)
    return {
        key = key,
        total_size = 0,
        count = 0,
        children_vec = {},
        children_map = {},
    }
end

-- 获取或创建子节点
-- @param key string 节点名字
-- @return MemNode 返回子节点
local function get_or_create_node(root, key)
    local node = root.children_map[key]
    if node then return node end

    node = new_node(key)
    root.children_map[key] = node
    table_insert(root.children_vec, node)
    return node
end

-- 获取数据占用大小
-- @param v any 待计算大小的数据
-- @return number 字节数
local function get_item_size(v)
--[[
关于TValue与GCObject的大小及其来源（64位系统）：

一、TValue（16字节）：
TValue是Lua中所有值外层的统一封装，存在于数组、哈希节点或者虚拟机的寄存器中。
包含一个8字节的Value联合体（存储指针、double或整数）和一个4字节的类型标签tag，
再加上C语言结构体4字节的内存对齐，总共为16字节。

二、GCObject（通常8到16字节）：
GCObject是所有被垃圾回收管理的对象（string、table、function等）的头部公共部分。
它通常包含一个指向下一个GC对象的next指针（8字节），一个类型标记（1字节），
以及GC的状态标记（1字节）。为了内存对齐，通常会占用8到16字节。

三、所有类型都有GCObject吗？
不是的！Lua的类型有“值类型”和“引用类型”的区别：
1. 值类型：number、boolean、nil以及lightuserdata。它们没有独立的堆内存分配，不需要被垃圾回收，
因此没有GCObject。它们的信息直接完全存储在外层的TValue（16字节）中。
2. 引用类型：string、table、function、thread、(full) userdata。它们在堆上动态分配，需要被GC接管，
所以它们各自的分配结构头部都包含了一个GCObject。它们占用的总内存= 指向它们的TValue(16字节) + 自身对象大小。

各种引用类型自身对象的基础大小推算（扣除动态装载数据以外的空壳大小）：
* string：基本结构为TString，包含GCObject + hash标记 + len。一般折合占约24字节，加字符串实际长度和结尾'\0'
* table：基本结构为Table，包含GCObject + metatable指针 + array指针 + node哈希表指针 + 一些大小标识。一般占约56字节
* function：LClosure/CClosure，一般占约40字节
* userdata：Udata头部，一般占约40字节
* thread：即协程的lua_State，约400多字节
]]
    local t = type(v)
    if t == "number" or t == "boolean" or t == "nil" then
        -- 值类型，只有外层的TValue，占用16字节
        return TVALUE_SIZE
    elseif t == "string" then
        -- TVALUE(16) + TString(24) + string长度 + 字符串末尾'\0'(1)
        return TVALUE_SIZE + 24 + string_len(v) + 1
    elseif t == "table" then
        -- TVALUE(16) + Table基础(56) (哈希和数组中的数据由上层遍历计算)
        return TVALUE_SIZE + 56
    elseif t == "function" then
        -- TVALUE(16) + Closure结构基础(40)
        return TVALUE_SIZE + 40
    elseif t == "userdata" then
        -- Lua脚本层面无法区分lightuserdata和full userdata（type返回都是"userdata"）
        -- 这里取full userdata的一个平均包裹头大小（40），若是lightuserdata本应为16
        -- 并且我们无法通过脚本获知userdata底层实际分配的内存空间
        return TVALUE_SIZE + 40
    elseif t == "thread" then
        -- 协程对象
        return TVALUE_SIZE + 400
    end
    -- 兜底，防止有遗漏类型或者引擎扩展的类型
    return TVALUE_SIZE + 16
end

-- 排序函数
-- @param a MemNode 节点
-- @param b MemNode 节点
-- @return boolean 排序结果
local function sort_func(a, b)
    return a.total_size > b.total_size
end

-- 格式化输出节点并将内容写入文件
-- @param fp file 文件句柄
-- @param node MemNode 节点
-- @param depth number 深度
local function write_node(fp, node, depth)
    if node.total_size <= 0 then return end

    local indent = ""
    for i = 1, depth do
        if i == depth then
            indent = indent .. "|+ "
        else
            indent = indent .. "|  "
        end
    end

    local line = string.format("%-10d %-10d %s%s",
        node.total_size, node.count, indent, node.key)
    fp:write(line .. "\n")

    table.sort(node.children_vec, sort_func)
    for _, child in ipairs(node.children_vec) do
        write_node(fp, child, depth + 1)
    end
end

-- 递归遍历
-- @param node MemNode 节点
-- @param tbl table 表格
-- @param visited table 去重集合
-- @return number 总大小
local function traverse(node, tbl, visited)
    if visited[tbl] then return 0 end

    visited[tbl] = true

    -- 自己本身所占空间
    local total = get_item_size(tbl)
    -- key、value所占空间
    for k, v in pairs(tbl) do
        local k_size = get_item_size(k)
        local v_size = 0
        if type(v) == "table" then
            local child_node = get_or_create_node(node, tostring(k))
            v_size = traverse(child_node, v, visited)
        else
            v_size = get_item_size(v)
        end

        node.count = node.count + 1
        total = total + k_size + v_size
    end

    local mt = getmetatable(tbl)
    if mt then
        local child_node = get_or_create_node(node, "__metatable")
        local mt_size = 0
        if type(mt) == "table" then
            mt_size = traverse(child_node, mt, visited)
        else
            mt_size = get_item_size(mt)
        end
        total = total + mt_size
    end

    node.total_size = total
    return total
end

-- 计算一个table所占用的内存（包括metatable）
-- @param tbl table 待计算的table
function MemProfile.table_mem(tbl, name)
    if type(tbl) ~= "table" then error("not a table") end

    local root = new_node(name or "root")
    local visited = {}
    traverse(root, tbl, visited)

    util.mkdir_p("profile")
    local now_str = os.date("%Y%m%d%H%M%S")
    local path = string.format("profile/mem_%s_%s.txt", LOCAL_NAME, now_str)

    local fp = io.open(path, "w")
    if not fp then
        print("Failed to open mem profile path: " .. path)
        return
    end

    fp:write(string.format("%-10s %-10s %s\n", "size", "count", "stack"))
    fp:write(string.rep("-", 80) .. "\n")
    write_node(fp, root, 0)

    fp:close()
    print("write profile to", path)
end

return MemProfile
