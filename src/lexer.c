#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

#define MAX_BLOCKS 16
#define DELIM_SIZE 16

typedef struct lexer_t lexer_t;

typedef int (*next_t)(lua_State*, lexer_t*);

typedef enum
{
  LINE_COMMENT,
  BLOCK_COMMENT,
  FREE_FORM,
  LINE_DIRECTIVE,
  BLOCK_DIRECTIVE
}
blocktype_t;

typedef struct
{
  char filler;
}
line_comment_t;

typedef struct
{
  char end[DELIM_SIZE];
}
block_comment_t;

typedef struct
{
  char end[DELIM_SIZE];
}
free_form_t;

typedef struct
{
  int at_start;
}
line_directive_t;

typedef struct
{
  char end[DELIM_SIZE];
}
block_directive_t;

typedef struct
{
  blocktype_t type;
  char        begin[DELIM_SIZE];

  union
  {
    line_comment_t    line_comment;
    block_comment_t   block_comment;
    free_form_t       free_form;
    line_directive_t  line_directive;
    block_directive_t block_directive;
  };
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
  block_t     blocks[MAX_BLOCKS];
  unsigned    num_blocks;
};

#define SPACE    " \f\r\t\v" /* \n is treated separately to keep track of the line number */
#define ALPHA    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_"
#define DIGIT    "0123456789"
#define ALNUM    ALPHA DIGIT
#define XDIGIT   DIGIT "ABCDEFabcdef"
#define ODIGIT   "01234567"
#define BDIGIT   "01"
#define NOTDELIM " ()\\\t\v\f\n"

#define PUSH(L, self, token, lexeme, length) push(L, self, token, sizeof(token) - 1, lexeme, length)

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

static int line_comment(lua_State* L, lexer_t* self, const block_t* block, int is_directive)
{
  const char* lexeme;
  const char* newline;

  (void)line_comment;

  if (is_directive)
  {
    if (block->line_directive.at_start && self->source != self->line_start)
    {
      return error(L, self, "directives must start at the beginning of the line");
    }

    if (strspn(self->line_start, SPACE) != (self->source - self->line_start))
    {
      return error(L, self, "directives must be the only thing in a line");
    }
  }

  lexeme = is_directive ? self->line_start : self->source;
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

  if (is_directive)
  {
    return PUSH(L, self, "<linedirective>", lexeme, self->source - lexeme);
  }
  else
  {
    return PUSH(L, self, "<linecomment>", lexeme, self->source - lexeme);
  }
}

static int block_comment(lua_State* L, lexer_t* self, const block_t* block, int is_directive)
{
  const char* lexeme;
  const char* end;
  char reject[3];
  size_t end_len;

  lexeme = self->source;
  end = is_directive ? block->block_directive.end : block->block_comment.end;

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

        if (is_directive)
        {
          return PUSH(L, self, "<blockdirective>", lexeme, self->source - lexeme);
        }
        else
        {
          return PUSH(L, self, "<blockcomment>", lexeme, self->source - lexeme);
        }
      }
    }
    else
    {
      return error(L, self, "unterminated comment");
    }
  }
}

static int free_form(lua_State* L, lexer_t* self, const block_t* block)
{
  const char* lexeme;
  const char* begin;
  const char* end;
  char reject[4];
  size_t begin_len, end_len;
  unsigned nested = 0;

  lexeme = self->source;
  begin = block->begin;
  end = block->free_form.end;

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
          return PUSH(L, self, "<freeform>", lexeme, self->source - lexeme);
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
  block_t* block;
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
  block = self->blocks;

  for (i = 0; i < self->num_blocks; i++, block++)
  {
    begin = block->begin;

    if (!strncmp(self->source, begin, strlen(begin)))
    {
      switch (block->type)
      {
      case LINE_COMMENT:    return line_comment(L, self, block, 0);
      case BLOCK_COMMENT:   return block_comment(L, self, block, 0);
      case FREE_FORM:       return free_form(L, self, block);
      case LINE_DIRECTIVE:  return line_comment(L, self, block, 1);
      case BLOCK_DIRECTIVE: return block_comment(L, self, block, 1);
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
  size_t length;

  lua_getfield(L, 1, "freeform");

  if (!lua_istable(L, -1))
  {
    lua_pop(L, 1);
    return 0;
  }

  for (int i = 1;; i++)
  {
    lua_rawgeti(L, -1, i);

    if (lua_isnil(L, -1))
    {
      lua_pop(L, 2);

      if (i == 1)
      {
        return luaL_error(L, "freeform array is empty");
      }

      return 0;
    }

    if (!lua_istable(L, -1))
    {
      lua_pop(L, 2);
      return luaL_error(L, "freeform array element %d is not a table", i);
    }

    lua_rawgeti(L, -1, 1);
    begin = lua_tolstring(L, -1, &length);

    if (begin == NULL)
    {
      return luaL_error(L, "freeform begin symbol must be a string");
    }
    else if (length >= DELIM_SIZE)
    {
      return luaL_error(L, "freeform begin symbol is too big, maximum length is %d", DELIM_SIZE - 1);
    }

    lua_rawgeti(L, -2, 2);
    end = lua_tolstring(L, -1, &length);

    if (end == NULL)
    {
      return luaL_error(L, "freeform end symbol must be a string");
    }
    else if (length >= DELIM_SIZE)
    {
      return luaL_error(L, "freeform end symbol is too big, maximum length is %d", DELIM_SIZE - 1);
    }

    if (self->num_blocks == MAX_BLOCKS)
    {
      return luaL_error(L, "freeform area exhausted");
    }

    self->blocks[self->num_blocks].type = FREE_FORM;
    strcpy(self->blocks[self->num_blocks].begin, begin);
    strcpy(self->blocks[self->num_blocks].free_form.end, end);
    self->num_blocks++;

    lua_pop(L, 3);
  }
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
