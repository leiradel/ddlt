return function(ddlt)
  local newLexer = ddlt.newLexer

  ddlt.newLexer = function(options)
    local newopts = {}

    for option, value in pairs(options) do
      newopts[option] = value
    end

    if newopts.symbols then
      local symbols = {}

      for i = 1, #newopts.symbols do
        symbols[newopts.symbols[i]] = true
      end

      newopts.symbols = symbols
    end

    local lexer, err = newLexer(newopts)

    if not lexer then
      return nil, err
    end

    if newopts.keywords then
      local keywords = {}

      for i = 1, #newopts.keywords do
        keywords[newopts.keywords[i]] = true
      end

      return {
        next = function(self, la)
          local la, err = lexer:next(la)

          if not la then
            return nil, err
          end

          if la.token == '<id>' and keywords[la.lexeme] then
            la.token = la.lexeme
          end

          return la
        end
      }
    else
      return lexer
    end
  end

  ddlt._VERSION = '2.8.0'
  ddlt._COPYRIGHT = 'Copyright (C) 2017-2018 Andre Leiradella'
  ddlt._DESCRIPTION = 'A generic lexer to help writing parsers using Lua'
  ddlt._URL = 'https://github.com/leiradel/ddlt'

  return ddlt
end
