#include <fcntl.h>    // open O_RDONLY
#include <sys/stat.h>

#include "lacism.h"

lacism::lacism( lua_State *L )
    : L(L)
{
    _psp   = NULL;
    _pattv = NULL;
    _loaded  = 0;
    _case_sensitive = 1;

    _patt.ptr = NULL;
    _patt.len = 0;
}

lacism::~lacism()
{
    if ( _patt.ptr )
    {
        delete []_patt.ptr;
        _patt.ptr = NULL;
        _patt.len = 0;
    }

    if ( _pattv )
    {
        delete []_pattv;
        _pattv = NULL;
    }

    if ( _psp )
    {
        acism_destroy(_psp);
        _psp = NULL;
    }
}

/* 返回0,继续搜索
 * 返回非0,终止搜索，并且acism_more返回该值
 */
int32 lacism::on_match( int32 strnum, int32 textpos, MEMREF const *pattv )
{
    (void)strnum, (void)pattv;

    return textpos;
}

/* 扫描关键字,扫描到其中一个即中止 */
int32 lacism::scan()
{
    if ( !_loaded )
    {
        return luaL_error( L,"no pattern load yet" );
    }

    /* no worlds loaded */
    if ( !_pattv || !_psp )
    {
        lua_pushinteger( L,0 );
        return 1;
    }

    size_t len = 0;
    const char *str = luaL_checklstring( L,1,&len );

    MEMREF text   = {str, len};

    int32 textpos = acism_scan( _psp, text,
        (ACISM_ACTION*)lacism::on_match, _pattv,_case_sensitive );

    lua_pushinteger( L,textpos );
    return 1;
}

int32 lacism::load_from_file()
{
    const char *path = luaL_checkstring( L,1 );
    int32 case_sensitive = 1;

    if ( lua_isboolean( L,2 ) )
    {
        case_sensitive = lua_toboolean( L,2 );
    }

    if ( this->acism_slurp( path ) )
    {
        return luaL_error( L,"can't read file[%s]:",path,strerror(errno) );
    }

    _loaded = 1;/* mark file loaded */
    if ( !case_sensitive )
    {
        for ( size_t i = 0;i < _patt.len;i ++ )
        {
            *(_patt.ptr + i) = ::tolower( *(_patt.ptr + i) );
        }
    }

    int32 npatts = 0;
    if ( _patt.ptr )
    {
        _pattv = this->acism_refsplit( _patt.ptr, '\n', &npatts );
        _psp = acism_create( _pattv, npatts );
    }
    _case_sensitive = case_sensitive;

    lua_pushinteger( L,npatts );
    return 1;
}

int32 lacism::acism_slurp( const char *path )
{
    int32 fd = ::open( path, O_RDONLY );
    if ( fd < 0 ) return errno;

    struct stat s;
    if ( fstat(fd, &s) || !S_ISREG(s.st_mode) )
    {
        ::close( fd );
        return errno;
    }

    /* empty file,do nothing */
    if ( s.st_size <= 0 )
    {
        ::close( fd );
        return 0;
    }

    /* In a 32-bit implementation, ACISM can handle about 10MB of pattern text.
     * Aho-Corasick-Interleaved_State_Matrix.pdf
     */

    assert( "filter worlds file too large",s.st_size < 1024*1024*10 );

    assert( "patt.ptr not free",!_patt.ptr ); /* code hot swap ? */

    _patt.ptr = new char[s.st_size+1];
    _patt.len = s.st_size;

    if ( _patt.len != (unsigned)read(fd, _patt.ptr, _patt.len) )
    {
        ::close( fd );
        return errno;
    }

    ::close(fd);
    _patt.ptr[_patt.len] = 0;

    return  0;
}

MEMREF *lacism::acism_refsplit( char *text, char sep, int *pcount )
{
    char *cp = NULL;
    char *ps = NULL;
    int32 i,nstrs = 0;
    MEMREF *strv = NULL;

    if (*text)
    {
        for (cp = text, nstrs = 1; (cp = strchr(cp, sep)); ++cp)
            ++nstrs;

        strv = (MEMREF *)new char[nstrs * sizeof(MEMREF)];

        for (i = 0, cp = text; (ps = strchr(cp, sep)); ++i, ++cp)
        {
            strv[i].ptr = cp;
            strv[i].len = ps - strv[i].ptr;

            cp = ps;
            *cp = 0;
        }

        /* handle last pattern,if text not end with \n */
        strv[i].ptr = cp;
        strv[i].len = strlen(strv[i].ptr);
    }

    if (pcount) *pcount = nstrs;
    return    strv;
}
