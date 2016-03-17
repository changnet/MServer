#include <math.h>
#include <limits.h>
#include <assert.h>

#include "parson.h"
#include "lparson.h"

#define ARRAY_KEY      "__array"
#define MAX_KEY_LEN    256
#define MAX_STACK_DEEP 1024

/* ========================== lua 5.1 ======================================= */
#ifndef LUA_MAXINTEGER
    #define LUA_MAXINTEGER (2147483647)
#endif

#ifndef LUA_MININTEGER
    #define LUA_MININTEGER (-2147483647-1)
#endif

/* this function was copy from src of lua5.3.1 */
LUALIB_API void luaL_setfuncs_ex (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}

#ifndef luaL_newlibtable
    #define luaL_newlibtable(L,l)	\
        lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)
#endif

#ifndef luaL_newlib
    #define luaL_newlib(L,l)  \
        (luaL_newlibtable(L,l), luaL_setfuncs_ex(L,l,0))
#endif

/* ========================== lua 5.1 ======================================= */

typedef const char* lua_parson_error_t;

static JSON_Value *encode_lua_value( lua_State *L,int index,lua_parson_error_t *lpe );

static inline void lua_parson_error( lua_State *L,lua_parson_error_t lpe )
{
    luaL_error( L,lpe );
}

static int is_array( lua_State *L,int index,int *array,int *max_index )
{
    double key = 0;
    assert( array && max_index );

    /* set default value */
    *array = 0;
    *max_index = -1;

    if ( luaL_getmetafield( L,index,ARRAY_KEY ) )
    {
        if ( LUA_TNIL != lua_type( L,-1 ) )
        {
            if ( lua_toboolean( L,-1 ) )
                *array = 1;
            else
            {
                lua_pop( L,1 );
                return 0;  /* it's object,use default value */
            }
        }

        lua_pop( L,1 );    /* pop metafield value */
    }

    /* get max array index */
    lua_pushnil( L );
    while ( lua_next( L,index ) != 0 )
    {
        /* stack status:table, key, value */
        /* array index must be interger and >= 1 */
        if ( lua_type( L, -2 ) != LUA_TNUMBER )
        {
            *max_index = -1;
            lua_pop( L,2 ); /* pop both key and value */
            return 0;
        }

        key = lua_tonumber( L, -2 );
        if ( floor(key) != key || key < 1 )
        {
            *max_index = -1;
            lua_pop( L,2 );
            return 0;
        }

        if ( key > INT_MAX ) /* array index over INT_MAX,must be object */
        {
            *array = 0;
            *max_index = -1;
            lua_pop( L,2 );

            return 0;
        }

        if ( key > *max_index ) *max_index = (int)key;

        lua_pop( L, 1 );
    }

    if ( *max_index > 0 ) *array = 1;

    return 0;
}

static inline JSON_Value *encode_value( lua_State *L,int index,lua_parson_error_t *lpe )
{
    JSON_Value *val = (JSON_Value *)0;
    int ty = lua_type( L,index );
    switch ( ty )
    {
        case LUA_TNIL     :
        {
            val = json_value_init_null();
        }break;
        case LUA_TBOOLEAN :
        {
            val = json_value_init_boolean( lua_toboolean(L,-1) );
        }break;
        case LUA_TSTRING  :
        {
            val = json_value_init_string( lua_tostring(L,-1) );
        }break;
        case LUA_TNUMBER  :
        {
            val = json_value_init_number( lua_tonumber(L,-1) );
        }break;
        case LUA_TTABLE   :
        {
            val = encode_lua_value( L,lua_gettop(L),lpe );
        }break;
        default :
        {
            *lpe = "table value must be nil,string,number,table,boolean";
        }
    }

    return val;
}

static JSON_Value *encode_invalid_key_array( lua_State *L,int index,lua_parson_error_t *lpe )
{
    JSON_Status st = 0;
    JSON_Value *val = json_value_init_array();
    JSON_Array *root_array = json_value_get_array( val );

    int stack_top = lua_gettop( L );
    lua_pushnil( L );
    while ( lua_next( L,index ) != 0 )
    {
        JSON_Value *array_val = (JSON_Value*)0;
        array_val =  encode_value( L,stack_top + 2,lpe );
        if ( !array_val )
        {
            lua_pop( L,2 );
            json_value_free( val );
            return 0;
        }

        st = json_array_append_value( root_array,array_val );
        if ( JSONSuccess != st )
        {
            lua_pop( L,2 );
            json_value_free( val );
            return 0;
        }

        lua_pop( L, 1 );
    }

    assert( val );
    return val;
}

static JSON_Value *encode_array( lua_State *L,int index,int max_index,lua_parson_error_t *lpe )
{
    int cur_key = 0;
    JSON_Status st = 0;
    JSON_Value *val = json_value_init_array();
    JSON_Array *root_array = json_value_get_array( val );

    int stack_top = lua_gettop(L);
    for ( cur_key = 1;cur_key <= max_index;cur_key ++ )
    {
        JSON_Value *array_val = (JSON_Value*)0;
        lua_rawgeti( L, -1, cur_key );
        array_val =  encode_value( L,stack_top + 1,lpe );
        if ( !array_val )
        {
            lua_pop( L,1 );
            json_value_free( val );
            return 0;
        }

        st = json_array_append_value( root_array,array_val );
        if ( JSONSuccess != st )
        {
            lua_pop( L,1 );
            json_value_free( val );
            return 0;
        }

        lua_pop( L, 1 );
    }

    assert( val );
    return val;
}

static inline JSON_Value *encode_object( lua_State *L,int index,lua_parson_error_t *lpe )
{
    JSON_Value *root_val = json_value_init_object();
    JSON_Object *root_object = json_value_get_object( root_val );

    lua_pushnil( L );
    while ( lua_next( L,index ) != 0 )
    {
        JSON_Status st = 0;
        char key[MAX_KEY_LEN];
        int ty = lua_type( L,-2 );
        const char *pkey = (char *)0;
        JSON_Value *obj_val = (JSON_Value *)0;

        switch ( ty )
        {
            case LUA_TNUMBER :
            {
                double val = lua_tonumber( L,-2 );
                if ( floor(val) == val ) /* interger key */
                    snprintf( key,MAX_KEY_LEN,"%.0f",val );
                else
                    snprintf( key,MAX_KEY_LEN,"%f",val );
                pkey = key;
            }break;
            case LUA_TSTRING :
            {
                size_t len = 0;
                pkey = lua_tolstring( L,-2,&len );
                if ( len <= 0 || len > MAX_KEY_LEN )
                {
                    *lpe = "table string key too long";
                    lua_pop( L,2 );
                    json_value_free( root_val );
                    return 0;
                }
            }break;
            default :
            {
                /* LUA_TNIL LUA_TBOOLEAN LUA_TTABLE LUA_TFUNCTION, LUA_TUSERDATA,
                 * LUA_TTHREAD, LUA_TLIGHTUSERDATA can't not be key
                 */
                *lpe = "table key must be a number or string";
                lua_pop( L,2 );
                json_value_free( root_val );
                return 0;
            }
        }
        assert( pkey );
        obj_val = encode_value( L,lua_gettop(L),lpe );
        if ( !obj_val )
        {
            lua_pop( L,2 );
            json_value_free( root_val );
            return 0;
        }

        st = json_object_set_value( root_object,pkey,obj_val );
        if ( JSONSuccess != st )
        {
            *lpe = "json_object_set_value fail";
            lua_pop( L,2 );
            json_value_free( root_val );
            return 0;
        }

        lua_pop( L,1 );
    }

    return root_val;
}

static JSON_Value *encode_lua_value( lua_State *L,int index,lua_parson_error_t *lpe )
{
    int array  = 0;
    int max_index  = -1;

    if ( lua_gettop( L ) > MAX_STACK_DEEP )
    {
        *lpe = "lua parson encode too deep";
        return (JSON_Value *)0;
    }

    if ( !lua_checkstack( L,2 ) )
    {
        *lpe = "lua parson stack overflow";
        return (JSON_Value *)0;
    }

    if ( is_array( L,index,&array,&max_index ) < 0 )
    {
        return (JSON_Value *)0;
    }

    if ( array )
    {
        if ( max_index > 0 )
            return encode_array( L,index,max_index,lpe );
        else
            return encode_invalid_key_array( L,index,lpe );
    }

    return encode_object( L,index,lpe );
}

static int encode( lua_State *L )
{
    char *str;
    int pretty = 0;
    JSON_Value *val;
    lua_parson_error_t lpe = (lua_parson_error_t)0;

    if ( lua_type( L,1 ) != LUA_TTABLE )
    {
        return luaL_error( L,"lua_parson encode,argument #1 table expect" );
    }

    pretty = lua_toboolean( L,2 );
    lua_settop( L,1 );  /* remove pretty,only table in stack now */

    val = encode_lua_value( L,1,&lpe );
    if ( !val )
    {
        lua_parson_error( L,lpe );
        return 0;
    }

    if ( pretty )
        str = json_serialize_to_string_pretty( val );
    else
        str = json_serialize_to_string( val );

    lua_pushstring( L,str );

    json_free_serialized_string( str );
    json_value_free( val );

    return 1;
}

static int encode_to_file( lua_State *L )
{
    int pretty = 0;
    JSON_Value *val;
    JSON_Status st = 0;
    const char *path = 0;
    lua_parson_error_t lpe = (lua_parson_error_t)0;

    if ( lua_type( L,1 ) != LUA_TTABLE )
    {
        return luaL_error( L,"lua_parson encode,argument #1 table expect" );
    }

    path = luaL_checkstring( L,2 );
    pretty = lua_toboolean( L,3 );
    lua_settop( L,1 );  /* remove path,pretty,only table in stack now */

    val = encode_lua_value( L,1,&lpe );
    if ( !val )
    {
        lua_parson_error( L,lpe );
        return 0;
    }

    if ( pretty )
        st = json_serialize_to_file_pretty( val,path );
    else
        st = json_serialize_to_file( val,path );

    json_value_free( val );

    if ( JSONSuccess != st )
        lua_pushboolean( L,0 );
    else
        lua_pushboolean( L,1 );

    return 1;
}

static int decode_parson_value( lua_State *L,const JSON_Value* js_val,lua_parson_error_t *lpe )
{
    int push_num  = 1;
    int stack_top = lua_gettop( L );
    if ( stack_top > MAX_STACK_DEEP )
    {
        *lpe = "lua parson decode too deep";
        return -1;
    }

    if ( !lua_checkstack( L,3 ) )
    {
        *lpe = "lua parson stack overflow";
        return -1;
    }

    switch ( json_value_get_type( js_val ) )
    {
        case JSONArray:
        {
            size_t index = 0;
            JSON_Array *array = json_value_get_array( js_val );
            size_t count = json_array_get_count( array );

            lua_createtable( L,count,0 );
            for ( index = 0;index < count;index ++ )
            {
                JSON_Value *temp_value = json_array_get_value( array,index );
                int stack_num = decode_parson_value( L,temp_value,lpe );
                if ( stack_num < 0 ) /* error */
                {
                    return -1;
                }
                else if ( stack_num > 0 )
                {
                    assert( 1 == stack_num );
                    /* lua table index start from 1 */
                    lua_rawseti( L,stack_top + 1,index + 1 );
                }
                /* stack_num == 0,value is nil */
            }
        }break;
        case JSONObject:
        {
            size_t index = 0;
            JSON_Object *object = json_value_get_object( js_val );
            size_t count = json_object_get_count( object );

            lua_createtable( L,0,count );
            for ( index = 0;index < count;index ++ )
            {
                int stack_num = -1;
                const char *key = json_object_get_name( object,index );
                JSON_Value *temp_value = json_object_get_value( object,key );

                lua_pushstring( L,key );
                stack_num = decode_parson_value( L,temp_value,lpe );
                if ( stack_num < 0 ) /* error */
                {
                    lua_pop( L,1 ); /* pop the key */
                    return -1;
                }
                else if ( stack_num > 0 )
                {
                    assert( 1 == stack_num );
                    lua_rawset( L,stack_top + 1 );
                }
                else
                {
                    lua_pop( L,1 ); /* pop the key */
                }
            }
        }break;
        case JSONString:
        {
            const char *val = json_value_get_string( js_val );
            lua_pushstring( L,val );
        }break;
        case JSONBoolean:
        {
            int val = json_value_get_boolean( js_val );
            lua_pushboolean( L,val );
        }break;
        case JSONNumber:
        {
            double val = json_value_get_number( js_val );
            /* if using lua5.1,interger is int32 */
            if ( floor(val) == val && (val <= LUA_MAXINTEGER && val >= LUA_MININTEGER) )
                lua_pushinteger( L,val );
            else
                lua_pushnumber( L,val );
        }break;
        case JSONNull:
        {
            push_num = 0;
        }break;
        case JSONError:
        {
            *lpe = "parson get error type";
            return -1;
        }break;
        default :
        {
            *lpe = "unhandle json value type";
            return -1;
        }
    }

    assert( 0 == push_num || 1 == push_num );
    return push_num;
}

static int decode( lua_State *L )
{
    int return_code = 0;
    JSON_Value *val = (JSON_Value*)0;
    const char *str = luaL_checkstring( L,1 );
    int comment = lua_toboolean( L,2 );
    lua_parson_error_t lpe = (lua_parson_error_t)0;

    assert( str );
    if ( comment )
        val = json_parse_string_with_comments( str );
    else
        val = json_parse_string( str );
    if ( !val )
    {
        return luaL_error( L,"invalid json string" );
    }

    return_code = decode_parson_value( L,val,&lpe );
    json_value_free( val );

    if ( return_code < 0 )
    {
        lua_parson_error( L,lpe );
        return 0;
    }

    /* decode string "null",0 == return_code */
    assert( 0 == return_code || 1 == return_code );
    return return_code;
}

static int decode_from_file( lua_State *L )
{
    int return_code = 0;
    JSON_Value *val = (JSON_Value*)0;
    const char *path = luaL_checkstring( L,1 );
    int comment = lua_toboolean( L,2 );
    lua_parson_error_t lpe = (lua_parson_error_t)0;

    assert( path );
    if ( comment )
        val = json_parse_file_with_comments( path );
    else
        val = json_parse_file( path );
    if ( !val )
    {
        return luaL_error( L,"invalid json string" );;
    }

    return_code = decode_parson_value( L,val,&lpe );
    json_value_free( val );

    if ( return_code < 0 )
    {
        lua_parson_error( L,lpe );
        return 0;
    }

    /* decode string "null",0 == return_code */
    assert( 0 == return_code || 1 == return_code );
    return return_code;
}

/* ====================LIBRARY INITIALISATION FUNCTION======================= */

static const luaL_Reg lua_parson_lib[] =
{
    {"encode", encode},
    {"decode", decode},
    {"encode_to_file", encode_to_file},
    {"decode_from_file", decode_from_file},
    {NULL, NULL}
};

int luaopen_lua_parson(lua_State *L)
{
    luaL_newlib(L, lua_parson_lib);
    return 1;
}
