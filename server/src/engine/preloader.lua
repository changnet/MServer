-- 一些比较基础，加载优先级比较高，所有worker都需要加载的文件

require "global.global" -- 加载错误处理函数
require "global.log" -- 加载log函数
require "system.define" -- 基础定义

require "engine.engine"
require "worker.worker"

require "engine.co_pool"
require "message.thread_message"
require "engine.signal"
require "engine.shutdown"
require "global.rtti"
require "rpc.rpc"
require "timer.timer"
require "network.socket_mgr"
require "cluster.cluster"
require "global.debug"

require "global.table"
require "global.math"
require "global.string"
require "global.time"
require "worker.worker_route"
