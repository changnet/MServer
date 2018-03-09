-- chat_cmd.lua
-- 2018-02-06
-- xzc

-- 聊天入口文件

local function chat_clt_cb( cmd,cb_func )
    local cb = function( conn,pid,pkt )
        local player = g_player_mgr:get_player( pid )
        if not player then
            return ELOG( "chat call back no player found:%d",pid )
        end
        return cb_func( player.chat,conn,pkt )
    end

    g_command_mgr:clt_register( cmd,cb )
end