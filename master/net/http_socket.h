#ifndef __HTTP_SOCKET_H__
#define __HTTP_SOCKET_H__

#include <map>
#include <http_parser.h>  /* deps/include */
#include "socket.h"

/* http协议并不那么严谨。至少部分字段可有可无，甚至content length也可以没有。
 * 因此，http_parser在每收到一个字符时，都努力地去解析协议。而每一次解析都会
 * 触发settings中的回调。比如：
 * GET /demo HTTP/1.1\r\n
 * 由于网络不好，socket收到GET /后，demo分4次到达，则会触发4次on_url，每次
 * 只有一个字符
 */

class http_socket : public socket
{
public:
    int32 get_head_field();
    int32 get_url();
    int32 get_body();
private:
    std::string _url;
    std::string _body;
    std::map< std::string,std::string > _head_field;
};

#endif /* __HTTP_SOCKET_H__ */
