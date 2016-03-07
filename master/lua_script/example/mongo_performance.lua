-- mongo_performance.lua
-- 2016-03-07
-- xzc

-- mongo db 测试用例

local json = require "lua_parson"
g_store_mongo:start( "127.0.0.1",27013,"test","test","mudrv" )

local collection = "item"
local max_insert = 100000

print( "start insert ",max_insert,"to mongo db with lua table" )
f_tm_start()
for i = 1,max_insert do
    local data = {}
    data._id    = i
    data.amount = i * 100
    data.array  = { "abc","def","ghj" }
    data.sparse = { [5] = 998 }
    data.object = { name = "xzc",{9,8,7,6,5,4,3,2,1} }
    data.desc   = "test item"
    data.op_time = ev:time()
    --g_store_mongo:insert( collection,data )
end
f_tm_stop( "insert done" )

g_store_mongo:count( function( err,info )
        print( "now mongo ",collection,"count return ",err )
        vd( info )
    end,
    collection
    )

print( "start insert ",max_insert,"to mongo db with json string" )
f_tm_start()
for i = max_insert + 1,max_insert*2 do
    local data = {}
    data._id   = i
    data.amount = i * 100
    data.array  = { "abc","def","ghj" }
    data.sparse = { [5] = 998 }
    data.object = { name = "xzc",{9,8,7,6,5,4,3,2,1} }
    data.desc   = "test item"
    data.op_time = ev:time()
    
    local str = json.encode( data )
    --g_store_mongo:insert( collection,str )
end
f_tm_stop( "insert done" )

g_store_mongo:count( function( err,info )
        print( "now mongo ",collection,"second count return ",err )
        vd( info )
    end,
    collection
    )

g_store_mongo:find( function( err,info )
        print( "now mongo ",collection,"find return ",err )
        vd( info )
    end,
    collection,
    '{"_id":{"$gt":20}}','{"_id":0}',10,10
    )

g_store_mongo:update( collection,'{"_id":{"$lt":5}}','{"$set":{"amount":999999999}}',false,true )

-- db.collection.createIndex( {amount:1} ) 需要先去数据库创建索引，不然引起
-- Overflow sort stage buffered data usage of 33598393 bytes exceeds internal limit of 33554432 bytes
-- 32M内存限制
g_store_mongo:find( function( err,info )
        print( "now mongo ",collection,"find (orderby) return ",err )
        vd( info )
    end,
    collection, -- -1是降序，+1是升序。不能是0
    '{"$query":{},"$orderby":{"amount":-1}}','{"desc":0,"array":0,"object":0,"sparse":0}',1,50
    )

g_store_mongo:remove( collection,'{"_id":{"$gt":20,"$lt":30}}',true )

g_store_mongo:count( function( err,info )
        print( "now mongo ",collection,"third count return ",err )
        vd( info )
    end,
    collection
    )
