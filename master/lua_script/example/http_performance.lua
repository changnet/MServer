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

--g_http_server_mgr:listen( IP,PORT )
--g_http_client_mgr:start( 10,IP,PORT )

local Http_url_connection = oo.class( nil,"example.Http_url_connection" )

function Http_url_connection:__init()
end

function Http_url_connection:on_message()
    local body = self.conn:get_body()

    print( body )
    print( "url recv data ok!!! kill now" )
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

local str_tb =
{
    'GET / HTTP/1.1\r\n',
    'Host: www.baidu.com\r\n',
    'Connection: keep-alive\r\n',
    'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n',
    'Upgrade-Insecure-Requests: 1\r\n',
    --'User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)'
    --'User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/49.0.2623.112 Safari/537.36\r\n',
    'Accept-Encoding: gzip, deflate, sdch\r\n',
    'Accept-Language: zh-CN,zh;q=0.8\r\n\r\n',
}

--local str = table.concat( str_tb )

function Http_url_connection:on_connection()
    print( "url connect ok" )
    local str = "GET / HTTP/1.1\r\nHost: www.baidu.com\r\nConnection: close\r\nAccept: */*\r\n\r\n";
    self.conn:send( str )
end

-- 阻塞获取，考虑到服务器起服后连后台域名解析并不常用，不提供多线程
local ip1,ip2 = util.gethostbyname( "www.baidu.com" )

local url = Http_url_connection()

--[[
1.部分网站是需要User-Agent才会返回数据的，不同的User-Agent导致返回不同的数据
2.有时候收到301、302返回的，需要从头部取出Location进行跳转。如www.bing.com是不存在的，
  应该是cn.bing.com才有返回
3.很多网站收到的是二进制压缩包，chrome会进行解压的，但这里不行。这时get_body通常都拿不
  到数据，但底层是有收到数据的，头部有Content-Encoding: gzip、
  Transfer-Encoding: chunked字段。
  cn.bing.com、www.163.com、www.oschina.net都是这样，但www.baidu.com不压缩
]]
-- connect是不支持域名解析的，只能写ip
url:connect( ip1,80 )
