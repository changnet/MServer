-- Rpc的代理和委托功能

--[[
proxy、agent、delegate的区别

假如现在启动了10个game worker负责游戏逻辑，3个data worker负责数据缓存，3个mongodb worker负责数据读写
现在game需要调用mongodb读取数据，但由于mongodb是由data负责分配，因此game必须先请求data，
再由data分配一个mongodb节点去读数据。于是

1. game创建一个proxy("MongoDB", W_DATA)，这样就能直接用MongoDB发起调用MongoDB.find()，
    这个请求会发给一个data节点
2. data本身不存在MongoDB模块，它创建一个agent("MongoDB", W_MONGODB)，
    在收到MongoDB请求时，会根据路由机制分配一个mongodb节点并把请求转发给这个节点
3. mongodb中，实际不存在MongoDB这个模块，它只有一个g_mongodb实例。这时候delegate("MongoDB", g_mongodb)
    将会接管所有MongoDB的请求，将以g_mongodb实体调用find函数，然后返回结果


Rpc.proxy_wtype("MySql", W_MYSQL)
Mysql.insert() -- 代理之后，将可以在当前worker直接调用，默认是无返回的

-- 如果需要返回值，则需要用Await来调用（TODO 这个写法有点坑，但我没想出更好的写法）
-- Call.MySql.select()肯定不行，这个Call已经被定义为最基础的rpc调用，不能引出歧义
-- 如果支持像ts那样的写法就完美：local data = await MySql.select()
-- 但实现不了，只能用Await来包一层Await.MySql.select()
]]

local send = Rpc.send
local call = Rpc.call
local name_to_func = Rtti.name_to_func

-- 调用函数并等待返回值，类似Call但用于Rpc.proxy_wtype代理过后的函数
Await = {}

local proxy = {} -- 通过proxy创建的模块

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

-- 给指定模块建立一个rpc代理，调用时按路由选择一个worker(需要支持默认路由)
-- @param name 模块名，如MySql，和远端模块名一致
-- @param addr 远端worker的类型
function Rpc.proxy_wtype(name, wtype)
    -- 每次热更Await会重置，如果不是则返回旧模块
    local mod = proxy[name]
    if mod then return mod end

    -- TODO 是否要放到全局，这个写法待定
    -- 可以在对应的模块文件上 MongoDB = Rpc.proxy_wtype("MongoDB", W_MONGODB)这样写
    -- 如果这个模块很通用，应该在module_loader中定义而不是直接在proxy里直接设置为全局

    -- 另外，Await这个是不是要弄一个local MongoDB = Rpc.await_wtype(name, wtype)
    -- 避免全局调用Await模块

    mod = Rpc.set_metatable({}, function (fname)
        return function(pid, ...)
            local addr = Router.find_worker_addr(wtype)
            if not addr then
                error(string.format(
                    "no suitable addr for %s, wtype = %d, func = %s",
                    name, wtype, fname))
            end

            return send(addr, fname, pid, ...)
        end
    end)
    proxy[name] = mod
end

-- 创建同步返回模块代理，调用时按路由选择一个worker(需要支持默认路由)
function Rpc.await_wtype(name, wtype)
    local mod = Await[name]
    if mod then return mod end

    mod = Rpc.set_metatable({}, function (fname)
        return function(pid, ...)
            local addr = Router.find_worker_addr(wtype)
            if not addr then
                error(string.format(
                    "no suitable addr for %s, wtype = %d, func = %s",
                    name, wtype, fname))
            end

            return call(addr, fname, pid, ...)
        end
    end)

    Await[name] = mod
    return mod
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

local function pid_send_func_factory(name)
    return function(pid, ...)
        local addr = Router.find_player_addr(pid)
        if not addr then error("no player address found") end

        return send(addr, name, pid, ...)
    end
end

local function pid_call_func_factory(name)
    return function(pid, ...)
        local addr = Router.find_player_addr(pid)
        if not addr then error("no player address found") end

        return call(addr, name, pid, ...)
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
        local addr = Router.find_player_addr(pid)
        if not addr then error("no player address found") end

        return send(addr, "invoke_player_call", name, pid, ...)
    end
end

local function player_call_func_factory(name)
    return function(pid, ...)
        local addr = Router.find_player_addr(pid)
        if not addr then error("no player address found") end

        return call(addr, "invoke_player_call", name, pid, ...)
    end
end


Rtti.name_func("invoke_player_call", invoke_player_call)
Rpc.set_metatable(PidCall, pid_call_func_factory)
Rpc.set_metatable(PidSend, pid_send_func_factory)
Rpc.set_metatable(PlayerCall, player_call_func_factory)
Rpc.set_metatable(PlayerSend, player_send_func_factory)
