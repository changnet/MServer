#pragma once

#include "../global/global.h"

struct lua_State;
class AsyncLog;

class LLog
{
public:
    ~LLog();
    explicit LLog(lua_State *L);

    int32_t stop(lua_State *L);
    int32_t start(lua_State *L);
    int32_t write(lua_State *L);

    // 用于实现stdout、文件双向输出日志打印函数
    static int32_t plog(lua_State *L);
    // 用于实现stdout、文件双向输出日志打印函数
    static int32_t elog(lua_State *L);
    // 设置日志参数
    static int32_t set_args(lua_State *L);
    // 设置进程名
    static int32_t set_name(lua_State *L);

private:
    class AsyncLog *_log;
};
