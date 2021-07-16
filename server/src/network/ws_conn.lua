local Conn = require "network.conn"

-- websocket公共层
local WsConn = oo.class(..., Conn)

function CsConn:set_conn_param(conn_id)
    network_mgr:set_conn_io(conn_id, network_mgr.IOT_NONE)
    network_mgr:set_conn_codec(conn_id, network_mgr.CT_NONE)

    -- 使用标准的websocket，内容不经过处理，下发啥就是咐，一般可以是text
    network_mgr:set_conn_packet(conn_id, network_mgr.PT_WEBSOCKET)
    -- 使用websocket二进制流
    -- network_mgr:set_conn_packet( conn_id,network_mgr.PT_WSSTREAM )

    -- set_send_buffer_size最后一个参数表示over_action，1 = 溢出后断开
    network_mgr:set_send_buffer_size(conn_id, 128, 8192, 1) -- 8k*128 = 1024k
    network_mgr:set_recv_buffer_size(conn_id, 8, 8192) -- 8k*8 = 64k
end

return WsConn
