#pragma once

#include "log/async_log.hpp"

struct lua_State;

/**
 * 异步日志
 */
class LLog final : public AsyncLog
{
public:
    ~LLog();
    explicit LLog(const char *name);

    /**
     * 停止日志线程，并把未写入文件的日志全部写入
     */
    void stop();

    /**
     * 启动日志线程
     * @param usec 可选参数，每隔多少微秒写入一次文件
     */
    int32_t start(lua_State *L);

    /**
     * 写入日志到指定设备
     * @param name 设备名字，如stdout和error等
     * @param mask 日志掩码，可指定颜色、来源等，参考async_log中的定义
     * @param str 日志内容
     * @param time 日志时间，不传则为当前主循环时间。传0表示不写入时间
     */
    int32_t append(lua_State *L);

    /**
     * 类似lua print函数，把多个变量同时打印到info文件和控制台
     * @param mask 日志掩码，可指定颜色、来源等，参考async_log中的定义
     * @param ... 需要打印的变量
     */
    int32_t print(lua_State *L);

    /**
     * 打印错误日志到error文件的同时输出到控制台
     * @param mask 日志掩码，可指定颜色、来源等，参考async_log中的定义
     * @param str 日志内容
     */
    int32_t error(lua_State *L);

    /**
     * 设置日志设备的参数
     * @param name 设备名字
     * @param path 日志文件路径
     * @param alive flush时间（秒数），0表示写入完即关闭
     * @paarm policy 日志策略，按大小、按日期拆分
     * @param policy_u1 日志策略参数，按大小拆分时，传单个文件大小
     */
    int32_t set_device(lua_State *L);

    /**
     * 设置日志线程名字
     * @param name 线程名字
     */
    static int32_t set_name(lua_State *L);
};
