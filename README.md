# ddlt

**ddlt** is a generic, C-like lexer to help write parsers using [Lua](https://www.lua.org/).

## Build

`make` should do the job.

## Usage

```
$ ./ddlt
ddlt: a generic, C-like lexer to help write parsers using Lua
Copyright 2017 Andre Leiradella @leiradel

Usage: ddlt <parser.lua> [args...]

ddlt runs the Lua script given as its first argument, and executes the
function returned by that script. All arguments but the first are passed to
that function.
```

If your parser code is

```Lua
local ddlt = require 'ddlt'

return function(args)
  local file = assert(io.open(args[1]))
  local source = file:read('*a')
  file:close()

  local lexer = ddlt.newLexer{
    source = source,
    file = args[1],
    isSymbol = function(s) return s == '{' or s == '}' or s == ',' or s == ';' end
  }

  local token
  local la = {}

  repeat
    lexer:next(la)
    token = la.token
    print(token, la.line, la.lexeme)
  until token == '<eof>'
end
```

and you feed a file with the following contents to it

```C
enum Weapons {
  "Fist",
  "Chainsaw",
  "Pistol",
  "Shotgun",
  "Chaingun",
  "Rocket Launcher",
  "Plasma Gun",
  "BFG9000"
};
```

the result will be (formatted for easier visualization)

```
<id>     1.0  enum
<id>     1.0  Weapons
{        1.0  {
<string> 2.0  "Fist"
,        2.0  ,
<string> 3.0  "Chainsaw"
,        3.0  ,
<string> 4.0  "Pistol"
,        4.0  ,
<string> 5.0  "Shotgun"
,        5.0  ,
<string> 6.0  "Chaingun"
,        6.0  ,
<string> 7.0  "Rocket Launcher"
,        7.0  ,
<string> 8.0  "Plasma Gun"
,        8.0  ,
<string> 9.0  "BFG9000"
}        10.0 }
;        10.0 ;
<eof>    10.0 <eof>
```

See `example/enum.lua` for a simple parser written using **ddlt**.

## Documentation

Your parser can [require](https://www.lua.org/manual/5.3/manual.html#pdf-require) **ddlt** to access functions to tokenize an input source code, and to create templates to generate code, as well as some functions to help deal with the file system.

* `canonic = realpath(path)`: returns the canonicalized absolute pathname
* `dir, name, ext = split(path)`: splits a path into its constituents, dir, file name, and extension
* `entries = scandir(path)`: returns a table with all the entries in the specified path
* `info = stat(path)`: returns a table with information about the object at path, as returned by [stat](https://linux.die.net/man/2/stat) containing `size`, `atime`, `mtime`, `ctime`, `sock`, `link`, `file`, `block`, `dir`, `char`, and `fifo`
* `lexer = newLexer(options)`: returns a new tokenizer (see below)
* `template = newTemplate(source)`: returns a compiled template (see below)

### newLexer

`newLexer` returns a tokenizer for a given soure code. It accepts a table with the following fields:

* `source`: a string with the entire source code that will be tokenized
* `file`: a string with the name of the object used to create the source code (usually the file name from where the source code was read, this is used for error messages)
* `isSymbol`: a function which takes a lexeme and must return `true` if that lexeme is a valid symbol

The resulting object only has one method, `next`. It takes a table where the information of the next produced token is stored:

* `token`: a string describing the lookahead. It can be:
  * `<id>` when the lookahead is an identifier
  * `<float>` when it's a floating point literal
  * `<octal>` when it's an octal literal
  * `<decimal>` when it's a decimal literal
  * `<hexadecimal>` when it's a hexadecimal literal
  * `<string>` when it's a string literal
  * `<eof>` when there are no more tokens in the source code
  * A symbol, as identified via the `isSymbol` function
* `lexeme`: a string with the value of the token as found in the source code
* `line`: the line number where the token is in the source code

`next` will also return the same table passed to it as an argument if successful. In case of errors, it will return `nil`, plus a string describing the error. The error message will always be in the format `<file>:<line>: message`, which is the standard way to describe errors in compilers.

Care has been taken to correctly recognize C strings, including all escape sequences, identifiers, number literals, and comments, both line and block.

### newTemplate

Templates can be used to make it easier to generate code. The template engine used in **ddlt** uses C comments to drive the code generation:

* `/*= ... */` causes `...` to be generated in the output
* `/*! ... */` causes `...` to be executed as Lua code

`newTemplate` accepts annotated code and returns a string containing Lua source code that will tun the template when executed. This function accepts two arguments, `defs`, which should be the result of your parser, and `emit` a function which must output all the arguments passed to it as a vararg.