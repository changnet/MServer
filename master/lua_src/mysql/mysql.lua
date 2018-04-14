-- mysql.lua
-- 2015-11-20
-- xzc

-- mysql数据存储处理

local Sql = require "Sql"
local LIMIT = require "global.limits"

local Mysql = oo.class( nil,... )

function Mysql:__init( dbid )
    self.next_id = 0
    self.query   = {}
    self.sql     = Sql( dbid )
end

--[[
底层id类型为int32，需要防止越界
]]
function Mysql:get_next_id()
    repeat
        if self.next_id >= LIMIT.INT32_MAX then self.next_id = 0 end

        self.next_id = self.next_id + 1
    until nil == self.query[self.next_id]

    return self.next_id
end

function Mysql:read_event( qid,ecode,res )
    if self.query[qid] then
        xpcall( self.query[qid],__G__TRACKBACK__,ecode,res )
        self.query[qid] = nil
    else
        ELOG( "mysql result no call back found" )
    end
end

function Mysql:start( ip,port,usr,pwd,db )
    self.sql:start( ip,port,usr,pwd,db )
end

function Mysql:stop()
    self.sql:stop()
end

function Mysql:exec_cmd( stmt )
    return self.sql:do_sql( 0,stmt )
end

function Mysql:select( this,method,stmt )
    local id = self:get_next_id()
    self.query[id] = function( ... ) return method( this,... ) end

    self.sql:do_sql( id,stmt )
end

function Mysql:insert( stmt )
    return self.sql:do_sql( 0,stmt )
end

function Mysql:update( stmt )
    return self.sql:do_sql( 0,stmt )
end

function Mysql:call( stmt )
    -- 调用存储过程，未实现(注意分为有返回，无返回两种)
    assert( false )
end

-- 直接将lua table转换为sql，支持二进制及特殊字符
-- 如果fields为nil，则取values中的Key作为mysql字段名
function Mysql:insert_ex( tbl_name,values,fields )
    assert( false ) -- 未实现
end

function Mysql:update_ex( tbl_name,values,fields )
    assert( false )
end

return Mysql
