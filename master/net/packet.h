#ifndef __PACKET_H__
#define __PACKET_H__

/* 网络通信消息包头格式定义
 */

#pragma pack (push, 1)

typedef uint16 array_header;
typedef uint16 packet_length;

/* 客户端发往服务器 */
struct c2s_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
};

/* 服务器发往客户端 */
struct s2c_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
    uint16 _errno ; /* 错误码 */
};

/* 服务器发往服务器 */
struct s2s_header
{
    packet_length _length; /* 包长度，不包含本身 */
    uint8  _mod   ; /* 模块号 */
    uint8  _func  ; /* 功能号 */
    uint16 _errno ; /* 错误码 */
};

#pragma pack(pop)


#if defined(__i386__) || defined(__x86_64__)

/* LDR/STR 对应汇编指令LDR/STR */
/* !!! unaligned access,部分arm cpu不支持 !!! */
# define LDR(from,to,type) (to = (*reinterpret_cast<const type *>(from)))
# define STR(to,from,type) ((*reinterpret_cast<type *>(to)) = from)

#else

/* memcpy 在所有平台上都是安全的，但效率稍慢 */
# define LDR(from,to,type) (memcpy( &to,from,sizeof type ))
# define STR(to,from,type) (memcpy( to,&from,sizeof type ))

#endif

class stream_packet
{
public:
    explicit stream_packet( class buffer *buff )
        : _buff( buff )
    {
        _length = 0;
    }
    void flush();

    T *operator T*();
public:
    template< class T >
    int32 write( const T &val )
    {
        static int32 sz = sizeof(T);
        reserved( sz );

        STR( _buff + _size + _vsz,val,T );
        _vsz += sz;

        return sz;
    }

    /* 写入字符串或二进制 */
    inline int32 write( const char *ptr,const int32 len )
    {
        assert( "write_string illegal argument",ptr && len > 0 );

        uint16 str_len = static_cast<uint16>( len );
        assert( "string length over UINT16_MAX",str_len == len );

        write( str_len ); /* 先写入长度 */
        reserved( len );
        memcpy( _buff + _size + _vsz,ptr,len );
        _vsz += len;

        return len;
    }

    template < class T >
    int32 read( T &val,bool move = true )
    {
        if ( data_size() < sizeof(T) )
        {
            return -1;
        }

        LDR( _buff + _pos,val,T );

        if ( move ) _pos += sizeof(T);

        return sizeof(T);
    }
private:
    uint32 _length;
    class buffer *_buff;
};

#undef LDR
#undef STR

#endif /* __PACKET_H__ */
