-- require_conf.lua
-- xzc
-- 2018-12-23

-- 配置加载函数
-- 配置路径以 . 分割

--[[
    {
        [1] = "a",
        [2] = "b"
    }

    https://www.cnblogs.com/coding-my-life/p/5223533.html
    这个表加载后，用 pairs 来遍历的话并不一定是按 1 2去遍历的，换用ipairs
]]

local conf_dir = "config."

local function set_kv( root,val,k,nk )
    -- 校验key唯一
    local kv = val[k]

    -- 已经是最后一级，设置值
    if not nk then assert( nil == root[kv] );root[kv] = val;return end

    if not root[kv] then root[kv] = {} end
    return root[kv]
end

--[[
    { stage = 1,level = 1,val = 1 },
    { stage = 1,level = 2,val = 2 },
    { stage = 2,level = 1,val = 3 },
    { stage = 2,level = 2,val = 4 },

    转换为

    [1] = 
    {
        [1] = { stage = 1,level = 1,val = 1 },
        [2] = { stage = 1,level = 2,val = 2 },
    },
    [2] = 
    {
        [1] = { stage = 2,level = 1,val = 3 },
        [2] = { stage = 2,level = 2,val = 4 },
    }
]]
local function to_kv(list,k1,k2,k3)
    local kv_tbl = {}
    for _,val in pairs(list) do
        -- TODO:不想用 {k1,k2,k3} 或者 select( "#",... ) 这逻辑写得还挺绕，以后优化下
        local root = set_kv(kv_tbl,val,k1,k2 )
        if k2 then root = set_kv(root,val,k2,k3) end
        if k3 then root = set_kv(root,val,k3,nil) end
    end

    return kv_tbl
end

-- 加载配置，不做任何处理
function require_conf( path )
    return require(conf_dir .. path)
end

-- 加载配置，根据key转换为kv结构
-- @k1,k2,k3: 多级key，有几级就传几级
function require_kv_conf( path,k1,k2,k3 )
    local raw_conf = require(conf_dir .. path)

    return to_kv( raw_conf,k1,k2,k3 )
end

--[[
--  test code

local tbl = 
{
    { stage = 1,level = 1,val = 1 },
    { stage = 1,level = 2,val = 2 },
    { stage = 2,level = 1,val = 3 },
    { stage = 2,level = 2,val = 4 },
}

local tbl_id = 
{
    { id = 1 },
    { id = 2 },
    { id = 3 },
    { id = 4 },
}

vd( to_kv(tbl,"stage","level") )
vd( to_kv(tbl_id,"id") )

]]
