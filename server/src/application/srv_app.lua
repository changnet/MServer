require "modules.system.define"
local AutoId = require "modules.system.auto_id"
local Application = require "application.application"

-- 用于服务器各进程之间的app基类
local SrvApp = oo.class(..., Application)

function SrvApp:__init(cmd, opts)
    Application.__init(self)

    self.cmd, self.name, self.index, self.id = cmd, opts.app, assert(
                                                   tonumber(opts.index),
                                                   "miss argument --index"),
                                               assert(tonumber(opts.id),
                                                      "miss argument --id")

    self.start_time = ev:time()
    self.session = self:srv_session(self.name, tonumber(self.index),
                                    tonumber(self.id))

    -- 设置当前session到C++
    network_mgr:set_curr_session(self.session)

    -- 系统定时器
    self.timer_5scb = {}
    self.auto_id = AutoId()
end

-- 生成服务器session id
-- @name  服务器名称，如gateway、world...
-- @index 服务器索引，如多个gateway，分别为1,2...
-- @id 服务器id，与运维相关。开了第N个服
function SrvApp:srv_session(name, index, id)
    local ty = SRV_NAME[name]

    assert(ty, "server name type not define")
    assert(index < (1 << 24), "server index out of boundry")
    assert(id < (1 << 16), "server id out of boundry")

    -- int32 ,8bits is ty,8bits is index,16bits is id
    return (ty << 24) + (index << 16) + id
end

-- 解析session id
function SrvApp:srv_session_parse(session)
    local ty = session >> 24;
    local index = (session >> 16) & 0xFF
    local id = session & 0xFFFF

    return ty, index, id
end

-- 加载各个子模块
function SrvApp:module_initialize()
    require "modules.module_header"
end

-- 定时器事件
function SrvApp:do_timer()
    for _, cb in pairs(self.timer_5scb) do cb() end
end

-- 注册1s定时器
function SrvApp:reg_5s_timer(this, method)
    local id = self.auto_id:next_id()

    self.timer_5scb[id] = method_thunk(this, method)
    return id
end

-- 取消1s定时器
function SrvApp:remove_5s_timer(id)
    self.timer_5scb[id] = nil
end

-- 进程初始化
function SrvApp:initialize()
    if not Cmd.load_protobuf() then
        os.exit(1)
    end

    Application.initialize(self)
end

-- 初始化完成
function SrvApp:final_initialize()
    -- 修正为整点触发(X分0秒)，但后面调时间就不对了
    local next = 5 - (ev:time() % 5)
    self.timer =
        g_timer_mgr:interval(next * 1000, 5000, -1, self, self.do_timer)

    -- Application.final_initialize(self)
    self.ok = true
    PRINTF("App %s(index = %d, id = %d, 0x%.8X) start OK", self.name,
           self.index, self.id, self.session)
end

-- 连接db
function SrvApp:db_initialize()
    -- 连接数据库
    g_mongodb:start(g_setting.mongo_ip, g_setting.mongo_port,
                    g_setting.mongo_user, g_setting.mongo_pwd,
                    g_setting.mongo_db, function()
        self:one_initialized("db_conn")
    end)
end

-- 加载自增id
function SrvApp:uniqueid_initialize()
    g_unique_id:db_load()
end

-- 初始化db日志
function SrvApp:db_logger_initialize()
    g_log_mgr:db_logger_init()
end

return SrvApp
