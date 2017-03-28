-- unique_id.lua  创建唯一id
-- 2017-03-28
-- xzc

require "global.define"

local Unique_id = oo.class( nil,... )

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

return Unique_id