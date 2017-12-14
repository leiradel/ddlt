local ddlt = require 'ddlt'

local function usage(out)
  out:write[[
ddlt: a generic lexer that helps writing parsers using Lua
Copyright 2017 Andre Leiradella @leiradel
https://github.com/leiradel/ddlt
Version 1.0

Usage: ddlt <parser.lua> [args...]

ddlt runs the Lua script given as its first argument, and executes the
function returned by that script.
]]
end

return function(args)
  if #args == 0 then
    usage(io.stderr)
    os.exit(1)
  end

  args[1] = ddlt.realpath(args[1])

  local main = assert(loadfile(args[1], 't'))()

  local newLexer = ddlt.newLexer

  ddlt.newLexer = function(options)
    local newopts = {}

    for option, value in pairs(options) do
      newopts[option] = value
    end

    if not newopts.isSymbol and newopts.symbols then
      local symbols = {}

      for i = 1, #newopts.symbols do
        symbols[newopts.symbols[i]] = true
      end

      newopts.isSymbol = function(symbol)
        return symbols[symbol]
      end
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
            la.token = string.format('[%s]', la.lexeme)
          end

          return la
        end
      }
    else
      return lexer
    end
  end

  main(args)
end
