#ifndef __CLOG_H__
#define __CLOG_H__

// 底层C日志相关函数，脚本最终也在调用C来打印日志

// 主要是实现屏幕、文件双向输出。一开始考虑使用重定向、tee来实现，但是在跨平台、日志文件截断
// 方面不太好用。nohub方式不停服无法截断日志文件，查看日志也不太方便

extern void set_log_args( bool dm,const char *ppath,const char *epath);
extern void cerror_log ( const char *prefix,const char *fmt,... );
extern void cprintf_log( const char *prefix,const char *fmt,... );

#endif  /* __CLOG_H__ */
