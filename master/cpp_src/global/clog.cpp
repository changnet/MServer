#include "clog.h"
#include "global.h"
#include "../system/static_global.h"

static bool is_daemon = false;    /* 是否后台运行。后台运行则不输出日志到stdout */
static char printf_path[PATH_MAX] = {0};
// 防止上层应用来不及设置日志参数就发生错误，默认输出到工作目录error文件
static char error_path[PATH_MAX]  = "error";
static char mongodb_path[PATH_MAX]  = "mongodb";

// app进程名
static char app_name[LOG_APP_NAME] = {0};

/* 设置日志参数：是否后台，日志路径 */
void set_log_args( bool dm,
    const char *ppath,const char *epath,const char *mpath)
{
    is_daemon = dm;
    snprintf( printf_path,PATH_MAX,"%s",ppath );
    snprintf( error_path ,PATH_MAX,"%s",epath );
    snprintf( mongodb_path ,PATH_MAX,"%s",mpath );
}

/* 设置app进程名 */
void set_app_name( const char *name )
{
#ifdef LOG_APP_NAME
    snprintf( app_name,LOG_APP_NAME,"%s",name );
#endif
}

// print app name，打印app进程名
#ifdef LOG_APP_NAME
    #define PAPPNAME(f) \
        do{ if (app_name[0]) fprintf(f,"[%s]",app_name); } while(0)
#else
    #define PAPPNAME(f)
#endif

// print file time，是否打印文件时间
#ifdef _PFILETIME_
    #define PFILETIME(f,ntm,prefix)                                  \
        do{                                                          \
            fprintf(f, "[%s%02d-%02d %02d:%02d:%02d]",               \
                prefix,(ntm.tm_mon + 1), ntm.tm_mday,                \
                ntm.tm_hour, ntm.tm_min,ntm.tm_sec);                 \
        }while(0)
#else
    #define PFILETIME(f)
#endif

#define FORMAT_TO_FILE(f)                                           \
    do{                                                             \
        va_list args;                                               \
        va_start(args,fmt);                                         \
        vfprintf(f,fmt,args);                                       \
        va_end(args);                                               \
        fprintf(f,"\n");                                            \
    }while(0)

// 这个本来想写成函数的，但是c++ 03不支持函数之间可变参传递
// 要使用主循环的时间戳，不然服务器卡的时候会造成localtime时间与主循环时间戳不一致
// 查找bug更麻烦
#define RAW_FORMAT( ctm,prefix,path,screen,fmt )                   \
    do{                                                            \
        struct tm ntm;                                             \
        ::localtime_r( &ctm,&ntm );                                \
        if ( screen ){                                             \
            PFILETIME( screen,ntm,prefix );                        \
            PAPPNAME( screen );                                    \
            FORMAT_TO_FILE( screen );                              \
        }                                                          \
        FILE * pf = ::fopen( path, "ab+" );                        \
        if ( !pf ) return;                                         \
        PFILETIME( pf,ntm,prefix );                                \
        FORMAT_TO_FILE( pf );                                      \
        ::fclose( pf );                                            \
    }while(0)

void cerror_log( const char *prefix,const char *fmt,... )
{
    time_t tm = static_global::ev()->now();
    RAW_FORMAT( tm,prefix,error_path,(is_daemon ? NULL : stderr),fmt );
}

void raw_mongodb_log( time_t tm,const char *prefix,const char *fmt,... )
{
    // mongodb的日志暂时不打印到stdout，太多了
    RAW_FORMAT( tm,prefix,mongodb_path,NULL,fmt );
}

void cprintf_log( const char *prefix,const char *fmt,... )
{
    time_t tm = static_global::ev()->now();
    // TODO:兼容低版本C++标准，没办法传...可变参，无法调用raw_cprintf_log
    // 如果尚未设置路径(这个不应该发生，设置路径的优先级很高的)，则转到ERROR
    if ( expect_false(!printf_path[0]) )
    {
        RAW_FORMAT( tm,prefix,error_path,(is_daemon ? NULL : stderr),fmt );
        return;
    }

    RAW_FORMAT( tm,prefix,printf_path,(is_daemon ? NULL : stdout),fmt );
}

void raw_cerror_log( time_t tm,const char *prefix,const char *fmt,... )
{
    RAW_FORMAT( tm,prefix,error_path,(is_daemon ? NULL : stderr),fmt );
}

void raw_cprintf_log( time_t tm,const char *prefix,const char *fmt,... )
{
    // 如果尚未设置路径(这个不应该发生，设置路径的优先级很高的)，则转到ERROR
    if ( expect_false(!printf_path[0]) )
    {
        RAW_FORMAT( tm,prefix,error_path,(is_daemon ? NULL : stderr),fmt );
        return;
    }

    RAW_FORMAT( tm,prefix,printf_path,(is_daemon ? NULL : stdout),fmt );
}
