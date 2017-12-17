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

  -- inception
  local expected = {
    {line = 1, token = [[<linecomment>]], lexeme = [[' Line comment\n]]},
    {line = 3, token = [[<linecomment>]], lexeme = [[rem Line comment\n]]},
    {line = 5, token = [[<linecomment>]], lexeme = [[REM Line comment\n]]},
    {line = 7, token = [[<freeform>]], lexeme = [=[[{\n  Free\n  form\n  block\n}]]=]},
    {line = 13, token = [[<id>]], lexeme = [[id]]},
    {line = 14, token = [[<id>]], lexeme = [[Id]]},
    {line = 15, token = [[<id>]], lexeme = [[iD]]},
    {line = 16, token = [[<id>]], lexeme = [[ID]]},
    {line = 17, token = [[<id>]], lexeme = [[_id]]},
    {line = 18, token = [[<id>]], lexeme = [[id_]]},
    {line = 19, token = [[<id>]], lexeme = [[_]]},
    {line = 21, token = [[<decimal>]], lexeme = [[0]]},
    {line = 22, token = [[<hexadecimal>]], lexeme = [[&habcd]]},
    {line = 23, token = [[<hexadecimal>]], lexeme = [[&HABCD]]},
    {line = 24, token = [[<octal>]], lexeme = [[&o123]]},
    {line = 25, token = [[<octal>]], lexeme = [[&O123]]},
    {line = 26, token = [[<binary>]], lexeme = [[&b01]]},
    {line = 27, token = [[<binary>]], lexeme = [[&B01]]},
    {line = 28, token = [[<decimal>]], lexeme = [[123]]},
    {line = 29, token = [[<float>]], lexeme = [[1.]]},
    {line = 30, token = [[<float>]], lexeme = [[.1]]},
    {line = 31, token = [[<float>]], lexeme = [[1.1]]},
    {line = 32, token = [[<float>]], lexeme = [[1.f]]},
    {line = 33, token = [[<float>]], lexeme = [[.1f]]},
    {line = 34, token = [[<float>]], lexeme = [[1.1f]]},
    {line = 35, token = [[<float>]], lexeme = [[1.e1]]},
    {line = 36, token = [[<float>]], lexeme = [[.1e1]]},
    {line = 37, token = [[<float>]], lexeme = [[1.1e1]]},
    {line = 38, token = [[<float>]], lexeme = [[1.e1f]]},
    {line = 39, token = [[<float>]], lexeme = [[.1e1f]]},
    {line = 40, token = [[<float>]], lexeme = [[1.1e1f]]},
    {line = 41, token = [[<float>]], lexeme = [[1.r]]},
    {line = 42, token = [[<float>]], lexeme = [[1.d]]},
    {line = 43, token = [[<float>]], lexeme = [[1.@]]},
    {line = 44, token = [[<float>]], lexeme = [[1.!]]},
    {line = 45, token = [[<float>]], lexeme = [[1.#]]},
    {line = 46, token = [[<decimal>]], lexeme = [[1s]]},
    {line = 47, token = [[<decimal>]], lexeme = [[1us]]},
    {line = 48, token = [[<decimal>]], lexeme = [[1i]]},
    {line = 49, token = [[<decimal>]], lexeme = [[1ui]]},
    {line = 50, token = [[<decimal>]], lexeme = [[1l]]},
    {line = 51, token = [[<decimal>]], lexeme = [[1ul]]},
    {line = 52, token = [[<decimal>]], lexeme = [[1%]]},
    {line = 53, token = [[<decimal>]], lexeme = [[1&]]},
    {line = 54, token = [[<decimal>]], lexeme = [[1UL]]},
    {line = 55, token = [[<float>]], lexeme = [[1#]]},
    {line = 57, token = [[<string>]], lexeme = [[""]]},
    {line = 58, token = [[<string>]], lexeme = [[""""]]},
    {line = 59, token = [[<string>]], lexeme = [["""a"""]]},
    {line = 60, token = [[<string>]], lexeme = [["a"""]]},
    {line = 61, token = [[<string>]], lexeme = [["""a"]]},
    {line = 62, token = [[<string>]], lexeme = [["a""b"]]},
    {line = 62, token = [[<eof>]], lexeme = [[<eof>]]},
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
