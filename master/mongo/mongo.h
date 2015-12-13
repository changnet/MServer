#ifndef __MONGO_H__
#define __MONGO_H__

#include "mongo_def.h"

class mongo
{
public:
    static void init();
    static void cleanup();

    mongo();
    ~mongo();

    void set( const char *_ip,int32 _port,const char *_usr,const char *_pwd,
        const char *_db );

    int64 count( const char *_collection,bson_error_t &_err,bson_t *query = NULL,
        int64 skip = 0,int64 limit = 0 );
    int32 ping( bson_error_t *error = NULL );
    int32 connect();
    void disconnect();
private:
    int32 port;
    char ip [MONGO_VAR_LEN];
    char usr[MONGO_VAR_LEN];
    char pwd[MONGO_VAR_LEN];
    char db [MONGO_VAR_LEN];

    mongoc_client_t *conn;
};

#endif
