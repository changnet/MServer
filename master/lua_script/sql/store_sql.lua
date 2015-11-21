-- store_sql.lua
-- 2015-11-20
-- xzc

-- mysql数据存储处理

local Sql = require "Sql"

local Store_sql = oo.class( nil,... )

function Store_sql:__init()
    self.next_id = 1
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

function Store_sql:on_result()
    local id,err,result = self._sql_:get_result()
    while id do
        local callback = self.query[id]
        if not callback then
            ELOG( "sql result not handle" )
        else
            callback()
        end

        id,result = self._sql_:get_result()
    end
end

function Store_sql:start()
    self._sql_:start( "127.0.0.1",3306,"test","test","mudrv_xzc" )
end

function Store_sql:do_sql( sql,callback,... )
    self._sql_:do_sql( next_id,sql,callback )
    if callback then
        local args = { ... }
        self.query[next_id] = function()
            xpcall( callback,__G__TRACKBACK__,table.unpack(args) )
        end
    end
end

return Store_sql
