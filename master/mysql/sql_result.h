#ifndef __SQL_RESULT_H__
#define __SQL_RESULT_H__

#include <mysql.h>
#include "../global/global.h"

#define SQL_FIELD_LEN    64

/* POD类型
 * 不要有构造、析构、虚函数
 * 请手动申请、释放内存
 */
struct sql_char
{
    char *_data;
    size_t _size;
    
    void set( const char *data,size_t size )
    {
        memcpy( _data,data,size );
        _size = size;
    }
    
    void malloc( size_t size )
    {
        _data = new char[size];
    }
    
    void free()
    {
        delete []_dadta;
    }
}

/* 每个col中field值
 * 这里的类型是需要转换为lua类型的(>=5.3),因此不细分为c类型
 * union只允许POD类型,sql_char这些类型得处理特殊
 */
union sql_col
{
    double _double;  /* double float */
    int64  _int64 ;  /* int8 int16 int32 int64 */
    uint64 _uint64;  /* uint8 uint16 uint32 uint64 */
    sql_char _char;  /* char string blob */
};

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

struct sql_row
{
    std::vector<sql_col *> cols;
};

struct sql_res
{
    int num_rows;
    int num_cols;
    std::vector<sql_field> fields;
    std::vector<sql_row *> rows  ;
    
    sql_res()
    {
        num_rows = 0;
        num_cols = 0;
    }
    
    ~sql_res()
    {
        /* 释放内存 */
        num_rows = 0;
        num_cols = 0;
    }
};

#endif /* __SQL_RESULT_H__ */
