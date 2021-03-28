#pragma once

#include <mariadb/mysql.h>
#include "../global/global.hpp"

/// 字段名字
struct SqlField
{
    char _name[64]; /// 字段名，如果超过最大限制将被截断
    /*
    define in mysql_com.h 
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
    enum_field_types _type;
};

/// 内容
class SqlCol
{
public:
    SqlCol()
    {
        _size  = 0;
        _value_ex = nullptr;
    }

    ~SqlCol()
    {
        if (_value_ex) delete[] _value_ex;

        _size  = 0;
        _value_ex = nullptr;
    }

    void set(const char *value, size_t size)
    {
        assert(0 == _size && !_value);
        if (!value || 0 >= size) return; /* 结果为NULL */

        _size  = size; /* 注意没加1 */
        char *mem_value = _value;

        // mysql把所有类型的数据都存为char数组，对于int、double之类的，使用_value避免内存
        // 分配，对于text、blob之类的，只能根据大小重新分配内存
        // TODO 是否要搞一个内存池，或者把_value的默认值再调大些?
        if (_size >= sizeof(_value))
        {
            _value_ex = new char[size + 1];
            mem_value = _value_ex;
        }

        // 无论何种数据类型(包括寸进制)，都统一加\0方便后面转换为int之类时可以直接atoi
        memcpy(mem_value, value, size);
        mem_value[size] = '\0';
    }
private:
    size_t _size;
    char _value[64];
    char *_value_ex;
};

/* 查询结果 */
struct SqlResult
{
    using SqlRow = std::vector<SqlCol>;

    int32_t _id; /* 标记查询的id，用于回调 */
    int32_t _ecode;
    uint32_t _num_rows;            // 行数
    uint32_t _num_cols;            // 列数
    std::vector<SqlField> _fields; // 字段名
    std::vector<SqlRow> _rows;     // 行数据
};

/// 查询请求
class SqlQuery
{
    explicit SqlQuery(int32_t id, size_t size, const char *stmt)
    {
        _id   = id;
        _size = size;

        char *mem_stmt = _stmt;
        if (size >= sizeof(_stmt))
        {
            _stmt_ex = new char[size + 1];
            mem_stmt = _stmt_ex;
        }
        memcpy(mem_stmt, stmt, size);
        mem_stmt[size] = 0; // 保证0结尾，因为有些地方需要打印stmt
    }

    ~SqlQuery()
    {
        if (_stmt_ex) delete[] _stmt_ex;

        _id   = 0;
        _size = 0;
        _stmt_ex = nullptr;
    }

private:
    int32_t _id;
    size_t _size;
    char _stmt[256];
    char *_stmt_ex;
};
