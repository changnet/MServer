#include "clog.h"

#include "global.h"

const char *clog_file_name( const char *name )
{
    const uint32 size = 128;
    static char file[size];
    
    struct tm* tminfo = localtime( &_start_tm );
    
    snprintf( file,size,"%s[%d]%04d-%02d-%02d %02d:%02d:%02d",name,
        getpid(),tminfo->tm_year + 1900,tminfo->tm_mon + 1, tminfo->tm_mday,
        tminfo->tm_hour, tminfo->tm_min,tminfo->tm_sec);

    return file;    /* return static pointer */
}

void cdebug_log( const char *format,... )
{
    static const char *pfile = clog_file_name( CDEBUG_FILE );

    time_t rawtime;
    time( &rawtime );
    struct tm *ntm = localtime( &rawtime );

    FILE * pf = fopen(pfile, "ab+");
    if ( !pf )
    {
        perror("cdebug_log");
        fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] ",(ntm->tm_year + 1900),
            (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
             ntm->tm_sec);

        va_list args;
        va_start(args,format);
        vfprintf(stderr,format,args);
        va_end(args);

        return;
    }

    fprintf(pf, "[%04d-%02d-%02d %02d:%02d:%02d] ",(ntm->tm_year + 1900),
        (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
         ntm->tm_sec);

    va_list args;
    va_start(args,format);
    vfprintf(pf,format,args);
    va_end(args);

    fclose(pf);
}

void cerror_log( const char *format,... )
{
    static const char *pfile = clog_file_name( CERROR_FILE );

    time_t rawtime;
    time( &rawtime );
    struct tm *ntm = localtime( &rawtime );

    FILE * pf = fopen(pfile, "ab+");
    if ( !pf )
    {
        perror("cdebug_log");
        fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] ",(ntm->tm_year + 1900),
            (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
             ntm->tm_sec);

        va_list args;
        va_start(args,format);
        vfprintf(stderr,format,args);
        va_end(args);

        return;
    }

    fprintf(pf, "[%04d-%02d-%02d %02d:%02d:%02d] ",(ntm->tm_year + 1900),
        (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
         ntm->tm_sec);

    va_list args;
    va_start(args,format);
    vfprintf(pf,format,args);
    va_end(args);

    fclose(pf);
}

void clog( const char *path,const char *format,... )
{
    FILE * pf = fopen(path, "ab+");
    if ( !pf )
    {
        perror("clog");

        va_list args;
        va_start(args,format);
        vfprintf(stderr,format,args);
        va_end(args);

        return;
    }

    va_list args;
    va_start(args,format);
    vfprintf(pf,format,args);
    va_end(args);

    fclose(pf);
}
