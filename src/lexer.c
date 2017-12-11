#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <lua.h>
#include <lauxlib.h>

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

typedef struct
{
  const char*   source_name;
  unsigned      line;
  const char*   source;
  const char*   end;

  int           last_char;
  int           source_ref;
  int           source_name_ref;
  int           symbols_ref;

  lua_CFunction next;
  block_t       blocks[8];
  unsigned      num_blocks;
}
lexer_t;

#define ISSPACE(k)  (isspace((unsigned char)k))
#define ISALPHA(k)  (k == '_' || isalpha((unsigned char)k))
#define ISALNUM(k)  (k == '_' || isalnum((unsigned char)k))
#define ISDIGIT(k)  (isdigit((unsigned char)k))
#define ISXDIGIT(k) (isxdigit((unsigned char)k))
#define ISODIGIT(k) (k >= '0' && k <= '9')

#define GET(s) ((s)->source < (s)->end ? *(s)->source : -1)

static int skip(lexer_t* self)
{
  self->line += *self->source == '\n';

  ptrdiff_t left = self->end - self->source;

  if (left > 0)
  {
    return *self->source++;
  }

  return -1;
}

static int error(lua_State* L, const lexer_t* self, const char* format, ...)
{
  va_list args;
  char buffer[1024];
  int written = snprintf(buffer, sizeof(buffer), "%s:%d: ", self->source_name, self->line);

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

  lua_pushinteger(L, self->line - (self->last_char == '\n'));
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

static int next(lua_State* L, lexer_t* self)
{
  int k, save_k;
  unsigned i, line;
  const char* j;
  const char* source;

  luaL_checktype(L, 2, LUA_TTABLE);

  k = self->last_char;

  for (;;)
  {
    if (k == -1)
    {
      return push(L, self, "<eof>", 5, "<eof>", 5);
    }
    else if (!ISSPACE(k))
    {
      break;
    }

    k = skip(self);
  }

  for (i = 0; i < self->num_blocks; i++)
  {
    save_k = k;
    line = self->line;
    const char* source = *self->source;
    j = self->blocks[i].begin;

    while (*j != 0 && *j == k)
    {
      j++;
      k = skip(self);
    }

    if (*j == 0)
    {
      switch (self->blocks[i].type)
      {
      case LINE_COMMENT:  return line_comment(L, self);
      case BLOCK_COMMENT: return block_comment(L, self, self->blocks[i].end);
      case FREE_FORMAT:   return free_format(L, self, self->blocks[i].end);
      }
    }

    self->line = line;
    self->source = source;
    k = save_k;
  }
}

static int l_index(lua_State* L)
{
  lexer_t* self;
  size_t length;
  const char* key = luaL_checklstring(L, 2, &length);

  if (length == 4 && !strcmp(key, "next"))
  {
    self = luaL_checkudata(L, 1, "lexer");
    lua_pushcfunction(L, self->next);
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

int newLexer_lua(lua_State* L)
{
  lexer_t* self;
  size_t source_length;
  size_t language_length;
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
  self->source      = lua_tolstring(L, 2, &source_length);
  self->end         = self->source + source_length;
  self->last_char   = skip(self);

  language = lua_tolstring(L, 5, &language_length);

  if (language_length == 3 && !strcmp(language, "cpp"))
  {
    setup_lexer_cpp(self);
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
