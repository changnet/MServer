#pragma once

#include "../global/global.hpp"

struct lua_State;
class AsyncLog;

/**
 * 异步日志
 */
class LLog
{
public:
    ~LLog();
    explicit LLog(lua_State *L);

    /**
     * 停止日志线程，并把未写入文件的日志全部写入
     */
    int32_t stop(lua_State *L);

    /**
     * 启动日志线程
     * @param usec 可选参数，每隔多少微秒写入一次文件
     */
    int32_t start(lua_State *L);

    /**
     * 写入日志内容
     * @param path 日志文件路径
     * @param ctx 日志内容
     * @param out_type 日志类型，参考 LogOut 枚举
     */
    int32_t write(lua_State *L);

    /**
     * stdout、文件双向输出日志打印函数
     * @param ctx 日志内容
     */
    int32_t plog(lua_State *L);

    /**
     * stdout、文件双向输出错误日志函数
     * @param ctx 日志内容
     */
    int32_t elog(lua_State *L);

    /**
     * 设置日志参数
     * @param is_daemon 是否后台进程，后台进程不在stdout打印日志
     * @param ppath print普通日志的输出路径
     * @param epath error错误日志的输出路径
     * @param mpath mongodb日志的输出路径
     */
    static int32_t set_args(lua_State *L);

    /**
     * 设置进程名，比如gateway world，输出的日志会加上名字
     * @param name 进程名
     */
    static int32_t set_name(lua_State *L);

private:
    class AsyncLog *_log;
};
