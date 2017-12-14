local ddlt = require 'ddlt'

local function usage(out)
  out:write[[
ddlt: a generic, C-like lexer to help write parsers using Lua
Copyright 2017 Andre Leiradella @leiradel

Usage: ddlt <parser.lua> [args...]

ddlt runs the Lua script given as its first argument, and executes the
function returned by that script. All arguments but the first are passed to
that function.
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
    if not options.isSymbol and options.symbols then
      local symbols = {}

      for i = 1, #options.symbols do
        symbols[options.symbols[i]] = true
      end

      options.isSymbol = function(symbol)
        return symbols[symbol]
      end
    end

    local lexer, err = newLexer(options)

    if not lexer then
      return nil, err
    end

    if options.keywords then
      local keywords = {}

      for i = 1, #options.keywords do
        keywords[options.keywords[i]] = true
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
