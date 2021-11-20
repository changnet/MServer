-- mongodb_mgr.lua mongodb连接管理
-- 2016-05-22
-- xzc
-- mongodb连接管理
local MongodbMgr = {}

local this = global_storage("MongodbMgr", {
    id = 0,
    db = {},
})

local Mongo = require "engine.Mongo"

local S_READY = Mongo.S_READY
local S_DATA = Mongo.S_DATA

-- 产生唯一的数据库id
function MongodbMgr.next_id()
    this.id = this.id + 1
    return this.id
end

function MongodbMgr.push(db)
    this.db[db.id] = db
end

function MongodbMgr.pop(id)
    this.db[id] = nil
end

function mongodb_event(ev, id, qid, ecode, res)
    local db = this.db[id]
    if not db then
        elogf("mongodb event no db found: id = %d", id)
        return
    end
    if ev == S_READY then
        db:on_ready()
    elseif ev == S_DATA then
        db:on_data(qid, ecode, res)
    else
        assert(false)
    end
end

-- 初始化db日志
local function on_app_start(check)
    if check then
    end

    -- TODO 这里再想想，是否需要多个mongodb连接，如果只有一个，那就简单很多
    -- 如果连多个库呢？

    -- 保留mongodbmgr，直接对接C++的mongo库
    -- 现有的mongodb.lua全改掉，

    -- 弄一个table形式的MongoDB，可以直接用MongoDB.find
    -- 如果需要多个库，则需要 XXMongoDB 这样多个库

        -- 连接数据库
--         g_mongodb:start(g_setting.mongo_ip, g_setting.mongo_port,
--         g_setting.mongo_user, g_setting.mongo_pwd,
--         g_setting.mongo_db, function()
-- self:one_initialized("db_conn")
-- end
    return false
end

-- 关闭文件日志线程及数据库日志线程
local function on_app_stop()
    -- this.async_file:stop()
    for id, db in pairs(this.db) do
        print("STOPING MONGODB", id)
        db:stop()
    end
end

-- 启动优先级高，其他模块依赖db来加载数据
g_app.reg_app_start("MongoDB", on_app_start, 10)

-- 关闭优先级低，需要等其他模块存完数据
g_app.reg_app_stop("MongoDB", on_app_stop, 28)

return MongodbMgr

