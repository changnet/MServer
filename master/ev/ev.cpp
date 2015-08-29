#include <fcntl.h>

#include "ev.h"
#include "ev_watcher.h"

#define EV_CHUNK    2048
#define array_resize(type,base,cur,cnt,init)    \
    if ( cnt > cur )                            \
    {                                           \
        uint32 size = cur;                      \
        while ( size < cnt )                    \
        {                                       \
            size *= 2;                          \
        }                                       \
        type *tmp = (type *)new type[size];     \
        init( tmp,sizeof(type)*size );        \
        memcpy( base,tmp,sizeof(type)*cur );    \
        delete []base;                          \
        base = tmp;                             \
        cur = size;                             \
    }
    
#define EMPTY(base,size)
#define array_zero(base,size)    \
    memset ((void *)(base), 0, sizeof (*(base)) * (size))

ev_loop::ev_loop()
{
    anfds = NULL;
    anfdmax = 0;
    anfdcnt = 0;
    
    pendings = NULL;
    pendingmax = 0;
    pendingcnt = 0;
    
    fdchanges = NULL;
    fdchangemax = 0;
    fdchangecnt = 0;
}

ev_loop::~ev_loop()
{
    if ( anfds )
    {
        delete []anfds;
        anfds = NULL;
    }
    
    if ( pendings )
    {
        delete []pendings;
        pendings = NULL;
    }
    
    if ( fdchanges )
    {
        delete []fdchanges;
        fdchanges = NULL;
    }
}

void ev_loop::init()
{
    anfds = new ANFD[EV_CHUNK];
    anfdmax = EV_CHUNK;
    memset( anfds,0,sizeof(ANFD)*anfdmax );
    
    pendings = new ANPENDING[EV_CHUNK];
    pendingmax = EV_CHUNK;
    
    fdchanges = new ANCHANGE[EV_CHUNK];
    fdchangemax = EV_CHUNK;
    
    backend_init();
}

void ev_loop::run()
{
    
}

void ev_loop::done()
{
    loop_done = true;
}

void ev_loop::io_start( ev_io *w )
{
    int32 fd = w->fd;

    array_resize( ANFD,anfds,anfdmax,uint32(fd + 1),array_zero );

    ANFD *anfd = anfds + fd;

    anfd->w = w;
    anfd->reify = anfd->emask ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

    fd_change( fd );
}

void ev_loop::fd_change( int32 fd )
{
    ++fdchangecnt;
    array_resize( ANCHANGE,fdchanges,fdchangemax,fdchangecnt,EMPTY );
    fdchanges [fdchangecnt - 1] = fd;
}

void ev_loop::fd_reify()
{
    for ( uint32 i = 0;i < fdchangecnt;i ++ )
    {
        int32 fd     = fdchanges[i];
        ANFD *anfd   = anfds + fd;
        
        switch ( anfd->reify )
        {
        case EPOLL_CTL_ADD :
        case EPOLL_CTL_MOD :
        {
            int32 events = (anfd->w)->events;
            /* 允许一个fd在一次loop中不断地del，再add，或者多次mod。
               但只要event不变，则不需要更改epoll */
            if ( anfd->emask == events ) /* no reification */
                continue;
            backend_modify( fd,events,anfd );
            anfd->emask = events;
        }
        case EPOLL_CTL_DEL :
            /* 此时watcher可能已被delete */
            backend_modify( fd,0,anfd );
            anfd->emask = 0;
            break;
        default :
            assert( "unknow epoll modify reification",false );
            return;
        }
        
        
    }
    
    fdchangecnt = 0;
}

void ev_loop::backend_modify( int32 fd,int32 events,ANFD *anfd )
{
    struct epoll_event ev;
    
    ev.data.fd = fd;
    ev.events  = (events & EV_READ  ? EPOLLIN  : 0)
               | (events & EV_WRITE ? EPOLLOUT : 0);
    /* pre-2.6.9 kernels require a non-null pointer with EPOLL_CTL_DEL, */
    /* The default behavior for epoll is Level Triggered. */
    /* LT同时支持block和no-block，持续通知 */
    /* ET只支持no-block，一个事件只通知一次 */
    if ( !epoll_ctl(backend_fd,anfd->reify,fd,&ev) )
        return;
        
    switch ( errno )
    {
    case EBADF  :
        assert ( "ev_loop::backend_modify EBADF",false );
        break;
    case EEXIST :
        ERROR( "ev_loop::backend_modify EEXIST" );
        break;
    case EINVAL :
        assert ( "ev_loop::backend_modify EINVAL",false );
        break;
    case ENOENT :
        assert ( "ev_loop::backend_modify ENOENT",false );
        break;
    case ENOMEM :
        assert ( "ev_loop::backend_modify ENOMEM",false );
        break;
    case ENOSPC :
        assert ( "ev_loop::backend_modify ENOSPC",false );
        break;
    case EPERM  :
        assert ( "ev_loop::backend_modify EPERM",false );
        break;
    }
}

void ev_loop::backend_init()
{
#ifdef EPOLL_CLOEXEC
    backend_fd = epoll_create1 (EPOLL_CLOEXEC);

    if (backend_fd < 0 && (errno == EINVAL || errno == ENOSYS))
#endif
    backend_fd = epoll_create (256);

    assert( "ev_loop::backend_init failed",backend_fd < 0 );

    fcntl (backend_fd, F_SETFD, FD_CLOEXEC);

    backend_mintime = 1e-3; /* epoll does sometimes return early, this is just to avoid the worst */
}
