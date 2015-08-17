#include "clog.h"

void cdebug_log( const char *format,... )
{
    static char pfile[128] = clog_file_name
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm *ntm;
    ntm = localtime(&tv.tv_sec);

    FILE * pf = fopen(pfile, "a+");
    if(pf)
    {
        fprintf(pf, "[%04d-%02d-%02d %02d:%02d:%02d.%03ld] %s\n",(ntm->tm_year + 1900) ,(ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min, ntm->tm_sec, (tv.tv_usec / 1000), plog);
        fclose(pf);
        va_start(args,format);
vfprintf(buf,format,args);
va_end(args);
    }
    else
    {
        printf("write_simple_log file %s ,conntent = %s,errno=%d\n",pfile,plog,errno);
    }
}

void cerror_log( const char *format,... )
{
    
}

void clog( const char *path,const char *msg )
{
    
}
