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
            ARRAY  = 10,
            DOUBLE = 11,
            TMAX
        } node_t;

        node_t _type;
        uint8 _optional;
        struct node *_next;
        struct node *_child;
        char _name[MAX_STREAM_PROTOCOL_KEY];

        explicit node( const char* name,node_t type,uint8 opt = 0 )
        {
            _type = type;
            _next = NULL;
            _child = NULL;
            _optional = opt;
            snprintf( _name,MAX_STREAM_PROTOCOL_KEY,"%s",name );
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
        struct node **_cur;
        struct node *_array[MAX_RECURSION_ARRAY];
    } _cur_protocol;

    void init( uint16 mod,uint16 func );
    void print_node( const struct node *nd,int32 indent );
public:
    stream_protocol();
    ~stream_protocol();

    void dump( uint16 mod,uint16 func );
    int32 check_dumplicate( const char *key );
    struct node *find( uint16 mod,uint16 func );
    void append( const char *key,node::node_t type,uint8 opt = 0 );

    int32 protocol_end();
    int32 protocol_begin( uint16 mod,uint16 func );

    int32 tag_array_end();
    int32 tag_array_begin( const char *key,uint8 opt = 0 );

    inline bool is_tagging() { return _tagging; }
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
