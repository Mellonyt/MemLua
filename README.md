# How to install

To use MemLua in your project, extract the MemLua zip, and place the extracted
folder in your project, along with the other C++ files.
Also, include my Lua copy if you don't already have Lua added, or, replace it
if your lua isn't working with mine.

It's been currently tested for Lua version 5.1-5.2 (mine may be 5.1.4)

and now, include "memlua.h" at the top of your project

I also recommend VS 2017 or VS 2019,
and support for wininet which should already come
with the default packages


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
- Fast and efficient x86 disassembly (with text translation)
- Extremely fast memory scanning
- Incredibly easy debugging, using page exceptions
- Safe, threaded trampoline detours
- Calling convention handling
- HTTP get for loading lua files from a server
- File reading/writing


To run the MemLua LBI, simply place this in whatever your main DLL function is:

```
MemLua::init();
MemLua::loadhttp("https://github.com/thedoomed/MemLua/blob/master/memlua_lbi.lua?raw=true");
```
(bytecode_example.bin may have to be updated)
Once it loads, you can chat scripts through the chat bar in any ROBLOX game and have it execute.
