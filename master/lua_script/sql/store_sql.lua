-- store_sql.lua
-- 2015-11-20
-- xzc

-- mysql数据存储处理

local Sql = require "Sql"
local LIMIT = require "global.limits"

local Store_sql = oo.class( nil,... )

function Store_sql:__init()
    self._next_id = 0
    self.query   = {}
    self._sql_   = Sql()

    self._sql_:self_callback( self )
    self._sql_:read_callback( self.on_result )
    self._sql_:error_callback( self.on_error )
end

function Store_sql:on_error()
    ELOG( "store sql error,system exit ..." )
    -- TODO exit
end

--[[
底层id类型为int32，需要防止越界
]]
function Store_sql:next_id()
    if self._next_id >= LIMIT.INT32_MAX then
        self._next_id = 0
    end

    self._next_id = self._next_id + 1
    return self._next_id
end

function Store_sql:on_result()
    local id,err,result = self._sql_:next_result()
    while id do
        local callback = self.query[id]
        self.query[id] = nil
        if not callback then
            ELOG( "sql result not handle" )
        else
            callback()
        end

        id,result = self._sql_:get_result()
    end
end

function Store_sql:start( ip,port,usr,pwd,db )
    self._sql_:start( ip,port,usr,pwd,db )
end

function Store_sql:stop()
    self._sql_:stop()
    self._sql_:join()
end

function Store_sql:do_sql( sql,callback,... )
    local id = self:next_id()
    self._sql_:do_sql( id,sql,callback )

    if callback then
        local args = { ... }
        self.query[id] = function()
            xpcall( callback,__G__TRACKBACK__,table.unpack(args) )
        end
    end
end

return Store_sql
