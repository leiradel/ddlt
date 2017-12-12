local ddlt = require 'ddlt'

local parse
parse = function(file)
  local keywords = {
    enum = true
  }

  local symbols = {
    ['{'] = true,
    ['}'] = true,
    [','] = true,
    [';'] = true
  }

  local parser = {
    new = function(self, file)
      self.file = file

      local input, err = io.open(self.file)

      if not input then
        self:error(0, err)
      end

      local source = input:read('*a')
      input:close()

      self.lexer = ddlt.newLexer{
        source = source,
        file = self.file,
        language = 'cpp',
        isSymbol = function(lexeme) return symbols[lexeme] end
      }

      self.defs = {}
      self.la = {}
      self:next()
    end,

    error = function(self, line, ...)
      local args = {...}
      --error(string.format('%s:%d: %s\n', self.file, line, table.concat(args, '')))
      io.stderr:write(string.format('%s:%d: %s\n', self.file, line, table.concat(args, '')))
      os.exit(1)
    end,

    next = function(self)
      local la, err = self.lexer:next(self.la)

      if not la then
        self:error(self.la.line, err);
      end

      if keywords[self.la.lexeme] then
         self.la.token = self.la.lexeme
      end

      if self.la.token == '<decimal>' then self:next() end
    end,

    match = function(self, token)
      if token and token ~= self.la.token then
        self:error(self.la.line, token, ' expected, ', self.la.token, ' found')
      end

      self:next()
    end,

    parse = function(self)
      self:parseEnums()
      self:match('<eof>')
      return self.defs
    end,

    parseEnums = function(self)
      while self.la.token == 'enum' do
        self.defs[#self.defs + 1] = self:parseEnum()
      end
    end,

    parseEnum = function(self)
      self:match('enum')
      
      local struct = {
        name = self.la.lexeme,
        line = self.la.line
      }

      self:match('<id>')
      self:match('{')

      struct.fields = self:parseFields()

      self:match('}')
      self:match(';')

      return struct
    end,

    parseFields = function(self)
      local fields = {}

      while true do
        fields[#fields + 1] = self:parseField()

        if self.la.token ~= ',' then
          break
        end

        self:match()
      end

      return fields
    end,

    parseField = function(self)
      local str = self.la.lexeme
      self:match('<string>')
      return str
    end
  }

  parser:new(file)
  return parser:parse()
end

local template = [[
/*!
local function djb2(str)
  local hash = 5381

  for i = 1, #str do
    hash = (hash * 33 + str:byte(i, i)) & 0xffffffff
  end

  return hash
end
*/

/*! for _, enum in ipairs(args) do */
enum class /*= enum.name */ {
  /*! for _, field in pairs(enum.fields) do */
  /*= field:sub(2, -2):gsub('%s', '_') */ = /*= string.format('0x%x', djb2(field)) */,
  /*! end */
};
/*! end */
]]

return function(args)
  if #args ~= 1 then
    io.stderr:write('error: missing input file\n')
    os.exit(1)
  end

  local defs = parse(args[1])

  local templ = ddlt.newTemplate(template)
  templ = assert(load(templ, 'template'))()
  templ(defs, io.write)
end
