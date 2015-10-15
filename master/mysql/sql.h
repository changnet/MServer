#ifndef __SQL_H__
#define __SQL_H__

#include <mysql/mysql.h>
#include "../global/global.h"

class sql
{
public:
    sql();
    ~sql();
private:
    MYSQL *conn;
};

#endif /* __SQL_H__ */
