#include <string.h>

#include <lua.h>
#include <lauxlib.h>

int newTemplate_lua(lua_State* L)
{
  size_t      length;
  const char* source = luaL_checklstring(L, 1, &length);
  
  const char* current = source;
  const char* end   = current + length;
  
  luaL_Buffer code;
  luaL_buffinit(L, &code);

  luaL_addstring(&code, "return function(defs, emit); ");
  
  for (;;)
  {
    const char* start = strstr(current, "/*");
    
    if (start == NULL)
    {
      luaL_addstring(&code, "emit[===[");
      luaL_addlstring(&code, current, end - current);
      luaL_addstring(&code, "]===]");
      break;
    }
    
    const char* finish = strstr(start + 2, "*/");
    
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
    
    switch (start[2])
    {
    case '=':
      luaL_addstring(&code, "emit(tostring(");
      luaL_addlstring(&code, start + 3, finish - 1 - (start + 3) + 1);
      luaL_addstring(&code, ")) ");
      break;
      
    case '!':
      luaL_addlstring(&code, start + 3, finish - 1 - (start + 3) + 1);
      luaL_addchar(&code, ' ');
      break;

    default:
      luaL_addstring(&code, "emit[===[");
      luaL_addlstring(&code, start, finish + 2 - start);
      luaL_addstring(&code, "]===] ");
      break;
    }
    
    current = finish + 2;
  }
  
  luaL_addstring(&code, "end\n");
  luaL_pushresult(&code);
  return 1;
}
