return function(t)
local o=t.newLexer
t.newLexer=function(a)
local e={}
for t,a in pairs(a)do
e[t]=a
end
if e.symbols then
local t={}
for a=1,#e.symbols do
t[e.symbols[a]]=true
end
e.symbols=t
end
local a,o=o(e)
if not a then
return nil,o
end
if e.keywords then
local t={}
for a=1,#e.keywords do
t[e.keywords[a]]=true
end
return{
next=function(o,e)
local e,a=a:next(e)
if not e then
return nil,a
end
if e.token=='<id>'and t[e.lexeme]then
e.token=e.lexeme
end
return e
end
}
else
return a
end
end
t._VERSION='2.7.0'
t._COPYRIGHT='Copyright (C) 2017-2018 Andre Leiradella'
t._DESCRIPTION='A generic lexer to help writing parsers using Lua'
t._URL='https://github.com/leiradel/ddlt'
return t
end
