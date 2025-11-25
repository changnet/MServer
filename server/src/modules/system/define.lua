-- define.lua 服务器全局定义
-- 2017-03-28
-- xzc

-- worker类型定义{类型, 名字}，名字用于打印日志
WORKER = {
    TEST    = {1, "test"}, -- 单元测试
    GATEWAY = {2, "gateway"}, -- 网关
    GAME    = {3, "game"}, -- 游戏公用逻辑
    DATA    = {4, "data"}, -- 缓存及db操作
    PLAYER  = {5, "player"}, -- 玩家个人逻辑
    SCENE   = {6, "scene"}, -- 场景
    MYSQL   = {7, "mysql"}, -- mysql数据库读写
    MONGODB = {8, "mongodb"}, -- mongodb数据库读写
    ACCOUNT = {9, "account"}, -- 帐号管理及登录
}
W_MAIN    = 256 -- 主线程，主线程的wtype通常和某个worker的wtype一致，如果要区分，则W_TEST | W_MAIN
W_TEST    = 1 -- 单元测试
W_GATEWAY = 2 -- 网关
W_GAME    = 3 -- 游戏公用逻辑
W_DATA    = 4 -- 缓存及db操作
W_PLAYER  = 5 -- 玩家个人逻辑
W_SCENE   = 6 -- 场景
W_MYSQL   = 7 -- mysql数据库读写
W_MONGODB = 8 -- mongodb数据库读写
W_ACCOUNT = 9 -- 帐号管理及登录

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
