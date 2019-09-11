#pragma once

/* 数据包路由
 *
 */

#include "net_header.h"

#define MAX_SRV 8     // 最大服务器类型
#define MAX_CMD 65535 // 最大协议号，协议由模块(8bit)和功能(8bit)构成

class routing
{
public:
    // 每个协议转发策划
    struct cmd
    {
        bool  _dyn; // 是否动态转发,dynamic
        uint8 _srv; // 服务器类型
    };

    // 动态转发时，各个类型服务器的索引(index)
    struct srv
    {
        uint8 _idx[MAX_SRV];
    };

private:
    struct cmd _cmd[MAX_CMD];
    map_t <owner_t,srv> _dyn_table; // 根据玩家id动态转发
};
