return function(ddlt)
  local newLexer = ddlt.newLexer

  ddlt.newLexer = function(options)
    local newopts = {}

    for option, value in pairs(options) do
      newopts[option] = value
    end

    if not newopts.isSymbol and newopts.symbols then
      local symbols = {}
      local maxlen = 0

      for i = 1, #newopts.symbols do
        local symbol = newopts.symbols[i]
        symbols[symbol] = true
        maxlen = math.max(maxlen, #symbol)
      end

      newopts.isSymbol = function(symbol)
        return symbols[symbol]
      end

      newopts.maxSymbolLength = maxlen
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

  ddlt._VERSION = '2.3.3'
  ddlt._COPYRIGHT = 'Copyright (C) 2017-2018 Andre Leiradella'
  ddlt._DESCRIPTION = 'A generic lexer to help writing parsers using Lua'
  ddlt._URL = 'https://github.com/leiradel/ddlt'

  return ddlt
end
