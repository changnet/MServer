-- http deamon

local network_mgr = network_mgr
local uri = require "global.uri"

local Httpd = oo.singleton( nil,... )

function Httpd:__init()
    self.conn = {}
    self.http_listen = nil
end

-- 监听http连接
function Httpd:http_listen( ip,port )
    self.http_listen = network_mgr:listen( ip,port,network_mgr.CNT_HTTP )
    PLOG( "%s listen for http at %s:%d",Main.srvname,ip,port )

    return true
end

local httpd = Httpd()

-- =============================================================================

function http_accept_new()
    print( "http_accept_new" )
end

function http_connect_new()
    print( "http_connect_new" )
end

function http_connect_del()
    print( "http_connect_del" )
end

-- http回调
function http_command_new( conn_id,url,body )
    -- url = /platform/pay?sid=99&money=200

    local raw_url,fields = uri.parse( url )

    -- package.path = './http/?.lua;' .. package.path
    local exec_obj = require( "http" .. raw_url )
    if not exec_obj then
        ELOG( "not http object found:%s",url )
        return;
    end

    return exec_obj.exec( fields,body )
end

return httpd