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

local Httpd_conn = require "http.httpd_conn"

local Httpd = oo.singleton( nil,... )

function Httpd:__init()
    self.conn = {}
    self.exec = {}
    self.http_listen = nil
end

-- 监听http连接
function Httpd:http_listen( ip,port )
    self.http_listen = network_mgr:listen( ip,port,network_mgr.CNT_SCCN )
    PLOG( "%s listen for http at %s:%d",Main.srvname,ip,port )

    g_conn_mgr:set_conn( self.http_listen,self )
    return true
end

-- http调用
function Httpd:do_exec( conn,path,fields,body )
    local exec_obj = require( path )
    return exec_obj:exec( conn,fields,body )
end

function Httpd:conn_accept( new_conn_id )
    network_mgr:set_conn_io( new_conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( new_conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_HTTP )

    print( "http_accept_new",new_conn_id )

    local new_conn = Httpd_conn( new_conn_id )

    self.conn[new_conn_id] = new_conn
    return new_conn
end

-- 对方断开连接
function Httpd:conn_del( conn_id )
    self.conn[conn_id] = nil
    print( "http_connect_del",conn_id )
end

-- 主动断开连接
function Httpd:conn_close( conn )
    self.conn[conn.conn_id] = nil
    conn:close()
end

-- http回调
function Httpd:do_command( conn,url,body )
    -- url = /platform/pay?sid=99&money=200
    local raw_url,fields = uri.parse( url )

    local path = self.exec[raw_url]
    if not path then
        -- 限定http请求的路径，不能随意运行其他路径文件
        -- 也不要随意放其他文件到此路径
        path = "lua_script/http/www" .. raw_url
        local exec_file = io.open( path .. ".lua","r" )

        if not exec_file then
            ELOG( "http request page not found:%s",raw_url )
            conn:send_pkt( page404 )

            return self:conn_close( conn )
        end

        path = string.gsub( path,"%/", "." ) -- 把/转为.来匹配lua的require格式
        -- 记录一个path而不是一个exec_obj，不影响热更，但不用每次都拼字符
        self.exec[raw_url] = path
    end

    local success = xpcall( 
        Httpd.do_exec, __G__TRACKBACK__,httpd,conn,path,fields,body )
    if not success then
        conn:send_pkt( page500 )

        return self:conn_close( conn )
    end
end

local httpd = Httpd()

return httpd
