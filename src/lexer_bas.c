static void bas_format_char(char* buffer, size_t size, int k)
{
  if (isprint(k) && k != '\'')
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
  const char* lexeme;
  size_t length;
  
  lexeme = self->source;
  self->source += strspn(self->source, ALNUM);
  length = self->source - lexeme;

  if (length == 3 && tolower(lexeme[0]) == 'r' && tolower(lexeme[1]) == 'e' && tolower(lexeme[2]) == 'm')
  {
    return line_comment(L, self);
  }

  return push(L, self, "<id>", 4, lexeme, length);
}

static int bas_get_number(lua_State* L, lexer_t* self)
{
  int base;
  const char* lexeme;
  unsigned suffix;
  char c[8];
  size_t length;

  lexeme = self->source;
  base = 10;
  
  if (*self->source == '&')
  {
    self->source++;

    if (*self->source == 'h' || *self->source == 'H')
    {
      self->source++;
      base = 16;
      length = strspn(self->source, XDIGIT);

      if (length == 0)
      {
        bas_format_char(c, sizeof(c), *self->source);
        return error(L, self, "invalid digit %s in hexadecimal constant", c);
      }

      self->source += length;
    }
    else if (*self->source == 'o' || *self->source == 'O')
    {
      self->source++;
      base = 8;
      length = strspn(self->source, ODIGIT);

      if (length == 0)
      {
        bas_format_char(c, sizeof(c), *self->source);
        return error(L, self, "invalid digit %s in octal constant", c);
      }

      self->source += length;
    }
    else if (*self->source == 'b' || *self->source == 'B')
    {
      self->source++;
      base = 2;
      length = strspn(self->source, BDIGIT);

      if (length == 0)
      {
        bas_format_char(c, sizeof(c), *self->source);
        return error(L, self, "invalid digit %s in binary constant", c);
      }

      self->source += length;
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

    length = strspn(self->source + 1, DIGIT);

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

  length = strspn(self->source, "FRDfrd@!#SUILsuil%&");
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
    case 'r':
    case 'd':
    case '@':
    case '!':
    case '#':
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
    case 's':
    case 'u' <<  8 | 's':
    case 'i':
    case 'u' <<  8 | 'i':
    case 'l':
    case 'u' <<  8 | 'l':
    case '%':
    case '&':
      break;
    
    default:
      return error(L, self, "invalid integer suffix");
    }
  }

  length = self->source - lexeme;

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
  const char* lexeme;
  
  lexeme = self->source++;

  for (;;)
  {
    self->source = strchr(self->source, '"');

    if (self->source == NULL)
    {
      return error(L, self, "unterminated string");
    }
    else if (self->source[1] != '"')
    {
      self->source++;
      break;
    }

    self->source += 2;
  }

  return push(L, self, "<string>", 8, lexeme, self->source - lexeme);
}

static int bas_next_lua(lua_State* L, lexer_t* self)
{
  int k;
  char c[8];

  if (strspn(self->source, ALPHA) != 0)
  {
    return bas_get_id(L, self);
  }

  if (strspn(self->source, DIGIT ".") != 0)
  {
    return bas_get_number(L, self);
  }

  if (*self->source == '&')
  {
    k = self->source[1];

    if (k == 'h' || k == 'H' || k == 'o' || k == 'O' || k == 'b' || k == 'B')
    {
      return bas_get_number(L, self);
    }
  }

  if (*self->source == '"')
  {
    return bas_get_string(L, self);
  }

  bas_format_char(c, sizeof(c), *self->source);
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
