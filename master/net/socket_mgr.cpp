#include "socket_mgr.h"
#include "socket.h"

class socket_mgr *socket_mgr::_socket_mgr = NULL;

class socket_mgr *socket_mgr::instance()
{
    if ( !_socket_mgr )
    {
        _socket_mgr = new socket_mgr();
    }
    
    return _socket_mgr;
}

void socket_mgr::uninstance()
{
    if ( _socket_mgr ) delete _socket_mgr;
    _socket_mgr = NULL;
}

socket_mgr::socket_mgr()
{
    anios = NULL;
    aniomax =  0;
    
    ansendings = NULL;
    ansendingmax =  0;
    ansendingcnt =  0;
    
    andeletes = NULL;
    andeletemax =  0;
    andeletecnt =  0;
}

socket_mgr::~socket_mgr()
{
    while ( aniomax > 0 )
    {
        --aniomax;
        if ( anios[aniomax] )
        {
            delete anios[aniomax];
            anios[aniomax] = NULL;
        }
    }
    
    if ( anios ) delete []anios;
    anios = NULL;
    aniomax = 0;
    
    if ( ansendings ) delete []ansendings;
    ansendings = NULL;
    ansendingcnt =  0;
    ansendingmax =  0;
    
    if ( andeletes ) delete []andeletes;
    andeletes = NULL;
    andeletemax =  0;
    andeletecnt =  0;
}

void socket_mgr::push( class socket *s )
{
    int32 fd = s->fd();
    assert( "push a invalid socket",fd > 0 ); // 0是有效fd，但不可能是socket
    
    array_resize( ANIO,anios,aniomax,new_fd + 1,array_zero );
    assert( "dirty ANIO detected!!\n",!(anios[new_fd]) );
    anios[new_fd] = _socket;
}

class socket * socket_mgr::pop( int32 fd )
{
    assert( "pop a illegal socket",fd > 0 && fd < aniomax && anios[fd] );
    
    class socket *s = anios[fd];
    anios[fd] = NULL;
    
    return s;
}

void socket_mgr::pending_send( int32 fd,class socket *s )
{
    if ( s->sending )  /* 已经标记为发送，无需再标记 */
        return;

    // 0位是空的，不使用
    ++ansendingcnt;
    array_resize( ANSENDING,ansendings,ansendingmax,ansendingcnt + 1,EMPTY );
    ansendings[ansendingcnt] = fd;
    
    s->sending = ansendingcnt;
}

void socket_mgr::pending_del( int32 fd )
{
    assert( "pending delete illegal fd",fd > 0 && fd < aniomax && anios[fd] );
    ++andeletecnt;
    array_resize( ANDELETE,andeletes,andeletemax,andeletecnt + 1,EMPTY );
    andeletes[andeletecnt] = fd;
}
