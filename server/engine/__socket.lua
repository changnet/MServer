---@diagnostic disable: missing-return

-- 导出类: Socket
Socket = {}
Socket.PT_HTTP = 1
Socket.PT_SSSTREAM = 2
Socket.PT_WEBSOCKET = 3
Socket.PT_WSSTREAM = 4

function Socket:fd()
end

--- 启动socket事件监听
---@param addr 当前worker地址
---@param fd 文件描述符
---@param ev 需要监听的事件 EV_READ等
function Socket:start(addr, fd, ev)
end

--- 停止socket事件监听(等待另一个线程处理完)并自动close
---@param flush 发送缓冲区中的数据再停止
---@return 是否成功（如果当前socket未曾调用start，则会停止失败）
function Socket:stop(flush)
end

---@brief 开始监听链接
---@param addr worker的地址
---@param host 监听的ip
---@param port 监听的端口
---@return 文件描述符，错误返回-1
function Socket:listen(addr, host, port)
end

---@brief 发起连接
---@param addr worker的地址
---@param host 连接的ip
---@param port 连接的端口
---@return 文件描述符，错误返回-1
function Socket:connect(addr, host, port)
end

--- 尝试接受一个fd
---@return 成功返回fd，失败返回-1
function Socket:accept()
end

function Socket:get_event()
end

---@brief 设置当前socket的事件
---@param ev 事件，例如EV_READ
function Socket:set_event(ev)
end

---@brief 设置io读写的参数
---@param io_type io读写方式，io或者ssl_io
---@param ls_ctx 如果为ssl_io，需要指定ssl指针
function Socket:set_io(io_type, tls_ctx)
end

function Socket:get_io()
end

---@brief 设置消息打包的类型
---@param packet_type PT_HTTP等类型
function Socket:set_packet(packet_type)
end

function Socket:is_remote_close()
end

--- 验证connect是否成功
---@return 成功0 失败返回非0错误码
function Socket:is_connect_success()
end

---@brief 设置缓冲区的参数
---@param send_max 发送缓冲区大小（字节）
---@param recv_max 接收缓冲区大小（字节）
---@param mask 缓冲区溢出时处理方式
function Socket:set_buffer_params(send_max, recv_max, mask)
end

function Socket:io_init_accept()
end

function Socket:io_init_connect()
end

function Socket:send_pkt()
end

function Socket:send_clt()
end

function Socket:send_srv()
end

function Socket:send_ctrl()
end

---@brief 获取ip地址及端口
---@return ip地址, 端口
function Socket:address()
end

--- 获取http头数据，仅当前socket的packet为http有效
function Socket:get_http_header()
end

function Socket:unpack_on_closed()
end

function Socket:get_errno()
end

--- 解析收到的数据，根据不同的packet类型返回不同数据
---@return 类型编码，数据1，数据2，数据3，...
function Socket:unpack()
end

--- 收到另一个线程关闭事件时，关闭socket
--- 不要在业务逻辑中调用此函数
---@return 错误码
function Socket:close()
end

---@brief 启用TCP的keep alive
---@param time 多少秒无通信后开始发送keep alive包
---@param interval 间隔多少秒发一次
---@param probes 总共发多少次
function Socket:set_keep_alive(time, interval, probes)
end

---@brief 启用TCP的 user timeout，windows无效
---@param timeout 单位milliseconds
function Socket:set_user_timeout(timeout)
end

--- 启用或者关闭TCP_NODELAY选项
---@param opt 1启用nodelay 0关闭
function Socket:set_nodelay(opt)
end

---@brief 设置socket读写的事件
---@param events 事件，例如EV_READ
function Socket:set_watcher_event(events)
end

---@brief 设置当前socket的版本
---@param version 0=ipv4，1=ipv6，2=ipv6双栈
function Socket:set_ip_version(version)
end
