-- http_mgr.lua
-- 2016-02-16
-- xzc

local Http_socket = require "Http_socket"
local Http_client = require "http.http_client"

local Http_mgr = oo.class( nil,... )

function Http_mgr:__init()
    self.conn = {}
    self.clts = {}
end

function Http_mgr:listen( ip,port )
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

function Http_mgr:on_acception( conn )
    local fd = conn:file_description()
    local addr = conn:address()
    
    local clt = Http_client()
    clt:set_conn( conn )
    self.clts[fd] = clt
    
    PLOG( "http accept %d at %s",fd,addr )
end

return Http_mgr
