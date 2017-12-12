#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "lexer.h"
#include "templ.h"
#include "path.h"
#include "boot_lua.h"

static int main_lua(lua_State* L)
{
  int i, argc;
  const char** argv;

  argc = lua_tointeger(L, lua_upvalueindex(1));
  argv = (const char**)lua_touserdata(L, lua_upvalueindex(2));
  
  /* Load main.lua */
  if (luaL_loadbufferx(L, boot_lua, boot_lua_len, "boot.lua", "t" ) != LUA_OK)
  {
    return lua_error(L);
  }
  
  /* Run it, it returns the bootstrap main function */
  lua_call(L, 0, 1);
  
  /* Prepare the args table */
  lua_newtable(L);
  
  for (i = 1; i < argc; i++)
  {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i);
  }
  
  /* Call the bootstrap main function with the args table */
  lua_call(L, 1, 1);
  
  /* Returns the integer result */
  return lua_tointeger(L, -1);
}

static int traceback(lua_State* L)
{
  luaL_traceback(L, L, lua_tostring(L, -1), 1);
  return 1;
}

static int open_lib(lua_State* L)
{
  static const luaL_Reg statics[] =
  {
    {"newLexer",    newLexer_lua},
    {"newTemplate", newTemplate_lua},
    {"realpath",    realpath_lua},
    {"split",       split_lua},
    {"join",        join_lua},
    {"scandir",     scandir_lua},
    {"stat",        stat_lua},
    {NULL,          NULL}
  };
  
  luaL_newlib(L, statics);
  return 1;
}

int main(int argc, const char *argv[]) 
{
  lua_State* L;
  int top;

  L = luaL_newstate();
  
  if (L != NULL)
  {
    top = lua_gettop(L);
    luaL_openlibs(L);
    luaL_requiref(L, "ddlt", open_lib, 0);
    lua_settop(L, top);
    
    lua_pushcfunction(L, traceback);
    
    lua_pushnumber(L, argc);
    lua_pushlightuserdata(L, (void*)argv);
    lua_pushcclosure(L, main_lua, 2);
    
    if (lua_pcall(L, 0, 1, -2) != 0)
    {
      fprintf(stderr, "%s\n", lua_tostring(L, -1));
      return 1;
    }
    
    lua_close(L);
    return 0;
  }
  
  fprintf(stderr, "could not create the Lua state\n");
  return 1;
}
