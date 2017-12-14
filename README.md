# ddlt

**ddlt** is a generic lexer to help write parsers using [Lua](https://www.lua.org/). It includes a tokenizer capable of recognizing **C++** and **BASIC** comments, identifiers, and number and string literals. A template engine is also included to easy the development of transpilers.

The tokenizer recognizes:

* **C++**
  * Line comments from `//` to the end of the line.
  * Block comments from `/*` to `*/`. Nested comments are *not* supported.
  * Identifiers in the form `[A-Za-z_][A-Za-z_0-9]*`.
  * Numbers in the form:
    * `0x[0-9A-Fa-f]+` as hexadecimal literals
    * `[0-9]+` as decimal literals
    * `0[0-7]+` as octal literals
    * `[0-9]*\.[0-9]+([Ee][+-]?[0-9]+)?` as float literals
    * `[0-9]+\.[0-9]*([Ee][+-]?[0-9]+)?` as float literals
    * `[0-9]+[Ee][+-]?[0-9]+` as float literals
    * Integer literals can be suffixed with `u`, `ul`, `ull`, `l`, `lu`, `ll`, or `llu`, in either lower and upper case
    * Float literals can be suffixed with `f` or `l`, in either lower and upper case
  * Strings, including the following [escape sequences](https://en.wikipedia.org/wiki/Escape_sequences_in_C):
    * `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, `\\`, `\'`, `\"`, `\?`
    * `\x` followed by at least one hexadecimal digit
    * `\u` followed by exactly four hexadecimal digits
    * `\U` followed by exactly eight hexadecimal digits
    * A `\` followed by at least one octal digit
* **BASIC**
  * Line comments from `'` to the end of the line.
  * Line comments from `REM`, independent of case, to the end of the line.
  * Identifiers in the form `[A-Za-z_][A-Za-z_0-9]*`.
  * Numbers in the form:
    * `&[Bb][01]+` as binary literals
    * `&[Oo][0-7]+` as octal literals
    * `&[Hh][0-9A-Fa-f]+` as hexadecimal literals
    * `[0-9]+` as decimal literals
    * `[0-9]*\.[0-9]+([Ee][+-]?[0-9]+)?` as float literals
    * `[0-9]+\.[0-9]*([Ee][+-]?[0-9]+)?` as float literals
    * `[0-9]+[Ee][+-]?[0-9]+` as float literals
    * Integer literals can be suffixed with `%`, `&`, `s`, `us`, `i`, `ui`, `l`, or `ul`, in either lower and upper case
    * Float literals can be suffixed with `@`, `!`, `#`, `f`, `r`, or `d`, in either lower and upper case
  * Strings, where `""` is interpreted as a single quote inside the string.

The tokenizer can also recognize and return *freeform* blocks, using user-defined delimiters which can have any content inside these delimiters.

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

local parse = function(file)
  local inp = assert(io.open(file, 'rb'))
  local source = inp:read('*a')
  inp:close()

  local lexer = ddlt.newLexer{
    source = source,
    file = file,
    language = 'cpp',
    symbols = {'{', '}', ',', ';', '='},
    keywords = {'enum', 'struct', 'string', 'int', 'float'},
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

local template = [[
/*! local tkfmt = '%-' .. args.max .. 's' */
/*! for i = 1, #args do */
/*!  local la = args[ i ] */
line = /*= string.format('%3d', la.line) */ token = /*= string.format(tkfmt, la.token) */ lexeme = /*= la.lexeme */
/*! end */
]]

return function(args)
  if #args ~= 2 then
    error('missing input file\n')
  end

  local res = {}
  local tokens = parse(args[2])
  local templ = assert(ddlt.newTemplate(template, '/*', '*/'))
  templ(tokens, function(out) res[#res + 1] = out end)

  res = table.concat(res):gsub('\n+', '\n')
  io.write(res)
end
```

and you feed a file with the following contents to it

```C
// The weapons available in the game
enum Weapons {
  kFist,
  kChainsaw,
  kPistol,
  kShotgun,
  kChaingun,
  kRocketLauncher,
  kPlasmaGun,
  kBFG9000
};

/* The player */
struct Hero {
  string name = "John \"Hero\" Doe";
  int health = 100;
  int armour = 0x0;
  float speed = 14.3;
  isAlive = [{
    return true;
  }]
};
```

the result will be

```
line =   2 token = [enum]        lexeme = enum
line =   2 token = <id>          lexeme = Weapons
line =   2 token = {             lexeme = {
line =   3 token = <id>          lexeme = kFist
line =   3 token = ,             lexeme = ,
line =   4 token = <id>          lexeme = kChainsaw
line =   4 token = ,             lexeme = ,
line =   5 token = <id>          lexeme = kPistol
line =   5 token = ,             lexeme = ,
line =   6 token = <id>          lexeme = kShotgun
line =   6 token = ,             lexeme = ,
line =   7 token = <id>          lexeme = kChaingun
line =   7 token = ,             lexeme = ,
line =   8 token = <id>          lexeme = kRocketLauncher
line =   8 token = ,             lexeme = ,
line =   9 token = <id>          lexeme = kPlasmaGun
line =   9 token = ,             lexeme = ,
line =  10 token = <id>          lexeme = kBFG9000
line =  11 token = }             lexeme = }
line =  11 token = ;             lexeme = ;
line =  14 token = [struct]      lexeme = struct
line =  14 token = <id>          lexeme = Hero
line =  14 token = {             lexeme = {
line =  15 token = [string]      lexeme = string
line =  15 token = <id>          lexeme = name
line =  15 token = =             lexeme = =
line =  15 token = <string>      lexeme = "John \"Hero\" Doe"
line =  15 token = ;             lexeme = ;
line =  16 token = [int]         lexeme = int
line =  16 token = <id>          lexeme = health
line =  16 token = =             lexeme = =
line =  16 token = <decimal>     lexeme = 100
line =  16 token = ;             lexeme = ;
line =  17 token = [int]         lexeme = int
line =  17 token = <id>          lexeme = armour
line =  17 token = =             lexeme = =
line =  17 token = <hexadecimal> lexeme = 0x0
line =  17 token = ;             lexeme = ;
line =  18 token = [float]       lexeme = float
line =  18 token = <id>          lexeme = speed
line =  18 token = =             lexeme = =
line =  18 token = <float>       lexeme = 14.3
line =  18 token = ;             lexeme = ;
line =  19 token = <id>          lexeme = isAlive
line =  19 token = =             lexeme = =
line =  21 token = <freeform>    lexeme = [{\n    return true;\n  }]
line =  22 token = }             lexeme = }
line =  22 token = ;             lexeme = ;
line =  23 token = <eof>         lexeme = <eof>
```

See the `example` folder for unit tests and a simple generator written using **ddlt**.

## Documentation

Your parser can [require](https://www.lua.org/manual/5.3/manual.html#pdf-require) **ddlt** to access functions to tokenize an input source code, and to create templates to generate code, as well as some functions to help deal with the file system.

* `canonic = realpath(path)`: returns the canonicalized absolute pathname
* `dir, name, ext = split(path)`: splits a path into its constituents, dir, file name, and extension
* `entries = scandir(path)`: returns a table with all the entries in the specified path
* `info = stat(path)`: returns a table with information about the object at path, as returned by [stat](https://linux.die.net/man/2/stat) containing `size`, `atime`, `mtime`, `ctime`, `sock`, `link`, `file`, `block`, `dir`, `char`, and `fifo`
* `lexer = newLexer(options)`: returns a new tokenizer (see below)
* `template = newTemplate(code, open_tag, close_tag, name)`: returns a function that, when called, will execute the template (see below)

### newLexer

`newLexer` returns a tokenizer for a given soure code. It accepts a table with the following fields:

* `source`: a string with the entire source code that will be tokenized.
* `file`: a string with the name of the object used to create the source code (usually the file name from where the source code was read, this is used for error messages).
* `isSymbol`: a function which takes a lexeme and must return `true` if that lexeme is a valid symbol.
* `language`: a string containing the language used to parse identifiers, string and number literals, and comments. Supported languages are `'cpp'` for **C++**, and `'bas'` for **BASIC**.
* `freeform`: an array with two elements, the *freeform* delimiters to recognize freeform blocks.

Optionally, the table can have these fields:

* `symbols`: an array of valid symbols, which will be used to automatically provide an `isSymbol` function to the tokenizer.
* `keywords`: an array of valid keywords, which will be returned instead of the generic `<id>` token.
  * Keyword tokens will be returned enclosed in square brackets to avoid confusion with other tokens, i.e. if a `string` keyword is defined, it will be returned with the `[string]` token to make it different from the `<string>` one which identifies a string literal

The resulting object only has one method, `next`. It takes a table where the information of the next produced token is stored:

* `token`: a string describing the lookahead. It can be:
  * `<id>` when the lookahead is an identifier
  * `<float>` when it's a floating point literal
  * `<binary>` when it's a binary literal (**BASIC** only)
  * `<octal>` when it's an octal literal
  * `<decimal>` when it's a decimal literal
  * `<hexadecimal>` when it's a hexadecimal literal
  * `<string>` when it's a string literal
  * `<eof>` when there are no more tokens in the source code
  * A symbol, as identified via the `isSymbol` function
  * A keyword enclosed in square brackets
* `lexeme`: a string with the value of the token as found in the source code
* `line`: the line number where the token is in the source code

`next` will also return the same table passed to it as an argument if successful. In case of errors, it will return `nil`, plus a string describing the error. The error message will always be in the format `<file>:<line>: message`, which is the standard way to describe errors in compilers.

### newTemplate

Templates can be used to make it easier to generate code. The `newTemplate` method accepts three of four arguments:

* `newTemplate(code, open_tag, close_tag, name)`
  * The template source code.
  * The open tag that delimits special template instructions.
  * The close tag.
  * An optional template name, which is used in error messages. `"template"` is used if this argument is not provided.

There are two template instructions, one to emit content to the output, and another to execute arbitrary Lua code. To emit content, use the open tag followed by `=`. To execute code, use the open tag followed by `!`.

As an example, if you use `/*` and `*/` as delimiters:

* `/*= ... */` causes `...` to be generated in the output
* `/*! ... */` causes `...` to be executed as Lua code

The return value of `newTemplate` is a Lua function that will run the template when executed. This returned function accepts two arguments, `args`, which is used to send arbitrary data to the template, including the result of your parser, and `emit`, a function which must output all the arguments passed to it as a vararg.

## Changelog

### 1.0.0

* First proper release
