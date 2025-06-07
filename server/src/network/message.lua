-- 网络消息
Message = {}

local callback = {} -- 回调数据
local local_type = LOCAL_TYPE -- 当前worker的类型

-- 注册网络消息回调
-- @param id 协议id
-- @param func 回调函数
-- @param wtype 仅在该类型的worker上回调
function Message.reg(id, func, wtype)
    if local_type ~= wtype then return end

    assert(not callback[id])
    callback[id] = {
        f = func,
        w = wtype
    }
end

-- 派发客户端网络消息
-- @param buffer 消息二进制数据
-- @param size 消息大小
function Message.dispatch(socket, id, buffer, size)
    local cb = callback[id]
    if not cb then
        printf("no callback for %d", id)
        return
    end

    if local_type == cb.w then
        return cb.f(socket, buffer, size)
    end
end

return Message
