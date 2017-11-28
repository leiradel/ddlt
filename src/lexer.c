#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <lua.h>
#include <lauxlib.h>

typedef struct
{
  const char* source_name;
  unsigned    line;
  const char* source;
  const char* end;
  int         last_char;
  int         source_ref;
  int         source_name_ref;
  int         symbols_ref;
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

static void format_char(char* buffer, size_t size, int k)
{
  if (isprint(k))
  {
    snprintf(buffer, size, "'%c'", k);
  }
  else if (k != -1)
  {
    snprintf(buffer, size, "'\\%c%c%c", (k >> 6) + '0', ((k >> 3) & 7) + '0', (k & 7) + '0');
  }
  else
  {
    strncpy(buffer, "eof", size);
  }
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

  lua_pushnumber(L, self->line - (self->last_char == '\n'));
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

static int get_id(lua_State* L, lexer_t* self, int k)
{
  const char* lexeme = self->source - 1;

  do
  {
    k = skip(self);
  }
  while (ISALNUM(k));

  self->last_char = k;
  return push(L, self, "<id>", 4, lexeme, self->source - lexeme - 1);
}

static int get_number(lua_State* L, lexer_t* self, int k)
{
  const char* lexeme = self->source - 1;
  int base = 10;
  unsigned suffix = 0;
  char c[8];
  size_t length;
  
  if (k != '0')
  {
    do
    {
      k = skip(self);
    }
    while (ISDIGIT(k));
  }
  else if (k != '.')
  {
    k = skip(self);

    if (k == 'x' || k == 'X')
    {
      base = 16;
      k = skip(self);

      if (!ISXDIGIT(k))
      {
        format_char(c, sizeof(c), k);
        return error(L, self, "invalid digit %s in hexadecimal constant", c);
      }

      do
      {
        k = skip(self);
      }
      while (ISXDIGIT(k));
    }
    else
    {
      if (ISODIGIT(k))
      {
        do
        {
          k = skip(self);
        }
        while (ISODIGIT(k));
      }

      if (ISDIGIT(k))
      {
        return error(L, self, "invalid digit '%c' in octal constant", k);
      }

      if (self->source - lexeme != 2)
      {
        base = 8;
      }
    }
  }

  if (base == 10 && k == '.')
  {
    base = 0; /* indicates a floating point constant */
    k = skip(self);

    if (ISDIGIT(k))
    {
      do
      {
        k = skip(self);
      }
      while (ISDIGIT(k));
    }
  }

  if ((base == 10 || base == 0) && (k == 'e' || k == 'E'))
  {
    base = 0;
    k = skip(self);

    if (k == '+' || k == '-')
    {
      k = skip(self);
    }

    if (!ISDIGIT(k))
    {
      return error(L, self, "exponent has no digits");
    }

    do
    {
      k = skip(self);
    }
    while (ISDIGIT(k));
  }

  if (sizeof(suffix) < 4)
  {
    return error(L, self, "unsigned int must have 32 bits");
  }
  
  while (ISALPHA(k))
  {
    suffix = suffix << 8 | tolower(k);
    k = skip(self);
  }
  
  if (base == 0)
  {
    switch (suffix)
    {
    case 0:
    case 'f':
    case 'l':
      break;
    
    default:
      return error(L, self, "invalid float suffix");
    }
  }
  else
  {
    switch (suffix)
    {
    case 0:
    case 'u':
    case 'u' <<  8 | 'l':
    case 'u' << 16 | 'l' << 8 | 'l':
    case 'l':
    case 'l' <<  8 | 'u':
    case 'l' <<  8 | 'l':
    case 'l' << 16 | 'l' << 8 | 'u':
      break;
    
    default:
      return error(L, self, "invalid integer suffix");
    }
  }

  self->last_char = k;
  length = self->source - lexeme - 1;

  switch (base)
  {
  case 0:  return push(L, self, "<float>", 7, lexeme, length);
  case 8:  return push(L, self, "<octal>", 7, lexeme, length);
  case 10: return push(L, self, "<decimal>", 9, lexeme, length);
  case 16: return push(L, self, "<hexadecimal>", 13, lexeme, length);
  }

  /* should never happen */
  return error(L, self, "internal error, base is %d", base);
}

static int get_string(lua_State* L, lexer_t* self, int k)
{
  const char* lexeme = self->source - 1;
  char c[8];
  int i;

  k = skip(self);

  for (;;)
  {
    if (k == '"')
    {
      k = skip(self);
      break;
    }
    else if (k == -1)
    {
      return error(L, self, "unterminated string");
    }
    else if (k == '\\')
    {
      k = skip(self);
    
      switch (k)
      {
      case 'a':
      case 'b':
      case 'f':
      case 'n':
      case 'r':
      case 't':
      case 'v':
      case '\\':
      case '\'':
      case '"':
      case '?':
        k = skip(self);
        continue;
      
      case 'x':
        k = skip(self);

        if (!ISXDIGIT(k))
        {
          return error(L, self, "\\x used with no following hex digits");
        }
    
        do
        {
          k = skip(self);
        }
        while (ISXDIGIT(k));
    
        continue;
      
      case 'u':
        for (i = 4; i != 0; i--)
        {
          k = skip(self);

          if (!ISXDIGIT(k))
          {
            return error(L, self, "\\u needs 4 hexadecimal digits");
          }
        }
    
        k = skip(self);
        continue;
            
      case 'U':
        for (i = 8; i != 0; i--)
        {
          k = skip(self);

          if (!ISXDIGIT(k))
          {
            return error(L, self, "\\U needs 8 hexadecimal digits");
          }
        }
    
        k = skip(self);
        continue;
      
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
        do
        {
          k = skip(self);
        }
        while (ISODIGIT(k));

        continue;
      }
    
      format_char(c, sizeof(c), k);
      return error(L, self, "unknown escape sequence: %s", c);
    }
    else
    {
      k = skip(self);
    }
  }

  self->last_char = k;
  return push(L, self, "<string>", 8, lexeme, self->source - lexeme - 1);
}

static void line_comment(lua_State* L, lexer_t* self)
{
  int k;

  skip(self);
  skip(self);
  
  do
  {
    k = skip(self);
  }
  while (k != '\n' && k != -1);

  self->last_char = skip(self);
}

static void block_comment(lua_State* L, lexer_t* self)
{
  int k;

  skip(self);
  skip(self);
  
  do
  {
    k = skip(self);

    while (k != '*')
    {
      if (k == -1)
      {
        error(L, self, "unterminated comment");
        return;
      }

      k = skip(self);
    }

    k = skip(self);
  }
  while (k != '/');

  self->last_char = skip(self);
}

static int l_next(lua_State* L)
{
  lexer_t* self;
  int k;
  const char* lexeme;
  size_t length;
  char c[8];

  self = luaL_checkudata(L, 1, "lexer");
  luaL_checktype(L, 2, LUA_TTABLE);
  
again:

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

  if (ISALPHA(k))
  {
    return get_id(L, self, k);
  }

  if (ISDIGIT(k))
  {
    return get_number(L, self, k);
  }

  if (k == '"')
  {
    return get_string(L, self, k);
  }

  lexeme = self->source - 1;

  if (self->end - lexeme >= 2 && lexeme[0] == '/')
  {
    if (lexeme[1] == '/')
    {
      line_comment(L, self);
      goto again;
    }

    if (lexeme[1] == '*')
    {
      block_comment(L, self);
      goto again;
    }
  }

  length = 1;

  while (is_symbol(L, self, lexeme, length))
  {
    k = skip(self);
    length++;
  }

  if (length > 1)
  {
    self->last_char = k;
    return push(L, self, lexeme, length - 1, lexeme, length - 1);
  }

  format_char(c, sizeof(c), k);
  return error(L, self, "Invalid character in input: %s", c);
}

static int l_gc(lua_State* L)
{
  lexer_t* self = (lexer_t*)lua_touserdata(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, self->source_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, self->source_name_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, self->symbols_ref);
  return 0;
}

int newLexer_lua(lua_State* L)
{
  static const luaL_Reg methods[] =
  {
    {"next", l_next},
    {NULL, NULL}
  };
  
  lexer_t* self;
  size_t source_length;
  const char* source;
  const char* source_name;

  luaL_checktype(L, 1, LUA_TTABLE);
  lua_settop(L, 1);
  
  lua_getfield(L, 1, "source");
  source = luaL_checklstring(L, -1, &source_length);

  lua_getfield(L, 1, "file");
  source_name = luaL_checkstring(L, -1);

  lua_getfield(L, 1, "isSymbol");
  luaL_checktype(L, -1, LUA_TFUNCTION);

  self = (lexer_t*)lua_newuserdata(L, sizeof(lexer_t));

  self->source_name = source_name;
  self->line        = 1;
  self->source      = source;
  self->end         = self->source + source_length;
  self->last_char   = skip(self);

  lua_pushvalue(L, 2);
  self->source_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushvalue(L, 3);
  self->source_name_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushvalue(L, 4);
  self->symbols_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  if (luaL_newmetatable(L, "lexer") != 0)
  {
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_gc);
    lua_setfield(L, -2, "__gc");
    luaL_setfuncs(L, methods, 0);
  }

  lua_setmetatable(L, -2);
  return 1;
}
