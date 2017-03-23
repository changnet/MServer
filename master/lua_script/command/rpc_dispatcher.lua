-- rpc分发

-- 所有rpc接口必须在起服时注册并分发到各个进程。否则rpc_dispatcher在收到rpc请求时无法
-- 分发到对应的进程。

local Rpc_dispatcher = oo.singleton( nil,... )

local rpc_dispatcher = Rpc_dispatcher()

function Rpc_dispatcher:__init()
    self.responser = {}
end

-- 本进程可以接受哪些rpc请求，在与其他进程连接时会进行广播
function Rpc_dispatcher:register( name,responser )
    assert( responser,"rpc responser can not be nil" )
    assert( "string" == type(name),"rpc responser name must be string" )
    self.responser[name] = responser
end

return rpc_dispatcher
