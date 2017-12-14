local ddlt = require 'ddlt'

local parse = function(file)
  local inp = assert(io.open(file, 'rb'))
  local source = inp:read('*a')
  inp:close()

  local lexer = ddlt.newLexer{
    source = source,
    file = file,
    language = 'bas',
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

  local expected = {
    {line = 8, token = [[<freeform>]], lexeme = [=[[{\n  Free\n  form\n  block\n}]]=]},
    {line = 9, token = [[<id>]], lexeme = [[id]]},
    {line = 9, token = [[<id>]], lexeme = [[Id]]},
    {line = 9, token = [[<id>]], lexeme = [[iD]]},
    {line = 9, token = [[<id>]], lexeme = [[ID]]},
    {line = 9, token = [[<id>]], lexeme = [[_id]]},
    {line = 9, token = [[<id>]], lexeme = [[id_]]},
    {line = 9, token = [[<id>]], lexeme = [[_]]},
    {line = 10, token = [[<decimal>]], lexeme = [[0]]},
    {line = 10, token = [[<hexadecimal>]], lexeme = [[&habcd]]},
    {line = 10, token = [[<hexadecimal>]], lexeme = [[&HABCD]]},
    {line = 10, token = [[<octal>]], lexeme = [[&o123]]},
    {line = 10, token = [[<octal>]], lexeme = [[&O123]]},
    {line = 10, token = [[<binary>]], lexeme = [[&b01]]},
    {line = 10, token = [[<binary>]], lexeme = [[&B01]]},
    {line = 10, token = [[<decimal>]], lexeme = [[123]]},
    {line = 10, token = [[<float>]], lexeme = [[1.]]},
    {line = 10, token = [[<float>]], lexeme = [[.1]]},
    {line = 10, token = [[<float>]], lexeme = [[1.1]]},
    {line = 10, token = [[<float>]], lexeme = [[1.f]]},
    {line = 10, token = [[<float>]], lexeme = [[.1f]]},
    {line = 10, token = [[<float>]], lexeme = [[1.1f]]},
    {line = 10, token = [[<float>]], lexeme = [[1.e1]]},
    {line = 10, token = [[<float>]], lexeme = [[.1e1]]},
    {line = 10, token = [[<float>]], lexeme = [[1.1e1]]},
    {line = 10, token = [[<float>]], lexeme = [[1.e1f]]},
    {line = 10, token = [[<float>]], lexeme = [[.1e1f]]},
    {line = 10, token = [[<float>]], lexeme = [[1.1e1f]]},
    {line = 10, token = [[<float>]], lexeme = [[1.r]]},
    {line = 10, token = [[<float>]], lexeme = [[1.d]]},
    {line = 10, token = [[<float>]], lexeme = [[1.@]]},
    {line = 10, token = [[<float>]], lexeme = [[1.!]]},
    {line = 10, token = [[<float>]], lexeme = [[1.#]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1s]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1us]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1i]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1ui]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1l]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1ul]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1%]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1&]]},
    {line = 10, token = [[<decimal>]], lexeme = [[1UL]]},
    {line = 10, token = [[<float>]], lexeme = [[1#]]},
    {line = 11, token = [[<string>]], lexeme = [[""]]},
    {line = 11, token = [[<string>]], lexeme = [[""""]]},
    {line = 11, token = [[<string>]], lexeme = [["""a"""]]},
    {line = 11, token = [[<string>]], lexeme = [["a"""]]},
    {line = 11, token = [[<string>]], lexeme = [["""a"]]},
    {line = 12, token = [[<eof>]], lexeme = [[<eof>]]},
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
