#include <http_parser.h>

#include "http_socket.h"
#include "../lua_cpplib/lnetwork_mgr.h"

// 开始解析报文，第一个回调的函数，在这里初始化数据
int32 on_message_begin( http_parser *parser )
{
    assert( "on_url no parser",parser && (parser->data) );

    class http_socket * http_socket = 
        static_cast<class http_socket *>(parser->data);
    http_socket->reset_parse();

    return 0;
}

// 解析到url报文，可能只是一部分
int32 on_url( http_parser *parser, const char *at, size_t length )
{
    assert( "on_url no parser",parser && (parser->data) );

    class http_socket * http_socket = 
        static_cast<class http_socket *>(parser->data);
    http_socket->append_url( at,length );

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

    class http_socket * http_socket = 
        static_cast<class http_socket *>(parser->data);
    http_socket->append_cur_field( at,length );

    return 0;
}

int32 on_header_value( http_parser *parser, const char *at, size_t length )
{
    assert( "on_header_value no parser",parser && (parser->data) );

    class http_socket * http_socket = 
        static_cast<class http_socket *>(parser->data);
    http_socket->append_cur_value( at,length );

    return 0;
}

int32 on_headers_complete( http_parser *parser )
{
    assert( "on_header_value no parser",parser && (parser->data) );

    class http_socket * http_socket = 
        static_cast<class http_socket *>(parser->data);
    http_socket->on_headers_complete();

    return 0;
}

int32 on_body( http_parser *parser, const char *at, size_t length )
{
    assert( "on_body no parser",parser && (parser->data) );

    class http_socket * http_socket = 
        static_cast<class http_socket *>(parser->data);
    http_socket->append_body( at,length );

    return 0;
}

int32 on_message_complete( http_parser *parser )
{
    assert( "on_message_complete no parser",parser && (parser->data) );

    class http_socket * http_socket = 
        static_cast<class http_socket *>(parser->data);
    http_socket->on_message_complete();

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
void http_socket::reset_parse()
{
    _cur_field.clear();
    _cur_value.clear();

    _http_info._url.clear();
    _http_info._body.clear();
    _http_info._head_field.clear();
}

void http_socket::on_headers_complete()
{
    if ( _cur_field.empty() ) return;

    _http_info._head_field[_cur_field] = _cur_value;
}

void http_socket::on_message_complete()
{
    /* 注意，这里回调脚本后，可能会接着解析下一个报文 */
    lnetwork_mgr::instance()->http_command_new( this );
}

void http_socket::append_url( const char *at,size_t len )
{
    _http_info._url.append( at,len );
}

void http_socket::append_body( const char *at,size_t len )
{
    _http_info._body.append( at,len );
}

void http_socket::append_cur_field( const char *at,size_t len )
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

void http_socket::append_cur_value( const char *at,size_t len )
{
    _cur_value.append( at,len );
}

/* ====================== CALL FUNCTION END ================================ */
http_socket::~http_socket()
{
    delete _parser;
}

http_socket::http_socket( uint32 conn_id,conn_t conn_ty )
    : socket( conn_id,conn_ty )
{
    //HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH
    _parser = new struct http_parser();
    http_parser_init( _parser,HTTP_BOTH );
    _parser->data = this;
}

void http_socket::command_cb ()
{
    /* 在回调脚本时，可能被脚本关闭当前socket，这时就不要再处理数据了 */

    int32 ret = socket::recv();
    if ( 0 == ret )  /* 对方主动断开 */
    {
        socket::stop();
        lnetwork_mgr::instance()->connect_del( _conn_id,_conn_ty );
    }
    else if ( 0 > ret ) /* 出错 */
    {
        if ( EAGAIN != errno && EWOULDBLOCK != errno )
        {
            socket::stop();
            ERROR( "stream socket recv error:%s\n",strerror(errno) );
            lnetwork_mgr::instance()->connect_del( _conn_id,_conn_ty );
        }
        return;
    }

    while ( fd() > 0 )
    {
        uint32 size = _recv.data_size();
        if ( size == 0 ) return;

        int32 nparsed = 
            http_parser_execute( _parser,&settings,_recv.data(),size );

        _recv.clear(); // http_parser不需要旧缓冲区

        /* web_socket报文,暂时不用回调到上层 */
        if ( _parser->upgrade )
        {
            return;
        }
        else if ( nparsed != (int32)size )  /* error */
        {
            int32 no = _parser->http_errno;
            ERROR( "http socket parse error(%d):%s",no,
                http_errno_name(static_cast<enum http_errno>(no)) );

            return;
        }
    }
}

void http_socket::connect_cb ()
{
    int32 ecode = socket::validate();

    if ( 0 == ecode )
    {
        KEEP_ALIVE( socket::fd() );
        USER_TIMEOUT( socket::fd() );

        socket::start();
    }

    /* 连接失败或回调脚本失败,都会被lnetwork_mgr删除 */
    bool is_ok = 
        lnetwork_mgr::instance()->connect_new( _conn_id,_conn_ty,ecode );

    if ( !is_ok || 0 != ecode ) socket::stop ();
}

void http_socket::listen_cb  ()
{
    lnetwork_mgr *network_mgr = lnetwork_mgr::instance();
    while ( socket::active() )
    {
        int32 new_fd = socket::accept();
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                ERROR( "http socket::accept:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        uint32 conn_id = network_mgr->generate_connect_id();
        /* 新增的连接和监听的连接类型必须一样 */
        class socket *new_sk = new class http_socket( conn_id,_conn_ty );

        bool ok = network_mgr->accept_new( conn_id,_conn_ty,new_sk );
        if ( ok ) new_sk->start( new_fd );
    }
}

bool http_socket::upgrade() const
{
    return _parser->upgrade;
}

uint32 http_socket::status() const
{
    return _parser->status_code;
}

const struct http_socket::http_info &http_socket::get_http() const
{
    return _http_info;
}

const char *http_socket::method() const
{
    return http_method_str( static_cast<enum http_method>( _parser->method ) );
}