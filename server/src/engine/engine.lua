local Engine = require "Engine" -- C++ engine api

local SERVER_ID = g_setting.server

-- 给每个线程生成一个独立的地址
-- @param wtype number worker类型(worker type)，如WORKER_ACCOUNT
-- @param main number 是否为主线程
function Engine.make_address(wtype, index, main)
    -- index|type|main，返回的地址最大为int32(现在使用23位，其他预留)
    -- wType最大为8位，最多只能有255个类型的worker
    -- index最大为14位，同一个类型的worker最多只能有16384个
    -- 0是个特殊的地址，表示当前worker

    -- 主线程作为一个管理和调试的特殊线程存在，它的第1位为1
    -- 在集群模式中，一个进程可能会启动多个同类型的worker，则主线程地址index为第一个worker的index
    -- 例如当前进程启动第100~120个worker，则主线程地址为(100<<9)|(wtype << 8)|1，保证每个主线程的地址都不一样
    return (index << 9) | (wtype << 1) | (main or 0)
end

-- 解码一个地址
-- @return worker类型，第几个worker,是否主线程
function Engine.unmake_address(addr)
    local main = addr & 0x01
    addr = addr >> 1

    local wtype = addr & 255
    addr = addr >> 8

    local index = addr & 16383 -- 2^14 - 1

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

-- 获取当前服务器id
function Engine.get_server_id()
    return SERVER_ID
end

-- 生成一个在所有节点唯一，对json安全的唯一id
function Engine.make_safe_id(seed, hash)
    -- double最大可以表示53位，小于2^53在json和不支持int64的语言（比如js）中是安全的

    seed = seed + 1
    if seed > 2^53 - 1 then seed = 1 end

    local id = (seed << 23) | LOCAL_ADDR
    if not hash[id] then return id, seed end

    for _ = 1, 1000000 do
        seed = seed + 1
        if seed > 2^53 - 1 then seed = 1 end

        id = seed | LOCAL_ADDR
        if not hash[id] then return id, seed end
    end

    assert(false)
end
