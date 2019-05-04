#include <ctime> // for clock
#include <cstdarg>
#include "lmongo.h"

#include "ltools.h"
#include <lbson.h>

#include "../system/static_global.h"

lmongo::lmongo( lua_State *L ) : thread("lmongo")
{
    _valid = -1;
    _dbid = luaL_checkinteger( L,2 );
}

lmongo::~lmongo()
{
}

// 连接数据库
// 由于是开启线程去连接，并且是异步的，需要上层定时调用valid来检测是否连接上
int32 lmongo::start( lua_State *L )
{
    if ( active() )
    {
        return luaL_error( L,"mongo thread already active" );
    }

    const char *ip   = luaL_checkstring  ( L,1 );
    const int32 port = luaL_checkinteger ( L,2 );
    const char *usr  = luaL_checkstring  ( L,3 );
    const char *pwd  = luaL_checkstring  ( L,4 );
    const char *db   = luaL_checkstring  ( L,5 );

    _mongo.set( ip,port,usr,pwd,db );
    thread::start( 5 );

    return 0;
}

int32 lmongo::stop( lua_State *L )
{
    _valid = -1;
    thread::stop();

    return 0;
}

// 该连接是否已连接上(不是当前状态，仅仅是第一次连接上)
int32 lmongo::valid( lua_State *L )
{
    lua_pushinteger( L,_valid );

    return 1;
}

bool lmongo::initialize()
{
    int32 ok = _mongo.connect();
    if ( ok > 0 )
    {
        _valid = 0;
        ERROR( "mongo connect fail" );
        return false;
    }
    else if ( -1 == ok )
    {
        // 初始化正常，但是没ping通，需要重试
        return true;
    }

    _valid = 1;
    return true;
}

size_t lmongo::busy_job( size_t *finished,size_t *unfinished )
{
    lock();
    size_t finished_sz = _result.size();
    size_t unfinished_sz = _query.size();

    if ( is_busy() ) unfinished_sz += 1;
    unlock();

    if ( finished ) *finished = finished_sz;
    if ( unfinished ) *unfinished = unfinished_sz;

    return finished_sz + unfinished_sz;
}

void lmongo::routine( notify_t notify )
{
    /* 如果某段时间连不上，只能由下次超时后触发
     * 超时时间由thread::start参数设定
     */
    if ( _mongo.ping() ) return;

    _valid = 1;
    invoke_command();
}

bool lmongo::uninitialize()
{
    if ( _mongo.ping() )
    {
        ERROR( "mongo ping fail at cleanup,data may lost" );
        /* TODO:write to file */
    }
    else
    {
        invoke_command();
    }

    _mongo.disconnect();

    return true;
}

void lmongo::notification( notify_t notify )
{
    if ( NTF_CUSTOM == notify )
    {
        invoke_result();
    }
    else if ( NTF_ERROR == notify )
    {
        ERROR( "mongo thread error" );
    }
    else
    {
        assert( "unhandle mongo event",false );
    }
}

void lmongo::push_query( const struct mongo_query *query )
{
    /* socket缓冲区大小是有限制的。如果query队列不为空，则表示已通知过子线程，无需再
     * 次通知。避免写入socket缓冲区满造成Resource temporarily unavailable
     */
    bool _notify = false;

    lock();
    if ( _query.empty() )  /* 不要使用_query.size() */
    {
        _notify = true;
    }
    _query.push( query );
    unlock();

    if ( _notify ) notify_child( NTF_CUSTOM );
}

int32 lmongo::count( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo count:collection not specify" );
    }

    const char *str_query  = luaL_optstring( L,3,NULL );
    int64 skip  = luaL_optinteger( L,4,0 );
    int64 limit = luaL_optinteger( L,5,0 );

    bson_t *query = NULL;
    if ( str_query )
    {
        bson_error_t error;
        query = bson_new_from_json(
            reinterpret_cast<const uint8 *>(str_query),-1,&error );
        if ( !query )
        {
            return luaL_error( L,error.message );
        }
    }

    struct mongo_query *mongo_count = new mongo_query();
    mongo_count->set( id,MQT_COUNT );  /* count必须有返回 */
    mongo_count->set_count( collection,query,skip,limit );

    push_query( mongo_count );

    return 0;
}

const struct mongo_result *lmongo::pop_result()
{
    const struct mongo_result *res = NULL;

    lock();
    if ( !_result.empty() )
    {
        res = _result.front();
        _result.pop();
    }

    unlock();

    return res;
}

void lmongo::invoke_result()
{
    static lua_State *L = static_global::state();
    lua_pushcfunction( L,traceback );

    const struct mongo_result *res = NULL;
    while ( (res = pop_result()) )
    {
        // 发起请求到返回主线程的时间，毫秒.thread是db线程耗时
        static thread_log *logger = static_global::async_log();
        int64 real = static_global::ev()->ms_now() - res->_time;
        if ( 0 == res->_error.code)
        {
            logger->raw_write("",LO_MONGODB,
                "%s.%s:%s real:" FMT64d " msec,thread:%.3f sec",
                res->_clt,MQT_NAME[res->_mqt],res->_query,real,res->_elaspe);
        }
        else
        {
            logger->raw_write("",LO_MONGODB,
                "%s.%s:%s,code:%d,msg:%s,real:" FMT64d "  msec,thread:%.3f sec",
                res->_clt,MQT_NAME[res->_mqt],res->_query,res->_error.code,
                res->_error.message,real,res->_elaspe);

            ERROR(
                "%s.%s:%s,code:%d,msg:%s,real:" FMT64d "  msec,thread:%.3f sec",
                res->_clt,MQT_NAME[res->_mqt],res->_query,res->_error.code,
                res->_error.message,real,res->_elaspe);
        }
        // 为0表示不需要回调到脚本
        if ( 0 == res->_qid )
        {
            delete res;
            continue;
        }

        lua_getglobal( L,"mongodb_read_event" );

        lua_pushinteger( L,_dbid      );
        lua_pushinteger( L,res->_qid   );
        lua_pushinteger( L,res->_error.code );

        int32 nargs = 3;
        if ( res->_data )
        {
            struct error_collector error;
            bson_type_t root_type =
                res->_mqt == MQT_FIND ? BSON_TYPE_ARRAY : BSON_TYPE_DOCUMENT;

            if ( lbs_do_decode( L,res->_data,root_type,&error ) < 0 )
            {
                lua_pop( L,4 );
                ERROR( "mongo result decode error:%s",error.what );

                // 即使出错，也回调到脚本
            }
            else
            {
                nargs ++;
            }
        }

        if ( LUA_OK != lua_pcall( L,nargs,0,1 ) )
        {
            ERROR( "mongo call back error:%s",lua_tostring( L,-1 ) );
            lua_pop(L,1); /* remove error message */
        }

        delete res;
    }
    lua_pop(L,1); /* remove stacktrace */
}

const struct mongo_query *lmongo::pop_query()
{
    const struct mongo_query *query = NULL;

    lock();
    if ( !_query.empty() )
    {
        query = _query.front();
        _query.pop();
    }

    unlock();

    return query;
}

/* 缓冲区大小有限，如果已经通知过了，则不需要重复通知 */
void lmongo::push_result( const struct mongo_result *result )
{
    bool is_notify = false;

    lock();
    if ( _result.empty() )
    {
        is_notify = true;
    }
    _result.push( result );
    unlock();

    if ( is_notify ) notify_parent( NTF_CUSTOM );
}

/* 在子线程触发查询命令 */
void lmongo::invoke_command()
{
    const struct mongo_query *query = NULL;
    while ( (query = pop_query()) )
    {
        bool ok = false;
        clock_t begin = clock();
        struct mongo_result *res = new mongo_result();
        switch( query->_mqt )
        {
            case MQT_COUNT  : ok = _mongo.count( query,res );break;
            case MQT_FIND   : ok = _mongo.find ( query,res );break;
            case MQT_FMOD   : ok = _mongo.find_and_modify( query,res );break;
            case MQT_INSERT : ok = _mongo.insert( query,res );break;
            case MQT_UPDATE : ok = _mongo.update( query,res );break;
            case MQT_REMOVE : ok = _mongo.remove( query,res );break;
            default:
            {
                ERROR( "unknow handle mongo command type:%d\n",query->_mqt );
                delete res;
                delete query;
                continue;
            }
        }

        if ( ok )
        {
            assert( "mongo result check", 0 == res->_error.code );
        }
        else
        {
            assert( "mongo result check", 0 != res->_error.code );
        }

        res->_qid  = query->_qid;
        res->_mqt  = query->_mqt;
        res->_time = query->_time;
        snprintf( res->_clt,MONGO_VAR_LEN,"%s",query->_clt );
        res->_elaspe = ((float)(clock() - begin))/CLOCKS_PER_SEC;

        if ( query->_query )
        {
            char *ctx = bson_as_json (query->_query, NULL);
            snprintf( res->_query,MONGO_VAR_LEN,"%s",ctx );
            bson_free(ctx);
        }

        push_result( res );

        delete query;
    }
}

// 把对应的json字符串或者lua table参数转换为bson
// @opt:可选参数：0 表示可以传入nil，返回NULL;1 表示未传入参数则创建一个新的bson
bson_t *lmongo::string_or_table_to_bson(
    lua_State *L,int index,int opt,bson_t *bs,... )
{
#define CLEAN_BSON( arg )    \
    do{\
        va_list args;\
        for (va_start(args,arg);arg != END_BSON;arg = va_arg(args,bson_t *)){\
            if (arg) bson_destroy(arg);\
        }\
        va_end(args);\
    }while(0)

    bson_t *bson = NULL;
    if ( lua_istable( L,index ) ) // 自动将lua table 转化为bson
    {
        struct error_collector error;
        if ( !( bson = lbs_do_encode( L,index,NULL,&error ) ) )
        {
            CLEAN_BSON(bs);
            luaL_error( L,"table to bson error:%s",error.what );
            return NULL;
        }

        return bson;
    }

    if ( lua_isstring( L,index ) ) // json字符串
    {
        const char *json = lua_tostring( L,index );
        bson_error_t error;
        bson = bson_new_from_json(
            reinterpret_cast<const uint8 *>(json),-1,&error );
        if ( !bson )
        {
            CLEAN_BSON(bs);
            luaL_error( L,"json to bson error:%s",error.message );
            return NULL;
        }

        return bson;
    }

    if ( 0 == opt ) return NULL;
    if ( 1 == opt ) return bson_new();

    luaL_error(
        L,"argument #%d expect table or json string",index );
    return NULL;

#undef CLEAN_BSON
}

/* find( id,collection,query,fields ) */
int32 lmongo::find( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo find:collection not specify" );
    }

    bson_t *query = string_or_table_to_bson( L,3,1 );
    bson_t *opts  = string_or_table_to_bson( L,4,0,query,END_BSON );

    struct mongo_query *mongo_find = new mongo_query();
    mongo_find->set( id,MQT_FIND );  /* count必须有返回 */
    mongo_find->set_find( collection,query,opts );

    push_query( mongo_find );

    return 0;
}


/* find( id,collection,query,sort,update,fields,remove,upsert,new ) */
int32 lmongo::find_and_modify( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo find_and_modify:collection not specify" );
    }

    bson_t *query  = string_or_table_to_bson( L,3,1 );
    bson_t *sort   = string_or_table_to_bson( L,4,0,query,END_BSON );
    bson_t *update = string_or_table_to_bson( L,5,1,query,sort,END_BSON );
    bson_t *fields = string_or_table_to_bson( L,6,0,query,sort,update,END_BSON );

    bool _remove  = lua_toboolean( L,7 );
    bool _upsert  = lua_toboolean( L,8 );
    bool _new     = lua_toboolean( L,9 );

    struct mongo_query *mongo_fmod = new mongo_query();
    mongo_fmod->set( id,MQT_FMOD );
    mongo_fmod->set_find_modify(
        collection,query,sort,update,fields,_remove,_upsert,_new );

    push_query( mongo_fmod );
    return 0;
}

/* insert( id,collections,info ) */
int32 lmongo::insert( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo insert:collection not specify" );
    }

    bson_t *query = string_or_table_to_bson( L,3 );

    struct mongo_query *mongo_insert = new mongo_query();
    mongo_insert->set( id,MQT_INSERT );
    mongo_insert->set_insert( collection,query );

    push_query( mongo_insert );

    return 0;
}

/* update( id,collections,query,update,upsert,multi ) */
int32 lmongo::update( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo update:collection not specify" );
    }

    bson_t *query  = string_or_table_to_bson( L,3 );
    bson_t *update = string_or_table_to_bson( L,4,-1,query,END_BSON );

    int32 upsert = lua_toboolean( L,5 );
    int32 multi  = lua_toboolean( L,6 );

    struct mongo_query *mongo_update = new mongo_query();
    mongo_update->set( id,MQT_UPDATE );
    mongo_update->set_update( collection,query,update,upsert,multi );

    push_query( mongo_update );

    return 0;
}

/* remove( id,collections,query,multi ) */
int32 lmongo::remove( lua_State *L )
{
    if ( !active() )
    {
        return luaL_error( L,"mongo thread not active" );
    }

    int32 id = luaL_checkinteger( L,1 );
    const char *collection = luaL_checkstring( L,2 );
    if ( !collection )
    {
        return luaL_error( L,"mongo remove:collection not specify" );
    }

    bson_t *query = string_or_table_to_bson( L,3 );

    int32 multi  = lua_toboolean( L,4 );

    struct mongo_query *mongo_remove = new mongo_query();
    mongo_remove->set( id,MQT_REMOVE );
    mongo_remove->set_remove( collection,query,multi );

    push_query( mongo_remove );

    return 0;
}
