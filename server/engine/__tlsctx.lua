---@diagnostic disable: missing-return

-- 导出类: TlsCtx
TlsCtx = {}
--- 初始化一个ssl上下文
---@param cert_file ca证书文件路径
---@param key_file 私钥文件路径
---@param passwd 私钥文件密码，如果无密码则为null
---@param ca_path 可信的ca证书路径，用于校验对方证书是否有效。一般是系统路径，如/etc/ssl/certs/ca-certificates.crt
---@return 0成功
function TlsCtx:init(cert_file, key_file, passwd, ca_path)
end
