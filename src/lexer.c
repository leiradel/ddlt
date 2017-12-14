#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <lua.h>
#include <lauxlib.h>

typedef struct lexer_t lexer_t;

typedef int (*next_t)(lua_State*, lexer_t*);

typedef enum
{
  LINE_COMMENT,
  BLOCK_COMMENT,
  FREE_FORMAT
}
blocktype_t;

typedef struct
{
  const char* begin;
  const char* end;
  blocktype_t type;
}
block_t;

struct lexer_t
{
  const char* source_name;
  unsigned    line;
  const char* source;

  int         source_ref;
  int         source_name_ref;
  int         symbols_ref;

  next_t      next;
  block_t     blocks[8];
  unsigned    num_blocks;
};

#define SPACE  " \f\r\t\v"
#define ALPHA  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_"
#define DIGIT  "0123456789"
#define ALNUM  ALPHA DIGIT
#define XDIGIT DIGIT "ABCDEFabcdef"
#define ODIGIT "01234567"
#define BDIGIT "01"

static int error(lua_State* L, const lexer_t* self, const char* format, ...)
{
  va_list args;
  char buffer[1024];
  int written;
  
  written = snprintf(buffer, sizeof(buffer), "%s:%d: ", self->source_name, self->line);

  va_start(args, format);
  vsnprintf(buffer + written, sizeof(buffer) - written, format, args);
  va_end(args);

  lua_pushnil(L);
  lua_pushstring(L, buffer);
  return 2;
}

static int push(lua_State* L, const lexer_t* self, const char* token, size_t token_length, const char* lexeme, size_t lexeme_length)
{
  lua_pushlstring(L, token, token_length);
  lua_setfield(L, 2, "token");

  lua_pushlstring(L, lexeme, lexeme_length);
  lua_setfield(L, 2, "lexeme");

  lua_pushinteger(L, self->line);
  lua_setfield(L, 2, "line");

  lua_pushvalue(L, 2);
  return 1;
}

static int is_symbol(lua_State* L, const lexer_t* self, const char* lexeme, size_t lexeme_length)
{
  int is_symbol;

  lua_rawgeti(L, LUA_REGISTRYINDEX, self->symbols_ref);
  lua_pushlstring(L, lexeme, lexeme_length);
  lua_call(L, 1, 1);
  is_symbol = lua_toboolean(L, -1);
  lua_pop(L, 1);

  return is_symbol;
}

static int line_comment(lua_State* L, lexer_t* self)
{
  const char* newline = strchr(self->source, '\n');

  if (newline != NULL)
  {
    self->source = newline + 1;
    self->line++;
    return 0;
  }

  return push(L, self, "<eof>", 5, "<eof>", 5);
}

static int block_comment(lua_State* L, lexer_t* self, const char* end)
{
  char reject[3];
  int i;

  reject[0] = '\n';
  reject[1] = *end;
  reject[2] = 0;

  for (;;)
  {
    self->source += strcspn(self->source, reject);

    if (*self->source == '\n')
    {
      self->source++;
      self->line++;
    }
    else if (*self->source == *end)
    {
      i = 0;

      do
      {
        i++;
      }
      while (end[i] != 0 && self->source[i] == end[i]);

      if (end[i] == 0)
      {
        self->source += i;
        return 0;
      }

      self->source++;
    }
    else
    {
      return error(L, self, "unterminated comment");
    }
  }
}

static int free_form(lua_State* L, lexer_t* self, const char* end)
{
  const char* lexeme;
  char reject[3];
  int i;

  lexeme = self->source;

  reject[0] = '\n';
  reject[1] = *end;
  reject[2] = 0;

  for (;;)
  {
    self->source += strcspn(self->source, reject);

    if (*self->source == '\n')
    {
      self->source++;
      self->line++;
    }
    else if (*self->source == *end)
    {
      i = 0;

      do
      {
        i++;
      }
      while (end[i] != 0 && self->source[i] == end[i]);

      if (end[i] == 0)
      {
        self->source += i;
        return push(L, self, "<freeform>", 10, lexeme, self->source - lexeme);
      }

      self->source++;
    }
    else
    {
      return error(L, self, "unterminated free-form block");
    }
  }
}

static int l_next(lua_State* L)
{
  lexer_t* self;
  unsigned i;
  const char* begin;

  self = luaL_checkudata(L, 1, "lexer");
  luaL_checktype(L, 2, LUA_TTABLE);

again:
  for (;;)
  {
    self->source += strspn(self->source, SPACE);

    if (*self->source == 0)
    {
      return push(L, self, "<eof>", 5, "<eof>", 5);
    }
    else if (*self->source == '\n')
    {
      self->source++;
      self->line++;
    }
    else
    {
      break;
    }
  }

  for (i = 0; i < self->num_blocks; i++)
  {
    begin = self->blocks[i].begin;

    if (!strncmp(self->source, begin, strlen(begin)))
    {
      switch (self->blocks[i].type)
      {
      case LINE_COMMENT:
        i = line_comment(L, self);
        break;

      case BLOCK_COMMENT:
        i = block_comment(L, self, self->blocks[i].end);
        break;

      case FREE_FORMAT:
        i = free_form(L, self, self->blocks[i].end);
        break;
      }

      if (i != 0)
      {
        return i;
      }

      goto again;
    }
  }

  i = 0;

  while (self->source[i] != 0 && is_symbol(L, self, self->source, i + 1))
  {
    i++;
  }

  if (i > 0)
  {
    begin = self->source;
    self->source += i;
    return push(L, self, begin, i, begin, i);
  }

  i = self->next(L, self);

  if (i != 0)
  {
    return i;
  }

  goto again;
}

static int l_index(lua_State* L)
{
  size_t length;
  const char* key;
  
  key = luaL_checklstring(L, 2, &length);

  if (length == 4 && !strcmp(key, "next"))
  {
    lua_pushcfunction(L, l_next);
    return 1;
  }

  return luaL_error(L, "unknown lexer method %s", key );
}

static int l_gc(lua_State* L)
{
  lexer_t* self = (lexer_t*)lua_touserdata(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, self->source_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, self->source_name_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, self->symbols_ref);
  return 0;
}

#include "lexer_cpp.c"
#include "lexer_bas.c"

int l_newLexer(lua_State* L)
{
  lexer_t* self;
  size_t length;
  const char* language;

  if (lua_type(L, 1) != LUA_TTABLE)
  {
    return luaL_error(L, "lexer options must be a table");
  }

  lua_settop(L, 1);

  lua_getfield(L, 1, "source");

  if (lua_type(L, -1) != LUA_TSTRING)
  {
    return luaL_error(L, "source must be a string");
  }

  lua_getfield(L, 1, "file");

  if (lua_type(L, -1) != LUA_TSTRING)
  {
    return luaL_error(L, "file name must be a string");
  }

  lua_getfield(L, 1, "isSymbol");

  if (lua_type(L, -1) != LUA_TFUNCTION)
  {
    return luaL_error(L, "isSymbol must be a function");
  }

  lua_getfield(L, 1, "language");

  if (lua_type(L, -1) != LUA_TSTRING)
  {
    return luaL_error(L, "language name must be a string");
  }

  self = (lexer_t*)lua_newuserdata(L, sizeof(lexer_t));

  self->source_name = lua_tostring(L, 3);
  self->line        = 1;
  self->source      = lua_tostring(L, 2);

  language = lua_tolstring(L, 5, &length);

  if (length == 3 && !strcmp(language, "cpp"))
  {
    cpp_setup_lexer(self);
  }
  else if (length == 3 && !strcmp(language, "bas"))
  {
    bas_setup_lexer(self);
  }
  else
  {
    return luaL_error(L, "invalid language: %s", language);
  }

  lua_pushvalue(L, 2);
  self->source_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushvalue(L, 3);
  self->source_name_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushvalue(L, 4);
  self->symbols_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  if (luaL_newmetatable(L, "lexer") != 0)
  {
    lua_pushcfunction(L, l_index);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_gc);
    lua_setfield(L, -2, "__gc");
  }

  lua_setmetatable(L, -2);
  return 1;
}
