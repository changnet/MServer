#ifndef __CLOG_H__
#define __CLOG_H__

extern bool is_daemon;
extern void cdebug_log( const char *format,... );
extern void cerror_log( const char *format,... );
extern void clog( const char *path,const char *format,... );

#endif  /* __CLOG_H__ */
