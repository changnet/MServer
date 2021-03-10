#pragma once

#include "../log/async_log.hpp"

struct lua_State;

/**
 * 异步日志
 */
class LLog final : public AsyncLog
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
     * 写入日志到指定文件
     * @param path 日志文件路径
     * @param ctx 日志内容
     * @param time 日志时间，不传则为当前主循环时间
     */
    int32_t append_log_file(lua_State *L);

    /**
     * 写入字符串到指定文件，不加日志前缀，不自动换行
     * @param path 文件路径
     * @param ctx 内容
     */
    int32_t append_file(lua_State *L);

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
     * 设置某个日志参数
     * @param path 日志的输出路径，用于区分日志
     * @param type 类型，详见PolicyType的定义
     */
    int32_t set_option(lua_State *L);

    /**
     * 设置printf、error等基础日志参数
     * @param is_daemon 是否后台进程，后台进程不在stdout打印日志
     * @param ppath print普通日志的输出路径
     * @param epath error错误日志的输出路径
     */
    static int32_t set_std_option(lua_State *L);

    /**
     * 设置进程名，比如gateway world，输出的日志会加上名字
     * @param name 进程名
     */
    static int32_t set_name(lua_State *L);
};
