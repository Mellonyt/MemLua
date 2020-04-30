# What is MemLua?
MemLua is a powerful memory-editing library for Lua, aimed towards DLL applications.
It may be used for its minimalistic, blissful exploiting experience, or for its
portability and ability to include exploits as if they're 'plugins'

It was designed to replicate the Lua Engine of Cheat Engine, but being
that it is built around DLL's, it can offer a much wider range of capablities.
It can be used for various applications that call for run-time debugging
and analysis, making tedious and time-consuming tasks as easy as one line.

I would also like to point out that it's far from done.
I'll be pushing updates continually to expand its capabilities.

Features include:
- Fast and efficient x86 disassembly/translation
- Fast memory scanning
- Safe, threaded detours/"trampolines"
- Automatic debug hook
- Automatic calling conventions
- HTTP get for loading lua files remotely
- File reading/writing

To use MemLua in your project, all you need are the
MemLua header files(there are 4 of them),
which are as follows:
`memlua.h`
`memscan.h`
`disassembler.h`
`httpreader.h`

You will need the standard Lua API included in your project. (currently tested for version 5.1-5.2)

I would recommend VS 2017 or VS 2019,
and support for wininet which should already come
with the default install packages


To run the MemLua roblox LBI, which allows you to run scripts by chatting them -- c/print("hi")
simply place this in your main function (DLL):

```
MemLua::init();
MemLua::loadhttp("https://github.com/thedoomed/MemLua/blob/master/memlua_lbi.lua?raw=true");
```
(sorry xD wait till I update bytecode_example.bin)
