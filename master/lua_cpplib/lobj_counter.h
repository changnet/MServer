#ifndef __LOBJ_COUNTER_H__
#define __LOBJ_COUNTER_H__

#include <map>
#include <lua.hpp>
#include "../global/global.h"

/* 对暴露给lua的C++对象进行计数 */
class lobj_counter
{
public:
    static int32 dump( lua_State *L );
    static int32 obj_count( lua_State *L );
};

extern int32 luaopen_obj_counter( lua_State *L );

class obj_counter
{
public:
    static void uninstance();
    static class obj_counter *instance();

    int32 add_count( const char *name );
    int32 dec_count( const char *name );
    int32 final_check();

    friend class lobj_counter;
private:
    /* 创建时可以区分create、push_gc、push_not_gc这三种途径，但销毁时无法区分。故暂不记录 */
    struct count_info
    {
        uint64 _max;
        uint64 _cur;

        count_info() : _max(0),_cur(0) {}
        ~count_info() { _max = 0;_cur = 0; }
    };

    struct cmp_str
    {
        bool operator()(char const *a, char const *b)
        {
            return std::strcmp(a, b) < 0;
        }
    };

    /* 所有注册到lua的C++类名都是static字符串,不要传入一个临时字符串 */
    typedef std::map< const char*,struct count_info,cmp_str > counter;
private:
    explicit obj_counter();
    ~obj_counter();

    counter _counter;
    static class obj_counter *_obj_counter;
};

#endif /* __LOBJ_COUNTER_H__ */
