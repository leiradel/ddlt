#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>

#ifdef WIN32
char __cdecl* realpath(const char* __restrict__ name, char* __restrict__ resolved);
#define PATH_SEPARATOR '\\'
#else
#define _MAX_PATH PATH_MAX
#define PATH_SEPARATOR '/'
#endif

int l_realpath(lua_State* L)
{
  const char* path;
  char buffer[_MAX_PATH];
  char* resolved;

  path = luaL_checkstring(L, 1);
  resolved = realpath(path, buffer);
  
  if (resolved != NULL)
  {
    while (*resolved != 0)
    {
      if (*resolved == '\\' )
      {
        *resolved = '/';
      }
      
      resolved++;
    }
    
    lua_pushstring(L, buffer);
    return 1;
  }
  
  lua_pushnil(L);
  lua_pushstring(L, strerror(errno));
  return 2;
}

int l_split(lua_State* L)
{
  size_t length;
  const char* path;
  const char* ext;
  const char* name;

  path = luaL_checklstring(L, 1, &length);
  ext = path + length;
  
  while (ext > path && *ext != '.' && *ext != '/' && *ext != '\\')
  {
    ext--;
  }
  
  name = ext;
  
  if (*ext != '.')
  {
    ext = NULL;
  }
  
  while (name > path && *name != '/' && *name != '\\')
  {
    name--;
  }

  if (*name == '/' || *name == '\\')
  {
    name++;
  }

  if (name - path - 1 > 0)
  {
    lua_pushlstring(L, path, name - path - 1);
  }
  else
  {
    lua_pushnil(L);
  }
  
  if (ext != NULL)
  {
    lua_pushlstring(L, name, ext - name);
    lua_pushstring(L, ext + 1);
  }
  else
  {
    lua_pushstring(L, name);
    lua_pushnil(L);
  }
  
  return 3;
}

int l_join(lua_State* L)
{
  size_t path_length, name_length, ext_length;
  const char* path;
  const char* name;
  const char* ext;
  luaL_Buffer join;

  path = lua_tolstring(L, 1, &path_length);
  name = lua_tolstring(L, 2, &name_length);
  ext  = lua_tolstring(L, 3, &ext_length);

  luaL_buffinit(L, &join);

  if (path != NULL)
  {
    luaL_addlstring(&join, path, path_length);
    luaL_addchar(&join, PATH_SEPARATOR);
  }

  if (name != NULL)
  {
    luaL_addlstring(&join, name, name_length);
  }

  if (ext != NULL)
  {
    luaL_addchar(&join, '.');
    luaL_addlstring(&join, ext, ext_length);
  }

  luaL_pushresult(&join);
  return 1;
}

int l_scandir(lua_State* L)
{
  const char* name;
  DIR* dir;
  struct dirent* entry;
  int ndx;

  name = luaL_checkstring(L, 1);
  dir = opendir( name );

  if (dir != NULL)
  {
    ndx = 1;
    
    lua_createtable(L, 0, 0);
    
    while ((entry = readdir(dir)) != NULL)
    {
      lua_pushfstring(L, "%s/%s", name, entry->d_name);
      lua_rawseti(L, -2, ndx++);
    }
    
    closedir(dir);
    return 1;
  }
  
  lua_pushnil(L);
  lua_pushstring(L, strerror(errno));
  return 2;
}

static void pushtime(lua_State* L, time_t* time)
{
  struct tm* tm;
  char buf[256];

  tm = gmtime(time);

  sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  lua_pushstring(L, buf);
}

int l_stat(lua_State* L)
{
  static const struct { unsigned flag; const char* name; } modes[] =
  {
#ifdef S_IFSOCK 
    { S_IFSOCK, "sock" },
#endif
#ifdef S_IFLNK 
    { S_IFLNK,  "link" },
#endif
    { S_IFREG,  "file" },
    { S_IFBLK,  "block" },
    { S_IFDIR,  "dir" },
    { S_IFCHR,  "char" },
    { S_IFIFO,  "fifo" },
  };
  
  const char* name;
  struct stat buf;
  int i;

  name = luaL_checkstring(L, 1);
  
  if (stat(name, &buf) == 0)
  {
    if (lua_istable(L, 1))
    {
      lua_pushvalue(L, 1);
    }
    else
    {
      lua_createtable(L, 0, 4 + sizeof(modes) / sizeof(modes[0]));
    }
    
    lua_pushinteger(L, buf.st_size);
    lua_setfield(L, -2, "size");
    
    pushtime(L, &buf.st_atime);
    lua_setfield(L, -2, "atime");
    
    pushtime(L, &buf.st_mtime);
    lua_setfield(L, -2, "mtime");
    
    pushtime(L, &buf.st_ctime);
    lua_setfield(L, -2, "ctime");
    
    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
      lua_pushboolean(L, (buf.st_mode & S_IFMT) == modes[i].flag);
      lua_setfield(L, -2, modes[i].name);
    }
    
    return 1;
  }
  
  lua_pushnil(L);
  lua_pushstring(L, strerror(errno));
  return 2;
}
