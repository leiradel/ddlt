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
  return PUSH(L, self, "<id>", lexeme, self->source - lexeme);
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
  case 0:  return PUSH(L, self, "<float>", lexeme, length);
  case 2:  return PUSH(L, self, "<binary>", lexeme, length);
  case 8:  return PUSH(L, self, "<octal>", lexeme, length);
  case 10: return PUSH(L, self, "<decimal>", lexeme, length);
  case 16: return PUSH(L, self, "<hexadecimal>", lexeme, length);
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
    self->source += strcspn(self->source, "'\n");

    if (*self->source == '\'')
    {
      self->source++;

      if (*self->source != '#')
      {
        break;
      }

      self->source++;

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

      self->source++;
    }
    else
    {
      return error(L, self, "unterminated string");
    }
  }

  return PUSH(L, self, "<string>", lexeme, self->source - lexeme);
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
  self->blocks[0].type = LINE_COMMENT;
  self->blocks[0].begin = "//";

  self->blocks[1].type = BLOCK_DIRECTIVE;
  self->blocks[1].begin = "(*$";
  self->blocks[1].block_directive.end = "*)";

  self->blocks[2].type = BLOCK_DIRECTIVE;
  self->blocks[2].begin = "{$";
  self->blocks[2].block_directive.end = "}";

  self->blocks[3].type = BLOCK_COMMENT;
  self->blocks[3].begin = "(*";
  self->blocks[3].block_comment.end = "*)";

  self->blocks[4].type = BLOCK_COMMENT;
  self->blocks[4].begin = "{";
  self->blocks[4].block_comment.end = "}";

  self->num_blocks = 5;
  self->next = pas_next_lua;
}
