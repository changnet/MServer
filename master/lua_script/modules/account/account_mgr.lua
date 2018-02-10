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

-- 玩家登录
function Account_mgr:player_login( clt_conn,pkt )
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
    if not self.account[sid][plat] then self.account[sid][plat] = {} end

    local role_info = self.account[sid][plat][account]
    if not role_info then
        role_info = {}
        role_info.sid     = sid
        role_info.plat    = plat
        role_info.account = account

        self.account[sid][plat][account] = role_info
    end

        -- 当前一个帐号只能登录一个角色,处理顶号
    if role_info.conn_id then
        self:login_otherwhere( role_info )

        -- 下面开始替换连接
        -- TODO 是否要等待其他服务器返回顶号处理成功再替换连接，防止新连接收到旧连接
        -- 的数据，看游戏需要
    end

    -- 连接认证成功，将帐号和连接绑定。现在可以发送其他协议了
    role_info.conn_id = conn_id
    self.conn_acc[conn_id] = role_info

    clt_conn:authorized()
    if role_info.pid then
        g_network_mgr:bind_role( role_info.pid,clt_conn )
    end

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    clt_conn:send_pkt( SC.PLAYER_LOGIN,role_info )

    PLOG( "client authorized success:%s--%d",pkt.account,role_info.pid or 0 )
end

-- 创角
function Account_mgr:create_role( clt_conn,pkt )
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

    -- TODO: 检测一个名字是否带有数据库非法字符和敏感字,是否重复

    return self:do_create_role( role_info,pkt )
end

--  创建角色逻辑
function Account_mgr:do_create_role( role_info,pkt )
    self.seed = self.seed + 1
    local pid = g_unique_id:player_id( role_info.sid,self.seed )

    local acc_info = {}
    acc_info._id = pid
    acc_info.sid = role_info.sid
    acc_info.plat = role_info.plat
    acc_info.name = pkt.name
    acc_info.account = role_info.account

    local callback = function( ... )
        self:on_role_create( acc_info,role_info,... )
    end
    g_mongodb:insert( "account",acc_info,callback )
end

-- 创建角色数据库返回
function Account_mgr:on_role_create( acc_info,role_info,ecode,res )
    -- 玩家可能断线了，这个clt_conn就不存在了
    local clt_conn = g_network_mgr:get_conn( role_info.conn_id )

    if ( 0 ~= ecode ) then -- 失败
        if clt_conn then
            clt_conn:send_pkt( SC.PLAYER_CREATE,role_info,E.UNDEFINE )
        end
        PLOG( "create role error:%s",acc_info.account )
        return
    end

    -- 完善玩家数据
    local pid = acc_info._id
    role_info.pid  = pid
    role_info.name = acc_info.name
    PLOG( "create role success:%s--%d",role_info.account,pid )

    if clt_conn then
        g_network_mgr:bind_role( pid,clt_conn )
        clt_conn:send_pkt( SC.PLAYER_CREATE,role_info )
    end
end

-- 玩家下线
function Account_mgr:role_offline( conn_id )
    local role_info = self.conn_acc[conn_id]
    if not role_info then return end -- 连接上来未登录就断线

    role_info.conn_id = nil
    self.conn_acc[conn_id] = nil
end

-- 帐号在其他地方登录
function Account_mgr:login_otherwhere( role_info )
    -- 告诉原连接被顶号
    local old_conn = g_network_mgr:get_conn( role_info.conn_id )
    old_conn:send_pkt( SC.PLAYER_OTHER,{} )

    -- 通知其他服务器玩家下线
    if role_info.pid then
        local pkt = { pid = role_info.pid }
        g_network_mgr:srv_multicast( SS.PLAYER_OTHERWHERE,pkt )
    end

    -- 关闭旧客户端连接
    g_network_mgr:clt_close( old_conn )
end

local g_account_mgr = Account_mgr()

return g_account_mgr
