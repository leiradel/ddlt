static void cpp_format_char(char* buffer, size_t size, int k)
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

static int cpp_get_id(lua_State* L, lexer_t* self)
{
  const char* lexeme;
  
  lexeme = self->source;
  while (ISALNUM(*self->source)) self->source++;
  return push(L, self, "<id>", 4, lexeme, self->source - lexeme);
}

static int cpp_get_number(lua_State* L, lexer_t* self)
{
  int base;
  const char* lexeme;
  unsigned suffix;
  char c[8];
  size_t length;

  lexeme = self->source;
  base = 10;
  
  if (*self->source == '0')
  {
    self->source++;

    if (*self->source == 'x' || *self->source == 'X')
    {
      self->source++;
      base = 16;

      if (!ISXDIGIT(*self->source))
      {
        cpp_format_char(c, sizeof(c), *self->source);
        return error(L, self, "invalid digit %s in hexadecimal constant", c);
      }

      self->source++;
      while (ISXDIGIT(*self->source)) self->source++;
    }
    else
    {
      while (ISODIGIT(*self->source)) self->source++;

      if (ISDIGIT(*self->source))
      {
        return error(L, self, "invalid digit '%c' in octal constant", *self->source);
      }

      if (self->source - lexeme != 1)
      {
        base = 8;
      }
    }
  }
  else if (*self->source != '.')
  {
    self->source++;
    while (ISDIGIT(*self->source)) self->source++;
  }

  if (base == 10 && *self->source == '.')
  {
    self->source++;
    base = 0; /* indicates a floating point constant */
    while (ISDIGIT(*self->source)) self->source++;
  }

  if ((base == 10 || base == 0) && (*self->source == 'e' || *self->source == 'E'))
  {
    self->source++;
    base = 0;

    if (*self->source == '+' || *self->source == '-')
    {
      self->source++;
    }

    if (!ISDIGIT(*self->source))
    {
      return error(L, self, "exponent has no digits");
    }

    self->source++;
    while (ISDIGIT(*self->source)) self->source++;
  }

  if (sizeof(suffix) < 4)
  {
    return error(L, self, "unsigned int must have 32 bits");
  }

  suffix = 0;
  
  while (ISALPHA(*self->source))
  {
    suffix = suffix << 8 | tolower(*self->source);
    self->source++;
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

  length = self->source - lexeme;

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

static int cpp_get_string(lua_State* L, lexer_t* self)
{
  const char* lexeme;
  char reject[3];
  int i;
  char c[8];

  lexeme = self->source++;

  reject[0] = '"';
  reject[1] = '\\';
  reject[2] = 0;

  for (;;)
  {
    self->source += strcspn(self->source, reject);

    if (*self->source == '"')
    {
      self->source++;
      break;
    }
    else if (*self->source == '\\')
    {
      self->source++;

      switch (*self->source++)
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
        continue;
      
      case 'x':
        if (!ISXDIGIT(*self->source))
        {
          return error(L, self, "\\x used with no following hex digits");
        }
    
        self->source++;
        while (ISXDIGIT(*self->source)) self->source++;
        continue;
      
      case 'u':
        for (i = 4; i != 0; i--, self->source++)
        {
          if (!ISXDIGIT(*self->source))
          {
            return error(L, self, "\\u needs 4 hexadecimal digits");
          }
        }
    
        continue;
            
      case 'U':
        for (i = 8; i != 0; i--, self->source++)
        {
          if (!ISXDIGIT(*self->source))
          {
            return error(L, self, "\\U needs 8 hexadecimal digits");
          }
        }
    
        continue;
      
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
        while (ISODIGIT(*self->source)) self->source++;
        continue;
      }
    
      cpp_format_char(c, sizeof(c), self->source[-1]);
      return error(L, self, "unknown escape sequence: %s", c);
    }
    else
    {
      return error(L, self, "unterminated string");
    }
  }

  return push(L, self, "<string>", 8, lexeme, self->source - lexeme);
}

static int cpp_next_lua(lua_State* L, lexer_t* self)
{
  char c[8];

  if (ISALPHA(*self->source))
  {
    return cpp_get_id(L, self);
  }

  if (ISDIGIT(*self->source) || *self->source == '.')
  {
    return cpp_get_number(L, self);
  }

  if (*self->source == '"')
  {
    return cpp_get_string(L, self);
  }

  cpp_format_char(c, sizeof(c), *self->source);
  return error(L, self, "Invalid character in input: %s", c);
}

static void cpp_setup_lexer(lexer_t* self)
{
  self->next = cpp_next_lua;
  self->blocks[0].begin = "//";
  self->blocks[0].type = LINE_COMMENT;
  self->blocks[1].begin = "/*";
  self->blocks[1].end = "*/";
  self->blocks[1].type = BLOCK_COMMENT;
  self->blocks[2].begin = "[{";
  self->blocks[2].end = "}]";
  self->blocks[2].type = FREE_FORMAT;
  self->num_blocks = 3;
}
