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
  block_t block;
  
  lexeme = self->source;
  self->source += strspn(self->source, ALNUM);
  length = self->source - lexeme;

  if (length == 3 && tolower(lexeme[0]) == 'r' && tolower(lexeme[1]) == 'e' && tolower(lexeme[2]) == 'm')
  {
    self->source -= 3;

    block.type = LINE_COMMENT;
    block.begin = "REM";

    return line_comment(L, self, &block, 0);
  }

  return PUSH(L, self, "<id>", lexeme, length);
}

static int bas_get_number(lua_State* L, lexer_t* self)
{
  int base;
  const char* lexeme;
  unsigned suffix;
  char c[8];
  size_t length, saved;

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

  saved = length = strspn(self->source, ALNUM "@!#%&");
  suffix = 0;
  
  while (length-- != 0)
  {
    suffix = suffix << 8 | tolower(*self->source++);
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
      return error(L, self, "invalid suffix \"%.*s\"", (int)saved, self->source - saved);
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
    
    case 'f':
    case 'r':
    case 'd':
    case '@':
    case '!':
    case '#':
      /* also include float suffixes, in this case the number is a float */
      base = 0;
      break;
    
    default:
      return error(L, self, "invalid suffix \"%.*s\"", (int)saved, self->source - saved);
    }
  }

  length = self->source - lexeme;

  switch (base)
  {
  case 0:  return PUSH(L, self, "<float>", lexeme, length);
  case 2:  return PUSH(L, self, "<binary>", lexeme, length);
  case 8:  return PUSH(L, self, "<octal>", lexeme, length);
  case 10: return PUSH(L, self, "<decimal>", lexeme, length);
  case 16: return PUSH(L, self, "<hexadecimal>", lexeme, length);
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
    self->source += strcspn(self->source, "\"\n");

    if (*self->source == '"')
    {
      if (self->source[1] != '"')
      {
        self->source++;
        break;
      }

      self->source += 2;
    }
    else
    {
      return error(L, self, "unterminated string");
    }
  }

  return PUSH(L, self, "<string>", lexeme, self->source - lexeme);
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
  self->blocks[0].type = LINE_COMMENT;
  self->blocks[0].begin = "'";

  self->num_blocks = 1;
  self->next = bas_next_lua;
}
