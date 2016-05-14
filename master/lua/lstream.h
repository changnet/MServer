#ifndef __LSTREAM_H__
#define __LSTREAM_H__

#include <lua.hpp>
#include "../stream/stream_protocol.h"

class lstream
{
public:
    ~lstream();
    explicit lstream( lua_State *L );
public: /* lua interface */
    int protocol_end();
    int protocol_begin();

    int tag();
    int dump();

    int tag_array_end();
    int tag_array_begin();

    int pack();
    int unpack();

    static int read_int8 ( lua_State *L );
    static int read_int16( lua_State *L );
    static int read_int32( lua_State *L );
    static int read_int64( lua_State *L );

    static int read_uint8 ( lua_State *L );
    static int read_uint16( lua_State *L );
    static int read_uint32( lua_State *L );
    static int read_uint64( lua_State *L );

    static int write_int8 ( lua_State *L );
    static int write_int16( lua_State *L );
    static int write_int32( lua_State *L );
    static int write_int64( lua_State *L );

    static int write_uint8 ( lua_State *L );
    static int write_uint16( lua_State *L );
    static int write_uint32( lua_State *L );
    static int write_uint64( lua_State *L );

    static int read_string( lua_State *L );
    static int write_string( lua_State *L );

    /* allocate a buffer and push it to lua stack as userdata */
    static int allocate( lua_State *L );
public: /* c++ interface */
    inline class stream_protocol::node *find( uint16 mod,uint16 func )
    {
        return _stream.find( mod,func );
    }
    /* convert a lua table into binary stream buffer */
    int pack_buffer( int mod,int func,const char *buffer,unsigned int size );
    /* convert binary stream buffer into a lua table */
    int unpack_buffer( int mod,int func,const char *buffer,unsigned int size );
private:
    lua_State *L;
    class stream_protocol _stream;
};

#endif /* __LSTREAM_H__ */
