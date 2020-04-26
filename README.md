# What is MemLua?
MemLua is a powerful memory-editing library for Lua, aimed towards DLL applications.
It may be used for extremely simplistic exploiting or for debug/analysis of any project at run-time.

It was designed to replicate the Lua Engine of Cheat Engine, but being
that it is built around DLL's, it can offer a much wider range of capablities.
It can be used for high-quality exploit development,
making tedious time-consuming tasks as easy as one line.

Features include:
- Fast and efficient x86 disassembly/translation
- Fast memory scanning
- Safe, threaded detours/"trampolines"
- Automatic debug hook
- Automatic calling conventions
- HTTP get for utilizing website data
- File reading/writing

To use MemLua in your project, all you need are the
MemLua header files, and the standard Lua API
(currently tested for lua version 5.1-5.2)


