---@diagnostic disable: missing-return

-- 导出模块: util
util = {}


--- 递归列出指定目录中的文件，和shell指令ls效果一致
---@param path linux下路径 path/to/dir
---@return table 指定目录下的文件数组(包括目录，但不包括.和..这两个特殊目录)
function util.ls(path)
end

--- 计算字符串的md5值
---@param upper 可选参数，返回值是否转换为大写
---@param ... 需要计算的字符串，可以值多个
---@return md5值
function util.md5(upper)
end

--- 产生一个新的uuid(Universally Unique Identifier)
---@param upper 可选参数，返回值是否转换为大写
---@return uuid
function util.uuid(upper)
end

--- 计算字符串sha1编码
---@param upper 可选参数，结果是否转换为大写
---@param ... 需要计算的字符串，可以多个，sha1(true, str1, str2, ...)
---@return sha1字符串
function util.sha1(upper)
end

--- 计算base64编码
---@param str 需要计算的字符串
---@return base6字符串
function util.base64(str)
end

--- 计算字符串的哈希值(使用 FNV-1a 算法)
--- 算法快速且分布均匀，适合作为一致性哈希、消息路由等场景的计算
---@param str 需要计算哈希的字符串
---@return 返回一个正整数哈希值
function util.hashstr(str)
end

--- 普通的取余路由（ID % 节点数），并且 ID 是自增的，那直接取余就可以了，因为自增的 ID 取余后本身就很均匀
--- 整数的快速均匀哈希 (基于 MurmurHash3 的 fmix64)
--- 用于解决连续ID在一致性哈希中聚集导致热点的问题
function util.hashint()
end

--- 计算字符串sha256，即sha2-256
---@param upper 可选参数，结果是否转换为大写
---@param ... 需要计算的字符串，可以多个，sha256(true, str1, str2, ...)
---@return sha2字符串
function util.sha256(upper)
end

--- 计算字符串sha3-256
---@param upper 可选参数，结果是否转换为大写
---@param ... 需要计算的字符串，可以多个，sha3-256(true, str1, str2, ...)
---@return sha3字符串
function util.sha3_256(upper)
end

function util.getcwd()
end

---@brief 获取当前程序(executable)的路径
function util.getexe()
end

--- 切换当前工作目录
function util.chdir()
end

function util.setenv()
end

--- 如果不存在则创建多层目录，和shell指令mkdir -p效果一致
---@param path linux下路径 path/to/dir
---@return boolean
function util.mkdir_p(path)
end

--- sha1编码，返回20byte的十六进制原始数据而不是字符串
---@param ... 需要计算的字符串，可以多个，sha1_raw(str1,str2,...)
---@return 20byte的十六进制原始sha1数据
function util.sha1_raw()
end

--- 取linux下errno或者win下WSAGetLastError对应的错误描述字符串
---@param error 错误码
---@return 错误描述字符串
function util.what_error(error)
end

--- 利用非标准base64编码uuid，产生一个22个字符串
---@return 22个字符的短uuid
function util.uuid_short()
end

---@brief 根据域名获取ip地址，此函数会阻塞
---@param addrs ip地址数组
---@param host 需要解析的域名
---@return 0成功
function util.get_addr_info(addrs, host, v4)
end

--- 解析一个由uuid_short产生的短uuid
---@param uuid 短uuid字符串
---@return 标准uuid
function util.uuid_short_parse(uuid)
end
