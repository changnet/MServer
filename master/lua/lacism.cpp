#ifdef __cplusplus
extern "C" {
#endif

#include <msutil.h>

#ifdef __cplusplus
}
#endif
#include "lacism.h"

lacism::lacism( lua_State *L )
    : L(L)
{
    _pattv = NULL;
    _psp   = NULL;
}

lacism::~lacism()
{
    if ( _pattv )
    {
        free(_pattv);
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
    if ( !_pattv || !_psp )
    {
        return luaL_error( L,"no pattern load yet" );
    }
    size_t len = 0;
    const char *str = luaL_checklstring( L,1,&len );

    MEMREF text   = {str, len};

    int32 textpos = acism_scan( _psp, text, (ACISM_ACTION*)lacism::on_match,
            _pattv);

    lua_pushinteger( L,textpos );
    return 1;
}

int32 lacism::load_from_file()
{
    const char *path = luaL_checkstring( L,1 );

    MEMBUF patt = chomp( slurp(path) );
    if (!patt.ptr)
    {
        return luaL_error( L,"can't load from file[%s]",path );
    }

    int npatts;
    _pattv = refsplit(patt.ptr, '\n', &npatts);
    _psp = acism_create(_pattv, npatts);

    lua_pushinteger( L,npatts );
    return 1;
}
