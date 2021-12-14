-- Acism
-- auto export by engine_api.lua do NOT modify!

-- AC自动机，用于关键字过滤
local Acism = {}

-- 搜索字符串中是否包含关键字
-- @param text 需要扫描的字符串
-- @return 首次出现关键字的位置，未扫描到则返回0
function Acism:scan(text)
end

-- 搜索字符串中是否包含关键字并替换出现的关键字
-- @param text 需要扫描的字符串
-- @param repl 替换字符串，出现的关键字将会被替换为此字符串
-- @return 替换后的字符串
function Acism:replace(text, repl)
end

-- 从指定文件加载关键字，文件以\n换行
-- @param path 包含关键字的文件路径，文件中每一行表示一个关键字
-- @param ci case_sensitive，是否区分大小写
-- @return 加载的关键字数量
function Acism:load_from_file(path, ci)
end

return Acism
