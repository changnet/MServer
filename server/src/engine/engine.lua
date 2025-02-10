local Engine = require "Engine" -- C++ engine api

-- 给每个线程生成一个独立的地址
-- @param wType worker type，worker类型，如WORKER_ACCOUNT
function Engine.make_address(wType, index)
    -- 返回的地址最大为int32，高8位暂时不用
    -- process_id最大为8位，最多支持255个进程
    -- wType最大为8位，一个进程里，最多只能有255个类型的worker
    -- index最大为16位，一个进程里，同一个类型的worker最多只能有65535个
    -- 0和1是两个特殊的地址 0表示当前worker，1表示当前进程的地址

    -- TODO worker的类型应该不会很多，感觉用2^6=64都够了，剩下给其他？？
    return (index << 8) + wType
end

-- 解码一个地址
-- @return worker类型，第几个worker
function Engine.unmake_address(addr)
    local wType = addr & 0xFF
    addr = addr >> 8

    local index = addr & 0xFFFF

    return wType, index
end
