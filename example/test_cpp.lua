local ddlt = require 'ddlt'

local parse = function(file)
  local inp = assert(io.open(file, 'rb'))
  local source = inp:read('*a')
  inp:close()

  local symbols = {
    ['{'] = true,
    ['}'] = true,
    [','] = true,
    [';'] = true,
    ['='] = true
  }

  local lexer = ddlt.newLexer{
    source = source,
    file = file,
    language = 'cpp',
    isSymbol = function(lexeme) return symbols[lexeme] end
  }

  local tokens = {}
  local max = 0

  repeat
    local la = {}
    assert(lexer:next(la))
    la.lexeme = la.lexeme:gsub('\n', '\\n')
    tokens[#tokens + 1] = la
    max = math.max(max, #la.token)
  until la.token == '<eof>'

  tokens.max = max
  return tokens
end

local template = [[
/*! local tkfmt = '%-' .. args.max .. 's' */
/*! for i = 1, #args do */
/*!  local la = args[ i ] */
line = /*= string.format('%3d', la.line) */ token = /*= string.format(tkfmt, la.token) */ lexeme = /*= la.lexeme */
/*! end */
]]

return function(args)
  if #args ~= 1 then
    error('missing input file\n')
  end

  local res = {}
  local tokens = parse(args[1])
  local templ = ddlt.newTemplate(template)
  templ = assert(load(templ, 'template'))()
  templ(tokens, function(out) res[#res + 1] = out end)

  res = table.concat(res):gsub('\n+', '\n')
  io.write(res)
end
