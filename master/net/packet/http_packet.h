#ifndef __HTTP_PACKET_H__
#define __HTTP_PACKET_H__

#include <map>
#include <queue>

#include "packet.h"

struct http_parser;
class http_packet : public packet
{
public:
    typedef std::map< std::string,std::string > head_map_t;
    struct http_info
    {
        std::string _url;
        std::string _body;
        head_map_t _head_field;
    };
public:
    virtual ~http_packet();
    explicit http_packet( class socket *sk );

    packet_t type() const { return PKT_HTTP; }

     int32 pack_clt( lua_State *L,int32 index );
     int32 pack_srv( lua_State *L,int32 index );
    /* 数据解包 
     * return: <0 error;0 success
     */
    int32 unpack();
    /* 解压http数据到lua堆栈 */
    int32 unpack_header( lua_State *L ) const;
public:
    /* http_parse 回调函数 */
    void reset();
    void on_headers_complete();
    void on_message_complete();
    void append_url( const char *at,size_t len );
    void append_body( const char *at,size_t len );
    void append_cur_field( const char *at,size_t len );
    void append_cur_value( const char *at,size_t len );
private:
    int32 pack_raw( lua_State *L,int32 index );

    http_parser *_parser;
    std::string _cur_field;
    std::string _cur_value;
    struct http_info _http_info;
};

#endif /* __HTTP_PACKET_H__ */
