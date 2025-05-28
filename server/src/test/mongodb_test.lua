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


        local keys = {
            score = 1,
            price = 1,
            category = 1,
        }
        local opts = {
            unique = true, -- 唯一索引，表示keys中的组合主键只能唯一
            name = "abc", -- 索引名
        }
        Test.equal(mongodb:create_index(collection, keys, opts), 0)
        Test.equal(mongodb:drop_index(collection, "abc"), 0)

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

        -- 用aggregate计算总数
        e, rows = mongodb:aggregate(collection, {
             -- 第一步，筛选出id > 5的记录
            {["$match"] = {_id = {["$gt"] = 5}}},

            -- 第二步，计算amount字段的和
            -- json中$group的_id=null表示不分组，但不能不指定_id，这在lua中没法表示
            -- 所以直接把所有数据分到组1即可
            {["$group"] = {_id = 1, total = {["$sum"] = "$amount"}}}
        })
        Test.equal(e, 0)
        Test.equal(#rows, 1)
        Test.equal(rows[1].total, 4000)

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

    Test.it(string.format("mongodb json insert %d perf test", max_insert),
         function()

        mongodb:remove(collection, 0, '{}')

        local data = {}
        -- data._id = i
        data.amount = 100
        data.array = {"abc", "def", "ghj"}
        data.sparse = {[5] = 998}
        data.object = {name = "xzc", {9, 8, 7, 6, 5, 4, 3, 2, 1}}
        data.desc = "test item"
        data.op_time = Engine.time()

        local jstr = json.encode(data)
        local sclock = Engine.steady_clock()
        for _ = 1, max_insert do
            mongodb:insert(collection, jstr)
        end
        local eclock = Engine.steady_clock()

        local count = mongodb:count(collection, '{}')
        Test.equal(count, max_insert)
        print("raw insert time", eclock - sclock)
    end)

    Test.it(string.format("mongodb table insert %d perf test", max_insert),
         function()
        mongodb:remove(collection, 0, '{}')
        local data = {}
        -- data._id = i
        data.amount = 100
        data.array = {"abc", "def", "ghj"}
        data.sparse = {[5] = 998}
        data.object = {name = "xzc", {9, 8, 7, 6, 5, 4, 3, 2, 1}}
        data.desc = "test item"
        data.op_time = Engine.time()

        local sclock = Engine.steady_clock()
        for _ = 1, max_insert do
            mongodb:insert(collection, data)
        end
        local eclock = Engine.steady_clock()

        local count = mongodb:count(collection, '{}')
        Test.equal(count, max_insert)
        print("raw insert time", eclock - sclock)
    end)

    -- 单条插入，10000条需要5000ms左右，批量插入则只需要200ms，怎么和MYSQL差不多？？
    Test.it(string.format("mongodb batch table insert %d perf test", max_insert),
         function()
        mongodb:remove(collection, 0, '{}')
        local data = {}
        -- data._id = i
        data.amount = 100
        data.array = {"abc", "def", "ghj"}
        data.sparse = {[5] = 998}
        data.object = {name = "xzc", {9, 8, 7, 6, 5, 4, 3, 2, 1}}
        data.desc = "test item"
        data.op_time = Engine.time()

        local sclock = Engine.steady_clock()
        local list = {}
        for _ = 1, max_insert do
            table.insert(list, data)
        end
        mongodb:insert_many(collection, list)
        local eclock = Engine.steady_clock()

        local count = mongodb:count(collection, '{}')
        Test.equal(count, max_insert)
        print("raw insert time", eclock - sclock)
    end)

    Test.after(function()
        mongodb:drop_collection(collection)
        mongodb:disconnect()
    end)
end)
