-- system libraries
sys = {}

local type = type
local next = next
local pairs = pairs
local tonumber = tonumber

sys.separator = package.config:sub(1,1) -- 路径分割斜杠，windows=\，linux=/

-- 一些通用又不好归到某个模块的函数，可以放这里

-- 获取文件路径的上一层路径(不包含最后的斜杠，如/etc/log的上一层为/etc)
function sys.path_up(path)
    return string.match(path, "(.*)[/\\]")
end

-- 遍历所有键，如果key是字符串并且可以转换为数字，则转换为数字
local function __restore_table(tbl)
    local k1 = next(tbl)
    if not k1 then return end

    -- 注意数字为key时，不能是double或者int64，因为即使mongodb支持，json库也不一定支持
    -- 会丢失精度

    -- 一个table中，要么全是数字键，要么全是字符串键，不能混用
    if type(k1) == "string" and tonumber(k1) then
        local i = 1
        local keys = {}

        -- 遍历table时，不允许新增key
        for k, v in pairs(tbl) do
            if type(v) == "table" then __restore_table(v) end

            keys[i] = k
            i = i + 1
        end

        for _, k in ipairs(keys) do
            local n = tonumber(k)
            local v = tbl[k]
            tbl[k] = nil
            tbl[n] = v
        end
    else
        for _, v in pairs(tbl) do
            if type(v) == "table" then __restore_table(v) end
        end
    end
end

-- 遍历所有键，如果key是字符串并且可以转换为数字，则转换为数字
function sys.restore_table(tbl)
    return __restore_table(tbl)
end

-- 把json字符串转换为table，并且把所有可以转换为数字的字符串键转换为数字
function sys.restore_json(str)
    local tbl = Json.decode(str)
    __restore_table(tbl)
    return tbl
end

return sys
