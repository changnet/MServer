-- mongo_performance.lua
-- 2016-03-07
-- xzc

-- mongo db 测试用例

g_mongodb_mgr = require "mongodb.mongodb_mgr"
g_mongodb = g_mongodb_mgr:new()

g_mongodb:start( "127.0.0.1",27013,"test","test","mudrv" )

local max_insert = 100000
local collection = "item"
local json = require "lua_parson"

local Mongo_performan = {}

--[[
    mongo 127.0.0.1:27013
    use admin
    db.createUser( {user:"xzc",pwd:"1",roles:["userAdminAnyDatabase","dbAdminAnyDatabase"]} )
    use mudrv
    db.createUser( {user:"test",pwd:"test",roles:["dbOwner","userAdmin","readWrite"]} )
测试插入数据，默认配置下大概为10000条/s
]]
function Mongo_performan:table_insert_test()
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
        g_mongodb:insert( collection,data )
    end
    f_tm_stop( "insert done" )
end

-- 测试count
function Mongo_performan:count_test()
    print( "start count test" )
    local callback = function( ... )
        self:on_count_test( ... )
    end
    g_mongodb:count( collection,nil,nil,callback )
end

function Mongo_performan:on_count_test( ecode,res )
    print( "now mongo ",collection,"count return ",ecode )
    vd( res )
end

function Mongo_performan:json_insertion_test()
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
        g_mongodb:insert( collection,str )
    end
    f_tm_stop( "insert done" )
end

function Mongo_performan:find_test()
    print( "start find test" )

    local callback = function( ... )
        self:on_find_test( ... )
    end
    g_mongodb:find( '{"_id":{"$gt":20}}',
        '{"projection":{"_id":0},"skip":10,"limit":10}',callback )
end

function Mongo_performan:on_find_test( ecode,res )
    print( "now mongo ",collection,"find return ",ecode )
    vd( res )
end

function Mongo_performan:update_test()
    print( "start update test" )
    g_mongodb:update( collection,
        '{"_id":{"$lt":5}}','{"$set":{"amount":999999999}}',false,true )
end

-- db.collection.createIndex( {amount:1} ) 需要先去数据库创建索引，不然引起
-- Overflow sort stage buffered data usage of 33598393 bytes exceeds internal limit of 33554432 bytes
-- 32M内存限制(db.item.getIndexes()来验证索引是否存在)
function Mongo_performan:sort_test()
    print( "start sort test" )
    -- -1是降序，+1是升序。不能是0
    local callback = function( ... )
        self:on_sort_test( ... )
    end
    g_mongodb:find( '{}',
        '{"projection":{"desc":0,"array":0,"object":0,"sparse":0},"limit":50,"sort":{"amount":-1}}',
        nil,nil,nil,callback )
end

function Mongo_performan:on_sort_test( ecode,res )
    print( "now mongo ",collection,"sort return ",ecode )
    vd( res )
end

function Mongo_performan:remove_test()
    print( "start remove test" )
    g_mongodb:remove( collection,'{"_id":{"$gt":20,"$lt":30}}',true )
end

Mongo_performan:table_insert_test()
Mongo_performan:count_test()
Mongo_performan:json_insertion_test()
Mongo_performan:find_test()
Mongo_performan:update_test()
Mongo_performan:sort_test()
Mongo_performan:remove_test()
