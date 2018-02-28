-- misc.lua
-- 2018-02-06
-- xzc

-- 处理一些杂七杂八的小功能，存储他们的数据

local Base_module = require "modules.player.base_module"
local Misc = oo.class( Base_module,... )

function Misc:__init( pid )
    Base_module.__init( self,pid )
    self.data_root = {}
    self.db_query = string.format( '{"_id":%d}',pid )
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
function Misc:db_save()
    g_mongodb:update( "misc",self.db_query,self.data_root )
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Misc:db_load( sync_db )
    local ecode,res = sync_db:find( "misc",self.db_query )
    if 0 ~= ecode then return false end -- 出错
    if not res then return end -- 新号，空数据
    
    self.data_root = res

    return true
end

-- 玩家数据已加载完成，进入场景
-- 必须返回操作结果
function Misc:on_login()
    self.data_root.login = ev:time()
    return true
end

-- 玩家退出游戏
-- 必须返回操作结果，但这个结果不影响玩家数据存库
function Misc:on_logout()
    self.data_root.logout = ev:time()
    return true
end

-- 获取根数据
function Misc:get_data_root()
    return self.data_root
end

-- 根据key获取某个数据集
-- 一般来说每个小功能都有自己的key，以避免与其他功能冲突
function Misc:get_data_set( key )
    if not self.data_root[key] then self.data_root[key] = {} end

    return self.data_root[key]
end

return Misc
