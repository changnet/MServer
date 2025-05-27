-- mongodb_test.lua
-- 2016-03-07
-- xzc
-- mongo db 测试用例
local max_insert = 10000
local collection = "perf_test"
local json = require "engine.lua_parson"

Test.describe("mongodb test", function()
    local MongoDB = require "mongodb.mongodb"
    local mongodb

    -- require mongodb就会往app那边注册app_start和app_stop事件
    -- 但有时候并不需要执行mongodb test
    Test.before(function()
        mongodb = MongoDB()

        local e = mongodb:connect(
            g_setting.mongo_ip, g_setting.mongo_port,
            g_setting.mongo_user, g_setting.mongo_pwd, g_setting.mongo_db)

        if 0 ~= e then print(mongodb:error()) end
        Test.equal(e, 0)
    end)


    Test.it("mongodb base test", function()
        mongodb:remove(collection, MongoDB.REMOVE_NONE, "{}") -- 清空已有数据

        local e, row, rows
        local max_count = 10
        for i = 1, max_count do
            e = mongodb:insert(collection, {
                _id = i,
                amount = i * 100,
                array = {"abc", "def", "ghj"},
                sparse = {[5] = 998},
                object = {name = "xzc", {9, 8, 7, 6, 5, 4, 3, 2, 1}},
                desc = "test item",
                op_time = Engine.time(),
            })
            Test.equal(e, 0)
        end

         -- 查询全部
        e, rows = mongodb:find(collection, "{}")
        Test.equal(e, 0)
        Test.equal(#rows, max_count)

        -- 通过json查询
        -- projection 表示哪些字段需要查询(1)或者不需要(0)
        -- sort 结果按给定字段升序(1)降序(-1)排列
        e, rows = mongodb:find(collection, '{"_id":{"$gt":5}}',
            '{"projection":{"desc":0,"array":0,"object":0,"sparse":0},\z
            "skip":1,"limit":3,"sort":{"amount":-1}}')
        Test.equal(e, 0)
        Test.equal(#rows, 3)
        Test.equal(rows[1]._id, 9)
        Test.equal(rows[1].desc, nil)

        -- 通过table查询（和上面的查询一样，只是换用table来描述）
        mongodb:find(collection, {_id = {["$gt"] = 5}}, {
            projection = {desc = 0, array = 0, object = 0, sparse = 0},
            skip = 1,
            limit = 3,
            sort = {amount = -1}
        })
        Test.equal(e, 0)
        Test.equal(#rows, 3)
        Test.equal(rows[1]._id, 9)
        Test.equal(rows[1].desc, nil)

        -- 查询并修改
        e, row = mongodb:find_and_modify(collection, '{"_id": 1}', nil,
                                '{"$set":{"amount":101}}', nil, nil, nil,
                                true)
        Test.equal(e, 0)
        Test.equal(row.lastErrorObject.n, 1)
        Test.equal(row.lastErrorObject.updatedExisting, true)
        Test.equal(row.value._id, 1)
        Test.equal(row.value.amount, 101)

        -- 查询数量
        local count = mongodb:count(collection, "{}")
        Test.equal(count, 10)
        count = mongodb:count(collection,
            '{"_id":{"$gt":5}}', '{"skip":1,"limit":3}')
        Test.equal(count, 3)

        -- 默认更新单个
        e = mongodb:update(collection, 0,
            {_id = 10}, {["$set"] = {amount = 9999999}})
        Test.equal(e, 0)

        -- upsert
        mongodb:update(collection, MongoDB.UPDATE_UPSERT,
            {_id = max_count + 1}, {["$set"] = {amount = 9999999}})
        Test.equal(e, 0)
        -- 更新多个（更新_id小于5的）
        mongodb:update(collection, MongoDB.UPDATE_MULTI_UPDATE,
            '{"_id":{"$lt":5}}', '{"$set":{"amount":999999999}}')
        Test.equal(e, 0)
        count = mongodb:count(collection, '{"amount":{"$gt":10000}}')
        Test.equal(count, 6)

        -- mongodb:remove(collection, 0, "{}")
        -- mongodb的collection不需要创建，可以drop掉然后直接写入
        -- 但原来的索引需要重新创建
        Test.equal(mongodb:drop_collection(collection), 0)
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
        e, rows = mongodb:find(collection, "{}")
        Test.equal(e, 0)
        Test.equal(#rows, 1)
        Test.equal(rows[1], array_tbl)

        -- 清空测试数据并结束测试
        mongodb:remove(collection, 0, "{}")
        e, rows = mongodb:find(collection, "{}")
        Test.equal(e, 0)
        Test.equal(#rows, 0)
    end)
    --[[
    -- 默认配置下插入大概为5000 ~ 10000条/s，没有加索引，远超mysql
    Test.it(string.format("mongodb json insert %d perf test", max_insert),
         function()
        Test.async(20000)
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
            Test.equal(e, 0)
            Test.done()
        end)
    end)

    Test.it(string.format("mongodb table insert %d perf test", max_insert),
         function()
        Test.async(20000)
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
            Test.equal(e, 0)
            Test.done()
        end)
    end)
]]
    Test.after(function()
        mongodb:drop_collection(collection)
        mongodb:disconnect()
    end)
end)
