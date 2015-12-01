#ifndef __MONGO_H__
#define __MONGO_H__

#include <bson.h>
#include <bcon.h>
#include <mongoc.h>
#include "../global/global.h"

/* mongo db会覆盖assert，这里必须重新覆盖 */
#include "../global/assert.h"

#define MONGO_VAR_LEN    64

class mongo
{
public:
    static void init();
    static void cleanup();

    mongo();
    ~mongo();

    void set( const char *_ip,int32 _port,const char *_usr,const char *_pwd,
        const char *_db );
        
    int32 ping();
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
