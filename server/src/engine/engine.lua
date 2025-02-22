local Engine = require "Engine" -- C++ engine api

-- 给每个线程生成一个独立的地址
-- @param wType worker type，worker类型，如WORKER_ACCOUNT
function Engine.make_address(wType, index)
    -- 返回的地址最大为int32，高8位暂时不用
    -- wType最大为8位，一个进程里，最多只能有255个类型的worker
    -- index最大为16位，一个进程里，同一个类型的worker最多只能有65535个
    -- 0和1是两个特殊的地址 0表示当前worker，1表示当前进程的地址

    -- TODO 进程当前作为一个管理和调试的特殊线程存在，使用同样的地址PROCESS_ADDR
    -- 它不与远程集群节点交互，只有同一进程的worker可以与它通信，所以地址相同是没问题的
    -- 如果有远程通信要求，需要在wType划分一位来表示是否为进程
    -- 当以节点启动时，gateway1的地址就是wType=gateway, index=1,bit0=1
    -- 如果是以单服模式gateway启动，则index=0即可，这样同样节点的不同进程地址就不会冲突
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

-- 生成服务器之间的签名
-- @param time 签名用的时间戳
-- @param ... 其他用于签名的数据，字符串或者数字
function Engine.make_srv_signature(time, ...)
    return Util.md5(SRV_KEY, time, ...)
end
