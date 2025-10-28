local Engine = require "Engine" -- C++ engine api

-- 给每个线程生成一个独立的地址
-- @param wtype worker type，worker类型，如WORKER_ACCOUNT
-- @param main 是否为主线程
function Engine.make_address(wtype, index, main)
    -- 返回的地址最大为int32，高8位暂时不用
    -- wType最大为8位，一个进程里，最多只能有255个类型的worker
    -- index最大为16位，一个进程里，同一个类型的worker最多只能有65535个
    -- 0是个特殊的地址 当前worker

    -- 主线程作为一个管理和调试的特殊线程存在，它的第1位为1
    -- 在集群模式中，一个进程可能会启动多个同类型的worker，则主线程地址index为第一个worker的index
    -- 例如当前进程启动第100~120个worker，则主线程地址为(100<<9)|(wtype << 8)|1，保证每个主线程的地址都不一样
    return (index << 9) | (wtype << 1) | (main or 0)
end

-- 解码一个地址
-- @return worker类型，第几个worker,是否主线程
function Engine.unmake_address(addr)
    local main = 1 == (addr & 0x01)
    addr = addr >> 1

    local wtype = addr & 0xFF
    addr = addr >> 8

    local index = addr & 0xFFFF

    return wtype, index, main
end

-- 该地址是否为主线程地址
function Engine.is_main_addr(addr)
    return 1 == (addr & 0x01)
end

-- 生成服务器之间的签名
-- @param time 签名用的时间戳
-- @param ... 其他用于签名的数据，字符串或者数字
function Engine.make_srv_signature(time, ...)
    return Util.md5(SRV_KEY, time, ...)
end
