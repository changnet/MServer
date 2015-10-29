#ifndef __SOCKET_MGR_H__
#define __SOCKET_MGR_H__

/* socket管理 */

#include "../global/global.h"

class socket;
class socket_mgr
{
public:
    static class socket_mgr *instance();
    static void uninstance();
    
    void invoke_sending();
    void invoke_delete();

    void push( class socket *s );
    class socket *pop( int32 fd );

    void pending_send( int32 fd,class socket *s );
    void pending_del( int32 fd );
    inline int32 pending_size()
    {
        return ansendingcnt;
    }
private:
    socket_mgr();
    ~socket_mgr();

    typedef class socket *ANIO;/* AN = array node */
    typedef int32 ANSENDING;
    typedef int32 ANDELETE;

    /* socket管理队列*/
    ANIO *anios;
    int32 aniomax;
    
    /* 待发送队列 */
    ANSENDING *ansendings;
    int32 ansendingmax;
    int32 ansendingcnt;
    
    /* 待关闭socket队列 */
    ANDELETE *andeletes;
    int32 andeletemax;
    int32 andeletecnt;
    
    static class socket_mgr *_socket_mgr;
};

#endif /* __SOCKET_MGR_H__ */
