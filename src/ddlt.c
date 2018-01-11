#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "lexer.h"
#include "templ.h"
#include "path.h"
#include "boot_lua.h"

typedef struct
{
  const char* code;
  size_t length;
}
udata_t;

static const char* reader(lua_State* L, void* data, size_t* size)
{
  udata_t* udata = (udata_t*)data;
  *size = udata->length;
  udata->length = 0;
  return udata->code;
}

LUAMOD_API int luaopen_ddlt(lua_State* L)
{
  static const luaL_Reg functions[] =
  {
    {"newLexer",    l_newLexer},
    {"newTemplate", l_newTemplate},
    {"realpath",    l_realpath},
    {"split",       l_split},
    {"join",        l_join},
    {"scandir",     l_scandir},
    {"stat",        l_stat},
    {NULL,          NULL}
  };

  udata_t udata;
  udata.code = boot_lua;
  udata.length = boot_lua_len;

  if (lua_load(L, reader, (void*)&udata, "boot.lua", "t") != LUA_OK)
  {
    return lua_error(L);
  }

  lua_call(L, 0, 1);
  luaL_newlib(L, functions);
  lua_call(L, 1, 1);
  return 1;
}
