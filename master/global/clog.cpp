#include "clog.h"

#include "global.h"

const char *cdebug_file()
{
    const uint32 size = 128;
    static char file[size];
    snprintf( file,size,"%s[%d]",CDEBUG_FILE,getpid() );

    return file;    /* return static pointer */
}

const char *cerror_file()
{
    const uint32 size = 128;
    static char file[size];
    snprintf( file,size,"%s[%d]",CERROR_FILE,getpid() );

    return file;    /* return static pointer */
}

void cdebug_log( const char *format,... )
{
    static const char *pfile = cdebug_file();

    time_t rawtime;
    time( &rawtime );
    struct tm *ntm;
    ntm = localtime( &rawtime );

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
    static const char *pfile = cerror_file();

    time_t rawtime;
    time( &rawtime );
    struct tm *ntm;
    ntm = localtime( &rawtime );

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

void clog( const char *path,const char *msg )
{
    
}
