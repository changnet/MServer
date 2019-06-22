-- socket_example.lua
-- 2019-06-22
-- xzc

local network_mgr = network_mgr
local conn_mgr = require "network.conn_mgr"

local IP = "0.0.0.0"
local PORT = 10002

local Clt_conn = oo.class( "Clt_conn" )

function Clt_conn:connect( ip,port )
    self.conn_id = network_mgr:connect( ip,port,network_mgr.CNT_CSCN )
    conn_mgr:set_conn( self.conn_id,self )
    print( "connnect to",ip,self.conn_id )
end

function Clt_conn:conn_new( ecode )
    if 0 ~= ecode then
        print( "clt_conn conn error" )
        return
    end

    network_mgr:set_conn_io( self.conn_id,network_mgr.IOT_NONE )
    network_mgr:set_conn_codec( self.conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( self.conn_id,network_mgr.PKT_HTTP )

    print( "conn_new",self.conn_id )
    network_mgr:send_raw_packet( self.conn_id,url_page )
end

function Clt_conn:conn_del()
    print("conn_del")
end

function Clt_conn:command_new( url,body )
    print("clt command new",url,body)
end

local Srv_conn = oo.class( "Srv_conn" )

function Srv_conn:__init( conn_id )
    self.conn_id = conn_id
end

function Srv_conn:listen( ip,port )
    self.conn_id = network_mgr:listen( ip,port,network_mgr.CNT_SCCN )
    conn_mgr:set_conn( self.conn_id,self )
    PRINTF( "https listen at %s:%d",ip,port )
end

function Srv_conn:conn_accept( new_conn_id )
    print( "srv conn accept new",new_conn_id )

    -- 弄成全局的，防止没有引用被释放
    new_conn = Srv_conn( new_conn_id )
    network_mgr:set_conn_io( new_conn_id,IOT,srv_idx )
    network_mgr:set_conn_codec( new_conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( new_conn_id,network_mgr.PKT_HTTP )

    return new_conn
end

function Srv_conn:conn_del()
    print( "srv conn del",self.conn_id )
end

-- http回调
function Srv_conn:command_new( url,body )
    print( "command_new",self.conn_id,url,body )

    local tips = "Mini-Game-Distribute-Server!\n"
    local ctx = string.format( page200,string.len(tips),tips )

    network_mgr:send_raw_packet( self.conn_id,ctx )
end

-- 对象弄成全局的，防止没有引用被释放

-- 在浏览器输入https://127.0.0.1:10002来测试
http_listen = Srv_conn()
http_listen:listen( IP,PORT )

local ip1,ip2 = util.gethostbyname( ssl_url )

http_conn = Clt_conn()
http_conn:connect( ip1,ssl_port )
