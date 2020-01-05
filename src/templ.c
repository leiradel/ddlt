#include <string.h>

#include <lua.h>
#include <lauxlib.h>

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

int l_newTemplate(lua_State* L)
{
  size_t length;
  const char* source;
  const char* current;
  const char* end;
  size_t open_length;
  const char* open_tag;
  size_t close_length;
  const char* close_tag;
  const char* chunkname;
  luaL_Buffer code;
  const char* start;
  const char* finish;
  udata_t udata;

  source = luaL_checklstring(L, 1, &length);
  current = source;
  end = current + length;

  open_tag = luaL_checklstring(L, 2, &open_length);
  close_tag = luaL_checklstring(L, 3, &close_length);
  chunkname = luaL_optstring(L, 4, "template");
  
  luaL_buffinit(L, &code);
  luaL_addstring(&code, "return function(args, emit); ");
  
  for (;;)
  {
    start = current;
    
    for (;;)
    {
      start = strstr(start, open_tag);

      if (start == NULL || start[open_length] == '=' || start[open_length] == '!')
      {
        break;
      }

      start++;
    }
    
    if (start == NULL)
    {
      luaL_addstring(&code, "emit[===[");
      luaL_addlstring(&code, current, end - current);
      luaL_addstring(&code, "]===]");
      break;
    }
    
    finish = strstr(start + open_length + 1, close_tag);
    
    if (finish == NULL)
    {
      finish = end;
    }
    
    if (*current == '\n' || *current == '\r')
    {
      luaL_addstring(&code, "emit(\'\\n\') emit[===[");
    }
    else
    {
      luaL_addstring(&code, "emit[===[");
    }
    
    luaL_addlstring(&code, current, start - current);
    luaL_addstring(&code, "]===] ");
    
    if (start[open_length] == '=')
    {
      start += open_length + 1;
      luaL_addstring(&code, "emit(tostring(");
      luaL_addlstring(&code, start, finish - start);
      luaL_addstring(&code, ")) ");
    }
    else
    {
      start += open_length + 1;
      luaL_addlstring(&code, start, finish - start);
      luaL_addchar(&code, ' ');
    }
    
    current = finish + close_length;
  }
  
  luaL_addstring(&code, "end\n");
  luaL_pushresult(&code);

  udata.code = lua_tolstring(L, -1, &udata.length);

  if (lua_load(L, reader, (void*)&udata, chunkname, "t") != LUA_OK)
  {
    lua_pushnil(L);
    lua_pushvalue(L, -2);
    return 2;
  }

  if (lua_pcall(L, 0, 1, 0) != LUA_OK)
  {
    lua_pushnil(L);
    lua_pushvalue(L, -2);
    return 2;
  }

  lua_pushvalue(L, -2);
  return 2;
}
