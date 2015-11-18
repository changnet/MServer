#ifndef __SQL_RESULT_H__
#define __SQL_RESULT_H__

/* db结果存储，实际就是把MYSQL_RES按自己的方式实现一遍
 * mysql的结果通过socket传输，因此只能取到char类型自己转换
 * 1.子线程将数据转换为对应类型，主线程直接使用。
 * 2.所有数据原封不动以char存储，主线程自己转换
 * 方案1中主线程省去转换时间，但需要用union将double、char混合存储，对以后做cache很不
 * 利。况且atoi之类的转换函数效率并不算太低，因此采用方案1
 */

#include <vector>
#include <mysql/mysql.h>
#include "../global/global.h"

#define SQL_FIELD_LEN    64
#define SQL_VAR_LEN      64

/*
enum enum_field_types { MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY,
            MYSQL_TYPE_SHORT,  MYSQL_TYPE_LONG,
            MYSQL_TYPE_FLOAT,  MYSQL_TYPE_DOUBLE,
            MYSQL_TYPE_NULL,   MYSQL_TYPE_TIMESTAMP,
            MYSQL_TYPE_LONGLONG,MYSQL_TYPE_INT24,
            MYSQL_TYPE_DATE,   MYSQL_TYPE_TIME,
            MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR,
            MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
            MYSQL_TYPE_BIT,
            MYSQL_TYPE_TIMESTAMP2,
            MYSQL_TYPE_DATETIME2,
            MYSQL_TYPE_TIME2,
            MYSQL_TYPE_NEWDECIMAL=246,
            MYSQL_TYPE_ENUM=247,
            MYSQL_TYPE_SET=248,
            MYSQL_TYPE_TINY_BLOB=249,
            MYSQL_TYPE_MEDIUM_BLOB=250,
            MYSQL_TYPE_LONG_BLOB=251,
            MYSQL_TYPE_BLOB=252,
            MYSQL_TYPE_VAR_STRING=253,
            MYSQL_TYPE_STRING=254,
            MYSQL_TYPE_GEOMETRY=255
};
*/
struct sql_field
{
    char name[SQL_FIELD_LEN];
    enum_field_types    type; /* define in mysql_com.h */
};

struct sql_col
{
    size_t size;
    char *value;
    
    sql_col()
    {
        size  = 0;
        value = NULL;
    }
    
    ~sql_col()
    {
        if ( value ) delete value;
        
        size  = 0;
        value = NULL;
    }

    void set( const char *_value,size_t _size)
    {
        assert ( "sql col not clean",0 == size && !value );
        if ( !_value || 0 >= _size ) return;  /* 结果为NULL */
        
        size = _size;  /* 注意没加1 */
        value = new char[_size+1];
        
        /* 无论何种数据类型(包括寸进制)，都统一加\0 */
        memcpy( value,_value,_size );
        value[_size] = '\0';
    }
};

struct sql_row
{
    std::vector<sql_col> cols;
};

struct sql_res
{
    int32 num_rows;
    int32 num_cols;
    std::vector<sql_field> fields;
    std::vector<sql_row  > rows  ;
    // 
    // sql_res()
    // {
    //     num_rows = 0;
    //     num_cols = 0;
    // }
    // 
    // ~sql_res()
    // {
    //     std::vector<sql_row *>::iterator itr = rows.begin();
    //     while ( itr != rows.end() )
    //     {
    //         delete *itr;
    //         ++itr;
    //     }
    // 
    //     fields.clear();
    //     rows.clear  ();
    //     num_rows =   0;
    //     num_cols =   0;
    // }
};

enum sql_type
{
    CALL   = 1,
    DELETE = 2,
    UPDATE = 3,
    SELECT = 4,
    INSERT = 5,
};

struct sql_query
{
    explicit sql_query( sql_type _type,size_t _size,const char *_stmt )
    {
        type = _type;
        size = _size;
        PDEBUG( "sql query call ..." );
        stmt = new char[size];
        memcpy( stmt,_stmt,size );
    }

    ~sql_query()
    {
        if ( stmt ) delete stmt;
        size = 0;
        PDEBUG( "sql query destruct call ..." );
    }

    sql_type type;
    size_t   size;
    char    *stmt;
};

#endif /* __SQL_RESULT_H__ */
