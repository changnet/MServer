-- util
-- auto export by engine_api.lua do NOT modify!

util = {}

-- 获取1970年以来的时间
-- clock_gettime(CLOCK_MONOTONIC)是内核调用，并且不受time jump影响
-- gettimeofday是用户空间调用
-- @return 秒 微秒
function util.timeofday()
end

-- 以阻塞方式获取域名对应的ip
-- @param name 域名，如：www.openssl.com
-- @return ip1 ip2 ...
function util.gethost(name)
end

-- 计算字符串的md5值
-- @param upper 可选参数，返回值是否转换为大写
-- @param ... 需要计算的字符串，可以值多个
-- @return md5值
function util.md5(upper, ...)
end

-- 产生一个新的uuid(Universally Unique Identifier)
-- @param upper 可选参数，返回值是否转换为大写
-- @return uuid
function util.uuid(upper)
end

-- 利用非标准base64编码uuid，产生一个22个字符串
-- @return 22个字符的短uuid
function util.uuid_short()
end

-- 解析一个由uuid_short产生的短uuid
-- @param uuid 短uuid字符串
-- @return 标准uuid
function util.uuid_short_parse(uuid)
end

-- 取linux下errno对应的错误描述字符串
-- @param error 错误码
-- @return 错误描述字符串
function util.what_error(error)
end

-- 计算字符串sha1编码
-- @param upper 可选参数，结果是否转换为大写
-- @param ... 需要计算的字符串，可以多个，sha1(true, str1, str2, ...)
-- @return sha1字符串
function util.sha1(upper, ...)
end

-- sha1编码，返回20byte的十六进制原始数据而不是字符串
-- @param ... 需要计算的字符串，可以多个，sha1_raw(str1,str2,...)
-- @return 20byte的十六进制原始sha1数据
function util.sha1_raw(...)
end

-- 计算base64编码
-- @param str 需要计算的字符串
-- @return base6字符串
function util.base64(str)
end

-- 获取当前进程pid
-- @return 当前进程pid
function util.get_pid()
end

-- 如果不存在则创建多层目录，和shell指令mkdir -p效果一致
-- @param path linux下路径 path/to/dir
-- @return boolean
function util.mkdir_p(path)
end

