#include <fcntl.h> // open O_RDONLY
#include <sys/stat.h>

#include "lacism.h"

LAcism::LAcism(lua_State *L)
{
    _psp            = NULL;
    _pattv          = NULL;
    _loaded         = 0;
    _case_sensitive = 1;

    _patt.ptr = NULL;
    _patt.len = 0;

    memset(&_memrpl, 0, sizeof(MemRpl));
}

LAcism::~LAcism()
{
    if (_patt.ptr)
    {
        delete[] _patt.ptr;
        _patt.ptr = NULL;
        _patt.len = 0;
    }

    if (_pattv)
    {
        delete[] _pattv;
        _pattv = NULL;
    }

    if (_psp)
    {
        acism_destroy(_psp);
        _psp = NULL;
    }

    if (_memrpl.rpl_text)
    {
        delete[] _memrpl.rpl_text;
        _memrpl.rpl_text = NULL;
    }
}

/* 返回0,继续搜索
 * 返回非0,终止搜索，并且acism_more返回该值
 */
int32_t LAcism::on_match(int32_t strnum, int32_t textpos, MEMREF const *pattv)
{
    (void)strnum, (void)pattv;

    return textpos;
}

int32_t LAcism::on_replace(int32_t strnum, int32_t textpos, void *context)
{
    ASSERT(context, "acism on_replace NULL context");

    class LAcism *acism = static_cast<class LAcism *>(context);
    return acism->do_replace(strnum, textpos);
}

int32_t LAcism::do_replace(int32_t strnum, int32_t textpos)
{
    ASSERT((size_t) textpos >= _memrpl.text_pos, "acism do_replace buffer error");

    //匹配到的单词长度
    size_t pattv_len = _pattv[strnum].len;
    /**
     * 同一个词，可能会被重复匹配，例如：
     * O、K和OK都是需要匹配的单词，OK是要匹配的字符串
     * 匹配到O时，会命中单词O，这时pattv_len为1
     * 匹配到OK时，会命中单词O，这时pattv_len为1
     * 匹配到OK时，会命中单词OK，这时pattv_len为2,计算出来的长度就为负数
     * 一共会被匹配到3次
     */
    if (textpos - pattv_len < _memrpl.text_pos)
    {
        _memrpl.text_pos = textpos;
        return 0;
    }

    size_t str_len   = textpos - pattv_len - _memrpl.text_pos;
    size_t mem_len   = str_len + _memrpl.word_len + _memrpl.rpl_len;

    /* 重新分配替换后的内存，必须是<=，防止mem_len为0的情况 */
    if (_memrpl.rpl_size <= mem_len) _memrpl.reserved(mem_len);

    ASSERT(_memrpl.rpl_size >= mem_len, "acism do_replace buffer overflow");

    // 复制被替换之前的字符串
    if (str_len > 0)
    {
        memcpy(_memrpl.rpl_text + _memrpl.rpl_len,
               _memrpl.text + _memrpl.text_pos, str_len);
        _memrpl.rpl_len += str_len;
    }

    // 复制要替换的字符串
    memcpy(_memrpl.rpl_text + _memrpl.rpl_len, _memrpl.word, _memrpl.word_len);
    _memrpl.rpl_len += _memrpl.word_len;

    _memrpl.text_pos = textpos;

    return 0; /* continue replace */
}

/* 扫描关键字,扫描到其中一个即中止 */
int32_t LAcism::scan(lua_State *L)
{
    if (!_loaded)
    {
        return luaL_error(L, "no pattern load yet");
    }

    /* no worlds loaded */
    if (!_pattv || !_psp)
    {
        lua_pushinteger(L, 0);
        return 1;
    }

    size_t len      = 0;
    const char *str = luaL_checklstring(L, 1, &len);

    MEMREF text = {str, len};

    int32_t textpos = acism_scan(_psp, text, (ACISM_ACTION *)LAcism::on_match,
                                 _pattv, _case_sensitive);

    lua_pushinteger(L, textpos);
    return 1;
}

/* 替换关键字 */
int32_t LAcism::replace(lua_State *L)
{
    if (!_loaded)
    {
        return luaL_error(L, "no pattern load yet");
    }

    MEMREF text = {NULL, 0};

    text.ptr     = luaL_checklstring(L, 1, &(text.len));
    _memrpl.word = luaL_checklstring(L, 2, &(_memrpl.word_len));

    /* no worlds loaded */
    if (!_pattv || !_psp)
    {
        lua_pushvalue(L, 1);
        return 1;
    }

    _memrpl.text     = text.ptr;
    _memrpl.rpl_len  = 0;
    _memrpl.text_pos = 0;

    (void)acism_scan(_psp, text, (ACISM_ACTION *)LAcism::on_replace, this,
                     _case_sensitive);

    if (0 == _memrpl.rpl_len)
    {
        lua_pushvalue(L, 1);
        return 1;
    }

    if (_memrpl.text_pos < text.len)
    {
        size_t str_len = text.len - _memrpl.text_pos;
        size_t mem_len = _memrpl.rpl_len + str_len;
        if (_memrpl.rpl_size <= mem_len)
        {
            _memrpl.reserved(mem_len);
        }

        memcpy(_memrpl.rpl_text + _memrpl.rpl_len,
               _memrpl.text + _memrpl.text_pos, str_len);

        _memrpl.text_pos = text.len;
        _memrpl.rpl_len += str_len;
    }

    lua_pushlstring(L, _memrpl.rpl_text, _memrpl.rpl_len);

    return 1;
}

int32_t LAcism::load_from_file(lua_State *L)
{
    const char *path       = luaL_checkstring(L, 1);
    int32_t case_sensitive = 1;

    if (lua_isboolean(L, 2))
    {
        case_sensitive = lua_toboolean(L, 2);
    }

    if (this->acism_slurp(path))
    {
        return luaL_error(L, "can't read file[%s]:", path, strerror(errno));
    }

    _loaded = 1; /* mark file loaded */
    if (!case_sensitive)
    {
        for (size_t i = 0; i < _patt.len; i++)
        {
            *(_patt.ptr + i) = ::tolower(*(_patt.ptr + i));
        }
    }

    int32_t npatts = 0;
    if (_patt.ptr)
    {
        _pattv = this->acism_refsplit(_patt.ptr, '\n', &npatts);
        _psp   = acism_create(_pattv, npatts);
    }
    _case_sensitive = case_sensitive;

    lua_pushinteger(L, npatts);
    return 1;
}

int32_t LAcism::acism_slurp(const char *path)
{
    int32_t fd = ::open(path, O_RDONLY);
    if (fd < 0) return errno;

    struct stat s;
    if (fstat(fd, &s) || !S_ISREG(s.st_mode))
    {
        ::close(fd);
        return errno;
    }

    /* empty file,do nothing */
    if (s.st_size <= 0)
    {
        ::close(fd);
        return 0;
    }

    /* In a 32-bit implementation, ACISM can handle about 10MB of pattern text.
     * Aho-Corasick-Interleaved_State_Matrix.pdf
     */

    ASSERT(s.st_size < 1024 * 1024 * 10, "filter worlds file too large");

    ASSERT(!_patt.ptr, "patt.ptr not free"); /* code hot fix ? */

    _patt.ptr = new char[s.st_size + 1];
    _patt.len = s.st_size;

    if (_patt.len != (unsigned)read(fd, _patt.ptr, _patt.len))
    {
        ::close(fd);
        return errno;
    }

    ::close(fd);
    _patt.ptr[_patt.len] = 0;

    return 0;
}

MEMREF *LAcism::acism_refsplit(char *text, char sep, int *pcount)
{
    char *cp = NULL;
    char *ps = NULL;
    int32_t i, nstrs = 0;
    MEMREF *strv = NULL;

    if (*text)
    {
        for (cp = text, nstrs = 1; (cp = strchr(cp, sep)); ++cp) ++nstrs;

        strv = (MEMREF *)new char[nstrs * sizeof(MEMREF)];

        for (i = 0, cp = text; (ps = strchr(cp, sep)); ++i, ++cp)
        {
            strv[i].ptr = cp;
            strv[i].len = ps - strv[i].ptr;

            cp  = ps;
            *cp = 0;
        }

        /* handle last pattern,if text not end with \n */
        strv[i].ptr = cp;
        strv[i].len = strlen(strv[i].ptr);
    }

    if (pcount) *pcount = nstrs;
    return strv;
}
