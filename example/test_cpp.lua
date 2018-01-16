local ddlt = require 'ddlt'

local parse = function(file)
  local inp = assert(io.open(file, 'rb'))
  local source = inp:read('*a')
  inp:close()

  local lexer = ddlt.newLexer{
    source = source,
    file = file,
    language = 'cpp',
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

if #arg ~= 1 then
  error('missing input file\n')
end

local res = {}
local tokens = parse(arg[1])

local templ = assert(ddlt.newTemplate(template, '--[[', ']]'))
templ(tokens, function(out) res[#res + 1] = out end)

res = table.concat(res):gsub('\n+', '\n')
io.write(res)

-- inception
local expected = {
  {line = 1, token = [[<linecomment>]], lexeme = [[// Line comment\n]]},
  {line = 3, token = [[<blockcomment>]], lexeme = [[/* Block comment */]]},
  {line = 5, token = [[<blockcomment>]], lexeme = [[/* Block comment\nspawning multiple\nlines */]]},
  {line = 9, token = [[<freeform>]], lexeme = [=[[{\n  Free\n  form\n  block\n\n  [{ Nested freeform block }]\n}]]=]},
  {line = 17, token = [[<id>]], lexeme = [[id]]},
  {line = 18, token = [[<id>]], lexeme = [[Id]]},
  {line = 19, token = [[<id>]], lexeme = [[iD]]},
  {line = 20, token = [[<id>]], lexeme = [[ID]]},
  {line = 21, token = [[<id>]], lexeme = [[_id]]},
  {line = 22, token = [[<id>]], lexeme = [[id_]]},
  {line = 23, token = [[<id>]], lexeme = [[_]]},
  {line = 25, token = [[<decimal>]], lexeme = [[0]]},
  {line = 26, token = [[<hexadecimal>]], lexeme = [[0xabcd]]},
  {line = 27, token = [[<hexadecimal>]], lexeme = [[0xABCD]]},
  {line = 28, token = [[<octal>]], lexeme = [[0123]]},
  {line = 29, token = [[<decimal>]], lexeme = [[123]]},
  {line = 30, token = [[<float>]], lexeme = [[1.]]},
  {line = 31, token = [[<float>]], lexeme = [[.1]]},
  {line = 32, token = [[<float>]], lexeme = [[1.1]]},
  {line = 33, token = [[<float>]], lexeme = [[1.f]]},
  {line = 34, token = [[<float>]], lexeme = [[.1f]]},
  {line = 35, token = [[<float>]], lexeme = [[1.1f]]},
  {line = 36, token = [[<float>]], lexeme = [[1.e1]]},
  {line = 37, token = [[<float>]], lexeme = [[.1e1]]},
  {line = 38, token = [[<float>]], lexeme = [[1.1e1]]},
  {line = 39, token = [[<float>]], lexeme = [[1.e1f]]},
  {line = 40, token = [[<float>]], lexeme = [[.1e1f]]},
  {line = 41, token = [[<float>]], lexeme = [[1.1e1f]]},
  {line = 42, token = [[<float>]], lexeme = [[1.l]]},
  {line = 43, token = [[<decimal>]], lexeme = [[1u]]},
  {line = 44, token = [[<decimal>]], lexeme = [[1ul]]},
  {line = 45, token = [[<decimal>]], lexeme = [[1ull]]},
  {line = 46, token = [[<decimal>]], lexeme = [[1l]]},
  {line = 47, token = [[<decimal>]], lexeme = [[1lu]]},
  {line = 48, token = [[<decimal>]], lexeme = [[1llu]]},
  {line = 49, token = [[<decimal>]], lexeme = [[1ULL]]},
  {line = 51, token = [[<string>]], lexeme = [[""]]},
  {line = 52, token = [[<string>]], lexeme = [["\""]]},
  {line = 53, token = [[<string>]], lexeme = [["\a\b\f\n\r\t\v\\\'\"\?\xab\xAB\uabcd\uABCD\U0123abcd\U0123ABCD\033"]]},
  {line = 54, token = [[<widestring>]], lexeme = [[L"wchar_t string"]]},
  {line = 55, token = [[<utf8string>]], lexeme = [[u8"UTF-8 encoded string"]]},
  {line = 56, token = [[<utf16string>]], lexeme = [[u"char16_t string"]]},
  {line = 57, token = [[<utf32string>]], lexeme = [[U"char32_t string"]]},
  {line = 58, token = [[<rawstring>]], lexeme = [[R"raw(raw (!) string)raw"]]},
  {line = 59, token = [[<rawwidestring>]], lexeme = [[LR"raw(raw (!) wchar_t string)raw"]]},
  {line = 60, token = [[<rawwidestring>]], lexeme = [[RL"raw(raw (!) wchar_t string)raw"]]},
  {line = 61, token = [[<rawutf16string>]], lexeme = [[uR"raw(raw (!) char16_t string)raw"]]},
  {line = 62, token = [[<rawutf16string>]], lexeme = [[Ru"raw(raw (!) char16_t string)raw"]]},
  {line = 63, token = [[<rawutf32string>]], lexeme = [[UR"raw(raw (!) char32_t string)raw"]]},
  {line = 64, token = [[<rawutf32string>]], lexeme = [[RU"raw(raw (!) char32_t string)raw"]]},
  {line = 65, token = [[<char>]], lexeme = [['char character']]},
  {line = 66, token = [[<widechar>]], lexeme = [[L'wchar_t character']]},
  {line = 67, token = [[<utf8char>]], lexeme = [[u8'UTF-8 encoded character']]},
  {line = 68, token = [[<utf16char>]], lexeme = [[u'char16_t character']]},
  {line = 69, token = [[<utf32char>]], lexeme = [[U'char32_t character']]},
  {line = 69, token = [[<eof>]], lexeme = [[<eof>]]},
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
