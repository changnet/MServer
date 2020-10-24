-- mysql.lua
-- 2015-11-20
-- xzc

-- mysql数据存储处理

local Sql = require "Sql"
local AutoId = require "modules.system.auto_id"

local Mysql = oo.class( ... )

function Mysql:__init( dbid )
    self.auto_id = AutoId()
    self.query   = {}
    self.sql     = Sql( dbid )
end

-- 定时器回调
function Mysql:do_timer()
   -- -1未连接或者连接中，0 连接失败，1 连接成功
   local ok = self.sql:valid()
   if -1 == ok then return end

   g_timer_mgr:stop( self.timer )
   self.timer = nil

   if 0 == ok then
       ERROR( "mysql connect error" )
       return
   end

   if self.conn_cb then
       self.conn_cb()
       self.conn_cb = nil
   end
end

function Mysql:read_event( qid,ecode,res )
    if self.query[qid] then
        xpcall( self.query[qid],__G__TRACKBACK,ecode,res )
        self.query[qid] = nil
    else
        ERROR( "mysql result no call back found" )
    end
end

function Mysql:start( ip,port,usr,pwd,db,callback )
    self.sql:start( ip,port,usr,pwd,db )

    -- 连接成功后回调
    if callback then
        self.conn_cb = callback
        self.timer = g_timer_mgr:interval( 1,1,-1,self,self.do_timer )
    end
end

function Mysql:stop()
    self.sql:stop()
end

function Mysql:exec_cmd( stmt )
    return self.sql:do_sql( 0,stmt )
end

function Mysql:select( this,method,stmt )
    local id = self.auto_id:next_id( self.query )
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
