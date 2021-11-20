require_define "modules.system.define"
local AutoId = require "modules.system.auto_id"
local Application = require "application.application"

-- 用于服务器各进程之间的app基类
local SrvApp = oo.class(..., Application)

-- 当前进程服务器类型，起服时赋值
APP_TYPE = APP_TYPE or 0

-- 定义几个常用的连接session，起服时赋值
GSE = GSE or 0 -- gateway session，网关连接
WSE = WSE or 0 -- world session，世界服连接
ASE = ASE or 0 -- area session, 第一个场景服连接

function SrvApp:__init(cmd, opts)
    Application.__init(self)

    self.cmd = cmd
    self.name = opts.app
    self.index = assert(tonumber(opts.index), "miss argument --index")
    self.id = assert(tonumber(opts.id), "miss argument --id")

    self.start_time = ev:time()

    GSE = self:encode_session(GATEWAY, 1, self.id)
    WSE = self:encode_session(WORLD, 1, self.id)
    ASE = self:encode_session(AREA, 1, self.id)

    APP_TYPE = APP[string.upper(self.name)]
    self.session = self:encode_session(APP_TYPE, self.index, self.id)

    -- 设置当前session到C++
    network_mgr:set_curr_session(self.session)

    -- 系统定时器
    self.timer_5scb = {}
    self.auto_id = AutoId()
end

-- 生成服务器session id
-- @param name  服务器类型，如gateway、world...，见APP类型定义
-- @param index 服务器索引，如多个gateway，分别为1,2...
-- @param id 服务器id，与运维相关。开了第N个服
function SrvApp:encode_session(app_type, index, id)
    assert(app_type, "server name type not define")
    assert(index < (1 << 24), "server index out of boundry")
    assert(id < (1 << 16), "server id out of boundry")

    -- int32 ,8bits is ty,8bits is index,16bits is id
    return (app_type << 24) + (index << 16) + id
end

-- 解析session id
-- @return 服务器id，服务器索引，服务器id
function SrvApp:decode_session(session)
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
        return false
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
    printf("App %s(index = %d, id = %d, 0x%.8X) start OK", self.name,
           self.index, self.id, self.session)
end

return SrvApp
