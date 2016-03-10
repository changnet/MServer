#include "clog.h"
#include "global.h"

/* win下不能创建带:的文件,故时间_分开 */
const char *debug_file( )
{
    static char file[PATH_MAX];
    
    struct tm* tminfo = localtime( &_start_tm );
    
    snprintf( file,PATH_MAX,"%s[%d]%04d-%02d-%02d#%02d_%02d_%02d",CDEBUG_FILE,
        getpid(),tminfo->tm_year + 1900,tminfo->tm_mon + 1, tminfo->tm_mday,
        tminfo->tm_hour, tminfo->tm_min,tminfo->tm_sec);

    return file;    /* return static pointer */
}

const char *error_file( )
{
    static char file[PATH_MAX];
    
    struct tm* tminfo = localtime( &_start_tm );
    
    snprintf( file,PATH_MAX,"%s[%d]%04d-%02d-%02d#%02d_%02d_%02d",CERROR_FILE,
        getpid(),tminfo->tm_year + 1900,tminfo->tm_mon + 1, tminfo->tm_mday,
        tminfo->tm_hour, tminfo->tm_min,tminfo->tm_sec);

    return file;    /* return static pointer */
}

void cdebug_log( const char *format,... )
{
    static const char *pfile = debug_file();

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
        
        fprintf( stderr,"\n" );

        return;
    }

    fprintf(pf, "[%04d-%02d-%02d %02d:%02d:%02d] ",(ntm->tm_year + 1900),
        (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
         ntm->tm_sec);

    va_list args;
    va_start(args,format);
    vfprintf(pf,format,args);
    va_end(args);
    
    fprintf( pf,"\n" );

    fclose(pf);
}

void cerror_log( const char *format,... )
{
    static const char *pfile = error_file();

    time_t rawtime;
    time( &rawtime );
    struct tm *ntm = localtime( &rawtime );

    FILE * pf = fopen(pfile, "ab+");
    if ( !pf )
    {
        perror("cerror_log");
        fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] ",(ntm->tm_year + 1900),
            (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
             ntm->tm_sec);

        va_list args;
        va_start(args,format);
        vfprintf(stderr,format,args);
        va_end(args);
        
        fprintf( stderr,"\n" );

        return;
    }

    fprintf(pf, "[%04d-%02d-%02d %02d:%02d:%02d] ",(ntm->tm_year + 1900),
        (ntm->tm_mon + 1), ntm->tm_mday, ntm->tm_hour, ntm->tm_min,
         ntm->tm_sec);

    va_list args;
    va_start(args,format);
    vfprintf(pf,format,args);
    va_end(args);
    
    fprintf( pf,"\n" );

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
        
        fprintf( stderr,"\n" );

        return;
    }

    va_list args;
    va_start(args,format);
    vfprintf(pf,format,args);
    va_end(args);
    
    fprintf( pf,"\n" );

    fclose(pf);
}
