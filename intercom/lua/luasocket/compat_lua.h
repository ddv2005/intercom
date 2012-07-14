#if !defined(__COMPAT_LUA_H__)
#define __COMPAT_LUA_H__
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


#if !defined(LUA_VERSION_NUM) || (LUA_VERSION_NUM >= 502)
#define LUA_GLOBALSINDEX    LUA_RIDX_GLOBALS

#define luaL_reg luaL_Reg
#define luaL_putchar(B,c)   luaL_addchar(B,c)
#define lua_open            luaL_newstate
LUALIB_API int (luaL_typerror) (lua_State *L, int narg, const char *tname);
#endif

#endif

