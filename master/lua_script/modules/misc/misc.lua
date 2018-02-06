-- misc.lua
-- 2018-02-06
-- xzc

-- 处理一些杂七杂八的小功能，存储他们的数据

local Base_module = require "modules.player.base_module"
local Misc = oo.class( Base_module,... )

function Misc:__init( pid )
    Base_module.__init( self,pid )
    self.data_root = {}
end

-- 数据存库接口，自动调用
-- 必须返回操作结果
function Misc:db_save()
    return true
end

-- 数据加载接口，自动调用
-- 必须返回操作结果
function Misc:db_load()
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
