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

static int cpp_get_string(lua_State* L, lexer_t* self, unsigned skip, int quote, const char* prefix)
{
  const char* lexeme;
  char reject[3];
  size_t length;
  char c[8];
  char token[32];

  lexeme = self->source;
  self->source += skip + 1;

  reject[0] = quote;
  reject[1] = '\\';
  reject[2] = 0;

  for (;;)
  {
    self->source += strcspn(self->source, reject);

    if (*self->source == quote)
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

  length = snprintf(token, sizeof(token), "<%s%s>", prefix, quote == '"' ? "string" : "char");
  return push(L, self, token, length, lexeme, self->source - lexeme);
}

static int cpp_get_rawstring(lua_State* L, lexer_t* self, unsigned skip, const char* token)
{
  const char* lexeme;
  char delimiter[24];
  size_t count;
  char c[8];
  
  lexeme = self->source;
  self->source += skip + 1;

  count = strcspn(self->source, NOTDELIM);

  if (count > 16)
  {
    return error(L, self, "raw string delimiter longer than 16 characters");
  }

  memcpy(delimiter, self->source, count);
  delimiter[count] = '"';
  delimiter[count + 1] = 0;

  self->source += count;

  if (*self->source != '(')
  {
    cpp_format_char(c, sizeof(c), *self->source);
    return error(L, self, "invalid character %s in raw string delimiter", c);
  }

  self->source = strchr(self->source + 1, ')');

  if (self->source == NULL || strncmp(self->source + 1, delimiter, count + 1))
  {
    return error(L, self, "missing raw string terminating delimiter )%s", delimiter);
  }

  self->source += count + 2;
  return push(L, self, token, strlen(token), lexeme, self->source - lexeme);
}

static int cpp_next_lua(lua_State* L, lexer_t* self)
{
  char k0, k1, k2;
  char c[8];

  if (strspn(self->source, DIGIT ".") != 0)
  {
    return cpp_get_number(L, self);
  }

  k0 = *self->source;

  if (k0 == '"' || k0 == '\'')
  {
    return cpp_get_string(L, self, 0, k0, "");
  }

  k1 = self->source[1];

  if (k1 == '"' || k1 == '\'')
  {
    if (k0 == 'L')
    {
      return cpp_get_string(L, self, 1, k1, "wide");
    }
    else if (k0 == 'u')
    {
      return cpp_get_string(L, self, 1, k1, "utf16");
    }
    else if (k0 == 'U')
    {
      return cpp_get_string(L, self, 1, k1, "utf32");
    }
    else if (k0 == 'R' && k1 == '"')
    {
      return cpp_get_rawstring(L, self, 1, "<rawstring>");
    }
  }

  k2 = k1 != 0 ? self->source[2] : 0;

  if (k2 == '"')
  {
    if (k0 == 'u' && k1 == '8')
    {
      return cpp_get_string(L, self, 2, k2, "utf8");
    }
    else if ((k0 == 'L' && k1 == 'R') || (k0 == 'R' && k1 == 'L'))
    {
      return cpp_get_rawstring(L, self, 2, "<rawwidestring>");
    }
    else if ((k0 == 'u' && k1 == 'R') || (k0 == 'R' && k1 == 'u'))
    {
      return cpp_get_rawstring(L, self, 2, "<rawutf16string>");
    }
    else if ((k0 == 'U' && k1 == 'R') || (k0 == 'R' && k1 == 'U'))
    {
      return cpp_get_rawstring(L, self, 2, "<rawutf32string>");
    }
  }

  if (k0 == 'u' && k1 == '8' && k2 == '\'')
  {
    return cpp_get_string(L, self, 2, k2, "utf8");
  }

  if (strspn(self->source, ALPHA) != 0)
  {
    return cpp_get_id(L, self);
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
