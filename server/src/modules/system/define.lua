-- define.lua 服务器全局定义
-- 2017-03-28
-- xzc

-- @class WorkerDef worker信息定义
-- @field type number worker类型
-- @field name string worker名字
-- @field sequ number 启动顺序(equence)，越小越先启动，越晚关闭
-- @field pobj boolean 是否创建玩家对象
-- @field paddr boolean 是否同步玩家地址路由信息

W = {
    -- 网关
    GATEWAY = {type = 1,   sequ = 9, pobj = 0, paddr = 1, name = "gateway"},
    -- 游戏公用逻辑
    GAME    = {type = 2,   sequ = 2, pobj = 1, paddr = 1, name = "game"},
    -- 缓存及db操作
    DATA    = {type = 3,   sequ = 2, pobj = 0, paddr = 0, name = "data"},
    -- 玩家个人逻辑
    PLAYER  = {type = 4,   sequ = 3, pobj = 0, paddr = 1, name = "player"},
    -- 场景
    SCENE   = {type = 5,   sequ = 3, pobj = 1, paddr = 1, name = "scene"},
    -- mysql数据库读写
    MYSQL   = {type = 6,   sequ = 0, pobj = 0, paddr = 0, name = "mysql"},
    -- mongodb数据库读写
    MONGODB = {type = 7,   sequ = 0, pobj = 0, paddr = 0, name = "mongodb"},
    -- 帐号管理及登录
    ACCOUNT = {type = 8,   sequ = 2, pobj = 0, paddr = 0, name = "account"},
    -- 日志写入
    LOG     = {type = 10,  sequ = 0, pobj = 0, paddr = 0, name = "log"},
    -- 单元测试
    TEST    = {type = 200, sequ = 0, pobj = 0, paddr = 0, name = "test"},
    -- 机器人测试
    BOT     = {type = 201, sequ = 0, pobj = 0, paddr = 0, name = "bot"},
    -- 主线程，这个不能用来启动，则W.TEST | W.MAIN
    MAIN    = {type = 256, sequ = 0, pobj = 0, paddr = 0, name = "main"},
}

-- @type table<number, WorkerDef> 原始的worker定义信息
WORKER = {}
for k, v in pairs(W) do
    -- 转为数字，方便调用W.GATEWAY = 1
    -- 不使用WORKER为数字的原因是W用得多，有代码提示
    local wtype = v.type
    W[k] = wtype
    WORKER[wtype] = v
end

EMPTY = {} -- 一个空table，避免频繁创建空table，稍后会设置为只读

SRV_KEY = "a5c7434a324a6f1c0ef7cb771668695b"
LOGIN_KEY = "409065b7570155637b95e38ca13542e0"

-- 一个玩家最大邮件数量
MAX_MAIL = 200

-- 系统邮件最大数量
MAX_SYS_MAIL = 500

-- 服务器连接超时设定(秒)
SRV_ALIVE_INTERVAL = 5
SRV_ALIVE_TIMES = 5

PID_SRV_BIT = 25 -- 创角服务器占的位数

UNIQUEID = {
    PLAYER = 1 -- 玩家pid
}

-- 自定义客户端广播方式
CLTCAST = {
    PIDS = 1, -- 按pid广播，这个在C++底层直接处理
    WORLD = 2, -- 全服广播，但仅限于已Enter World的玩家
    LEVEL = 3 -- 按等级筛选玩家
}
