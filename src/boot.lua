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

  local main = assert(loadfile(args[1], 't'))()
  table.remove(args, 1)
  main(args)
end
