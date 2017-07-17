-- http_server_mgr.lua
-- 2016-02-16
-- xzc

local Http_socket = require "Http_socket"
local Http_server_connection = require "example.http.http_server_connection"

local Http_server_mgr = oo.singleton( nil,... )

function Http_server_mgr:__init()
    self.conn = {}
    self.clts = {}
end

function Http_server_mgr:listen( ip,port )
    local conn = Http_socket()
    if not conn:listen( ip,port ) then
        ELOG( "http socket listen fail" )
        return false
    end

    conn:set_self_ref( self )
    conn:set_on_acception( self.on_acception )
    self.conn = conn

    return true
end

function Http_server_mgr:on_acception( conn )
    local fd = conn:file_description()
    local addr = conn:address()

    local clt = Http_server_connection()
    clt:set_conn( conn )
    self.clts[fd] = clt

    PLOG( "http accept %d at %s",fd,addr )
end

function Http_server_mgr:remove_connection( fd )
    self.clts[fd] = nil
end

return Http_server_mgr
