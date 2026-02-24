---@diagnostic disable: missing-return

-- 导出类: IO
IO = {}
---@brief 设置ssl的sni(service name indicator)
---@param sni service name indicator
function IO:set_ssl_sni(sni)
end

---@brief 设置ssl的alpn(Application-Layer Protocol Negotiation )
---@param alpn 应该层协议协商
function IO:set_ssl_alpn(alpn)
end

---@brief 设置ssl的证书host
---@param host ssl证书对应的地址
function IO:set_ssl_cert_host(host)
end

---@brief 设置ssl的验证模式
---@param mode 值必须对应 SSL_VERIFY_PEER 等宏定义
function IO:set_ssl_verify_mode(mode)
end
