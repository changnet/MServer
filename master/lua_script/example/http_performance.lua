-- http_performance.lua
-- 2016-04-19
-- xzc

local Http_socket = require "Http_socket"
local Http_server_mgr = require "example.http.http_server_mgr"
local Http_client_mgr = require "example.http.http_client_mgr"

local IP = "127.0.0.1"
local PORT = 8887

g_http_server_mgr = Http_server_mgr()
g_http_client_mgr = Http_client_mgr()

g_http_server_mgr:listen( IP,PORT )
g_http_client_mgr:start( 10,IP,PORT )

local Http_url_connection = oo.class( nil,"example.Http_url_connection" )

local get_str = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\nConnection: close\r\nAccept: */*\r\n\r\n";

function Http_url_connection:__init()
end

function Http_url_connection:on_message()
    local body = self.conn:get_body()

    print( body )
    self.conn:kill()
end

function Http_url_connection:on_disconnect()
    local fd  = self.conn:file_description()
    self.conn = nil

    PLOG( "url disconnect:%d",fd )
end

function Http_url_connection:connect( ip,port )
    local conn = Http_socket()
    conn:connect( ip,port )

    conn:set_self_ref( self )
    conn:set_on_message( self.on_message )
    conn:set_on_disconnect( self.on_disconnect )
    conn:set_on_connection( self.on_connection )
    self.conn = conn

    return conn:file_description()
end

function Http_url_connection:on_connection()
    print( "connect ok" )
    self.conn:send( get_str )
end

local url = Http_url_connection()
url:connect( "www.baidu.com",80 )
