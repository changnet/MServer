-- 封装一个常用的MongoDB连接实例
-- 如果数据量大，需要多个db连接，可参照当前实例额外创建另一个

local MongoDBInterface = require "mongodb.mongodb_interface"

local this = global_storage("MongoDB", {}, function(storage)
    storage.db = MongoDBInterface()
end)


-- 初始化db日志
local function on_app_start(check)
    if check then
        return this.ok
    end

    -- 连接数据库
    this.db:start(g_setting.mongo_ip, g_setting.mongo_port,
        g_setting.mongo_user, g_setting.mongo_pwd,
        g_setting.mongo_db, function()
            this.ok = true
    end)

    return false
end

-- 关闭MongoDB线程
local function on_app_stop()
    this.db:stop()

    return true
end

-- 启动优先级高，其他模块依赖db来加载数据
g_app:reg_start("MongoDB", on_app_start, 10)

-- 关闭优先级低，需要等其他模块存完数据
g_app:reg_stop("MongoDB", on_app_stop, 28)

return this.db
