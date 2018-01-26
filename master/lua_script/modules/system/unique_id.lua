-- unique_id.lua  创建唯一id
-- 2017-03-28
-- xzc

local Unique_id = oo.class( nil,... )

-- 初始化
function Unique_id:__init()
    self.net_id_seed = 0
end

-- 生成服务器session id
-- @name  服务器名称，如gateway、world...
-- @index 服务器索引，如多个gateway，分别为1,2...
-- @srvid 服务器id，与运维相关。开了第N个服
function Unique_id:srv_session( name,index,srvid )
    local ty = SRV_NAME[name]

    assert( ty,"server name type not define" )
    assert( index < (1 << 24),"server index out of boundry" )
    assert( srvid < (1 << 16),   "server id out of boundry" )

    -- int32 ,8bits is ty,8bits is index,16bits is srvid
    return (ty << 24) + (index <<16) + srvid
end

-- 解析session id
function Unique_id:srv_session_parse( session )
    local ty = session >> 24;
    local index = (session >> 16) & 0xFF
    local srvid = session & 0xFFFF

    return ty,index,srvid
end

-- 产生一个标识socket连接的id
-- 只是简单的自增，如果用完，则循环
-- 调用此函数时请自己检测id是否重复，如果重复则重新取一个
function Unique_id:conn_id()
    if self.net_id_seed >= 0xFFFFFFFF then self.net_id_seed = 0 end

    self.net_id_seed = self.net_id_seed + 1

    return self.net_id_seed
end

-- 产生一个玩家pid
function Unique_id:player_id( srvid,seed )
    local _srvid = srvid & 0xFFFF -- 保证为16bit
    local _seed  = seed  & 0xFFFF

    -- 一个int32类型，前16bit为srvid，后16bit为自增。自增数量放数据库
    -- 合服后，必须取所有合服中最大值
    return ( _srvid << 16 ) | _seed
end

local uid = Unique_id()

return uid