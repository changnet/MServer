#include "sql.h"

sql::sql()
{
    conn = msql_init( NULL );
    if ( !conn )
    {
        FATAL( "mysql library init fail:%s\n",mysql_error(conn) );
        return;
    }
}
