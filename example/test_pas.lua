local ddlt = require 'ddlt'

local parse = function(file)
  local inp = assert(io.open(file, 'rb'))
  local source = inp:read('*a')
  inp:close()

  local lexer = ddlt.newLexer{
    source = source,
    file = file,
    language = 'pas',
    isSymbol = function(lexeme) return false end,
    freeform = {'[{', '}]'}
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

local template = [===[
local expected = {
--[[! for i = 1, #args do ]]
--[[!  local la = args[ i ] ]]
  {line = --[[= la.line ]], token = [[--[[= la.token ]]]], lexeme = [[--[[= la.lexeme ]]]]},
--[[! end ]]
}
]===]
  
return function(args)
  if #args ~= 2 then
    error('missing input file\n')
  end

  local res = {}
  local tokens = parse(args[2])
  local templ = assert(ddlt.newTemplate(template, '--[[', ']]'))
  templ(tokens, function(out) res[#res + 1] = out end)

  res = table.concat(res):gsub('\n+', '\n')
  io.write(res)

  -- inception
  local expected = {
    {line = 13, token = [[<freeform>]], lexeme = [=[[{\n  Free\n  form\n  block\n}]]=]},
    {line = 15, token = [[<id>]], lexeme = [[id]]},
    {line = 16, token = [[<id>]], lexeme = [[Id]]},
    {line = 17, token = [[<id>]], lexeme = [[iD]]},
    {line = 18, token = [[<id>]], lexeme = [[ID]]},
    {line = 19, token = [[<id>]], lexeme = [[_id]]},
    {line = 20, token = [[<id>]], lexeme = [[id_]]},
    {line = 21, token = [[<id>]], lexeme = [[_]]},
    {line = 23, token = [[<decimal>]], lexeme = [[0]]},
    {line = 24, token = [[<hexadecimal>]], lexeme = [[$abcd]]},
    {line = 25, token = [[<hexadecimal>]], lexeme = [[$ABCD]]},
    {line = 26, token = [[<octal>]], lexeme = [[&123]]},
    {line = 27, token = [[<binary>]], lexeme = [[%01]]},
    {line = 28, token = [[<decimal>]], lexeme = [[123]]},
    {line = 29, token = [[<float>]], lexeme = [[1.1]]},
    {line = 30, token = [[<float>]], lexeme = [[1e1]]},
    {line = 31, token = [[<float>]], lexeme = [[1.1e1]]},
    {line = 32, token = [[<string>]], lexeme = [['']]},
    {line = 33, token = [[<string>]], lexeme = [['a']]},
    {line = 34, token = [[<string>]], lexeme = [['ab']]},
    {line = 35, token = [[<string>]], lexeme = [[#48]]},
    {line = 36, token = [[<string>]], lexeme = [[#48'']]},
    {line = 37, token = [[<string>]], lexeme = [[''#48]]},
    {line = 38, token = [[<string>]], lexeme = [[''#48'']]},
    {line = 39, token = [[<string>]], lexeme = [['a'#48]]},
    {line = 40, token = [[<string>]], lexeme = [[#48'a']]},
    {line = 41, token = [[<string>]], lexeme = [['a'#48'b']]},
    {line = 42, token = [[<string>]], lexeme = [[#48#49]]},
    {line = 43, token = [[<string>]], lexeme = [[#48#49'']]},
    {line = 44, token = [[<string>]], lexeme = [[''#48#49]]},
    {line = 45, token = [[<string>]], lexeme = [[''#48#49'']]},
    {line = 46, token = [[<string>]], lexeme = [['a'#48#49]]},
    {line = 47, token = [[<string>]], lexeme = [[#48#49'a']]},
    {line = 48, token = [[<string>]], lexeme = [['a'#48#49'b']]},
    {line = 49, token = [[<eof>]], lexeme = [[<eof>]]},
  }

  assert(#tokens == #expected, 'Wrong number of tokens produced')
  
  for i = 1, #tokens do
    local la1 = tokens[i]
    local la2 = expected[i]

    local msg1 = string.format('{line = %d, token = [[%s]], lexeme = [[%s]]}', la1.line, la1.token, la1.lexeme)
    local msg2 = string.format('{line = %d, token = [[%s]], lexeme = [[%s]]}', la2.line, la2.token, la2.lexeme)

    if la1.line ~= la2.line then
      io.write(string.format('Lines are different in\n  %s\n  %s\n', msg1, msg2))
      os.exit(1)
    end

    if la1.token ~= la2.token then
      io.write(string.format('Tokens are different in\n  %s\n  %s\n', msg1, msg2))
      os.exit(1)
    end
    
    if la1.lexeme ~= la2.lexeme then
      io.write(string.format('Lexemes are different in\n  %s\n  %s\n', msg1, msg2))
      os.exit(1)
    end
  end

  io.write('-- tests passed\n')
end
