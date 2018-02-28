-- mongodb_sync.lua
-- 2018-0228
--xzc 

-- 用coroutine来封装一套接近同步操作的数据库接口

local function after_coroutine_resume( co,ok,args,... )
    if ok then return args,... end

    -- __G__TRACKBACK__( args,co )
    print( args,debug.traceback( co ))

    print( debug.traceback() )
    return false
end

local Mongodb_sync = oo.class( nil,... )

function Mongodb_sync:__init( mongodb,co )
    self.co = co
    self.mongodb = mongodb
    self.callback = function( ecode,res )
        print( "call back =========================" )
        return after_coroutine_resume ( co,coroutine.resume( co,ecode,res ) )
    end
end

function Mongodb_sync:start( ... )
    local ok,msg = coroutine.resume( self.co,... )
    return after_coroutine_resume( self.co,msg )
end

-- 这些数据库操作接口同mongodb.lua中的一样

function Mongodb_sync:count( collection,query,skip,limit )
    self.mongodb:count( collection,query,skip,limit,self.callback )

    return coroutine.yield()
end

function Mongodb_sync:find( collection,query,opts )
    self.mongodb:find( collection,query,opts,self.callback )
print( "find yield =========================" )
    return coroutine.yield()
end

return Mongodb_sync
