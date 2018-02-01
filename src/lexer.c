#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

typedef struct lexer_t lexer_t;

typedef int (*next_t)(lua_State*, lexer_t*);

typedef enum
{
  LINE_COMMENT,
  BLOCK_COMMENT,
  FREE_FORMAT,
  DIRECTIVE
}
blocktype_t;

typedef struct
{
  const char* begin;
  const char* end;
  int         at_start;
  blocktype_t type;
}
block_t;

struct lexer_t
{
  const char* source_name;
  unsigned    line;
  const char* source;
  const char* line_start;
  unsigned    la_line;

  int         source_ref;
  int         source_name_ref;
  int         symbols_ref;

  char*       symbol_chars;

  next_t      next;
  block_t     blocks[8];
  unsigned    num_blocks;

  char        freeform_begin[16];
  char        freeform_end[16];
};

#define SPACE    " \f\r\t\v" /* \n is treated separately to keep track of the line number */
#define ALPHA    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_"
#define DIGIT    "0123456789"
#define ALNUM    ALPHA DIGIT
#define XDIGIT   DIGIT "ABCDEFabcdef"
#define ODIGIT   "01234567"
#define BDIGIT   "01"
#define NOTDELIM " ()\\\t\v\f\n"

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

  lua_pushinteger(L, self->la_line);
  lua_setfield(L, 2, "line");

  lua_pushvalue(L, 2);
  return 1;
}

static int line_comment(lua_State* L, lexer_t* self, int spaces_important, const char* token)
{
  const char* lexeme;
  const char* newline;

  lexeme = spaces_important ? self->line_start : self->source;
  newline = strchr(lexeme, '\n');

  if (newline != NULL)
  {
    self->source = newline + 1;
    self->line_start = self->source;
    self->line++;
  }
  else
  {
    self->source += strlen(self->source);
  }

  return push(L, self, token, strlen(token), lexeme, self->source - lexeme);
}

static int block_comment(lua_State* L, lexer_t* self, const char* end, const char* token)
{
  const char* lexeme;
  char reject[3];
  size_t end_len;

  lexeme = self->source;

  reject[0] = '\n';
  reject[1] = *end;
  reject[2] = 0;

  end_len = strlen(end) - 1;

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
      self->source++;

      if (!strncmp(self->source, end + 1, end_len))
      {
        self->source += end_len;
        return push(L, self, token, strlen(token), lexeme, self->source - lexeme);
      }
    }
    else
    {
      return error(L, self, "unterminated comment");
    }
  }
}

static int directive(lua_State* L, lexer_t* self, const char* end, int at_start)
{
  if (at_start && self->source != self->line_start)
  {
    return error(L, self, "directives must start at the beginning of the line");
  }

  if (end == NULL)
  {
    return line_comment(L, self, 1, "<directive>");
  }
  else
  {
    return block_comment(L, self, end, "<directive>");
  }
}

static int free_form(lua_State* L, lexer_t* self, const char* begin, const char* end)
{
  const char* lexeme;
  char reject[4];
  size_t begin_len, end_len;
  unsigned nested = 0;

  lexeme = self->source;

  reject[0] = '\n';
  reject[1] = *begin;
  reject[2] = *end;
  reject[3] = 0;

  begin_len = strlen(begin) - 1;
  end_len = strlen(end) - 1;

  self->source += begin_len + 1;

  for (;;)
  {
    self->source += strcspn(self->source, reject);

    if (*self->source == '\n')
    {
      self->source++;
      self->line++;
    }
    else if (*self->source == *begin)
    {
      self->source++;

      if (!strncmp(self->source, begin + 1, begin_len))
      {
        self->source += begin_len;
        nested++;
      }
    }
    else if (*self->source == *end)
    {
      self->source++;

      if (!strncmp(self->source, end + 1, end_len))
      {
        self->source += end_len;

        if (nested == 0)
        {
          return push(L, self, "<freeform>", 10, lexeme, self->source - lexeme);
        }

        nested--;
      }
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
  int i;
  const char* begin;
  size_t length;

  self = luaL_checkudata(L, 1, "lexer");
  luaL_checktype(L, 2, LUA_TTABLE);

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
      self->line_start = self->source;
    }
    else
    {
      break;
    }
  }

  self->la_line = self->line;

  for (i = 0; i < self->num_blocks; i++)
  {
    begin = self->blocks[i].begin;

    if (!strncmp(self->source, begin, strlen(begin)))
    {
      switch (self->blocks[i].type)
      {
      case LINE_COMMENT:  return line_comment(L, self, 0, "<linecomment>");
      case BLOCK_COMMENT: return block_comment(L, self, self->blocks[i].end, "<blockcomment>");
      case FREE_FORMAT:   return free_form(L, self, begin, self->blocks[i].end);

      case DIRECTIVE:
        printf("=== at=%d src=%p str=%p end=%s spn=%zu\n",
          self->blocks[i].at_start,
          self->source,
          self->line_start,
          self->blocks[i].end == NULL ? "--" : self->blocks[i].end,
          strspn(self->line_start, SPACE)
        );

        if (self->blocks[i].at_start && self->source != self->line_start)
        {
          /* if the directive must be at the beginning of the line but isn't, it's not a directive */
          break;
        }

        if (self->blocks[i].end == NULL && strspn(self->line_start, SPACE) != (self->source - self->line_start))
        {
          /* if the directive ends at the new line but the directive has extraneous characters before it,
             it's not a directive */
          break;
        }

        return directive(L, self, self->blocks[i].end, self->blocks[i].at_start);
      }
    }
  }

  begin = self->source;
  length = strspn(self->source, self->symbol_chars);
  lua_rawgeti(L, LUA_REGISTRYINDEX, self->symbols_ref);

  while (length > 0)
  {
    lua_pushlstring(L, begin, length);
    lua_rawget(L, -2);
    i = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (i)
    {
      lua_pop(L, 1);
      self->source += length;
      return push(L, self, begin, length, begin, length);
    }

    length--;
  }

  lua_pop(L, 1);
  return self->next(L, self);
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
  free(self->symbol_chars);
  return 0;
}

#include "lexer_cpp.c"
#include "lexer_bas.c"
#include "lexer_pas.c"

static int init_source(lua_State* L, lexer_t* self)
{
  lua_getfield(L, 1, "source");

  if (!lua_isstring(L, -1))
  {
    return luaL_error(L, "source must be a string");
  }

  self->source = lua_tostring(L, -1);
  self->source_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  self->line_start = self->source;
  return 0;
}

static int init_file(lua_State* L, lexer_t* self)
{
  lua_getfield(L, 1, "file");

  if (!lua_isstring(L, -1))
  {
    return luaL_error(L, "file name must be a string");
  }

  self->source_name = lua_tostring(L, -1);
  self->source_name_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  return 0;
}

static int init_symbol_chars(lua_State* L, lexer_t* self)
{
  char used[256 - 32 + 1];
  char* aux;
  const char* symbol;
  int i;

  lua_getfield(L, 1, "symbols");

  if (!lua_istable(L, -1))
  {
    return luaL_error(L, "symbols must be a table");
  }

  memset(used, 0, sizeof(used));
  lua_pushvalue(L, -1);
  self->symbols_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushnil(L);

  while (lua_next(L, -2) != 0)
  {
    if (lua_isstring(L, -2))
    {
      symbol = lua_tostring(L, -2);

      while (*symbol != 0)
      {
        i = (unsigned char)*symbol++;

        if (i >= 32 && i <= 255)
        {
          used[i - 32] = 1;
        }
      }
    }

    lua_pop(L, 1);
  }

  lua_pop(L, 1);

  for (i = 0, aux = used; i < 256 - 32; i++)
  {
    if (used[i] != 0)
    {
      *aux++ = i + 32;
    }
  }

  *aux = 0;
  self->symbol_chars = strdup(used);

  if (self->symbol_chars == NULL)
  {
    return luaL_error(L, "out of memory");
  }

  return 0;
}

static int init_language(lua_State* L, lexer_t* self)
{
  const char* language;

  lua_getfield(L, 1, "language");

  if (!lua_isstring(L, -1))
  {
    return luaL_error(L, "language name must be a string");
  }

  language = lua_tostring(L, -1);

  if (!strcmp(language, "cpp"))
  {
    cpp_setup_lexer(self);
  }
  else if (!strcmp(language, "bas"))
  {
    bas_setup_lexer(self);
  }
  else if (!strcmp(language, "pas"))
  {
    pas_setup_lexer(self);
  }
  else
  {
    return luaL_error(L, "invalid language %s", language);
  }

  lua_pop(L, 1);
  return 0;
}

static int init_freeform(lua_State* L, lexer_t* self)
{
  const char* begin;
  const char* end;

  lua_getfield(L, 1, "freeform");

  if (!lua_istable(L, -1))
  {
    lua_pop(L, 1);
    return 0;
  }

  lua_rawgeti(L, -1, 1);
  begin = lua_tostring(L, -1);

  if (begin == NULL)
  {
    return luaL_error(L, "freeform begin symbol must be a string");
  }
  else if (strlen(begin) >= sizeof(self->freeform_begin))
  {
    return luaL_error(L, "freeform begin symbol is too big, maximum length is %d", sizeof(self->freeform_begin) - 1);
  }

  lua_rawgeti(L, -2, 2);
  end = lua_tostring(L, -1);

  if (end == NULL)
  {
    return luaL_error(L, "freeform end symbol must be a string");
  }
  else if (strlen(end) >= sizeof(self->freeform_end))
  {
    return luaL_error(L, "freeform end symbol is too big, maximum length is %d", sizeof(self->freeform_end) - 1);
  }

  strcpy(self->freeform_begin, begin);
  strcpy(self->freeform_end, end);

  self->blocks[self->num_blocks].begin = self->freeform_begin;
  self->blocks[self->num_blocks].end = self->freeform_end;
  self->blocks[self->num_blocks].type = FREE_FORMAT;
  self->num_blocks++;

  lua_pop(L, 3);
  return 0;
}

int l_newLexer(lua_State* L)
{
  lexer_t* self;

  if (!lua_istable(L, 1))
  {
    return luaL_error(L, "lexer options must be a table");
  }

  lua_settop(L, 1);

  self = (lexer_t*)lua_newuserdata(L, sizeof(lexer_t));
  self->line = 1;
  self->source_name_ref = LUA_NOREF;
  self->source_ref = LUA_NOREF;
  self->symbols_ref = LUA_NOREF;

  init_source(L, self);
  init_file(L, self);
  init_symbol_chars(L, self);
  init_language(L, self);
  init_freeform(L, self);

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
