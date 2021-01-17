#include "lacism.hpp"

static inline int isspace_ex(int c)
{
    return (c == '\t' || c == '\n' ||
        c == '\v' || c == '\f' || c == '\r' || c == ' ' ? 1 : 0);
}

LAcism::LAcism(lua_State *L)
{
    _psp            = nullptr;
    _pattv          = nullptr;
    _case_sensitive = 1;

    _patt.ptr = nullptr;
    _patt.len = 0;

    UNUSED(L);
}

LAcism::~LAcism()
{
    // 调用msutil里分配的内存，要用那边的分配方式释放
    if (_patt.ptr)
    {
        buffree(_patt);
        _patt.ptr = nullptr;
        _patt.len = 0;
    }

    if (_pattv)
    {
        free(_pattv);
        _pattv = nullptr;
    }

    if (_psp)
    {
        acism_destroy(_psp);
        _psp = nullptr;
    }
}

/* 扫描关键字,扫描到其中一个即中止 */
int32_t LAcism::scan(lua_State *L)
{
    if (!_psp)
    {
        return luaL_error(L, "no pattern load yet");
    }

    MEMREF text;
    text.ptr = luaL_checklstring(L, 1, &(text.len));

    int32_t pos = acism_scan(_psp, text,
        [](int32_t str_num, int32_t text_pos, void *fn_data) {
            (void)str_num;
            (void)fn_data;

            return text_pos; // 返回非0表示不再匹配
        }, nullptr, _case_sensitive);

    lua_pushinteger(L, pos);
    return 1;
}

/* 替换关键字 */
int32_t LAcism::replace(lua_State *L)
{
    MEMREF text;

    text.ptr       = luaL_checklstring(L, 1, &(text.len));
    const char *ch = luaL_checkstring(L, 2);

    /* no worlds loaded */
    if (!_psp)
    {
        lua_pushvalue(L, 1);
        return 1;
    }

    static const int MAX_TEXT = 1024 * 10;
    if (text.len >= MAX_TEXT)
    {
        return luaL_error(L, "replace text too large: %d", text.len);
    }

    //https://stackoverflow.com/questions/28746744/passing-capturing-lambda-as-function-pointer
    // lambda capture变量后，无法直接转为函数指针，这里需要构建一个data传参数
    struct FnData
    {
        char ch;
        size_t pos;
        MEMREF *pattv;
        const char *raw;
        char buff[MAX_TEXT];
    } thread_local fn_data;

    fn_data.ch = *ch;
    fn_data.pos = 0;
    fn_data.pattv = _pattv;
    fn_data.raw = text.ptr;

    (void)acism_scan(_psp, text,
        [](int32_t str_num, int32_t text_pos, void *fn_data) {
            // @param str_num 匹配到的字库索引
            // @param text_pos 在原字符串中匹配结束的位置
            // @param fn_data 自定义数据

            FnData *d = (FnData *)fn_data;
            size_t &pos = d->pos;
            // 把未发生替换的那部分复制到缓存区
            while (pos < text_pos - d->pattv[str_num].len)
            {
                d->buff[pos] = d->raw[pos];
                pos++;
            }
            // 把需要替换的，都写入需要替换的字符
            while (pos < (size_t)text_pos) d->buff[pos++] = d->ch;

            return 0; // 返回0表示继续匹配
        }, &fn_data, _case_sensitive);

    // 没有发生替换，无需拷贝字符串
    if (!fn_data.pos)
    {
        lua_pushvalue(L, 1);
    }
    else
    {
        // 最后可能还有一部分未复制，这里处理一下
        size_t &pos = fn_data.pos;
        while (pos < text.len)
        {
            fn_data.buff[pos] = text.ptr[pos];
            pos ++;
        }
        lua_pushlstring(L, fn_data.buff, pos);
    }

    return 1;
}

int32_t LAcism::load_from_file(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    if (lua_isboolean(L, 2))
    {
        _case_sensitive = lua_toboolean(L, 2);
    }

    _patt = slurp(path);
    if (!_patt.ptr)
    {
        return luaL_error(L, "can't read file[%s]:", path, strerror(errno));
    }

    // 去掉文件尾的空格
    // msutil.c里有chomp函数，但这个函数调用了isspace，在win下不能处理中文
    // https://stackoverflow.com/questions/3108282/unicode-supported-isdigit-and-isspace-function
    while (_patt.len > 0 && isspace_ex(_patt.ptr[_patt.len - 1]))
    {
        _patt.ptr[--_patt.len] = 0;
    }

    // 区分大小写，全部转成小写
    if (!_case_sensitive)
    {
        for (size_t i = 0; i < _patt.len; i++)
        {
            *(_patt.ptr + i) = ::tolower(*(_patt.ptr + i));
        }
    }

    int32_t npatts = 0;
    _pattv = refsplit(_patt.ptr, '\n', &npatts);
    _psp   = acism_create(_pattv, npatts);

    lua_pushinteger(L, npatts);
    return 1;
}
