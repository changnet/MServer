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
function sys.restore_table(tbl)
    local k1 = next(tbl)
    if not k1 then return end

    -- 一个table中，要么全是数字键，要么全是字符串键，不能混用
    if type(k1) == "string" and tonumber(k1) then
        for k, v in pairs(tbl) do
            local n = tonumber(k)
            tbl[n] = v
            tbl[k] = nil
        end
    else
        for _, v in pairs(tbl) do
            if type(v) == "table" then
                sys.restore_table(v)
            end
        end
    end
end

-- 把json字符串转换为table，并且把所有可以转换为数字的字符串键转换为数字
function sys.restore_json(str)
    local tbl = Json.decode(str)
    sys.restore_table(tbl)
    return tbl
end
