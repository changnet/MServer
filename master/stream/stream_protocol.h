#ifndef __STREAM_PROTOCOL_H__
#define __STREAM_PROTOCOL_H__

#if(__cplusplus >= 201103L)
# include <unordered_map>
#else
# include <tr1/unordered_map>
namespace std
{
    using std::tr1::unordered_map;
}
#endif

#include "../global/global.h"

/*
stream只做流协议解析
另一种方法是做lbuffer,那么read、write、reserver在buffer做就行了，不需要stream.注意设定一个最大buffer值
那么要把buffer改成纯buffer类，把recv、send放到socket，(这时又怎样访问buffer指针呢？buffer变量设计为public)

leventloop与socket之前的耦合过高，直接访问了socket的buffer、sending标志位，看能不能优化一下
把三级socket、lsocket、http_socket改为继承，设计并完成stream_socket
*/

/* 协议中数组最大递归 */
#define MAX_RECURSION_ARRAY 8

/* 协议中变量名最大长度 */
#define MAX_STREAM_PROTOCOL_KEY 32

class stream_protocol
{
public:
    struct node
    {
        typedef enum
        {
            NONE   =  0,
            INT8   =  1,
            UINT8  =  2,
            INT16  =  3,
            UINT16 =  4,
            INT32  =  5,
            UINT32 =  6,
            INT64  =  7,
            UINT64 =  8,
            STRING =  9,
            ARRAY  = 10
        } node_t;

        node_t _type;
        struct node *_next;
        struct node *_child;
        char _name[MAX_STREAM_PROTOCOL_KEY];

        explicit node( node_t type )
        {
            _type = type;
            _next = NULL;
            _child = NULL;
            memset( _name,0,MAX_STREAM_PROTOCOL_KEY );
        }

        ~node()
        {
            if ( _child ) delete _child;
            if ( _next  ) delete _next;

            _child = NULL;
            _next  = NULL;
        }
    };
private:
    struct
    {
        /* 记录当前正在操作的协议信息 */
        uint16 _mod;
        uint16 _func;
        int32 _index;
        struct node *_node;
        struct node *_tail;
        struct node *_array[MAX_RECURSION_ARRAY];
    } _cur_protocol;

    void init();
    void append( const char *key,node::node_t type );
public:
    stream_protocol();
    ~stream_protocol();

    int32 protocol_end();
    int32 protocol_begin( uint16 mod,uint16 func );

    int32 tag_int8 ( const char *key );
    int32 tag_int16( const char *key );
    int32 tag_int32( const char *key );
    int32 tag_int64( const char *key );

    int32 tag_uint8 ( const char *key );
    int32 tag_uint16( const char *key );
    int32 tag_uint32( const char *key );
    int32 tag_uint64( const char *key );

    int32 tag_string( const char * key );

    int32 tag_array_end();
    int32 tag_array_begin( const char *key );
private:
    /* 对于制定协议，16+16=32bit已足够 */
    typedef std::pair<uint16,uint16> pair_key_t;
    struct pair_hash
    {
        uint32 operator () (const pair_key_t& pk) const
        {
            return (0xffff0000 & (pk.first << 16)) | (0x0000ffff & pk.second);
        }
    };

    struct pair_equal
    {
        bool operator () (const pair_key_t& a, const pair_key_t& b) const
        {
            return a.first == b.first && a.second == b.second;
        }
    };

    /* 记录当前正在操作的节点 */
    bool _tagging;

    typedef std::unordered_map< pair_key_t,struct node *,pair_hash,pair_equal > unordered_map_t;
    unordered_map_t _protocol;
};

#endif /* __STREAM_PROTOCOL_H__ */
