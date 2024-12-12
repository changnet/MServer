local Engine = require "Engine" -- C++ engine api

-- 给每个线程生成一个独立的地址
-- @param pid process id，进程id，如PROCESS_GATEWAY
-- @param wType worker type，worker类型，如WORKER_ACCOUNT
function Engine.make_address(process_id, wType, index)
    -- 返回的地址最大为int32，高8位暂时不用
    -- process_id最大为8位，最多支持255个进程
    -- wType最大为8位，一个进程里，最多只能有255个类型的worker
    -- index最大为8位，一个进程里，同一个类型的worker最多只能有255个

    -- process_id用于标记每个独立的进程
    -- 小服开服时，不同服的相同进程互不通信，因此是可以重复的。使用常用的定义即可，如：PROCESS_GATEWAY = 2
    -- 同一个进程多开构建集群时，需要分配不同的进程id
    -- 假如开100个网关，进程id为101、102、103...，可以配置也可以自动生成，不重复即可
    -- 集群最大有255个进程，每个进程可构建255个该类型的worker，因此最大可以用255*255个线程
    return process_id << 16 + wType << 8 + index
end

-- 解码一个地址
-- @return 进程id，worker类型，第几个worker
function Engine.unmake_address(addr)
    local index = addr & 0xFF
    addr = addr >> 8

    local wType = addr & 0xFF
    addr = addr >> 8

    return addr, wType, index
end
