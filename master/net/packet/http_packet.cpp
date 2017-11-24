#include "http_packet.h"

http_packet::~http_packet()
{
}

http_packet::http_packet( class socket *sk ) : _socket( sk )
{
    _is_complete = false;
}

int32 http_packet::pack()
{
    return 0;
}

int32 http_packet::unpack()
{
    return 0;
}


// 开始解析报文，第一个回调的函数，在这里初始化数据
int32 on_message_begin( http_parser *parser )
{
    assert( "on_url no parser",parser && (parser->data) );

    class http_packet * http_packet = 
        static_cast<class http_packet *>(parser->data);
    http_packet->reset();

    return 0;
}

// 解析到url报文，可能只是一部分
int32 on_url( http_parser *parser, const char *at, size_t length )
{
    assert( "on_url no parser",parser && (parser->data) );

    class http_packet * http_packet = 
        static_cast<class http_packet *>(parser->data);
    http_packet->append_url( at,length );

    return 0;
}

int32 on_status( http_parser *parser, const char *at, size_t length )
{
    UNUSED( parser );
    UNUSED( at );
    UNUSED( length );
    // parser->status_code 本身有缓存，这里不再缓存
    return 0;
}

int32 on_header_field( http_parser *parser, const char *at, size_t length )
{
    assert( "on_header_field no parser",parser && (parser->data) );

    class http_packet * http_packet = 
        static_cast<class http_packet *>(parser->data);
    http_packet->append_cur_field( at,length );

    return 0;
}

int32 on_header_value( http_parser *parser, const char *at, size_t length )
{
    assert( "on_header_value no parser",parser && (parser->data) );

    class http_packet * http_packet = 
        static_cast<class http_packet *>(parser->data);
    http_packet->append_cur_value( at,length );

    return 0;
}

int32 on_headers_complete( http_parser *parser )
{
    assert( "on_header_value no parser",parser && (parser->data) );

    class http_packet * http_packet = 
        static_cast<class http_packet *>(parser->data);
    http_packet->on_headers_complete();

    return 0;
}

int32 on_body( http_parser *parser, const char *at, size_t length )
{
    assert( "on_body no parser",parser && (parser->data) );

    class http_packet * http_packet = 
        static_cast<class http_packet *>(parser->data);
    http_packet->append_body( at,length );

    return 0;
}

int32 on_message_complete( http_parser *parser )
{
    assert( "on_message_complete no parser",parser && (parser->data) );

    class http_packet * http_packet = 
        static_cast<class http_packet *>(parser->data);
    http_packet->on_message_complete();

    return 0;
}

/* http chunk应该用不到，暂不处理 */
static const struct http_parser_settings settings = 
{
    on_message_begin,
    on_url,
    on_status,
    on_header_field,
    on_header_value,
    on_headers_complete,
    on_body,
    on_message_complete,

    NULL,NULL
};

/* ====================== HTTP FUNCTION END ================================ */
void http_packet::reset()
{
    _is_complete = false;

    _cur_field.clear();
    _cur_value.clear();

    _http_info._url.clear();
    _http_info._body.clear();
    _http_info._head_field.clear();
}

void http_packet::on_headers_complete()
{
    if ( _cur_field.empty() ) return;

    _http_info._head_field[_cur_field] = _cur_value;
}

void http_packet::on_message_complete()
{
    _is_complete = true;
}

void http_packet::append_url( const char *at,size_t len )
{
    _http_info._url.append( at,len );
}

void http_packet::append_body( const char *at,size_t len )
{
    _http_info._body.append( at,len );
}

void http_packet::append_cur_field( const char *at,size_t len )
{
    /* 报文中的field和value是成对的，但是http-parser解析完一对字段后并没有回调任何函数
     * 如果检测到value不为空，则说明当前收到的是新字段
     */
    if ( !_cur_value.empty() )
    {
        _http_info._head_field[_cur_field] = _cur_value;

        _cur_field.clear();
        _cur_value.clear();
    }

    _cur_field.append( at,len );
}

void http_packet::append_cur_value( const char *at,size_t len )
{
    _cur_value.append( at,len );
}
