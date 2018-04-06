-- unique_id.lua  创建唯一id
-- 2017-03-28
-- xzc

local UNIQUEID = UNIQUEID
local Unique_id = oo.class( nil,... )

local player_query = string.format( '{"_id":%d}',UNIQUEID.PLAYER )

-- update = { ["$inc"] = { seed = 1 } }, -- 把seed字段的值+1
local player_update  = '{"$inc":{"seed":1}}'

-- 初始化
function Unique_id:__init()
    self.net_id_seed = 0

    self.unique_seed = {} -- 需要存库的seed
end

-- 从数据库加载
function Unique_id:db_load()
    local callback = function( ... )
        self:on_db_loaded( ... )
    end

    g_mongodb:find( "uniqueid",nil,nil,callback )
end

function Unique_id:on_db_loaded( ecode,res )
    if 0 ~= ecode then
        PLOG( "account db load error" )
        return
    end

    local seed_map = {}
    for _,raw_seed in pairs( res ) do
        local seed_id = raw_seed._id
        seed_map[seed_id] = raw_seed.seed
    end

    for _,seed_id in pairs( UNIQUEID ) do
        self.unique_seed[seed_id] = seed_map[seed_id] or 0
    end

    g_app.one_initialized( "uniqueid_data",1 )
end

-- 产生一个标识socket连接的id
-- 只是简单的自增，如果用完，则循环
-- 调用此函数时请自己检测id是否重复，如果重复则重新取一个
function Unique_id:conn_id()
    if self.net_id_seed >= 0xFFFFFFFF then self.net_id_seed = 0 end

    self.net_id_seed = self.net_id_seed + 1

    return self.net_id_seed
end
--[[
table: 0x18eb940
{
    "ok" = 1.0
    "value" = table: 0x18efdd0
    {
        "seed" = 1
        "_id" = 1
    }
    "lastErrorObject" = table: 0x18de7b0
    {
        "upserted" = 1
        "updatedExisting" = false
        "n" = 1
    }
}
]]
function Unique_id:on_player_id( srvid,raw_cb,ecode,res )
    if 0 ~= ecode or 1 ~= res.ok then
        ELOG( "update player id error" )
        return
    end

    -- 这个数据是比较重要的，如果对不上，有可能造成玩家pid冲突，严格校验
    local seed = res.value.seed or 0
    local local_seed = self.unique_seed[UNIQUEID.PLAYER] + 1
    if seed ~= local_seed then
        ELOG( "update player id not match,expect %d,got %d",seed,local_seed )
        return
    end

    local _srvid = srvid & 0xFFFF -- 保证为16bit
    local _seed  = seed  & 0xFFFF

    -- 一个int32类型，前16bit为srvid，后16bit为自增。自增数量放数据库
    -- 合服后，必须取所有合服中最大值
    local pid =  ( _srvid << 16 ) | _seed
    self.unique_seed[UNIQUEID.PLAYER] = seed

    return raw_cb( pid )
end

-- 产生一个玩家pid
function Unique_id:player_id( srvid,raw_cb )
    local callback = function( ... )
        self:on_player_id( srvid,raw_cb,... )
    end

    -- new = true, -- 返回Modify后的值
    -- upsert = true, -- 如果不存在，则插入一个记录
    return g_mongodb:find_and_modify( 
        "uniqueid",player_query,nil,player_update,nil,false,true,true,callback )
end

local uid = Unique_id()

return uid