-- clientmgr.lua
-- 2015-10-04
-- xzc

-- 客户端连接管理

local Socket = require "Socket"
local Player = require "module.player.player"

local Client_mgr = oo.singleton( nil,... )

-- 构造函数
function Client_mgr:__init()
    self.conn = {}
    self.player = {}
    self.id     = 0
end

-- 开始监听端口
function Client_mgr:listen( ip,port )
    local conn = Socket()
    if not conn:listen( ip,port,Socket.CLIENT ) then
        ELOG( "listen fail" )
        return
    end
    
    conn:set_self( self )
    conn:set_accept( self.on_accept )
    self.conn = conn
end

-- 连接事件
function Client_mgr:on_accept( conn )
    vd( conn )
    PLOG( "on accept ")
    local fd = conn:file_description()
    local addr = conn:address()
    
    self.id = self.id + 1
    local pid = self.id
    local player = Player( pid )
    player:set_conn( conn )
    
    self.player[pid] = player
    
    PLOG( "accept %d at %s",fd,addr )
end

local client_mgr = Client_mgr()

return client_mgr
