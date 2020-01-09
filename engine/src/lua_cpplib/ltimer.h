#pragma once

#include <lua.hpp>
#include "../global/global.h"
#include "../ev/ev_watcher.h"

/**
 * 定时器
 */
class LTimer
{
public:
    explicit LTimer(lua_State *L);
    ~LTimer();

    /**
     * 设置定时器参数
     * @param after 延迟N毫秒后第一次回调
     * @param repeated 可选参数，每隔M毫秒后循环一次
     * @param time_jump boolean，可选参数，时间跳跃时(比如服务器卡了)是否补偿回调
     */
    int32_t set(lua_State *L);

    /**
     * 停止定时器
     */
    int32_t stop(lua_State *L);

    /**
     * 启动定时器
     */
    int32_t start(lua_State *L);

    /**
     * 定时器是否启动
     * @return boolean，是否启动
     */
    int32_t active(lua_State *L);

    void callback(EVTimer &w, int32_t revents);

private:
    int32_t _timer_id;
    class EVTimer _timer;
};