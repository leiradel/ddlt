local ddlt = require 'ddlt'

local function dump(t, i)
  i = i or ''

  if type(t) == 'table' then
    io.write(string.format('%s%s {\n', i, tostring(table)))
    for k, v in pairs(t) do
      io.write(string.format('%s  %s = ', i, tostring(k)))

      if type(v) == 'table' then
        io.write('\n')
        dump(v, i .. '  ')
      else
        io.write(string.format('%s', tostring(v)))
      end

      io.write('\n')
    end
    io.write(string.format('%s}\n', i))
  else
    io.write(string.format('%s%s', i, tostring(t)))
  end
end

local function fatal(path, line, format, ...)
  local location = string.format('%s:%u: ', path, line)
  local message = string.format(format, ...)
  error(string.format('%s%s', location, message))
  io.stderr:write(location, message, '\n')
  os.exit(1)
end

-------------------------------------------------------------------------------
-- Tokenizes the source code
-------------------------------------------------------------------------------
local function tokenize(path)
  local function getTokens(path, source, startLine)
    local lexer = ddlt.newLexer{
      source = source,
      startline = startLine,
      file = path,
      language = 'cpp',
      symbols = {'=>', '(', ')', ';', ','},
      keywords = {'header', 'fsm', 'class', 'as', 'before', 'after', 'allow', 'forbid'},
      freeform = {{'{', '}'}}
    }

    local tokens = {}
    local la
  
    repeat
      repeat
        local err
        la, err = lexer:next({})

        if err then
          io.stderr:write(err)
          os.exit(1)
        end
      until la.token ~= '<linecomment>' and la.token ~= '<blockcomment>'

      tokens[#tokens + 1] = la
    until la.token == '<eof>'

    return tokens
  end
  
  local inp, err = io.open(path, 'rb')

  if not inp then
    fatal(path, 0, 'Error opening input file: %s', err)
  end

  local source = inp:read('*a')
  inp:close()

  -- Get the top-level tokens
  local tokens = getTokens(path, source, 1)

  -- Tokenize the header and fsm freeform blocks
  local i = 1

  while i < #tokens do
    local la0 = tokens[i]
    local la1 = tokens[i + 1]
    local la2 = tokens[i + 2]

    if la0.token == 'fsm' and la1.token == '<id>' and la2.token == '<freeform>' then
      local freeform = getTokens(path, la2.lexeme:sub(2, -2), la2.line)
      tokens[i + 2] = {line = la2.line, token = '{', lexeme = '{'}

      for j = 1, #freeform - 1 do
        table.insert(tokens, i + 3, freeform[j])
        i = i + 1
      end

      table.insert(tokens, i + 3, {line = la2.line, token = '}', lexeme = '}'})
      i = i + 4
    else
      i = i + 1
    end
  end

  -- Fixes the state freeform blocks
  local i = 1

  while i < #tokens do
    local la0 = tokens[i]
    local la1 = tokens[i + 1]

    if la0.token == '<id>' and la1.token == '<freeform>' then
      local freeform = getTokens(path, la1.lexeme:sub(2, -2), la1.line)
      tokens[i + 1] = {line = la1.line, token = '{', lexeme = '{'}

      for j = 1, #freeform - 1 do
        table.insert(tokens, i + 2, freeform[j])
        i = i + 1
      end

      table.insert(tokens, i + 2, {line = la1.line, token = '}', lexeme = '}'})
      i = i + 3
    else
      i = i + 1
    end
  end

  for i = 1, #tokens do
    local la = tokens[i]
  end

  return tokens
end

-------------------------------------------------------------------------------
-- Creates a new parser with predefined error, match and line methods
-------------------------------------------------------------------------------
local function newParser(path)
  local parser = {
    init = function(self)
      self.tokens = tokenize(path)
      self.current = 1
    end,

    error = function(self, line, format, ...)
      fatal(path, line, format, ...)
    end,

    token = function(self, index)
      index = index or 1
      return self.tokens[self.current + index - 1].token
    end,

    lexeme = function(self, index)
      index = index or 1
      return self.tokens[self.current + index - 1].lexeme
    end,

    line = function(self, index)
      index = index or 1
      return self.tokens[self.current + index - 1].line
    end,

    match = function(self, token)
      if token and token ~= self:token() then
        self:error(self:line(), '"%s" expected, found "%s"', token, self:token())
      end

      self.current = self.current + 1
    end,
  }

  parser:init()
  return parser
end

-------------------------------------------------------------------------------
-- Parses a state definition
-------------------------------------------------------------------------------
local function parseState(source, path, startLine, fsm, state)
  local parser = newParser(source, path, startLine)


  return parser:parse()
end

local function parseFsm(path)
  local parser = newParser(path)

  parser.parse = function(self)
    local fsm = {states = {}}

    -- Parse directives and statements that will be copied verbatim to the
    -- generated header
    if self:token() == 'header' then
      self:match()
      fsm.header = self:lexeme():sub(2, -2)
      self:match('<freeform>')
    else
      fsm.header = ''
    end

    self:match('fsm')
    fsm.id = self:lexeme()
    fsm.line = self:line()
    self:match('<id>')
    self:match('{')

    -- Get the name of the class that the fsm will control
    self:match('class')
    fsm.class = self:lexeme()
    self:match('<id>')

    -- Get the name of the field that holds the reference to the class instance
    self:match('as')
    fsm.ctx = self:lexeme()
    self:match('<id>')
    self:match(';')

    -- Parse the pre and pos actions
    while self:token() == 'before' or self:token() == 'after' do
      local event = self:token()

      if fsm[event] then
        self:error(self:line(), 'duplicated event "%s" in fsm', event)
      end

      self:match()
      fsm[event] = self:lexeme():sub(2, -2)
      self:match('<freeform>')
    end

    -- Parse all the states in the fsm
    while self:token() == '<id>' do
      self:parseState(fsm, state)
    end

    self:match('}')
    self:match('<eof>')

    -- Sort states and transitions by id to allow for better git diffs
    local states = {}

    for _, state in pairs(fsm.states) do
      states[#states + 1] = state
      states[state.id] = state

      local transitions = {}

      for _, transition in pairs(state.transitions) do
        transitions[#transitions + 1] = transition
        transitions[transition.id] = transition
      end

      table.sort(transitions, function(e1, e2) return e1.id < e2.id end)
      state.transitions = transitions
    end

    table.sort(states, function(e1, e2) return e1.id < e2.id end)
    fsm.states = states

    return fsm
  end

  parser.parseState = function(self, fsm, state)
    local state = {transitions = {}, id = self:lexeme(), line = self:line()}
    self:match('<id>')

    if fsm.states[state.id] then
      self:error(state.line, 'duplicated state "%s" in "%s"', state.id, fsm.id)
    end

    fsm.states[state.id] = state

    -- Set the first state as the initial state
    if not fsm.begin then
      fsm.begin = state.id
    end

    -- Parse the state
    self:match('{')

    while self:token() == 'before' or self:token() == 'after' do
      local event = self:token()

      if state[event] then
        self:error(self:line(), 'duplicated event "%s" in "%s"', event, state.id)
      end

      self:match()
      self:match('{')
      state[event] = self:lexeme()
      self:match('<freeform>')
      self:match('}')
    end

    while self:token() == '<id>' do
      self:parseTransition(fsm, state)
    end

    self:match('}')
  end

  parser.parseTransition = function(self, fsm, state)
    -- Get the transition id
    local transition = {id = self:lexeme(), line = self:line()}
    self:match('<id>')

    if state.transitions[transition.id] then
      self:error(transition.line, 'duplicated transition "%s" in "%s"', transition.id, state.id)
    end

    state.transitions[transition.id] = transition
    transition.parameters = self:parseParameters()

    if self:token(3) ~= '(' then
      -- It's a direct transition to another state
      self:match('=>')
      transition.type = 'state'
      transition.target = {id = self:lexeme(), line = self:line()}
      self:match('<id>')

      -- Check for a precondition for the transition
      if self:token() == '<freeform>' and self:lexeme():sub(1, 1) == '{' then
        transition.precondition = self:lexeme():sub(2, -2):gsub('allow', 'return true'):gsub('forbid', 'return false')
        self:match('<freeform>')
      else
        self:match(';')
      end
    else
      -- It's a sequence of transitions that'll arrive on another state
      transition.type = 'sequence'

      local steps = {}
      transition.steps = steps

      -- Parse the other transitions in the sequence
      while self:token() == '=>' do
        self:match()

        local step = {id = self:lexeme(), line = self:line()}
        self:match('<id>')
        steps[#steps + 1] = step

        step.arguments = self:parseArguments()
      end

      self:match(';')
    end
  end

  parser.parseParameters = function(self)
    self:match('(')
    local parameters = {}

    if self:token() ~= ')' then
      while true do
        local parameter = {}
        parameters[#parameters + 1] = parameter

        parameter.type = self:lexeme()
        self:match('<id>')

        parameter.id = self:lexeme()
        parameter.line = self:line()
        self:match('<id>')

        if self:lexeme() == ')' then
          break
        end

        self:match(',')
      end
    end

    self:match(')')
    return parameters
  end

  parser.parseArguments = function(self)
    self:match('(')
    local arguments = {}

    if self:token() ~= ')' then
      while true do
        local argument = {}
        arguments[#arguments + 1] = argument

        argument.id = self:lexeme()
        argument.line = self:line()
        self:match('<id>')

        if self:lexeme() == ')' then
          break
        end

        self:match(',')
      end
    end

    self:match(')')
    return arguments
  end

  return parser:parse()
end

local function validate(fsm, path)
  local function walk(fsm, state, id)
    local transition = state.transitions[id]
  
    if not transition then
      fatal('', 0, '"Unknown transition "%s"', id)
    end
  
    if transition.type == 'state' then
      state = fsm.states[transition.target.id]
  
      if not state then
        fatal('', 0, '"Unknown state "%s"', transition.target.id)
      end
    elseif transition.type == 'sequence' then
      for _, step in ipairs(transition.steps) do
        state = walk(fsm, state, step.id)
      end
    end
  
    return state
  end
  
  for id, state in pairs(fsm.states) do
    for id, transition in pairs(state.transitions) do
      if transition.type == 'sequence' then
        transition.target = walk(fsm, state, transition.id)
      end

      if not fsm.states[transition.target.id] then
        fatal(path, transition.target.line, '"Unknown state "%s"', transition.target.id)
      end
    end
  end
end

local function emit(fsm, path)
  local dir, name = ddlt.split(path)

  -- Emit the header
  local out = assert(io.open(ddlt.join(dir, name, 'h'), 'w'))

  out:write('#pragma once\n\n')
  out:write(fsm.header, '\n')
  out:write('class ', fsm.class, ';\n\n')
  out:write('class ', fsm.id, ' {\n')
  out:write('public:\n')

  out:write('  enum class State {\n')
  for _, state in ipairs(fsm.states) do
    out:write('    ', state.id, ',\n')
  end
  out:write('  };\n\n')

  out:write('  ', fsm.id, '(', fsm.class, '& slave): ', fsm.ctx,'(slave), __state(State::', fsm.begin, ') {}\n\n')
  out:write('  State currentState() const { return __state; }\n\n')

  local allTransitions = {}
  local visited = {}

  for _, state in ipairs(fsm.states) do
    for _, transition in ipairs(state.transitions) do
      if not visited[transition.id] then
        visited[transition.id] = true
        allTransitions[#allTransitions + 1] = transition
      end
    end
  end

  table.sort(allTransitions, function(e1, e2) return e1.id < e2.id end)

  for _, transition in ipairs(allTransitions) do
    out:write('  bool ', transition.id, '(')
    local comma = ''

    for _, parameter in ipairs(transition.parameters) do
      out:write(comma, parameter.type, ' ', parameter.id)
      comma = ', '
    end

    out:write(');\n')
  end

  out:write('\n')
  out:write('protected:\n')
  out:write('  bool before() const;\n')
  out:write('  bool before(State state) const;\n')
  out:write('  void after() const;\n')
  out:write('  void after(State state) const;\n\n')
  out:write('  ', fsm.class, '& ', fsm.ctx, ';\n')
  out:write('  State __state;\n')
  out:write('};\n')
  out:close()

  -- Emit the code
  local out = assert(io.open(ddlt.join(dir, name, 'cpp'), 'w'))

  -- Include the necessary headers
  out:write('#include "', name, '.h"\n')
  out:write('#include "', fsm.class, '.h"\n\n')

  -- Emit the global before event
  out:write('bool ', fsm.id, '::before() const {\n')

  if fsm.before then
    out:write(fsm.before, '\n')
  end

  out:write('  return true;\n')
  out:write('}\n\n')

  -- Emit the state-specific before events
  out:write('bool ', fsm.id, '::before(State state) const {\n')
  out:write('  switch (state) {\n')

  for _, state in ipairs(fsm.states) do
    if state.before then
      out:write('    case State::', state.id, ': {\n')
      out:write(state.before, '\n')
      out:write('    }\n')
      out:write('    break;\n');
    end
  end

  out:write('    default: break;\n')
  out:write('  }\n\n')
  out:write('  return true;\n')
  out:write('}\n\n')

  -- Emit the global after event
  out:write('void ', fsm.id, '::after() const {\n')

  if fsm.after then
    out:write(fsm.after, '\n')
  end

  out:write('}\n\n')

  -- Emit the state-specific after events
  out:write('void ', fsm.id, '::after(State state) const {\n')
  out:write('  switch (state) {\n')

  for _, state in ipairs(fsm.states) do
    if state.after then
      out:write('    case State::', state.id, ': {\n')
      out:write(state.after, '\n')
      out:write('    }\n')
      out:write('    break;\n');
    end
  end

  out:write('    default: break;\n')
  out:write('  }\n')
  out:write('}\n\n')

  -- Emit the transitions
  for _, transition in ipairs(allTransitions) do
    out:write('bool ', fsm.id, '::', transition.id, '(')
    local comma = ''

    for _, parameter in ipairs(transition.parameters) do
      out:write(comma, parameter.type, ' ', parameter.id)
      comma = ', '
    end

    out:write(') {\n')
    out:write('  switch (__state) {\n')

    local valid, invalid = {}, {}

    for _, state in ipairs(fsm.states) do
      if state.transitions[transition.id] then
        valid[#valid + 1] = state
      else
        invalid[#invalid + 1] = state
      end
    end

    for _, state in ipairs(valid) do
      local transition2 = state.transitions[transition.id]

      out:write('    case State::', state.id, ': {\n')
      out:write('      if (!before()) {\n')
      out:write('#ifdef DEBUG_FSM\n')
      out:write('        ', fsm.ctx, '.printf("FSM %s:%u Failed global precondition while switching to ', transition2.target.id, '", __FUNCTION__, __LINE__);\n')
      out:write('#endif\n\n')
      out:write('        return false;\n')
      out:write('      }\n\n')
      out:write('      if (!before(__state)) {\n')
      out:write('#ifdef DEBUG_FSM\n')
      out:write('        ', fsm.ctx, '.printf("FSM %s:%u Failed state precondition while switching to ', transition2.target.id, '", __FUNCTION__, __LINE__);\n')
      out:write('#endif\n\n')
      out:write('        return false;\n')
      out:write('      }\n\n')

      if transition2.precondition then
        out:write(transition2.precondition, '\n')
      end

      if transition2.type == 'state' then
        out:write('      __state = State::', transition2.target.id, ';\n')
      elseif transition2.type == 'sequence' then
        local arguments = function(args)
          local list = {}

          for _, arg in ipairs(args) do
            list[#list + 1] = arg
          end

          return table.concat(list, ', ')
        end

        if #transition2.steps == 1 then
          out:write('      bool __ok = ', transition2.steps[1].id, '(', arguments(transition2.steps[1].arguments), ');\n')
        else
          out:write('      bool __ok = ', transition2.steps[1].id, '(', arguments(transition2.steps[1].arguments), ') &&\n')

          for i = 2, #transition2.steps do
            out:write('                  ', transition2.steps[i].id, '(', arguments(transition2.steps[i].arguments), ')', i == #transition2.steps and ';' or ' &&', '\n')
          end

          out:write('\n')
        end
      end

      if transition2.type == 'state' then
        out:write('      after(__state);\n')
        out:write('      after();\n\n')
        out:write('#ifdef DEBUG_FSM\n')
        out:write('      ', fsm.ctx, '.printf("FSM %s:%u Switched to ', transition2.target.id, '", __FUNCTION__, __LINE__);\n')
        out:write('#endif\n')
        out:write('      return true;\n')
      elseif transition2.type == 'sequence' then
        out:write('      if (__ok) {\n')
        out:write('        after(__state);\n')
        out:write('        after();\n\n')
        out:write('      }\n')
        out:write('      else {\n')
        out:write('#ifdef DEBUG_FSM\n')
        out:write('        ', fsm.ctx, '.printf("FSM %s:%u Failed to switch to ', transition2.target.id, '", __FUNCTION__, __LINE__);\n')
        out:write('#endif\n')
        out:write('      }\n\n')
        out:write('      return __ok;\n')
      end

      out:write('    }\n')
      out:write('    break;\n')

      out:write('\n')
    end

    out:write('    default: break;\n')
    out:write('  }\n\n')
    out:write('  return false;\n')
    out:write('}\n\n')
  end

  out:close()
end

if #arg ~= 1 then
  io.stderr:write('Usage: lua fsmc.lua inputfile\n')
  os.exit(1)
end

local fsm = parseFsm(arg[1])
validate(fsm, arg[1])
emit(fsm, arg[1])
