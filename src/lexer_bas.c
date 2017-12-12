static void bas_format_char(char* buffer, size_t size, int k)
{
  if (isprint(k))
  {
    snprintf(buffer, size, "'%c'", k);
  }
  else if (k != -1)
  {
    snprintf(buffer, size, "'Chr(%d)", k);
  }
  else
  {
    strncpy(buffer, "eof", size);
  }
}

static int bas_get_id(lua_State* L, lexer_t* self)
{
  int k;
  const char* lexeme;
  size_t length;
  
  lexeme = self->source - 1;

  do
  {
    k = skip(self);
  }
  while (ISALNUM(k));

  self->last_char = k;
  length = self->source - lexeme - 1;

  if (length == 3 && tolower(lexeme[0]) == 'r' && tolower(lexeme[1]) == 'e' && tolower(lexeme[2]) == 'm')
  {
    return line_comment(L, self);
  }

  return push(L, self, "<id>", 4, lexeme, length);
}

static int bas_get_number(lua_State* L, lexer_t* self)
{
  int k, base;
  const char* lexeme;
  unsigned suffix;
  char c[8];
  size_t length;

  k = self->last_char;
  base = 10;
  lexeme = self->source - 1;
  
  if (k == '&')
  {
    k = skip(self);

    if (k == 'h' || k == 'H')
    {
      base = 16;
      k = skip(self);

      if (!ISXDIGIT(k))
      {
        bas_format_char(c, sizeof(c), k);
        return error(L, self, "invalid digit %s in hexadecimal constant", c);
      }

      do
      {
        k = skip(self);
      }
      while (ISXDIGIT(k));
    }
    else if (k == 'o' || k == 'O')
    {
      base = 8;
      k = skip(self);

      if (!ISODIGIT(k))
      {
        bas_format_char(c, sizeof(c), k);
        return error(L, self, "invalid digit %s in octal constant", c);
      }

      do
      {
        k = skip(self);
      }
      while (ISODIGIT(k));
    }
    else if (k == 'b' || k == 'B')
    {
      base = 2;
      k = skip(self);

      if (!ISBDIGIT(k))
      {
        bas_format_char(c, sizeof(c), k);
        return error(L, self, "invalid digit %s in binary constant", c);
      }

      do
      {
        k = skip(self);
      }
      while (ISBDIGIT(k));
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
      if (k == '@' || k == '!' || k == '#')
      {
        k = skip(self);
      }

      break;
      
    case 'f':
    case 'r':
    case 'd':
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
      if (k == '%' || k == '&')
      {
        k = skip(self);
      }

      break;
      
    case 's':
    case 'u' <<  8 | 's':
    case 'i':
    case 'u' <<  8 | 'i':
    case 'l':
    case 'u' <<  8 | 'l':
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
  case 2:  return push(L, self, "<binary>", 8, lexeme, length);
  case 8:  return push(L, self, "<octal>", 7, lexeme, length);
  case 10: return push(L, self, "<decimal>", 9, lexeme, length);
  case 16: return push(L, self, "<hexadecimal>", 13, lexeme, length);
  }

  /* should never happen */
  return error(L, self, "internal error, base is %d", base);
}

static int bas_get_string(lua_State* L, lexer_t* self)
{
  int k;
  const char* lexeme;
  
  lexeme = self->source - 1;
  k = skip(self);

  for (;;)
  {
    if (k == '"')
    {
      k = skip(self);

      if (k != '"')
      {
        break;
      }
    }
    else if (k == -1)
    {
      return error(L, self, "unterminated string");
    }

    k = skip(self);
  }

  self->last_char = k;
  return push(L, self, "<string>", 8, lexeme, self->source - lexeme - 1);
}

static int bas_next_lua(lua_State* L, lexer_t* self)
{
  int k;
  char c[8];

  k = self->last_char;

  if (ISALPHA(k))
  {
    return bas_get_id(L, self);
  }

  if (ISDIGIT(k) || k == '.')
  {
    return bas_get_number(L, self);
  }

  if (k == '&' && self->end - self->source >= 1)
  {
    k = *self->source;

    if (k == 'h' || k == 'H' || k == 'o' || k == 'O' || k == 'b' || k == 'B')
    {
      return bas_get_number(L, self);
    }

    k = self->last_char;
  }

  if (k == '"')
  {
    return bas_get_string(L, self);
  }

  bas_format_char(c, sizeof(c), k);
  return error(L, self, "Invalid character in input: %s", c);
}

static void bas_setup_lexer(lexer_t* self)
{
  self->next = bas_next_lua;
  self->blocks[0].begin = "'";
  self->blocks[0].type = LINE_COMMENT;
  self->blocks[1].begin = "[{";
  self->blocks[1].end = "}]";
  self->blocks[1].type = FREE_FORMAT;
  self->num_blocks = 2;
}
