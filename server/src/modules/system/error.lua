-- 错误码定义，方便出问题时查找问题
local E = {
    OK            = 0, -- 成功，无错误
    UNDEFINE      = 1, -- 未定义，如果出错了，又不关心是什么错，可以用这个
    SRV_NOT_READY = 2, -- 服务器未启动完成
    SRV_ERROR     = 3, -- 服务器内部错误
}

return E
