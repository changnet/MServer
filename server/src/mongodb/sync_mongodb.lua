-- sync_mongodb.lua
-- 2018-02-28
--xzc

-- 为了能判断coroutine是否出错，又要能够返回可变参数，这里
-- 要wrap一层，把返回值变成...参数
local function after_coroutine_resume( co,ok,args,... )
    if ok then return args,... end

    __G__TRACKBACK( args,co )
    return false
end

local function after_coroutine_start( co,ok,args,... )
    if ok then return ok,args,... end

    __G__TRACKBACK( args,co )
    return false
end

--//////////////////////////////////////////////////////////////////////////////

-- TODO 这个实现很繁琐，只是尝试一下
-- 1. 由于框架的其他逻辑不是在coroutine中执行的，创建对象时需要传入routine函数
-- 2. wrap原有Mongodb的find、count这些函数需要重写一次，不太优雅

-- 用coroutine来封装一套接近同步操作的数据库接口
local SyncMongodb = oo.class( ... )

function SyncMongodb:__init(mongodb, routine)
    local co = coroutine.create( routine )

    self.co = co
    self.mongodb = mongodb
    self.callback = function(e, res)
        return after_coroutine_resume(co,coroutine.resume(co, e,res))
    end
end

function SyncMongodb:start( ... )
    return after_coroutine_start( self.co,coroutine.resume( self.co,... ) )
end

-- 是否有效
function SyncMongodb:valid()
    return "dead" ~= coroutine.status( self.co )
end

-- 这些数据库操作接口同mongodb.lua中的一样

function SyncMongodb:count( collection,query,opts )
    self.mongodb:count( collection,query,opts,self.callback )

    return coroutine.yield()
end

function SyncMongodb:find( collection,query,opts )
    self.mongodb:find( collection,query,opts,self.callback )

    return coroutine.yield()
end

return SyncMongodb
