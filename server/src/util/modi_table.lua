-- Modification table
-- 通过元表来判断一个table是否被修改过，通常用于判断数据是否需要保存
-- 这个table大约比原生table慢10倍(详见misc_test.lua中的modi test)，慎用

local type = type
local setmetatable = setmetatable

local function set_modify_metatable(tbl, root, data, shadow)
    shadow = shadow or {}

    local function set_sub_metatable(tbl_sub, root_sub, data_sub)
        local shadow_sub = {}
        for k1, v1 in pairs(data_sub) do
            if type(v1) == "table" then
                shadow_sub[k1] = set_sub_metatable({}, root_sub, v1)
            end
        end
        return set_modify_metatable(tbl_sub, root_sub, data_sub, shadow_sub)
    end

    -- initialize nested shadows when none provided
    if not next(shadow) then
        for k1, v1 in pairs(data) do
            if type(v1) == "table" then
                shadow[k1] = set_sub_metatable({}, root, v1)
            end
        end
    end

    local mt = {
        __newindex = function(t, k, v)
            if type(v) == "table" and data[k] ~= v then
                shadow[k] = set_sub_metatable({}, root, v)
            end

            data[k] = v
            root.modify = true
        end,
        __index = function(t, k)
            return shadow[k] or data[k]
        end,
    }

    return setmetatable(tbl, mt)
end

-- 创建一个Modification table
-- @param v 用来初始化的原始数据
function table.new_modi(v)
    local tbl = {
        data = v or {}, -- 原始的数据集合
        modify = false, -- 是否修改过
    }
    if v then
        return set_modify_metatable(tbl, tbl, tbl.data)
    else
        return set_modify_metatable(tbl, tbl, tbl.data, {})
    end
end
