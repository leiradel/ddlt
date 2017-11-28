#ifndef PATH_H
#define PATH_H

#include <lua.h>

int realpath_lua(lua_State* L);
int split_lua(lua_State* L);
int scandir_lua(lua_State* L);
int stat_lua(lua_State* L);

#endif /* PATH_H */
