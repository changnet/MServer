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
g_http_client_mgr:start( 1,IP,PORT )
