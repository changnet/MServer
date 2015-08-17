#ifndef __CLOG_H__
#define __CLOG_H__

extern void cdebug_log( const char *format,... );
extern void cerror_log( const char *format,... );
extern void clog( const char *path,const char *msg );

#endif  /* __CLOG_H__ */
