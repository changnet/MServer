-- authorize.lua
-- 2018-05-19
-- xzc

-- 授权管理

-- 用来管理玩家授权，只有已授权的玩家的指令才会派发，除非该指令不需要授权(比如登录)

-- 玩家未进行授权时(比如说登录失败，或者服务端踢掉玩家)，其他进程与网关的交互会有延时，服务端
-- 可能还会收到客户端的协议，但这时候玩家的数据是不正确的或者无数据，这时候对应的逻辑可能会产
-- 生大量错误。这里就是要防止这种情况

local Authorize = oo.singleton( ... )

function Authorize:__init()
    self.pid = {}
end

-- 获取玩家授权数据
-- 如果每次派发协议都来这里查询，函数调用太频繁，效率太慢
function Authorize:get_player_data()
    return self.pid
end

-- 给玩家授权
function Authorize:set_player( pid )
    self.pid[pid] = true
end

-- 取消玩家授权
function Authorize:unset_player( pid )
    self.pid[pid] = nil
end

local authorize = Authorize()

return authorize
