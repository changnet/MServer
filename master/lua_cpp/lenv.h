#ifndef __LENV_H__
#define __LENV_H__

#include <hpp>
#include "../global/global.h"
#include "lclass.h"

/* lua环境初始化头文件 */

extern int luaopen_util (lua_State *L);
extern int luaopen_ev (lua_State *L);

void lua_initenv( lua_State *L );

#endif /* __LENV_H__
