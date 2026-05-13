-- 一些比较基础，加载优先级比较高，所有worker都需要加载的文件

require "global.global" -- 加载错误处理函数
require "log.log" -- 加载log函数
require "system.define" -- 基础定义
require "system.error" -- 错误码定义

require "global.table"
require "global.math"
require "global.string"
require "global.time"
require "global.debug"
require "global.sys"
require "util.json"

require "global.rtti" -- 这个必须放前面，否则其他模块注册时会有问题

require "data.global_data"
require "engine.engine"
require "worker.worker"

-- 定义一些常用的地址，方便直接调用
MASTER_ADDR = Engine.make_address(W.GAME, 1, 1) -- 单个服中，用于统一协商的地址
GAME_ADDR = Engine.make_address(W.GAME, 1) -- 游戏逻辑的地址
DATA_ADDR = Engine.make_address(W.DATA, 1) -- 数据读写的地址
GATEWAY_ADDR = Engine.make_address(W.GATEWAY, 1) -- 网关的地址
ACCOUNT_ADDR = Engine.make_address(W.ACCOUNT, 1) -- 帐号管理的地址
LOG_ADDR = Engine.make_address(W.LOG, 1) -- 日志的地址

require "engine.co_pool"
require "message.thread_message"
require "engine.signal"
require "engine.shutdown"
require "rpc.rpc"
require "timer.timer"
require "network.socket_mgr"
require "cluster.cluster"
require "cluster.cluster_proxy"
require "event.event"
require "router.router"
require "router.router_policy"
require "gm.gm"
require "gm.gm_misc"
