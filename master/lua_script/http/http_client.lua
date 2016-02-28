-- http_client.lua
-- 2016-02-16
-- xzc

local Http_client = oo.class( nil,... )

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

function Http_client:__init()
end

function Http_client:set_conn( conn )
    conn:set_self_ref( self )
    conn:set_on_message( self.on_message )
    conn:set_on_disconnect( self.on_disconnect )

    self.conn = conn
end

function Http_client:on_message()
    local body = self.conn:get_body()

    PLOG( "http message start ==============================" )
    vd( body )
    vd( self.conn:get_url() )
    vd( self.conn:get_head_field("host") )
    PLOG( "http message end   ===============================" )

    local data = '{ "data":{"ext":1,"password":"test"} }'
    local header = string.format( http_response_head,data:len(),data )

    self.conn:send( header )
end

function Http_client:on_disconnect()
    local fd  = self.conn:file_description()
    self.conn = nil
    -- TODO 这里要从mgr删除client
    PLOG( "http client disconnect:%d",fd )
end

return Http_client
