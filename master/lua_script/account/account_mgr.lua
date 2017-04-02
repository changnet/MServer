-- account_mgr.lua
-- 2017-04-02
-- xzc

-- 帐号管理

local util = require "util"
local g_command_mgr = g_command_mgr
local Account_mgr = oo.singleton( nil,... )

-- 初始化
function Account_mgr:__init()
    self.account  = {}
    self.seed     = 0  -- 这个起服从数据库加载
    self.conn_acc = {} -- conn_id为key，帐号信息为value
end

function Account_mgr.player_login( self )
    return function( clt_conn,pkt )
        local sign = util.md5( LOGIN_KEY,pkt.time,pkt.account )
        if sign ~= pkt.sign then
            ELOG( "clt sign error:%s",pkt.account )
            return
        end

        if not PLATFORM[pkt.plat] then
            ELOG( "clt platform error:%s",tostring(pkt.plat) )
            return
        end

        local conn_id = clt_conn.conn_id
        -- 不能重复发送(不是顶号，conn_id不应该会重复)
        if self.conn_acc[conn_id] then
            ELOG( "player login pkt dumplicate send" )
            return
        end

        -- 考虑到合服，服务器sid、平台plat、帐号account才能确定一个玩家
        local sid     = pkt.sid
        local plat    = pkt.plat
        local account = pkt.account

        if not self.account[sid] then self.account[sid] = {} end
        if not self.account[sid][plat] then
            self.account[sid][plat] = {}
            self.account[sid][plat].sid     = sid
            self.account[sid][plat].account = account
        end

        local role_info = self.account[sid][plat]

        -- 当前一个帐号只能登录一个角色
        if role_info.conn_id then
            -- TODO: 顶号
            assert( false,"login again" )
            return
        end

        -- 连接认证成功，将帐号和连接绑定。现在可以发送其他协议了
        role_info.conn_id = conn_id
        self.conn_acc[conn_id] = role_info

        clt_conn:authorized()

        -- 返回角色信息(如果没有角色，则pid和name都为nil)
        g_command_mgr:clt_send( clt_conn,SC.PLAYER_LOGIN,role_info )

        PLOG( "client authorized success:%s",pkt.account )
    end
end

-- 创角
function Account_mgr.create_role( self )
    return function( clt_conn,pkt )
        local role_info = self.conn_acc[clt_conn.conn_id]
        if not role_info then
            ELOG( "create role,no account info" )
            return
        end

        -- 当前一个帐号只能创建一个角色
        if role_info.pid then
            ELOG( "role already create" )
            return
        end

        -- TODO: 检测一个名字是否带有数据库非法字符和敏感字

        -- TODO 如果没有角色，这里要到数据库创建
        self.seed = self.seed + 1
        local pid = g_unique_id:player_id( role_info.sid,self.seed )

        role_info.pid  = pid
        role_info.name = pkt.name

        g_command_mgr:clt_send( clt_conn,SC.PLAYER_CREATE_ROLE,role_info )
    end
end

local g_account_mgr = Account_mgr()

return g_account_mgr