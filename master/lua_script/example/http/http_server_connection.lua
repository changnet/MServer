-- http_server_connection.lua
-- 2016-02-16
-- xzc

local Http_socket = require "Http_socket"
local Http_server_connection = oo.class( nil,... )

--[[
HTTP/1.1 200 OK
Date: Mon, 27 Jul 2009 12:28:53 GMT
Server: Apache/2.2.14 (Win32)
Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT
Content-Length: 88
Content-Type: text/html
Connection: Closed
<html>
<body>
<h1>Hello, World!</h1>
</body>
</html>
]]
local http_response_head = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%s"

function Http_server_connection:__init()
end

function Http_server_connection:set_conn( conn )
    conn:set_self_ref( self )
    conn:set_on_message( self.on_message )
    conn:set_on_disconnect( self.on_disconnect )

    self.conn = conn
end

function Http_server_connection:on_message()
    local body = self.conn:get_body()

    -- vd( body )
    -- vd( self.conn:get_url() )
    -- vd( self.conn:get_head_field("host") )


    local data = '{ "data":{"ext":1,"password":"test"} }'
    local header = string.format( http_response_head,data:len(),data )

    self.conn:send( header )
end

function Http_server_connection:on_disconnect()
    local fd  = self.conn:file_description()
    self.conn = nil

    g_http_server_mgr:remove_connection( fd );
    PLOG( "http server connection disconnect:%d",fd )
end

function Http_server_connection:connect( ip,port )
    local conn = Http_socket()
    conn:connect( ip,port )

    conn:set_self_ref( self )
    conn:set_on_message( self.on_message )
    conn:set_on_disconnect( self.on_disconnect )
    conn:set_on_connection( self.on_connection )
    self.conn = conn
end

function Http_server_connection:on_connection()
    print( "connect ok" )
    self.conn:send( "hello world" )
end

return Http_server_connection
