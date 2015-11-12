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
    size_t _size;
    char *_value;
    
    explicit sql_col( const char *value,size_t size )
    {
        _size  = size;
        _value = new char[_size];
        
        memcpy( _value,value,_size );
    }
    
    ~sql_col()
    {
        if ( _value ) delete _value;
        _value = NULL;
        _size  = 0;
    }
};

struct sql_row
{
    std::vector<sql_col *> cols;
    
    sql_row() {}
    ~sql_row()
    {
        std::vector<sql_col *>::iterator itr = cols.begin();
        while ( itr != cols.end() )
        {
            if ( *itr ) delete *itr;  // 可能是NULL
            ++itr;
        }
        
        cols.clear();
    }
};

struct sql_res
{
    int32 num_rows;
    int32 num_cols;
    std::vector<sql_field> fields;
    std::vector<sql_row *> rows  ;
    
    sql_res()
    {
        num_rows = 0;
        num_cols = 0;
    }
    
    ~sql_res()
    {
        std::vector<sql_row *>::iterator itr = rows.begin();
        while ( itr != rows.end() )
        {
            delete *itr;
            ++itr;
        }

        fields.clear();
        rows.clear  ();
        num_rows =   0;
        num_cols =   0;
    }
};

#endif /* __SQL_RESULT_H__ */
