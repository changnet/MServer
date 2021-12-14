-- NetworkMgr
-- auto export by engine_api.lua do NOT modify!

-- 网络管理
local NetworkMgr = {}

-- 设置客户端发往服务端协议参数
-- @param cmd 协议号
-- @param schema 模块名(比如protobuf的pb文件名)
-- @param object 协议结构(比如protobuf的message)
-- @param mask 掩码(参考net_header.h中的定义)
-- @param session 会话id，用于自动转发到对应的app进程，0表示当前进程处理
function NetworkMgr:set_cs_cmd(cmd, schema, object, mask, session)
end

-- 设置服务端发往服务端协议参数
-- @param cmd 协议号
-- @param schema 模块名(比如protobuf的pb文件名)
-- @param object 协议结构(比如protobuf的message)
-- @param mask 掩码(参考net_header.h中的定义)
-- @param session 会话id，用于自动转发到对应的app进程，0表示当前进程处理
function NetworkMgr:set_ss_cmd(cmd, schema, object, mask, session)
end

-- 设置服务端发往客户端协议参数
-- @param cmd 协议号
-- @param schema 模块名(比如protobuf的pb文件名)
-- @param object 协议结构(比如protobuf的message)
-- @param mask 掩码(参考net_header.h中的定义)
-- @param session 会话id，用于自动转发到对应的app进程，0表示当前进程处理
function NetworkMgr:set_sc_cmd(cmd, schema, object, mask, session)
end

-- 设置连接的io方式
-- @param conn_id 连接id
-- @param io_type io类型，见io.h中的定义，1表示ssl
-- @param io_ctx io内容，根据不同io类型意义不一样，ssl则表示ssl_mgr中的索引
function NetworkMgr:set_conn_io(conn_id, io_type, io_ctx)
end

-- 设置socket的编译方式
-- @param conn_id 连接id
-- @param codec_type 编码类型，见codec.h中的定义(bson、protobuf、flatbuffers...)
function NetworkMgr:set_conn_codec(conn_id, codec_type)
end

-- 设置socket的打包方式
-- @param conn_id 连接id
-- @param packet_type 打包类型，见packet.h中的定义(http、tcp、websocket...)
function NetworkMgr:set_conn_packet(conn_id, packet_type)
end

-- 设置(客户端)连接所有者
-- @param conn_id 连接id
-- @param owner 所有者id，一般是玩家id，用于自动转发
function NetworkMgr:set_conn_owner(conn_id, owner)
end

-- 解除(客户端)连接所有者
-- @param conn_id 连接id
-- @param owner 所有者id，一般是玩家id(TODO: 这个参数应该可以精简掉)
function NetworkMgr:unset_conn_owner(conn_id, owner)
end

-- 设置(服务器)连接session，用于标记该连接是和哪个进程连接的
-- @param conn_id 连接id
-- @param session 会话id
function NetworkMgr:set_conn_session(conn_id, session)
end

-- 设置当前服务器的session
-- @param conn_id 连接id
-- @param session 当前服务器会话id
function NetworkMgr:set_curr_session(conn_id, session)
end

-- 重置协议描述文件
-- @param codec_type 编码方式(protobuf、flatbuffers)
function NetworkMgr:reset_schema(codec_type)
end

-- 加载某个目录下的所有schema文件(比如protobuf的pb文件)
-- @param codec_type 编码方式(protobuf、flatbuffers)
-- @param path 目录路径
function NetworkMgr:load_one_schema(codec_type, path)
end

-- 加载某个schema文件(比如protobuf的pb文件)
-- @param codec_type 编码方式(protobuf、flatbuffers)
-- @param path 文件路径
function NetworkMgr:load_one_schema_file(codec_type, path)
end

-- 获取http报文头数据
-- @param conn_id 连接id
-- @return upgrade code method fields
-- upgrade 是否为websocket
-- code http错误码
-- method http方式 GET POST
-- fields 报文各个字段(lua table,kv格式)
function NetworkMgr:get_http_header(conn_id)
end

-- 客户端向服务器发包
-- @param conn_id 连接id
-- @param cmd 协议号
-- @param pkt 数据包(lua table)
function NetworkMgr:send_srv_packet(conn_id, cmd, pkt)
end

-- 服务器向客户端发包
-- @param conn_id 连接id
-- @param cmd 协议号
-- @param errno 错误码
-- @param pkt 数据包(lua table)
function NetworkMgr:send_clt_packet(conn_id, cmd, errno, pkt)
end

-- 发送原始的数据包(该数据不会经过任何编码、打包，直接发送出去)
-- @param conn_id 连接id
-- @param content 数据(string)
function NetworkMgr:send_raw_packet(conn_id, content)
end

-- 服务器向服务器发包
-- @param conn_id 连接id
-- @param cmd 协议号
-- @param errno 错误码
-- @param pkt 数据包(lua table)
function NetworkMgr:send_s2s_packet(conn_id, cmd, errno, pkt)
end

-- 在非网关发送客户端数据包，该包会由网关直接转发给客户端
-- @param conn_id 连接id
-- @param cmd 协议号
-- @param errno 错误码
-- @param pkt 数据包(lua table)
function NetworkMgr:send_ssc_packet(conn_id, cmd, errno, pkt)
end

-- 发送rpc请求
-- @param conn_id 连接id
-- @param unique_id 唯一id，用于返回值回调
-- @param name 函数名
-- @param ... 可变参数
function NetworkMgr:send_rpc_packet(conn_id, unique_id, name, ...)
end

-- 发送ping-pong等控制数据包，仅连接为websocket时有效
-- @param conn_id 连接id
-- @param mask 控制掩码，如WS_OP_PING
-- @param ... 数据，根据不同的掩码不一样，可能是字符串或其他数据
function NetworkMgr:send_ctrl_packet(conn_id, mask, ...)
end

-- 广播到多个服务器
-- @param conn_list 连接id数组
-- @param codec_type 编码方式(protobuf、flatbuffers)
-- @param cmd 协议号
-- @param errno 错误码
-- @param pkt 数据包(lua table)
function NetworkMgr:srv_multicast(conn_list, codec_type, cmd, errno, pkt)
end

-- 网关进程广播数据到客户端
-- @param conn_list 连接id数组
-- @param codec_type 编码方式(protobuf、flatbuffers)
-- @param cmd 协议号
-- @param errno 错误码
-- @param pkt 数据包(lua table)
function NetworkMgr:clt_multicast(conn_list, codec_type, cmd, errno, pkt)
end

-- 非网关数据广播数据到客户端
-- @param conn_id 网关连接id
-- @param mask 掩码，见net_header.h clt_multicast_t定义
-- @param arg_list, 参数列表，如果掩码是根据连接广播，则该列表为连接id
-- 如果根据玩家id广播，则为玩家id
-- @param codec_type 编码方式(protobuf、flatbuffers)
-- @param cmd 协议号
-- @param errno 错误码
-- @param pkt 数据包(lua table)
function NetworkMgr:ssc_multicast(conn_id, mask, arg_list, codec_type, cmd, errno, pkt)
end

-- 设置发送缓冲区大小
-- @param conn_id 网关连接id
-- @param chunk_max 缓冲区数量
-- @param chunk_size 单个缓冲区大小
-- @param action 缓冲区溢出处理方式，见socket.h中OverActionType定义(1断开，2阻塞)
function NetworkMgr:set_send_buffer_size(conn_id, chunk_max, chunk_size, action)
end

-- 设置接收缓冲区大小
-- @param conn_id 网关连接id
-- @param chunk_max 缓冲区数量
-- @param chunk_size 单个缓冲区大小
function NetworkMgr:set_recv_buffer_size(conn_id, chunk_max, chunk_size)
end

-- 创建一个ssl上下文
-- @param sslv ssl版本，见ssl_mgr.h中的定义
-- @param cert_file 可选参数,服务端认证文件路径(通常不开启服务端认证)
-- @param key_type 服务端密钥类型(pem文件的加密类型，如RSA，见ssl_mgr.h中定义)
-- @param key_file 服务端密钥文件(pem文件路径)
-- @param password 密钥文件的密码
function NetworkMgr:new_ssl_ctx(sslv, cert_file, key_type, key_file, password)
end

-- 设置玩家当前所在的session(比如玩家在不同场景进程中切换时，需要同步玩家数据到场景)
-- @param pid 玩家id
-- @param session 会话id
function NetworkMgr:set_player_session(pid, session)
end

-- 设置、获取玩家当前所在的session
-- @param pid 玩家id
function NetworkMgr:get_player_session(pid)
end

-- 仅关闭socket，但不销毁内存
-- @param conn_id 网关连接id
-- @param flush 是否发送还在缓冲区的数据
function NetworkMgr:close(conn_id, flush)
end

-- 获取某个连接的ip地址
-- @param conn_id 网关连接id
function NetworkMgr:address(conn_id)
end

-- 监听连接
-- @param host 监听的主机，0.0.0.0或127.0.0.1
-- @param port 监听端口
-- @param conn_type 连接类型（cs、sc、ss），见socket.h中的定义
-- @return fd, message，成功只返回fd；失败fd为-1，则message为错误原因
function NetworkMgr:listen(host, port, conn_type)
end

-- 主动连接其他服务器
-- @param host 目标主机
-- @param port 目标端口
-- @param conn_type 连接类型（cs、sc、ss），见socket.h中的定义
function NetworkMgr:connect(host, port, conn_type)
end

-- 获取连接类型
-- @param conn_id 网关连接id
-- @return 连接类型，参考 ConnType 定义
function NetworkMgr:get_connect_type(conn_id)
end

return NetworkMgr
