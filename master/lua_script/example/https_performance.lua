-- https_performance.lua
-- 2017-12-11
-- xzc

local network_mgr = network_mgr

local IP = "0.0.0.0"
local PORT = 8887

local url_tbl =
{
    'GET / HTTP/1.1\r\n',
    'Host: www.openssl.com\r\n',
    'Connection: keep-alive\r\n',
    'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n',
    'Upgrade-Insecure-Requests: 1\r\n',
    --'User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)'
    --'User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/49.0.2623.112 Safari/537.36\r\n',
    --'Accept-Encoding: gzip, deflate, sdch\r\n',
    'Accept-Language: zh-CN,zh;q=0.8\r\n\r\n',
}

local url_page = table.concat( url_tbl )

local no_cert_idx = network_mgr:new_ssl_ctx( 4 )
print( "create no cert ssl ctx at ",no_cert_idx )


function http_accept_new( conn_id )
    network_mgr:set_conn_io( conn_id,network_mgr.IOT_SSL,no_cert_idx )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_HTTP )

    print( "http_accept_new",conn_id )
end

function http_connect_new( conn_id )
    network_mgr:set_conn_io( conn_id,network_mgr.IOT_SSL,no_cert_idx )
    network_mgr:set_conn_codec( conn_id,network_mgr.CDC_NONE )
    network_mgr:set_conn_packet( conn_id,network_mgr.PKT_HTTP )

    print( "http_connect_new",conn_id )

    network_mgr:send_raw_packet( conn_id,url_page )
end

function http_connect_del( conn_id )
    print( "http_connect_del",conn_id )
end

-- http回调
function http_command_new( conn_id,url,body )
    print( "http_command_new",conn_id,url,body )
end

local http_listen = network_mgr:listen( IP,PORT,network_mgr.CNT_HTTP )
PLOG( "http listen at %s:%d",IP,PORT )

local ssl_port = 443
local ssl_url = "www.openssl.com"
local ip1,ip2 = util.gethostbyname( ssl_url )

local url_conn_id = network_mgr:connect( ip1,ssl_port,network_mgr.CNT_HTTP )
print( "connnect to https ",ssl_url,ip1,url_conn_id )
