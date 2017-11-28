-- http deamon

local page404 = 
[[
HTTP/1.1 404 Not Found
Content-Length: 0
Content-Type: text/html
Server: Mini-Game-Distribute-Server/1.0
Connection: close
]]

local page500 = 
[[
HTTP/1.1 500 Not Found
Content-Length: 0
Content-Type: text/html
Server: Mini-Game-Distribute-Server/1.0
Connection: close
]]

local network_mgr = network_mgr
local uri = require "global.uri"

local Httpd = oo.singleton( nil,... )

function Httpd:__init()
    self.conn = {}
    self.exec = {}
    self.http_listen = nil
end

-- 监听http连接
function Httpd:http_listen( ip,port )
    self.http_listen = network_mgr:listen( ip,port,network_mgr.CNT_HTTP )
    PLOG( "%s listen for http at %s:%d",Main.srvname,ip,port )

    return true
end

-- http调用
function Httpd:do_exec( conn_id,path,fields,body )
    local exec_obj = require( path )
    return exec_obj:exec( conn_id,fields,body )
end

local httpd = Httpd()

-- =============================================================================

function http_accept_new( conn_id )
    network_mgr:set_conn_io( conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_HTTP )

    print( "http_accept_new" )
end

function http_connect_new( conn_id )
    network_mgr:set_conn_io( conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_HTTP )

    print( "http_connect_new" )
end

function http_connect_del()
    print( "http_connect_del" )
end

-- http回调
function http_command_new( conn_id,url,body )
    -- url = /platform/pay?sid=99&money=200
    local raw_url,fields = uri.parse( url )

    local path = httpd.exec[raw_url]
    if not path then
        -- 限定http请求的路径，不能随意运行其他路径文件
        path = "lua_script/http" .. raw_url
        local exec_file = io.open( path .. ".lua","r" )

        if not exec_file then
            ELOG( "http request page not found:%s",raw_url )
            network_mgr:send_s2c_packet( conn_id,page404 )

            return network_mgr:close( conn_id )
        end

        -- 记录一个path而不是一个exec_obj，不影响热更，但不用每次都拼字符
        httpd.exec[raw_url] = path
    end

    local success = xpcall( 
        Httpd.do_exec, __G__TRACKBACK__,httpd,conn_id,path,fields,body )
    if not success then
        network_mgr:send_s2c_packet( conn_id,page500 )

        return network_mgr:close( conn_id )
    end
end

return httpd