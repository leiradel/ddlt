static void pas_format_char(char* buffer, size_t size, int k)
{
  if (isprint(k) && k != '\'')
  {
    snprintf(buffer, size, "'%c'", k);
  }
  else if (k != -1)
  {
    snprintf(buffer, size, "'#%d", k);
  }
  else
  {
    strncpy(buffer, "eof", size);
  }
}

static int pas_get_id(lua_State* L, lexer_t* self)
{
  const char* lexeme;
  
  lexeme = self->source;
  self->source += strspn(self->source, ALNUM);
  return push(L, self, "<id>", 4, lexeme, self->source - lexeme);
}

static int pas_get_number(lua_State* L, lexer_t* self)
{
  int base;
  const char* lexeme;
  char c[8];
  size_t length;

  lexeme = self->source;
  base = 10;
  
  if (*self->source == '$')
  {
    self->source++;
    base = 16;
    length = strspn(self->source, XDIGIT);

    if (length == 0)
    {
      pas_format_char(c, sizeof(c), *self->source);
      return error(L, self, "invalid digit %s in hexadecimal constant", c);
    }

    self->source += length;
  }
  else if (*self->source == '&')
  {
    self->source++;
    base = 8;
    length = strspn(self->source, ODIGIT);

    if (length == 0)
    {
      pas_format_char(c, sizeof(c), *self->source);
      return error(L, self, "invalid digit %s in octal constant", c);
    }

    self->source += length;
  }
  else if (*self->source == '%')
  {
    self->source++;
    base = 2;
    length = strspn(self->source, BDIGIT);

    if (length == 0)
    {
      pas_format_char(c, sizeof(c), *self->source);
      return error(L, self, "invalid digit %s in binary constant", c);
    }

    self->source += length;
  }
  else
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

static int pas_get_string(lua_State* L, lexer_t* self)
{
  const char* lexeme;
  size_t length;
  
  lexeme = self->source++;

  if (*lexeme == '#')
  {
    goto control;
  }

  for (;;)
  {
    self->source = strchr(self->source, '\'');

    if (self->source == NULL)
    {
      return error(L, self, "unterminated string");
    }
    else if (self->source[1] == '#')
    {
      self->source += 2;

control:
      length = strspn(self->source, DIGIT);

      if (length == 0)
      {
        return error(L, self, "control string used with no following digits");
      }
      
      self->source += length;

      if (*self->source == '#')
      {
        self->source++;
        goto control;
      }
      else if (*self->source != '\'')
      {
        break;
      }
    }
    else
    {
      self->source++;
      break;
    }

    self->source++;
  }

  return push(L, self, "<string>", 8, lexeme, self->source - lexeme);
}

static int pas_next_lua(lua_State* L, lexer_t* self)
{
  char c[8];

  if (strspn(self->source, ALPHA) != 0)
  {
    return pas_get_id(L, self);
  }

  if (strspn(self->source, DIGIT "$&%") != 0)
  {
    return pas_get_number(L, self);
  }

  if (*self->source == '\'' || *self->source == '#')
  {
    return pas_get_string(L, self);
  }

  pas_format_char(c, sizeof(c), *self->source);
  return error(L, self, "Invalid character in input: %s", c);
}

static void pas_setup_lexer(lexer_t* self)
{
  self->next = pas_next_lua;
  self->blocks[0].begin = "//";
  self->blocks[0].type = LINE_COMMENT;
  self->blocks[1].begin = "(*";
  self->blocks[1].end = "*)";
  self->blocks[1].type = BLOCK_COMMENT;
  self->blocks[2].begin = "{";
  self->blocks[2].end = "}";
  self->blocks[2].type = BLOCK_COMMENT;
  self->num_blocks = 3;
}
