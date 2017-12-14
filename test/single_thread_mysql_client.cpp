/*
    single_thread_mysql_client.cpp
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <unistd.h>

#define DBHOST      "localhost"
#define DBUSER      "pca"
#define DBPASS      "pca"
#define DBPORT      3306
#define DBNAME      "dxponline"
#define DBSOCK      NULL //"/var/lib/mysql/mysql.sock"
#define DBPCNT      0

int main()
{
    MYSQL_RES *result;
    MYSQL_ROW row;
    MYSQL_FIELD *field;
    unsigned int num_fields;
    unsigned int i;
    const char *pStatement = "SHOW TABLES";
    mysql_library_init(0, NULL, NULL);
    MYSQL *mysql = mysql_init(NULL);
    unsigned int timeout = 3000;
    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    if (mysql_real_connect(mysql, DBHOST, DBUSER, DBPASS, DBNAME, DBPORT, DBSOCK, DBPCNT) == NULL)
    {
        printf("connect failed: %s\n", mysql_error(mysql));
        mysql_close(mysql);
        mysql_library_end();
        return 0;
    }

    printf("connect succssfully\n");

    if (0 != mysql_real_query(mysql, pStatement, strlen(pStatement)))
    {
        printf("query failed: %s\n", mysql_error(mysql));
        mysql_close(mysql);
        mysql_library_end();
        return 0;
    }

    result = mysql_store_result(mysql);

    if (result == NULL)
    {
        printf("fetch result failed: %s\n", mysql_error(mysql));
        mysql_close(mysql);
        mysql_library_end();
        return 0;
    }

    num_fields = mysql_num_fields(result);
    printf("numbers of result: %d\n", num_fields);

    while (NULL != (field = mysql_fetch_field(result)))
    {
        printf("field name: %s\n", field->name);
    }

    while (NULL != (row = mysql_fetch_row(result)))
    {
        unsigned long *lengths;
        lengths = mysql_fetch_lengths(result);

        for (i = 0; i < num_fields; i++)
        {
            printf("{%.*s} ", (int) lengths[i], row[i] ? row[i] : "NULL");
        }

        printf("\n");
    }

    mysql_free_result(result);
    mysql_close(mysql);
    mysql_library_end();
    return 0;
}
