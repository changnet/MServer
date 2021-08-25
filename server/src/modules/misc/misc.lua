-- misc.lua
-- 2018-02-06
-- xzc
-- 处理一些杂七杂八的小功能，存储他们的数据
local Module = require "modules.player.module"
local Misc = oo.class(..., Module)

function Misc:__init(pid, player)
    Module.__init(self, pid, player)
    self.storage = {}
    self.db_query = string.format('{"_id":%d}', pid)
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
function Misc:db_save()
    g_mongodb:update("misc", self.db_query, self.storage, true)
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Misc:db_load(sync_db)
    local ecode, res = sync_db:find("misc", self.db_query)
    if 0 ~= ecode then return false end -- 出错
    if not res then return true end -- 新号，空数据

    self.storage = res[1] or {} -- find函数查不到数据时返回一个空数组

    return true
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
function Misc:on_login()
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Misc:on_logout()
    return true
end

return Misc
