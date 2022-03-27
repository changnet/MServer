-- mongodb_test.lua
-- 2016-03-07
-- xzc
-- mongo db 测试用例
local max_insert = 10000
local collection = "perf_test"
local json = require "engine.lua_parson"

t_describe("mongodb test", function()
    local mongodb
    local Mongodb
    local SyncMongodb

    -- require mongodb就会往app那边注册app_start和app_stop事件
    -- 但有时候并不需要执行mongodb test
    t_before(function()
        Mongodb = require "mongodb.mongodb"
        SyncMongodb = require "mongodb.sync_mongodb"
    end)

    t_it("mongodb base test", function()
        t_wait(10000)

        mongodb = Mongodb()

        local g_setting = require "setting.setting"
        mongodb:start(g_setting.mongo_ip, g_setting.mongo_port,
                      g_setting.mongo_user, g_setting.mongo_pwd,
                      g_setting.mongo_db, function()
            t_print(string.format("mongodb(%s:%d) ready ...",
                                  g_setting.mongo_ip, g_setting.mongo_port))

            mongodb:remove(collection, "{}") -- 清空已有数据

            local max_count = 10
            for i = 1, max_count do
                mongodb:insert(collection, {
                    _id = i,
                    amount = i * 100,
                    array = {"abc", "def", "ghj"},
                    sparse = {[5] = 998},
                    object = {name = "xzc", {9, 8, 7, 6, 5, 4, 3, 2, 1}},
                    desc = "test item",
                    op_time = ev:time()
                })
            end

            -- 查询全部
            mongodb:find(collection, "{}", nil, function(e, res)
                t_equal(e, 0)
                t_equal(#res, max_count)
            end)

            -- 通过json查询
            -- projection 表示哪些字段需要查询(1)或者不需要(0)
            -- sort 结果按给定字段升序(1)降序(-1)排列
            mongodb:find(collection, '{"_id":{"$gt":5}}',
                         '{"projection":{"desc":0,"array":0,"object":0,"sparse":0},\z
                    "skip":1,"limit":3,"sort":{"amount":-1}}', function(e, res)
                t_equal(e, 0)
                t_equal(#res, 3)
                t_equal(res[1]._id, 9)
                t_equal(res[1].desc, nil)
            end)
            -- 通过table查询（和上面的查询一样，只是换用table来描述）
            mongodb:find(collection, {_id = {["$gt"] = 5}}, {
                projection = {desc = 0, array = 0, object = 0, sparse = 0},
                skip = 1,
                limit = 3,
                sort = {amount = -1}
            }, function(e, res)
                t_equal(e, 0)
                t_equal(#res, 3)
                t_equal(res[1]._id, 9)
                t_equal(res[1].desc, nil)
            end)

            -- 查询并修改
            mongodb:find_and_modify(collection, '{"_id": 1}', nil,
                                    '{"$set":{"amount":101}}', nil, nil, nil,
                                    true, function(e, res)
                t_equal(e, 0)
                t_equal(res.lastErrorObject.n, 1)
                t_equal(res.lastErrorObject.updatedExisting, true)
                t_equal(res.value._id, 1)
                t_equal(res.value.amount, 101)
            end)

            -- 查询数量
            mongodb:count(collection, "{}", nil, function(e, res)
                t_equal(e, 0)
                t_equal(res.count, 10)
            end)
            mongodb:count(collection, '{"_id":{"$gt":5}}',
                          '{"skip":1,"limit":3}', function(e, res)
                t_equal(e, 0)
                t_equal(res.count, 3)
            end)

            -- 默认更新单个
            mongodb:update(collection, {_id = 10},
                           {["$set"] = {amount = 9999999}}, false, false,
                           function(e, res)
                t_equal(e, 0)
                t_equal(res, nil)
            end)
            -- upsert
            mongodb:update(collection, {_id = max_count + 1},
                           {["$set"] = {amount = 9999999}}, true, false,
                           function(e, res)
                t_equal(e, 0)
                t_equal(res, nil)
            end)
            -- 更新多个（更新_id小于5的）
            mongodb:update(collection, '{"_id":{"$lt":5}}',
                           '{"$set":{"amount":999999999}}', false, true,
                           function(e, res)
                t_equal(e, 0)
                t_equal(res, nil)
            end)
            mongodb:count(collection, '{"amount":{"$gt":10000}}', nil,
                          function(e, res)
                t_equal(e, 0)
                t_equal(res.count, 6)
            end)

            mongodb:remove(collection, "{}")
            mongodb:set_array_opt(8.6)

            -- 测试数组转换，保证Lua中的数字key存到数据库，读取出来后还能保持为数字key
            local array_tbl = {
                _id = 1111,
                array = {"abc", "def", "ghj"},
                object = {
                    a = "a",
                    b = "b",
                    c = "c",
                },
                mix_object = {
                    a = "a",
                    b = "b",
                    c = "c",
                    {1, 2, false, true, "abc"}
                },
                sparse_array = {
                    [10003] = {
                        [20003] = {
                            [40006] = true
                        }
                    }
                },
                special_key = {
                    -- 要注意，$和.是mongodb的保留符号，key里不能包含这两字符 !!!
                    -- [1.5] = true,
                    [math.mininteger] = {
                        [math.maxinteger] = {
                            [-99] = false
                        }
                    }
                },
            }
            mongodb:insert(collection, array_tbl)
            mongodb:find(collection, "{}", nil, function(e, res)
                t_equal(e, 0)
                t_equal(#res, 1)
                t_equal(res[1], array_tbl)
            end)

            -- 清空测试数据并结束测试
            mongodb:remove(collection, "{}")
            mongodb:find(collection, "{}", nil, function(e, res)
                t_equal(e, 0)
                t_equal(#res, 0)
                t_done()
            end)
        end)
    end)

    --[[
        默认配置下插入大概为5000 ~ 10000条/s，没有加索引，远超mysql
    ]]
    t_it(string.format("mongodb json insert %d perf test", max_insert),
         function()
        t_wait(20000)
        for i = 1, max_insert do
            local data = {}
            data._id = i
            data.amount = i * 100
            data.array = {"abc", "def", "ghj"}
            data.sparse = {[5] = 998}
            data.object = {name = "xzc", {9, 8, 7, 6, 5, 4, 3, 2, 1}}
            data.desc = "test item"
            data.op_time = ev:time()

            mongodb:insert(collection, json.encode(data))
        end

        mongodb:remove(collection, '{}', false, function(e)
            t_equal(e, 0)
            t_done()
        end)
    end)

    t_it(string.format("mongodb table insert %d perf test", max_insert),
         function()
        t_wait(20000)
        for i = 1, max_insert do
            local data = {}
            data._id = i
            data.amount = i * 100
            data.array = {"abc", "def", "ghj"}
            data.sparse = {[5] = 998}
            data.object = {name = "xzc", {9, 8, 7, 6, 5, 4, 3, 2, 1}}
            data.desc = "test item"
            data.op_time = ev:time()
            mongodb:insert(collection, data)
        end

        mongodb:remove(collection, "{}", false, function(e)
            t_equal(e, 0)
            t_done()
        end)
    end)

    t_it("mongodb coroutine sync test", function()
        t_wait(5000)
        local sync_mongodb = SyncMongodb(mongodb, function(sync_db)
            local e, res = sync_db:count(collection, '{}')
            t_equal(e, 0)
            t_equal(res.count, 0)
            t_done()
        end)
        sync_mongodb:start(sync_mongodb)
    end)

    t_after(function()
        mongodb:stop()
    end)
end)
