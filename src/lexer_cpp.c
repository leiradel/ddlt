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
  int k;
  const char* lexeme;
  
  k = self->last_char;
  lexeme = self->source - 1;

  do
  {
    k = skip(self);
  }
  while (ISALNUM(k));

  self->last_char = k;
  return push(L, self, "<id>", 4, lexeme, self->source - lexeme - 1);
}

static int cpp_get_number(lua_State* L, lexer_t* self)
{
  int k, base;
  const char* lexeme;
  unsigned suffix;
  char c[8];
  size_t length;

  k = self->last_char;
  base = 10;
  lexeme = self->source - 1;
  
  if (k == '0')
  {
    k = skip(self);

    if (k == 'x' || k == 'X')
    {
      base = 16;
      k = skip(self);

      if (!ISXDIGIT(k))
      {
        cpp_format_char(c, sizeof(c), k);
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
  else if (k != '.')
  {
    do
    {
      k = skip(self);
    }
    while (ISDIGIT(k));
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

  suffix = 0;
  
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

static int cpp_get_string(lua_State* L, lexer_t* self)
{
  int k, i;
  const char* lexeme;
  char c[8];

  lexeme = self->source - 1;
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
    
      cpp_format_char(c, sizeof(c), k);
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

static int cpp_next_lua(lua_State* L, lexer_t* self)
{
  int k;
  const char* lexeme;
  size_t length;
  char c[8];

  k = self->last_char;

  if (ISALPHA(k))
  {
    return cpp_get_id(L, self);
  }

  if (ISDIGIT(k) || k == '.')
  {
    return cpp_get_number(L, self);
  }

  if (k == '"')
  {
    return cpp_get_string(L, self);
  }

  lexeme = self->source - 1;
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

  cpp_format_char(c, sizeof(c), k);
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
