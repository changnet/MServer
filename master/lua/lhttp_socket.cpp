#include "lhttp_socket.h"
#include "lclass.h"
#include "leventloop.h"

int32 on_message_begin( http_parser *parser )
{
    assert( "on_message_begin empty parser",parser && (parser->data) );

    class lhttp_socket *http_socket = static_cast<class lhttp_socket *>(parser->data);
    http_socket->set_state( lhttp_socket::PARSE_PROC );

    return 0;
}

int32 on_url( http_parser *parser, const char *at, size_t length )
{
    assert( "on_url empty parser",parser && (parser->data) );

    class lhttp_socket * http_socket = static_cast<class lhttp_socket *>(parser->data);
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
    assert( "on_header_field empty parser",parser && (parser->data) );

    class lhttp_socket * http_socket = static_cast<class lhttp_socket *>(parser->data);
    http_socket->append_cur_field( at,length );

    return 0;
}

int32 on_header_value( http_parser *parser, const char *at, size_t length )
{
    assert( "on_header_value empty parser",parser && (parser->data) );

    class lhttp_socket * http_socket = static_cast<class lhttp_socket *>(parser->data);
    http_socket->append_cur_value( at,length );

    return 0;
}

int32 on_headers_complete( http_parser *parser )
{
    UNUSED( parser );
    return 0;
}

int32 on_body( http_parser *parser, const char *at, size_t length )
{
    assert( "on_body empty parser",parser && (parser->data) );

    class lhttp_socket * http_socket = static_cast<class lhttp_socket *>(parser->data);
    http_socket->append_body( at,length );

    return 0;
}

int32 on_message_complete( http_parser *parser )
{
    assert( "on_message_complete empty parser",parser && (parser->data) );

    class lhttp_socket * http_socket = static_cast<class lhttp_socket *>(parser->data);
    http_socket->set_state( lhttp_socket::PARSE_DONE );

    return 0;
}

/* http chunk应该用不到，暂不处理 */
static http_parser_settings settings = {
    .on_message_begin = on_message_begin,
    .on_url = on_url,
    .on_status = on_status,
    .on_header_field = on_header_field,
    .on_header_value = on_header_value,
    .on_headers_complete = on_headers_complete,
    .on_body = on_body,
    .on_message_complete = on_message_complete,
};

lhttp_socket::lhttp_socket( lua_State *L )
    : lsocket( L )
{
    _parser = NULL;
    _state  = PARSE_NONE;
    //HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH
    enum http_parser_type ty = static_cast<enum http_parser_type>(
        luaL_checkinteger( L,1 ) );
    if ( HTTP_REQUEST != ty && HTTP_RESPONSE != ty && HTTP_BOTH != ty )
    {
        luaL_error( L,"http socket parser type error" );
        return;
    }

    _parser = new struct http_parser();
    http_parser_init( _parser,ty );
    _parser->data = this;
}

lhttp_socket::~lhttp_socket()
{
    if ( _parser ) delete _parser;
    _parser = NULL;
}

void lhttp_socket::listen_cb( ev_io &w,int32 revents )
{
    assert( "libev listen cb  error",!(EV_ERROR & revents) );

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    while ( w.is_active() )
    {
        int32 new_fd = ::accept( w.fd,NULL,NULL );
        if ( new_fd < 0 )
        {
            if ( EAGAIN != errno && EWOULDBLOCK != errno )
            {
                FATAL( "accept fail:%s\n",strerror(errno) );
                return;
            }

            break;  /* 所有等待的连接已处理完 */
        }

        socket::non_block( new_fd );
        KEEP_ALIVE( new_fd );
        USER_TIMEOUT( new_fd );

        /* 直接进入监听 */
        class lhttp_socket *_socket = new class lhttp_socket( L );
        (_socket->w).set( loop );
        (_socket->w).set<lsocket,&lsocket::message_cb>( _socket );
        (_socket->w).start( new_fd,EV_READ );  /* 这里会设置fd */

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_acception);
        int32 param = 1;
        if ( ref_self )
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
            param ++;
        }
        lclass<lhttp_socket>::push( L,_socket,true );
        if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
        {
            /* 如果失败，会出现几种情况：
             * 1) lua能够引用socket，只是lua其他逻辑出错.需要lua层处理此异常
             * 2) lua不能引用socket，导致lua层gc时会销毁socket,ev还来不及fd_reify，又
             *    将此fd删除，触发一个错误
             */
            ERROR( "listen cb call accept handler fail:%s\n",lua_tostring(L,-1) );
            return;
        }
    }
}

bool lhttp_socket::is_message_complete()
{
    assert( "http socket is_message_complete empty",_recv.data_size() > 0 );

    return _state == PARSE_DONE;
}

void lhttp_socket::set_state( enum parse_state st )
{
    _state = st;
}

void lhttp_socket::append_url( const char *at,size_t len )
{
    _url.append( at,len );
}

void lhttp_socket::append_body( const char *at,size_t len )
{
    _body.append( at,len );
}

void lhttp_socket::append_cur_field( const char *at,size_t len )
{
    /* 解析到新字段，旧字段需要清除，否则value值错误 */
    if ( !_old_field.empty() ) _old_field.clear();

    _cur_field.append( at,len );
}

void lhttp_socket::append_cur_value( const char *at,size_t len )
{
    /* 当前字段名已解析完，切换到旧字段 */
    if ( !_cur_field.empty() )
    {
        _old_field = _cur_field;
        _cur_field.clear();
    }

    std::string &val = _head_field[_old_field];
    val.append( at,len );
}

int32 lhttp_socket::get_head_field()
{
    const char *field = luaL_checkstring( L,1 );
    std::map<std::string,std::string>::iterator itr = _head_field.find( field );
    if ( itr == _head_field.end() )
    {
        return 0;
    }

    std::string &val = itr->second;
    lua_pushstring( L,val.c_str() );

    return 1;
}

int32 lhttp_socket::get_url()
{
    lua_pushstring( _url.c_str() );

    return 1;
}

int32 lhttp_socket::get_body()
{
    lua_pushstring( _body.c_str() );

    return 1;
}

int32 lhttp_socket::get_method()
{
    if ( !_parser )
    {
        return luaL_error( L,"http socket get method uninit socket");
    }

    const char *method = http_method_str( _parser->method );
    lua_pushstring( L,method );

    return 1;
}

int32 lhttp_socket::get_status()
{
    if ( !_parser )
    {
        return luaL_error( L,"http socket get method uninit socket");
    }

    int32 status_code = _parser->status_code;
    lua_pushinteger( L,status_code );

    return 1;
}
