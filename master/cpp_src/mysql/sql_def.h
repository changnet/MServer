#pragma once

#include <vector>
#include <mysql.h>
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
    char _name[SQL_FIELD_LEN];
    enum_field_types    _type; /* define in mysql_com.h */
};

struct sql_col
{
    size_t _size;
    char *_value;

    sql_col()
    {
        _size  = 0;
        _value = NULL;
    }

    ~sql_col()
    {
        if ( _value ) delete []_value;

        _size  = 0;
        _value = NULL;
    }

    void set( const char *value,size_t size)
    {
        ASSERT ( 0 == _size && !_value, "sql col not clean" );
        if ( !value || 0 >= size ) return;  /* 结果为NULL */

        _size  = size;  /* 注意没加1 */
        _value = new char[size+1];

        /* 无论何种数据类型(包括寸进制)，都统一加\0
         * 方便后面转换为int之类时可以直接atoi
         */
        memcpy( _value,value,size );
        _value[size]         = '\0';
    }
};

struct sql_row
{
    std::vector<sql_col> _cols;
};

struct sql_res
{
    uint32 _num_rows;  // 行数
    uint32 _num_cols;  // 列数
    std::vector<sql_field> _fields; // 字段名
    std::vector<sql_row  > _rows  ; // 行数据
};

/* 查询结果 */
struct sql_result
{
    int32 _id;      /* 标记查询的id，用于回调 */
    int32 _ecode;
    struct sql_res *_res;
};

/* 查询请求 */
struct sql_query
{
    explicit sql_query( int32 id,size_t size,const char *stmt )
    {
        _id   = id;
        _size = size;

        _stmt = new char[size + 1];
        memcpy( _stmt,stmt,size );
        _stmt[size] = 0; // 保证0结尾，因为有些地方需要打印stmt
    }

    ~sql_query()
    {
        if ( _stmt ) delete []_stmt;

        _id       = 0;
        _size     = 0;
        _stmt     = NULL;
    }

    int32  _id;
    size_t _size;
    char  *_stmt;
};
