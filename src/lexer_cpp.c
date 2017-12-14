static void cpp_format_char(char* buffer, size_t size, int k)
{
  if (isprint(k) && k != '\'')
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
  self->source += strspn(self->source, ALNUM);
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
      length = strspn(self->source, XDIGIT);

      if (length == 0)
      {
        cpp_format_char(c, sizeof(c), *self->source);
        return error(L, self, "invalid digit %s in hexadecimal constant", c);
      }

      self->source += length;
    }
    else
    {
      length = strspn(self->source, ODIGIT);

      if (strspn(self->source + length, DIGIT) != 0)
      {
        return error(L, self, "invalid digit '%c' in octal constant", *self->source);
      }

      self->source += length;

      if (length != 0)
      {
        base = 8;
      }
    }
  }
  else if (*self->source != '.')
  {
    self->source += strspn(self->source, DIGIT);
  }

  if (base == 10 && *self->source == '.')
  {
    self->source++;
    self->source += strspn(self->source, DIGIT);
    base = 0; /* indicates a floating point constant */
  }

  if ((base == 10 || base == 0) && (*self->source == 'e' || *self->source == 'E'))
  {
    self->source++;
    base = 0;

    if (*self->source == '+' || *self->source == '-')
    {
      self->source++;
    }

    length = strspn(self->source, DIGIT);

    if (length == 0)
    {
      return error(L, self, "exponent has no digits");
    }

    self->source += length;
  }

  if (sizeof(suffix) < 4)
  {
    return error(L, self, "unsigned int must have 32 bits");
  }

  length = strspn(self->source, "FLUflu");
  suffix = 0;
  
  while (length-- != 0)
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
  size_t length;
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
        length = strspn(self->source, XDIGIT);

        if (length == 0)
        {
          return error(L, self, "\\x used with no following hex digits");
        }
    
        self->source += length;
        continue;
      
      case 'u':
        length = strspn(self->source, XDIGIT);

        if (length != 4)
        {
          return error(L, self, "\\u needs 4 hexadecimal digits");
        }
    
        self->source += length;
        continue;
            
      case 'U':
        length = strspn(self->source, XDIGIT);

        if (length != 8)
        {
          return error(L, self, "\\u needs 4 hexadecimal digits");
        }
    
        self->source += length;
        continue;
      
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
        self->source += strspn(self->source, ODIGIT);
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

  if (strspn(self->source, ALPHA) != 0)
  {
    return cpp_get_id(L, self);
  }

  if (strspn(self->source, DIGIT ".") != 0)
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
  self->num_blocks = 2;
}
