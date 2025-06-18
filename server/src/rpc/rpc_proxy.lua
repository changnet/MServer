-- Rpc的代理和委托功能

--[[
Lua的rpc调用，现在是基于字符串来实现的。把函数名、变量名传到远端，接收方收再从_G解析
出对应的函数和参数，进行调用。这种方法最大的优点就是不需要函数注册。

在接收方，要求被调用的函数必须是全局函数，或者通过某种机制能被Rpc通过字符串解析出来。而
实际情况中，有不少函数是成员函数。这就需要一个机制，把这些成员函数转为可识别函数。这个机
制就是委托（deletegate）

而在发送方，调用远程函数通常需要传地址，还经常因为参数不匹配需要转换一次。例如：
rcp调用为：Call.Player.get_name(WorkerRoute.get_player_addr(), pid)
这有两个问题，一是每次都需要用get_player_addr()获取地址。另一个是因为get_name是一个成
员函数，它需要一个player对象才能调用，于是只能再写一个函数来转换
function get_name_by_pid(pid)
    local player = PlayerMgr.get(pid)
    return player:get_name()
end
这是十分繁琐的，需要一个机制来统一处理这些东西。这个机制称为代理（proxy）

Rpc.proxy_wtype("MySql", W_MYSQL)
Mysql.insert() -- 代理之后，将可以在当前worker直接调用，默认是无返回的

-- 如果需要返回值，则需要用Sync来调用
-- TODO 这个写法有点坑，但我没想出更好的写法
-- Call.MySql.select()肯定不行，这个Call已经被定义为最基础的rpc调用，不能引出歧义
-- 如果支持像ts那样的写法就完美：local data = await MySql.select()
-- 但实现不了，只能用Sync来包一层
Sync.MySql.select()
]]

local g_lcodec = g_lcodec
local g_mthread = g_mthread
local WorkerHash = WorkerHash
local RPC_REQ = ThreadMessage.RPC_REQ
local lcodec_encode_to_buffer = g_lcodec.encode_to_buffer

local name_to_func = Rtti.name_to_func

-- 把一个对象委托给rpc，允许通过rpc调用该对象的成员函数
-- @param name 模块名
-- @param self 委托的对象
function Rpc.delegate(name, self)
    -- 暂不支持模块的委托，如果是模块，声明为global就可以调用，不需要委托

    -- 委托产生的模块不能覆盖已经存在的真实模块
    local mod = _G[name]
    if mod and not mod.__delegate then
        error("delegate module already exist")
    end

    if not mod then mod = {__delegate = true, __self = self} end
    local mt =
    {
        __index = function(tbl, k)
            local func = self[k]
            return function(...) return func(self, ...) end
        end,
    }

    setmetatable(mod, mt)
    _G[name] = mod
end

-- 给指定模块建立一个指定worker地址的rpc代理
-- @param name 模块名，如MySql，和远端模块名一致
-- @param addr 远端worker的地址
function Rpc.proxy_waddr(name, addr)
end

-- 给指定模块建立一个rpc代理
-- 调用时按worker_route自动选择一个worker，适合无状态要求的模块。
-- @param name 模块名，如MySql，和远端模块名一致
-- @param addr 远端worker的类型
function Rpc.proxy_wtype(name, wtype)
end


-- 通过pid发起rpc调用，目标为玩家所在player worker，第一个参数必须为pid
PidSend = {}

-- 通过pid发起rpc调用并获取返回值，目标为玩家所在player worker，第一个参数必须为pid
PidCall = {}

-- 通过pid发起rpc调用，目标为玩家所在player worker
-- 第一个参数必须为pid，调用的函数第一个参数为player
-- 如果玩家不在线，将会return nil，需要自己处理好异常
PlayerSend = {}

-- 通过pid发起rpc调用并获取返回值，目标为玩家所在player worker
-- 第一个参数必须为pid，调用的函数第一个参数为player
-- 如果玩家不在线，将会return nil，需要自己处理好异常
PlayerCall = {}

local call_return = Rpc.call_return

local function pid_send_func_factory(name)
    return function(pid, ...)
        local addr = WorkerRoute.get_player_addr(pid)
        if not addr then error("no player address found") end

        local w = WorkerHash[addr] or g_mthread
        local ptr, size = lcodec_encode_to_buffer(g_lcodec, 0, name, pid, ...)
        return w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)
    end
end

local function pid_call_func_factory(name)
    return function(pid, ...)
        local addr = WorkerRoute.get_player_addr(pid)
        if not addr then error("no player address found") end

        local w = WorkerHash[addr] or g_mthread

        -- 每个协程需要分配一个session，这样返回时才知道唤醒哪个协程
        local session = CoPool.current_session()
        local ptr, size = lcodec_encode_to_buffer(
            g_lcodec, session, name, pid, ...)
        w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)

        return call_return(coroutine.yield())
    end
end

local function invoke_player_call(name, pid, ...)
    local player = PlayerMgr.get_player(pid)
    if not player then return end

    local func = name_to_func(name)
    if not func then error("no such function found:" .. tostring(name)) end
    return func(player, ...)
end


local function player_send_func_factory(name)
    return function(pid, ...)
        local addr = WorkerRoute.get_player_addr(pid)
        if not addr then error("no player address found") end

        local w = WorkerHash[addr] or g_mthread
        local ptr, size = lcodec_encode_to_buffer(
            g_lcodec, 0, "invoke_player_call", name, pid, ...)
        return w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)
    end
end

local function player_call_func_factory(name)
    return function(pid, ...)
        local addr = WorkerRoute.get_player_addr(pid)
        if not addr then error("no player address found") end

        local w = WorkerHash[addr] or g_mthread

        -- 每个协程需要分配一个session，这样返回时才知道唤醒哪个协程
        local session = CoPool.current_session()
        local ptr, size = lcodec_encode_to_buffer(
            g_lcodec, session, "invoke_player_call", name, pid, ...)
        w:emplace_message(LOCAL_ADDR, addr, RPC_REQ, ptr, size)

        return call_return(coroutine.yield())
    end
end


Rtti.name_func("invoke_player_call", invoke_player_call)
Rpc.set_metatable(PidCall, pid_call_func_factory)
Rpc.set_metatable(PidSend, pid_send_func_factory)
Rpc.set_metatable(PlayerCall, player_call_func_factory)
Rpc.set_metatable(PlayerSend, player_send_func_factory)
