
static int util_md5( lua_State *L )
{
    return 0;
}

static const luaL_Reg utillib[] =
{
  {"md5", util_md5},
  {NULL, NULL}
};


int luaopen_util (lua_State *L)
{
  luaL_newlib(L, utillib);
  return 1;
}
