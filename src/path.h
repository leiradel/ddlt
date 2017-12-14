#ifndef PATH_H
#define PATH_H

#include <lua.h>

int l_realpath(lua_State* L);
int l_split(lua_State* L);
int l_join(lua_State* L);
int l_scandir(lua_State* L);
int l_stat(lua_State* L);

#endif /* PATH_H */
