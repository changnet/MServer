-- 网络消息
Message = {}

local callback = {} -- 回调数据
local local_type = LOCAL_TYPE -- 当前worker的类型
local CLT_MSG = ThreadMessage.CLT_MSG
local forward = ThreadMessage.forward

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

-- 注册不需要认证(未完成登录流程)的网络消息回调
-- @param id 协议id
-- @param func 回调函数
-- @param wtype 仅在该类型的worker上回调
function Message.reg_noauth(id, func, wtype)
    if local_type ~= wtype then return end

    assert(not callback[id])
    callback[id] = {
        f = func,
        w = wtype,
        n = true, -- noauth
    }
end

local function do_callback()
end

-- 派发客户端网络消息
-- @param buffer 消息二进制数据
-- @param size 消息大小
function Message.dispatch(socket, id, buffer, size)
    local cb = callback[id]
    if not cb then
        printf("message dispatch no callback for %d", id)
        return
    end

    if not socket.auth and not cb.n then
        eprintf("%s socket not auth for message %d", socket.account, id)
        return
    end

    if local_type == cb.w then
        return cb.f(socket, buffer, size)
    end
    -- 对于player scene等类型的worker，存在多个，需要根据玩家当前在哪个worker进行转发
    local addr = get_player_addr(socket.pid)

    -- 注意： buffer里是包含cmd的
    return forward(addr, CLT_MSG, buffer, size)
end

local function dispatch_clt_message(src, udata, size)
    local cb = callback[id]
    if not cb then
        printf("dispatch_clt_message callback for %d", id)
        return
    end
end

ThreadMessage.reg(CLT_MSG, dispatch_clt_message)

return Message
