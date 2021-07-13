-- account_mgr.lua
-- 2017-04-02
-- xzc
-- 帐号管理
local util = require "util"
local AccountMgr = oo.singleton(...)

-- 初始化
function AccountMgr:__init()
    self.account = {} -- 三级key [sid][plat][account]
    self.conn_acc = {} -- conn_id为key，帐号信息为value
    self.role_acc = {} -- 玩家pid为key
end

-- 根据Pid获取角色数据
function AccountMgr:get_role_info(pid)
    return self.role_acc[pid]
end

-- 玩家登录
function AccountMgr:player_login(clt_conn, pkt)
    local sign = util.md5(LOGIN_KEY, pkt.time, pkt.account)
    if sign ~= pkt.sign then
        ERROR("clt sign error:%s", pkt.account)
        return
    end

    if not PLATFORM[pkt.plat] then
        ERROR("clt platform error:%s", tostring(pkt.plat))
        return
    end

    local conn_id = clt_conn.conn_id
    -- 不能重复发送(不是顶号，conn_id不应该会重复)
    if self.conn_acc[conn_id] then
        ERROR("player login pkt dumplicate send")
        return
    end

    -- 考虑到合服，服务器sid、平台plat、帐号account才能确定一个玩家
    local sid = pkt.sid
    local plat = pkt.plat
    local account = pkt.account

    if not self.account[sid] then self.account[sid] = {} end
    if not self.account[sid][plat] then self.account[sid][plat] = {} end

    local role_info = self.account[sid][plat][account]
    if not role_info then
        role_info = {}
        role_info.sid = sid
        role_info.plat = plat
        role_info.account = account

        self.account[sid][plat][account] = role_info
    end

    -- 当前一个帐号只能登录一个角色,处理顶号
    if role_info.conn_id then
        self:login_otherwhere(role_info)

        -- 下面开始替换连接
        -- TODO 是否要等待其他服务器返回顶号处理成功再替换连接，防止新连接收到旧连接
        -- 的数据，看游戏需要
    end

    -- 连接认证成功，将帐号和连接绑定。现在可以发送其他协议了
    role_info.conn_id = conn_id
    self.conn_acc[conn_id] = role_info

    clt_conn:authorized()
    if role_info.pid then g_clt_mgr:bind_role(role_info.pid, clt_conn) end

    -- 返回角色信息(如果没有角色，则pid和name都为nil)
    clt_conn:send_pkt(PLAYER.LOGIN, role_info)

    PRINTF("client authorized success:%s--%d", pkt.account, role_info.pid or 0)
end

-- 创角
function AccountMgr:create_role(clt_conn, pkt)
    local role_info = self.conn_acc[clt_conn.conn_id]
    if not role_info then
        ERROR("create role,no account info")
        return
    end

    -- 当前一个帐号只能创建一个角色
    if role_info.pid then
        ERROR("role already create")
        return
    end

    -- TODO: 检测一个名字是否带有数据库非法字符和敏感字,是否重复

    local callback = function(pid)
        return self:do_create_role(role_info, pkt, pid)
    end
    return g_unique_id:player_id(role_info.sid, callback)
end

--  创建角色逻辑
function AccountMgr:do_create_role(role_info, pkt, pid)
    local base = {}
    base.new = 1 -- 标识为新创建，未初始化用户
    base._id = pid
    base.tm = ev:time()
    base.sid = role_info.sid
    base.plat = role_info.plat
    base.name = pkt.name
    base.account = role_info.account

    local callback = function(...)
        self:on_role_create(base, role_info, ...)
    end
    g_mongodb:insert("base", base, callback)
end

-- 创建角色数据库返回
-- 角色base库是由world维护的，这里创建新角色比较重要，需要入库确认.再由world加载
function AccountMgr:on_role_create(base, role_info, ecode, res)
    if 0 ~= ecode then
        ERROR("role create error:name = %s,account = %s,srv = %d,plat = %d",
              base.name, role_info.account, role_info.sid, role_info.plat)
        self:send_role_create(role_info, E.UNDEFINE)
        return
    end

    -- 将角色与帐号绑定
    return self:do_acc_create(role_info, base.name, base._id)
end

--  创建帐号
function AccountMgr:do_acc_create(role_info, name, pid)
    local acc_info = {}
    acc_info._id = pid
    acc_info.tm = ev:time()
    acc_info.sid = role_info.sid
    acc_info.plat = role_info.plat
    acc_info.name = name
    acc_info.account = role_info.account

    local callback = function(...)
        self:on_acc_create(acc_info, role_info, ...)
    end
    g_mongodb:insert("account", acc_info, callback)
end

-- 创建角色数据库返回
function AccountMgr:on_acc_create(acc_info, role_info, ecode, res)
    if 0 ~= ecode then -- 失败
        self:send_role_create(role_info, E.UNDEFINE)
        PRINTF("create role error:%s", acc_info.account)
        return
    end

    -- 完善玩家数据
    local pid = acc_info._id
    role_info.pid = pid
    role_info.name = acc_info.name
    PRINTF("create role success:%s--%d", role_info.account, pid)

    self.role_acc[pid] = role_info

    -- 玩家可能断线了，这个clt_conn就不存在了
    local clt_conn = g_clt_mgr:get_conn(role_info.conn_id)
    if clt_conn then
        g_clt_mgr:bind_role(pid, clt_conn)
        self:send_role_create(role_info)
    end
end

-- 发送角色创建结果
function AccountMgr:send_role_create(role_info, ecode)
    -- 玩家可能断线了，这个clt_conn就不存在了
    local clt_conn = g_clt_mgr:get_conn(role_info.conn_id)
    if not clt_conn then return end

    clt_conn:send_pkt(PLAYER.CREATE, role_info, ecode)
end

-- 玩家下线
function AccountMgr:role_offline(conn_id)
    local role_info = self.conn_acc[conn_id]
    if not role_info then return end -- 连接上来未登录就断线

    role_info.conn_id = nil
    self.conn_acc[conn_id] = nil
end

-- 根据pid下线
function AccountMgr:role_offline_by_pid(pid)
    local role_info = self.role_acc[pid]
    if not role_info then
        ERROR("role_offline_by_pid no role_info found:%d", pid)
        return
    end

    self:role_offline(role_info.conn_id)
end

-- 帐号在其他地方登录
function AccountMgr:login_otherwhere(role_info)
    -- 告诉原连接被顶号
    local old_conn = g_clt_mgr:get_conn(role_info.conn_id)
    old_conn:send_pkt(PLAYER.OTHER, {})

    -- 通知其他服务器玩家下线
    if role_info.pid then
        local pkt = {pid = role_info.pid}
        g_clt_mgr:send_world_pkt(SYS.PLAYER_OTHERWHERE, pkt)
    end

    -- 关闭旧客户端连接
    g_clt_mgr:clt_close(old_conn)
end

-- 加载帐号数据
function AccountMgr:db_load()
    local callback = function(...)
        self:on_db_loaded(...)
    end

    g_mongodb:find("account", nil, nil, callback)
end

-- db数据加载
function AccountMgr:on_db_loaded(ecode, res)
    if 0 ~= ecode then
        ERROR("account db load error")
        return
    end

    for _, role_info in pairs(res) do
        local sid = role_info.sid
        local plat = role_info.plat
        local account = role_info.account

        if not self.account[sid] then self.account[sid] = {} end
        if not self.account[sid][plat] then self.account[sid][plat] = {} end

        role_info.pid = role_info._id
        self.role_acc[role_info.pid] = role_info
        self.account[sid][plat][account] = role_info
    end

    g_app:one_initialized("acc_data", 1)
end

local g_account_mgr = AccountMgr()

return g_account_mgr
