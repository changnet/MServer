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
    int32 iomax = aniomax;
    while ( iomax > 0 )
    {
        --iomax;
        if ( anios[iomax] )
        {
            /* TODO delete会触发lsocket的析构，从而调用socket_mgr的pop
             * 可能有更好的办法解耦
             */
            anios[iomax]->close();
            delete anios[iomax];
            anios[iomax] = NULL;
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
    
    array_resize( ANIO,anios,aniomax,fd + 1,array_zero );
    assert( "dirty ANIO detected!!\n",!(anios[fd]) );
    anios[fd] = s;
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

/* 把数据攒到一起，一次发送
 * 好处是：把包整合，减少发送次数，提高效率
 * 坏处是：需要多一个数组管理；如果发送的数据量很大，在逻辑处理过程中就不能利用带宽
 * 然而，游戏中包多，但数据量不大
 */
void socket_mgr::invoke_sending()
{
    if ( ansendingcnt <= 0 )
        return;

    int32 fd   = 0;
    int32 empty = 0;
    int32 empty_max = 0;
    class socket *_socket = NULL;
    
    /* 这里使用一个数组来管理要发送数据的socket。还有一种更简单粗暴的方法是每次都遍历所有
     * socket，如果有数据在缓冲区则发送，这种方法在多链接，低活跃场景中效率不高。
     */
    for ( int32 i = 1;i <= ansendingcnt;i ++ )/* 0位是空的，不使用 */
    {
        if ( !(fd = ansendings[i]) || !(_socket = anios[fd]) )
        {
            /* 假如socket发送了很大的数据并直接关闭socket。一次event loop无法发送完数据，
             * socket会被强制关闭。此时就会出现anios[fd] = NULL。考虑到这种情况在游戏中
             * 并不常见，可当异常处理
             */
            ERROR( "invoke sending empty socket" );
            ++empty;
            empty_max = i;
            continue;
        }
        
        assert( "invoke sending index not match",i == _socket->sending );

        /* 处理发送 */
        int32 ret = _socket->_send.send( fd );

        if ( 0 == ret || (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK) )
        {
            ERROR( "invoke sending unsuccess:%s\n",strerror(errno) );
            _socket->on_disconnect();
            ++empty;
            empty_max = i;
            continue;
        }
        
        /* 处理sendings移动 */
        if ( _socket->_send.data_size() <= 0 )
        {
            _socket->sending = 0;   /* 去除发送标识 */
            _socket->_send.clear(); /* 去除悬空区 */
            ++empty;
            empty_max = i;
        }
        else if ( empty )  /* 将发送数组向前移动，防止中间留空 */
        {
            int32 empty_min = empty_max - empty + 1;
            _socket->sending = empty_min;
            --empty;
        }
        /* 数据未发送完，也不需要移动，则do nothing */
    }

    ansendingcnt -= empty;
    assert( "invoke sending sending counter fail",ansendingcnt >= 0 && ansendingcnt < ansendingmax );
}

void socket_mgr::invoke_delete()
{
    while ( andeletecnt )
    {
        int32 fd = andeletes[andeletecnt];
        --andeletecnt;
        if ( !anios[fd] )
        {
            ERROR( "invoke delete empty socket,maybe double kill" );
            continue;
        }
        
        /* andeletes是处理主动关闭队列的，因此无需调用on_disconect告知lua */
        anios[fd]->close();
        delete anios[fd];
        anios[fd] = NULL;
    }
}
