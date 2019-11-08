-- http接口相关宏定义

-- http接口错误码
HTTPE = 
{
    OK = { 0,"success" }, -- 0 成功,返回 succes 字符串
    OK_NIL = { 0 }, -- 0, 成功，返回的内容由对应的模块构建，不根据错误码构建
    PENDING = { 1 }, -- 阻塞，等待异步返回数据
    INVALID = { 2,"invalid arguments" }, -- 1 无效的gm
}
