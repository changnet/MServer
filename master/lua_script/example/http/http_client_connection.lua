-- http_client_connection.lua
-- 2016-02-16
-- xzc

local Http_socket = require "Http_socket"
local Http_client_connection = oo.class( nil,... )

local get_str = [==[
GET /default.htm HTTP/1.1\r\n
Host: 127.0.0.1\r\n
Accept: */*\r\n
\r\n
]==]

local http_response_head = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%s"

function Http_client_connection:__init()
end

function Http_client_connection:set_conn( conn )
    conn:set_self_ref( self )
    conn:set_on_message( self.on_message )
    conn:set_on_disconnect( self.on_disconnect )

    self.conn = conn
end

function Http_client_connection:on_message()
    local body = self.conn:get_body()

    local data = '{ "data":{"ext":1,"password":"test"} }'
    print( body )
    if body ~= data then
        PLOG( "client assert error" )
    end

    local fd = self.conn:file_description()
    self.conn:kill()
    g_http_client_mgr:remove_connection( fd )
end

function Http_client_connection:on_disconnect()
    local fd  = self.conn:file_description()
    self.conn = nil

    g_http_client_mgr:remove_connection( fd );
    PLOG( "http client disconnect:%d",fd )
end

function Http_client_connection:connect( ip,port )
    local conn = Http_socket()
    conn:connect( ip,port )

    conn:set_self_ref( self )
    conn:set_on_message( self.on_message )
    conn:set_on_disconnect( self.on_disconnect )
    conn:set_on_connection( self.on_connection )
    self.conn = conn

    return conn:file_description()
end

function Http_client_connection:on_connection()
    print( "connect ok" )
    self.conn:send( get_str )
end

return Http_client_connection
