#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "disassembler.h"
#include "memscan.h"
#include "httpreader.h"
#include <fstream>

extern "C" {
	#include "Lua/lua.h"
	#include "Lua/lualib.h"
	#include "Lua/lauxlib.h"
	#include "Lua/ldo.h"
	#include "Lua/lapi.h"
}

#define lua_pushfunction(L, x, name) \
						lua_pushcfunction(L, x); \
						lua_setglobal(L, name)

#define isPrologue(x)	((x % 16 == 0) && \
						((*(BYTE*)x == 0x55 && *(WORD*)(x + 1) == 0xEC8B) || \
						(*(BYTE*)x == 0x53 && *(WORD*)(x + 1) == 0xDC8B) || \
						(*(BYTE*)x == 0x56 && *(WORD*)(x + 1) == 0xF48B)))

#define isEpilogue(x)	(*(WORD*)(x - 1) == 0xC35D || \
						(*(WORD*)(x - 1) == 0xC25D && *(WORD*)(x + 1) >= 0 && *(WORD*)(x + 1) % 4 == 0))

#define isValidCode(x)	!(*(ULONGLONG*)x == NULL && *(ULONGLONG*)(x + 8) == NULL)

#define getRel(x)		((x+*(DWORD*)(x+1)+5) % 16 == 0 ? (x+*(DWORD*)(x+1)+5) : NULL)

#define addInteger(L, name, x)\
	lua_pushinteger(L, x); \
	lua_setfield(L, -2, name)

#define addGlobalInteger(L, name, x)\
	lua_pushinteger(L, x); \
	lua_setglobal(L, name)

//
// For easy assembler code
//
#define memPushLargeArg(x, value) \
	*(BYTE*)(x++) = 0x68; \
	*(DWORD*)(x) = value; \
	x += sizeof(DWORD);

#define memPushSmallArg(x, value) \
	*(BYTE*)(x++) = 0x6A; \
	*(BYTE*)(x++) = value; \

// Places a call instruction at x, to func, plus stack clean (if specified)
#define memPlaceCall(x, func, cleanup) \
	*(BYTE*)x = 0xE8; \
	*(DWORD*)(x + 1) = (reinterpret_cast<DWORD>(func) - x) - 5; \
	x += 5; \
	if (cleanup > 0) { \
		*(BYTE*)(x++) = 0x83; \
		*(BYTE*)(x++) = 0xC4; \
		*(BYTE*)(x++) = cleanup; \
	}


BYTE toByte(const char* str) {
	// convert 2 chars to byte
	int id = 0, n = 0, byte = 0;
convert:
	if (str[id] > 0x60) n = str[id] - 0x57; // n = A-F (10-16)
	else if (str[id] > 0x40) n = str[id] - 0x37; // n = a-f (10-16)
	else if (str[id] >= 0x30) n = str[id] - 0x30; // number chars
	if (id != 0) byte += n; else {
		id++, byte += (n * 16);
		goto convert;
	}
	return byte;
}

void placeHook(DWORD address, size_t hookSize, DWORD func) {
	DWORD oldProtect;
	VirtualProtect(reinterpret_cast<void*>(address), hookSize, PAGE_EXECUTE_READWRITE, &oldProtect);

	*(BYTE*)address = 0xE9;
	*(DWORD*)(address + 1) = ((DWORD)func - address) - 5;

	// Place NOP's
	for (size_t i = 5; i < hookSize; i++) {
		*(BYTE*)(address + i) = 0x90;
	}

	VirtualProtect(reinterpret_cast<void*>(address), hookSize, oldProtect, &oldProtect);
}

const char* getConvention(DWORD func, size_t n_Expected_Args) {
	const char* conv = nullptr;

	DWORD epilogue = func + 16;
	while (!isPrologue(epilogue)) epilogue += 16;

	DWORD args = 0;
	DWORD func_start = func;
	DWORD func_end = epilogue;
	while (!isEpilogue(epilogue)) epilogue--;

	if (*(BYTE*)epilogue == 0xC2) {
		conv = "__stdcall";
	} else {
		conv = "__cdecl";
	}

	// search for the highest ebp offset, which will indicate the number of
	// args that were pushed onto the stack, rather than placed in ECX/EDX
	DWORD at = func_start;
	while (at < func_end) {
		disassembler::inst i = disassembler::read(at);

		if (i.flags & Fl_dest_imm8 && i.dest.r32 == disassembler::reg_32::ebp && i.dest.imm8 != 4 && i.dest.imm8 < 0x7F) {
			//printf("arg offset: %02X\n", i.dest.imm8);
			if (i.dest.imm8 > args) {
				args = i.dest.imm8;
			}
		}
		else if (i.flags & Fl_src_imm8 && i.src.r32 == disassembler::reg_32::ebp && i.src.imm8 != 4 && i.src.imm8 < 0x7F) {
			//printf("arg offset: %02X\n", i.src.imm8);
			if (i.src.imm8 > args) {
				args = i.src.imm8;
			}
		}

		at += i.len;
	}

	// no pushed args were used, but we know there
	// must be 1 or 2 args so it is either a fastcall
	// or a thiscall!
	if (args == 0) {
		switch (n_Expected_Args) {
		case 1:
			return "__thiscall";
			break;
		case 2:
			return "__fastcall";
			break;
		}
	}

	args -= 8;
	args = (args / 4) + 1;

	if (args == n_Expected_Args - 1) {
		conv = "__thiscall";
	}
	else if (args == n_Expected_Args - 2) {
		conv = "__fastcall";
	}

	return conv;
}

namespace MemLua {
	lua_State* L = nullptr;
	int lastCodeCave = NULL;
	int _BASE = NULL;
	int _VMP0 = NULL;
	int _VMP1 = NULL;

	namespace Functions {
		static int wait(lua_State* L) {
			long time = lua_tointeger(L, 1);
			Sleep(time * static_cast<long>(1000));
			return 0;
		}

		static int spawnThread(lua_State* L) {
			lua_getglobal(L, "coroutine");
			lua_getfield(L, -1, "create");
			lua_pushvalue(L, -3);
			lua_pcall(L, 1, 1, 0);

			lua_getglobal(L, "coroutine");
			lua_getfield(L, -1, "resume");
			lua_pushvalue(L, -3);
			lua_pcall(L, 1, 0, 0);
			return 0;
		}

		static int scanMemory(lua_State* L) {
			std::vector<int> results = {};

			const char* aob = lua_tostring(L, 1);
			if (lua_type(L, 2) == LUA_TBOOLEAN) {
				if (lua_type(L, 3) != LUA_TNUMBER) {
					results = MemScanner::scan(aob, lua_toboolean(L, 2));
				} else {
					results = MemScanner::scan(aob, lua_toboolean(L, 2), 1, lua_tointeger(L, 3));
				}
			} else {
				if (lua_type(L, 2) == LUA_TNUMBER) {
					results = MemScanner::scan(aob, false, 1, lua_tointeger(L, 2));
				} else {
					results = MemScanner::scan(aob);
				}
			}

			lua_newtable(L);
			for (int i = 0; i < results.size(); i++) {
				lua_pushinteger(L, results[i]);
				lua_rawseti(L, -2, i + 1);
			}

			return 1;
		}

		static int disassemble(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			disassembler::inst i = disassembler::read(address);
			
			lua_newtable(L); // instruction table
			addInteger(L, "len", i.len);
			addInteger(L, "flags", i.flags);
			addInteger(L, "pref", i.pref);
			addInteger(L, "address", i.address);
			addInteger(L, "rel8", i.rel8);
			addInteger(L, "rel16", i.rel16);
			addInteger(L, "rel32", i.rel32);
			lua_pushstring(L, i.opcode);
			lua_setfield(L, -2, "opcode");
			lua_pushstring(L, i.data);
			lua_setfield(L, -2, "data");

			lua_newtable(L); // src
			addInteger(L, "r8", i.src.r8);
			addInteger(L, "r16", i.src.r16);
			addInteger(L, "r32", i.src.r32);
			addInteger(L, "imm8", i.src.imm8);
			addInteger(L, "imm16", i.src.imm16);
			addInteger(L, "imm32", i.src.imm32);
			addInteger(L, "disp8", i.src.disp8);
			addInteger(L, "disp16", i.src.disp16);
			addInteger(L, "disp32", i.src.disp32);
			lua_setfield(L, -2, "src");

			lua_newtable(L); // dest
			addInteger(L, "r8", i.dest.r8);
			addInteger(L, "r16", i.dest.r16);
			addInteger(L, "r32", i.dest.r32);
			addInteger(L, "imm8", i.dest.imm8);
			addInteger(L, "imm16", i.dest.imm16);
			addInteger(L, "imm32", i.dest.imm32);
			addInteger(L, "disp8", i.dest.disp8);
			addInteger(L, "disp16", i.dest.disp16);
			addInteger(L, "disp32", i.dest.disp32);
			lua_setfield(L, -2, "dest");

			return 1;
		}

		static int messagebox(lua_State* L) {
			const char* msg = lua_tostring(L, 1);
			MessageBoxA(NULL, msg, "", MB_OK);
			return 0;
		}

		static int lua_readRawHttp(lua_State* L) {
			const char* strUrl = lua_tostring(L, 1);

			size_t len;
			const char* data = readRawHttp(strUrl, &len);
			lua_pushlstring(L, data, len);

			return 1;
		}

		static int readString(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			int i = 0;

			if (lua_type(L, 2) > LUA_TNIL) {
				i = lua_tointeger(L, 2);

				char* str = new char[i];
				str[0] = '\0';
				memcpy(str, reinterpret_cast<void*>(address), i);

				lua_pushlstring(L, str, i);
				delete[] str;
			}
			else {
				char str[1024];

				while (i < 1024) {
					BYTE x = *(BYTE*)(address++);

					if (x >= 0x20 && x <= 0x7E) {
						str[i++] = static_cast<char>(x);
					} else {
						break;
					}
				}

				lua_pushlstring(L, str, i);
			}
			
			return 1;
		}

		static int readByte(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			lua_pushinteger(L, static_cast<int>(*(BYTE*)address));
			return 1;
		}

		static int readBytes(lua_State* L) {
			void* address = reinterpret_cast<void*>(lua_tointeger(L, 1));
			DWORD len = lua_tointeger(L, 2);

			BYTE* bytes = new BYTE[len];
			memcpy(bytes, address, len);

			lua_newtable(L);
			for (int i = 0; i < len; i++) {
				lua_pushinteger(L, bytes[i]);
				lua_rawseti(L, -2, i + 1);
			}

			delete[] bytes;

			return 1;
		}

		static int readWord(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			lua_pushinteger(L, static_cast<int>(*(WORD*)address));
			return 1;
		}

		static int readDword(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			lua_pushinteger(L, static_cast<int>(*(DWORD32*)address));
			return 1;
		}

		static int readQword(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			ULONGLONG value = *(ULONGLONG*)address;
			lua_pushnumber(L, static_cast<double>(value));
			return 1;
		}

		static int writeString(lua_State* L) {
			void* address = reinterpret_cast<void*>(lua_tointeger(L, 1));
			size_t len = 0;
			const char* str = lua_tolstring(L, 2, &len);

			DWORD oldProtect;
			VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			memcpy(address, str, len);
			VirtualProtect(address, 1, oldProtect, &oldProtect);
			return 0;
		}

		static int writeByte(lua_State* L) {
			DWORD oldProtect, address = lua_tointeger(L, 1);
			VirtualProtect(reinterpret_cast<void*>(address), 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			*(BYTE*)(address) = static_cast<BYTE>(lua_tointeger(L, 2));
			VirtualProtect(reinterpret_cast<void*>(address), 1, oldProtect, &oldProtect);
			return 0;
		}

		static int writeBytes(lua_State* L) {
			void* address = reinterpret_cast<void*>(lua_tointeger(L, -2));
			DWORD oldProtect, len = lua_objlen(L, -1);

			BYTE* bytes = new BYTE[len];
			for (int i = 0; i < len; i++) {
				lua_rawgeti(L, -1, i + 1);
				bytes[i] = lua_tointeger(L, -1);
				lua_pop(L, 1);
			}

			VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			memcpy(address, bytes, len);
			VirtualProtect(address, 1, oldProtect, &oldProtect);
			
			delete[] bytes;
			return 0;
		}

		static int writeWord(lua_State* L) {
			DWORD oldProtect, address = lua_tointeger(L, -2);
			VirtualProtect(reinterpret_cast<void*>(address), 1, PAGE_EXECUTE_READWRITE, &oldProtect);

			WORD value = static_cast<WORD>(lua_tointeger(L, -2));
			*(WORD*)(address) = value;

			VirtualProtect(reinterpret_cast<void*>(address), 1, oldProtect, &oldProtect);
			return 0;
		}

		static int writeDword(lua_State* L) {
			DWORD oldProtect, address = lua_tointeger(L, -2);
			VirtualProtect(reinterpret_cast<void*>(address), 1, PAGE_EXECUTE_READWRITE, &oldProtect);

			DWORD32 value = static_cast<DWORD32>(lua_tointeger(L, -2));
			*(DWORD32*)(address) = value;

			VirtualProtect(reinterpret_cast<void*>(address), 1, oldProtect, &oldProtect);
			return 0;
		}

		static int writeQword(lua_State* L) {
			DWORD oldProtect, address = lua_tointeger(L, -2);
			VirtualProtect(reinterpret_cast<void*>(address), 1, PAGE_EXECUTE_READWRITE, &oldProtect);
			
			DWORD64 value = static_cast<DWORD64>(lua_tonumber(L, -2));
			*(DWORD64*)(address) = value;

			VirtualProtect(reinterpret_cast<void*>(address), 1, oldProtect, &oldProtect);
			return 0;
		}

		static int setMemoryPage(lua_State* L) {
			DWORD address = lua_tointeger(L, -3);
			DWORD dwSize = lua_tointeger(L, -2);
			DWORD newProtect = lua_tointeger(L, -1);
			DWORD oldProtect;

			VirtualProtect(reinterpret_cast<void*>(address), dwSize, newProtect, &oldProtect);
			lua_pushinteger(L, oldProtect);

			return 1;
		}

		static int queryMemoryRegion(lua_State* L) {
			DWORD address = lua_tointeger(L, -1);

			MEMORY_BASIC_INFORMATION mbi = { 0 };
			VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(MEMORY_BASIC_INFORMATION));

			lua_newtable(L);
			lua_pushinteger(L, reinterpret_cast<DWORD>(mbi.AllocationBase));
			lua_setfield(L, -2, "AllocationBase");
			lua_pushinteger(L, mbi.AllocationProtect);
			lua_setfield(L, -2, "AllocationProtect");
			lua_pushinteger(L, reinterpret_cast<DWORD>(mbi.BaseAddress));
			lua_setfield(L, -2, "BaseAddress");
			lua_pushinteger(L, mbi.Protect);
			lua_setfield(L, -2, "Protect");
			lua_pushinteger(L, mbi.RegionSize);
			lua_setfield(L, -2, "RegionSize");
			lua_pushinteger(L, mbi.State);
			lua_setfield(L, -2, "State");
			lua_pushinteger(L, mbi.Type);
			lua_setfield(L, -2, "Type");

			return 1;
		}

		static int allocPageMemory(lua_State* L) {
			size_t size = 0x3FF;
			if (lua_type(L, 1) > LUA_TNIL) {
				size = lua_tointeger(L, 1);
			}
			void* address = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			lua_pushinteger(L, reinterpret_cast<lua_Integer>(address));
			return 1;
		}

		static int freePageMemory(lua_State* L) {
			void* address = reinterpret_cast<void*>(lua_tointeger(L, 1));
			VirtualFree(address, 0, MEM_RELEASE);
			return 0;
		}

		static int hex_to_str(lua_State* L) {
			DWORD value = lua_tointeger(L, 1);
			size_t bits = 8;
			
			if (lua_type(L, 2) > LUA_TNIL)
				bits = lua_tointeger(L, 2);

			char str[16];
			sprintf_s(str, "%08X", value << (32 - (bits * 4)));
			lua_pushlstring(L, str, bits);

			return 1;
		}

		static int str_to_byte(lua_State* L) {
			const char* str = lua_tostring(L, 1);
			lua_pushinteger(L, toByte(str));
			return 1;
		}

		static int str_to_addr(lua_State* L) {
			std::string str = lua_tostring(L, 1);
			if (str.substr(0, 2) == "0x") {
				str.erase(0, 2);
			}

			BYTE* bytes = new BYTE[4];
			bytes[3] = toByte(str.substr(0, 2).c_str());
			bytes[2] = toByte(str.substr(2, 2).c_str());
			bytes[1] = toByte(str.substr(4, 2).c_str());
			bytes[0] = toByte(str.substr(6, 2).c_str());

			lua_pushinteger(L, *(int*)(int)bytes);
			return 1;
		}

		static int getExport(lua_State* L) {
			const char* moduleName = lua_tostring(L, -2);
			const char* functionName = lua_tostring(L, -1);

			HMODULE mod = GetModuleHandleA(moduleName);
			if (mod == nullptr) {
				lua_pushnil(L);
			} else {
				DWORD address = reinterpret_cast<DWORD>(GetProcAddress(mod, functionName));
				if (address == NULL) {
					lua_pushnil(L);
				} else {
					lua_pushinteger(L, address);
				}
			}

			return 1;
		}

		// local data = debugRegister(location, strRegister, (optional)offset);
		static int debugRegister(lua_State* L) {
			DWORD address = lua_tointeger(L, 1); // location
			DWORD hookSize = 0;
			DWORD reg = 0;
			DWORD offset = 0;
			if (lua_isnumber(L, 3)){
				offset = lua_tointeger(L, 3);
			}

			while (hookSize < 5) {
				hookSize += disassembler::read(address + hookSize).len;
			}

			// read the register string
			const char* strRegister = lua_tostring(L, 2);
			const char* strRegisters[] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };

			// identify the register/get the number value of it
			for (int i = 0; i < 8; i++) {
				if (strcmp(strRegister, strRegisters[i]) == 0) {
					reg = i;
					break;
				}
			}

			BYTE* oldBytes = new BYTE[hookSize];
			memcpy(oldBytes, reinterpret_cast<void*>(address), hookSize);

			void* detourFunc = VirtualAlloc(nullptr, 0x3FF, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			DWORD at = reinterpret_cast<DWORD>(detourFunc);
			printf("Detour function: %08X\n", detourFunc);

			// Copy original bytes to our hook
			for (int i = 0; i < hookSize; i++) {
				*(BYTE*)(at++) = *(BYTE*)(address + i);
			}

			BOOL dataRead = FALSE;
			DWORD value = NULL;

			// *(int*)dataRead = 1;
			// 
			*(BYTE*)(at++) = 0xC7; // mov [dataRead], 00000001
			*(BYTE*)(at++) = 0x05;
			*(PVOID*)(at) = &dataRead;
			at += sizeof(DWORD);
			*(DWORD*)(at) = 1;
			at += sizeof(DWORD);

			// read the offset of the register specified
			*(BYTE*)(at++) = 0x56; // push esi

			*(BYTE*)(at++) = 0x8B; // mov esi, [???+offset]
			*(BYTE*)(at++) = 0xB0 + reg;
			*(DWORD*)(at) = offset;
			at += sizeof(DWORD);

			*(BYTE*)(at++) = 0x89; // mov [value], esi
			*(BYTE*)(at++) = 0x35;
			*(PVOID*)(at) = &value;
			at += sizeof(DWORD);

			*(BYTE*)(at++) = 0x5E; // pop esi

			placeHook(at, 5, address + hookSize);
			placeHook(address, hookSize, reinterpret_cast<DWORD>(detourFunc));

			while (!dataRead) Sleep(10);

			// Restore original bytes
			DWORD oldProtect;
			VirtualProtect(reinterpret_cast<void*>(address), hookSize, PAGE_EXECUTE_READWRITE, &oldProtect);
			memcpy(reinterpret_cast<void*>(address), oldBytes, hookSize);
			VirtualProtect(reinterpret_cast<void*>(address), hookSize, oldProtect, &oldProtect);

			delete[] oldBytes;
			VirtualFree(detourFunc, 0, MEM_RELEASE);

			lua_pushinteger(L, value);
			return 1;
		}

		// use this instead of createDetour
		// this will load it onto its own thread
		// entirely
		static int createSafeDetour(lua_State* L) {
			DWORD address = lua_tointeger(L, -2);

			char funcGlobalName[32];
			sprintf(funcGlobalName, "DETOUR_FUNC_%08X", address);
			lua_pushvalue(L, -1);
			lua_setglobal(L, funcGlobalName);

			char locGlobalName[32];
			sprintf(locGlobalName, "DETOUR_LOC_%08X", address);
			lua_pushvalue(L, -2);
			lua_setglobal(L, locGlobalName);

			std::string script = "spawnThread(function()\n";
			script += "createDetour_UNSAFE(\"";
			script += locGlobalName;
			script += "\", \"";
			script += funcGlobalName;
			script += "\")\n";
			script += "end)\n";

			luaL_loadbuffer(L, script.c_str(), script.length(), "DETOUR_PRELOAD");
			lua_pcall(L, 0, 0, 0);
			return 0;
		}

		// createDetour(location, function(data) end)
		//
		// by using createDetour at a specific address,
		// it will write a jmp there which leads to our
		// auto-generated assembly code.
		// Our assembly code grabs a couple registers/whatever
		// we can grab from the stack, and stores it in temporary
		// globals.
		// We also plant lua calls in the assembly code to run
		// the lua function you pass in a coroutine,
		// where all of the temporary globals are placed in a table
		// and usable as the first function arg
		// 
		static int createDetour(lua_State* L) {
			const char* locGlobalName = lua_tostring(L, 1);
			const char* funcGlobalName = lua_tostring(L, 2);

			lua_getglobal(L, locGlobalName);
			DWORD address = lua_tointeger(L, -1); // location
			lua_pop(L, 1);

			// Calculate the size (# of instructions to overwrite)
			DWORD hookSize = 0;
			while (hookSize < 5) {
				hookSize += disassembler::read(address + hookSize).len;
			}

			// Get the original bytes
			BYTE* oldBytes = new BYTE[hookSize];
			memcpy(oldBytes, reinterpret_cast<void*>(address), hookSize);

			// Create a new global for the address of our detour
			char* dataGlobalName = (char*)luaM_malloc(L, 32);
			sprintf(dataGlobalName, "DETOUR_DATA_%08X", address);

			DWORD detourHook = reinterpret_cast<DWORD>(VirtualAlloc(nullptr, 0x3FF, MEM_COMMIT, PAGE_EXECUTE_READWRITE));
			DWORD at = detourHook;

			// Create a new global for the detour hook data
			lua_newtable(L);
			lua_newtable(L);
			for (int i = 0; i < hookSize; i++) {
				lua_pushinteger(L, oldBytes[i]);
				lua_rawseti(L, -2, i + 1);
			}
			delete[] oldBytes;
			lua_setfield(L, -2, "bytes");
			lua_pushinteger(L, detourHook + 132 + 0);
			lua_setfield(L, -2, "eax");
			lua_pushinteger(L, detourHook + 132 + 4);
			lua_setfield(L, -2, "ecx");
			lua_pushinteger(L, detourHook + 132 + 8);
			lua_setfield(L, -2, "edx");
			lua_pushinteger(L, detourHook + 132 + 12);
			lua_setfield(L, -2, "ebx");
			lua_pushinteger(L, detourHook + 132 + 16);
			lua_setfield(L, -2, "esp");
			lua_pushinteger(L, detourHook + 132 + 20);
			lua_setfield(L, -2, "ebp_deprecated");
			lua_pushinteger(L, detourHook + 132 + 64); // we can dump most of the ebp args here
			lua_setfield(L, -2, "ebp");
			lua_pushinteger(L, detourHook + 132 + 24);
			lua_setfield(L, -2, "esi");
			lua_pushinteger(L, detourHook + 132 + 28);
			lua_setfield(L, -2, "edi");
			lua_setglobal(L, dataGlobalName);

			// Copy original bytes to our hook
			for (int i = 0; i < hookSize; i++) {
				*(BYTE*)(at++) = *(BYTE*)(address + i);
			}

			*(BYTE*)(at++) = 0x60; // pushad
			*(BYTE*)(at++) = 0x50; // push eax

			/**(BYTE*)(at++) = 0x8A; // mov al, [currentlyRunning]
			*(BYTE*)(at++) = 0x05;
			*(DWORD*)(at) = detourHook + 128;
			at += sizeof(DWORD);
			*(BYTE*)(at++) = 0x84; // test al,al
			*(BYTE*)(at++) = 0xC0;
			*(BYTE*)(at++) = 0x74; // jnz +5
			*(BYTE*)(at++) = 5;
			placeHook(at, 5, address + hookSize);	// jmp back; don't spawn another coroutine
			at += 5;								// until the current one is done

			*(BYTE*)(at++) = 0xFF; // inc [currentlyRunning]
			*(BYTE*)(at++) = 0x05;
			*(DWORD*)(at) = detourHook + 128;
			at += sizeof(DWORD);*/

			// Let's load 8 EBP args into the ebp location...
			// (sorry for the limitations my guy)
			for (int i = 2; i < 10; i++) {
				*(BYTE*)(at++) = 0x8B; // mov eax,[ebp+xx]
				*(BYTE*)(at++) = 0x45;
				*(BYTE*)(at++) = 0 + (i * 4);
				*(BYTE*)(at++) = 0x89; // mov [ebpLocation],eax
				*(BYTE*)(at++) = 0x05;
				*(DWORD*)(at) = (detourHook + 132 + 64) + (i * 4);
				at += sizeof(DWORD);
			}

			*(BYTE*)(at++) = 0x56; // push esi

			*(BYTE*)(at++) = 0xBE; // mov esi, L
			*(DWORD*)(at) = reinterpret_cast<DWORD>(L);
			at += sizeof(DWORD);

			char strDetourHook[32];
			sprintf_s(strDetourHook, "0x%08X", detourHook);
			char strAddress[32];
			sprintf_s(strAddress, "0x%08X", address);

			std::string script = "";// "spawnThread(function()\n";

			// write the cleanup function here
			script += "function endDetour()\n writeBytes(";
			script += strAddress;
			script += ", ";
			script += dataGlobalName;
			script += ".bytes);\n";
			script += "freePageMemory(";
			script += strDetourHook;
			script += ")\n";
			script += "end\n";

			// this will invoke the lua function with
			// table data (containing most of the temporary
			// globals we made)
			script += funcGlobalName;
			script += "(";
			script += dataGlobalName;
			script += ")\n";

			//script += "end)\n";

			const char* bufferName = "DETOUR_PRELOAD";

			luaL_loadbuffer(L, script.c_str(), script.length(), bufferName);
			lua_setglobal(L, bufferName);

			memPushLargeArg(at, reinterpret_cast<DWORD>(bufferName));	// push bufferName
			memPushLargeArg(at, LUA_GLOBALSINDEX);	// push GLOBALSINDEX
			*(BYTE*)(at++) = 0x56;					// push L
			memPlaceCall(at, lua_getfield, 0xC);	// lua_getfield(L, GLOBALSINDEX, name3)

			memPushSmallArg(at, 0);					// push 0
			memPushSmallArg(at, 0);					// push 0
			memPushSmallArg(at, 0);					// push 0
			*(BYTE*)(at++) = 0x56;					// push L
			memPlaceCall(at, lua_pcall, 0x10);		// lua_pcall(L, 0, 0, 0)

			*(BYTE*)(at++) = 0x5E; // pop esi

			/**(BYTE*)(at++) = 0xC7; // mov eax, 0
			*(BYTE*)(at++) = 0xC0;
			*(DWORD*)(at) = 0;
			at += sizeof(DWORD);
			*(BYTE*)(at++) = 0xA3; // mov [currentlyRunning],eax
			*(DWORD*)(at) = detourHook + 128;
			at += sizeof(DWORD);*/

			*(BYTE*)(at++) = 0x58; // pop eax
			*(BYTE*)(at++) = 0x61; // popad

			placeHook(at, 5, address + hookSize);
			placeHook(address, hookSize, detourHook);

			return 0;
		}

		// Converts any function into a cdecl (default convention)
		// by creating a new routine that mimics a cdecl
		// 
		static int createRoutine(lua_State* L) {
			DWORD func = lua_tointeger(L, 1);
			DWORD n_Args = lua_tointeger(L, 2);
			DWORD size = 0;
			BYTE data[128];

			const char* convention = getConvention(func, n_Args);
			if (strcmp(convention, "__cdecl") == 0) {
				return func;
			}

			DWORD new_func = reinterpret_cast<DWORD>(VirtualAlloc(nullptr, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
			if (new_func == NULL) {
				printf("Error while allocating memory\n");
				return func;
			}

			data[size++] = 0x55; // push ebp
			data[size++] = 0x8B; // mov ebp,esp
			data[size++] = 0xEC;

			/*if (strcmp(convention, "__cdecl") == 0) {
				for (int i = (n_Args * 4) + 8; i > 8; i -= 4) {
					data[size++] = 0xFF; // push [ebp+??]
					data[size++] = 0x75;
					data[size++] = i - 4;
				}

				data[size++] = 0xE8; // call func
				*(DWORD*)(data + size) = func - (new_func + size + 4);
				size += 4;

				data[size++] = 0x83;
				data[size++] = 0xC4;
				data[size++] = n_Args * 4;
			
			else */if (strcmp(convention, "__stdcall") == 0) {
				for (int i = (n_Args * 4) + 8; i > 8; i -= 4) {
					data[size++] = 0xFF; // push [ebp+??]
					data[size++] = 0x75;
					data[size++] = i - 4;
				}

				data[size++] = 0xE8; // call func
				*(DWORD*)(data + size) = func - (new_func + size + 4);
				size += 4;
			}
			else if (strcmp(convention, "__thiscall") == 0) {
				data[size++] = 0x51; // push ecx

				for (int i = n_Args; i > 1; i--) {
					data[size++] = 0xFF; // push [ebp+??]
					data[size++] = 0x75;
					data[size++] = (i + 1) * 4;
				}

				data[size++] = 0x8B; // mov ecx,[ebp+08]
				data[size++] = 0x4D;
				data[size++] = 0x08;

				data[size++] = 0xE8; // call func
				*(DWORD*)(data + size) = func - (new_func + size + 4);
				size += 4;

				data[size++] = 0x59; // pop ecx
			}
			else if (strcmp(convention, "__fastcall") == 0) {
				data[size++] = 0x51; // push ecx
				data[size++] = 0x52; // push edx

				for (int i = n_Args; i > 2; i--) {
					data[size++] = 0xFF; // push [ebp+??]
					data[size++] = 0x75;
					data[size++] = (i + 1) * 4;
				}

				data[size++] = 0x8B; // mov ecx,[ebp+08]
				data[size++] = 0x4D;
				data[size++] = 0x08;
				data[size++] = 0x8B; // mov edx,[ebp+0C]
				data[size++] = 0x55;
				data[size++] = 0x0C;

				data[size++] = 0xE8; // call func
				*(DWORD*)(data + size) = func - (new_func + size + 4);
				size += 4;

				data[size++] = 0x59; // pop ecx
				data[size++] = 0x5A; // pop edx
			}

			data[size++] = 0x5D; // pop ebp
			data[size++] = 0xC3; // retn
			//data[size++] = 0xC2; // ret xx
			//data[size++] = n_Args * 4;
			//data[size++] = 0x00;

			memcpy_s((void*)new_func, size, &data, size);
			lua_pushinteger(L, new_func);
			return 1;
		}
		 
		static int startRoutine(lua_State* L) {
			DWORD func = lua_tointeger(L, 1);
			LPVOID param = 0;
			if (lua_type(L, 2) > LUA_TNIL) {
				param = reinterpret_cast<void*>(lua_tointeger(L, 2));
			}
			HANDLE thread = CreateThread(0, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(func), param, 0, 0);
			lua_pushinteger(L, reinterpret_cast<lua_Integer>(thread));
			return 1;
		}

		static int cloneFunction(lua_State* L) {
			DWORD start = lua_tointeger(L, 1);
			DWORD fSize, end = start + 16;

			while (!(isPrologue(end) && isValidCode(end))) {
				end += 16;
			}

			fSize = end - start;

			DWORD newFunction = reinterpret_cast<DWORD>(VirtualAlloc(nullptr, fSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
			DWORD at = newFunction;
			
			for (int i = 0; i < fSize; i++){
				if (*(BYTE*)start == 0xE8) {
					DWORD oldrel = *(DWORD*)(start + 1);
					DWORD relfunc = (start + oldrel) + 5;

					if (relfunc % 0x10 == 0) {
						DWORD newrel = relfunc - (at + 5);

						*(BYTE*)(at++) = *(BYTE*)(start++);
						*(DWORD*)(at) = newrel;
						at += sizeof(DWORD);
						start += sizeof(DWORD);
					}
				} else {
					*(BYTE*)(at++) = *(BYTE*)(start++);
				}
			}

			lua_pushinteger(L, newFunction);
			lua_pushinteger(L, fSize);
			return 2;
		}

		static int lua_getPrologue(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			
			if    (!isPrologue(address)) address -= (address % 16);
			while (!isPrologue(address)) address -= 16;

			lua_pushinteger(L, address);
			return 1;
		}

		static int isFuncPrologue(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			lua_pushboolean(L, isPrologue(address));
			return 1;
		}

		static int isFuncEpilogue(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			lua_pushboolean(L, isEpilogue(address));
			return 1;
		}

		static int getFuncEpilogue(lua_State* L) {
			DWORD address = lua_tointeger(L, 1) + 16;
			while (!(isPrologue(address) && isValidCode(address))) {
				address += 16;
			}
			while (!isEpilogue(address)) address--;

			lua_pushinteger(L, address);
			return 1;
		}

		static int getNextPrologue(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);

			if (isPrologue(address)) {
				address += 16;
			} else {
				address += (address % 16);
			}
			while (!(isPrologue(address) && isValidCode(address))) {
				address += 16;
			}

			lua_pushinteger(L, address);
			return 1;
		}

		static int getNextRef(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			DWORD address_look_for = lua_tointeger(L, 2);
			BOOL returnPrologue = lua_toboolean(L, 3);

			MEMORY_BASIC_INFORMATION mbi = { 0 };

			while (true) {
				if (address > reinterpret_cast<DWORD>(mbi.BaseAddress) + mbi.RegionSize) {
					VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi));
					if (mbi.Protect != PAGE_EXECUTE_READ) {
						break;
					}
				}

				if (*(BYTE*)address == 0xE8) {
					if (getRel(address) == address_look_for) {
						break;
					}
					address += 5;
					continue;
				}
				address++;
			}

			if (!returnPrologue) {
				lua_pushinteger(L, address);
			} else {
				if    (!isPrologue(address)) address -= (address % 16);
				while (!isPrologue(address)) address -= 16;
				lua_pushinteger(L, address);
			}
			return 1;
		}

		static int getNextRefs(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			DWORD address_look_for = lua_tointeger(L, 2);

			std::vector<DWORD>xrefs = {};

			MEMORY_BASIC_INFORMATION mbi = { 0 };
			VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(MEMORY_BASIC_INFORMATION));

			while (address < reinterpret_cast<DWORD>(mbi.BaseAddress) + mbi.RegionSize)
			{
				if (*(BYTE*)address == 0xE8)
				{
					if (getRel(address) == address_look_for)
					{
						xrefs.push_back(address);
					}
					address += 5;
					continue;
				}
				address++;
			}

			int i = 1;
			lua_newtable(L);
			for (int xref : xrefs) {
				lua_pushinteger(L, xref);
				lua_rawseti(L, -2, i++);
			}

			return 1;
		}

		static int getFunctionConvention(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			DWORD args = lua_tointeger(L, 2);
			lua_pushstring(L, getConvention(address, args));
			return 1;
		}

		static int getFunctionSize(lua_State* L) {
			DWORD start = lua_tointeger(L, 1);
			DWORD end = start;

			if (isPrologue(end)) {
				end += 16;
			}
			while (!(isPrologue(end) && isValidCode(end))) {
				end += 16;
			}

			lua_pushinteger(L, end - start);
			return 1;
		}

		static int getFunctionRet(lua_State* L) {
			WORD ret = 0;
			DWORD epilogue = lua_tointeger(L, 1) + 16;

			while (!(isPrologue(epilogue) && isValidCode(epilogue))) {
				epilogue += 16;
			}
			while (!isEpilogue(epilogue)) epilogue--;

			if (*(BYTE*)epilogue == 0xC2) {
				ret = *(WORD*)(epilogue + 1);
			}

			lua_pushinteger(L, ret);
			return 1;
		}

		static int getNextCall(lua_State* L) {
			DWORD address = lua_tointeger(L, 1);
			BOOL loc = false;
			DWORD rel = 0;

			if (lua_type(L, 2) == LUA_TBOOLEAN) {
				loc = lua_toboolean(L, 2);
			}

			// Skip current call if possible
			if (*(BYTE*)address == 0xE8) {
				address++;
			}

			// Increment until we reach the very next call instruction
			while (true) {
				if (*(BYTE*)address == 0xE8 || *(BYTE*)address == 0xE9) {
					rel = getRel(address);
					if (rel > _BASE) {
						if (isPrologue(rel)) {
							break;
						}
					}
				}
				address++;
			}

			if (!loc) {
				lua_pushinteger(L, rel);
			} else {
				lua_pushinteger(L, address);
			}

			return 1;
		}

		static int getFunctionCalls(lua_State* L) {
			std::vector<DWORD> calls = {};

			DWORD start = lua_tointeger(L, 1);
			DWORD end = start + 16;

			while (!(isPrologue(end) && isValidCode(end))) {
				end += 16;
			}

			while (start < end) {
				if (*(BYTE*)start == 0xE8 || *(BYTE*)start == 0xE9){
					DWORD rel = getRel(start);
					if (rel > _BASE) {
						if (isPrologue(rel)) {
							calls.push_back(rel);
							start += 5;
							continue;
						}
					}
				}
				start++;
			}

			lua_newtable(L);
			for (int i = 0; i < calls.size(); i++) {
				lua_pushinteger(L, calls[i]);
				lua_rawseti(L, -2, i + 1);
			}

			return 1;
		}

		static int getFunctionPointers(lua_State* L) {
			std::vector<DWORD> pointers = {};

			DWORD start = lua_tointeger(L, 1);
			DWORD end = start + 16;

			while (!(isPrologue(end) && isValidCode(end))) {
				end += 16;
			}

			while (start < end) {
				disassembler::inst i = disassembler::read(start);

				if (i.flags & Fl_src_disp32 && i.src.disp32 % 4 == 0) {
					pointers.push_back(i.src.disp32);
				} else if (i.flags & Fl_dest_disp32 && i.dest.disp32 % 4 == 0){
					pointers.push_back(i.dest.disp32);
				}

				start += i.len;
			}

			lua_newtable(L);
			for (int i = 0; i < pointers.size(); i++) {
				lua_pushinteger(L, pointers[i]);
				lua_rawseti(L, -2, i + 1);
			}

			return 1;
		}

		// invokes a function, whether it be a __stdcall or __cdecl.
		// if it is a different calling convention, try using createRoutine
		// to produce a new function which will act as an __stdcall
		// 
		int invokeFunc(lua_State* L) {
			std::vector<DWORD> oldData;
			DWORD args = 0;
			DWORD result = 0;
			DWORD func = lua_tointeger(L, 1);
			DWORD size_of_args = 0;
			DWORD epilogue = func + 16;
			while (!isPrologue(epilogue)) epilogue += 16;
			while (!isEpilogue(epilogue)) epilogue--;

			for (int i = 2;; i++) {
				int type = lua_type(L, i);
				if (type <= LUA_TNIL) break;
				if (type == LUA_TNUMBER) {
					oldData.push_back(lua_tointeger(L, i));
				} else if (type == LUA_TSTRING) {
					oldData.push_back(reinterpret_cast<DWORD>(lua_tostring(L, i)));
				}
				if (*(BYTE*)(epilogue) == 0xC3) {
					size_of_args += 4;
				}
			}

			std::reverse(oldData.begin(), oldData.end());
			DWORD data = reinterpret_cast<DWORD>(oldData.data());
			args = oldData.size() * 4;

			__asm {
				push edi;
				push eax;
				xor eax, eax;
			append_arg:
				add eax, 0x00000004;
				mov edi, [data];
				add edi, eax;
				sub edi, 4;
				mov edi, [edi];
				push edi;
				cmp eax, [args];
				jl append_arg;
				call func;
				mov result, eax;
				mov al, BYTE PTR [size_of_args];
				test al, al;
				jz skip_cleanup;
				add esp, [size_of_args];
				skip_cleanup:
				pop eax
				pop edi
			}

			lua_pushinteger(L, result);
			return 1;
		}

		static int saveFile(lua_State* L) {
			size_t path_len, at = 0;
			const char* rawPath = lua_tolstring(L, 1, &path_len);
			std::string path = "";
			
			while (at++ < path_len) {
				if (rawPath[at - 1] == '%') {
					std::string var = "";
					for (; at < path_len; at++) {
						if (rawPath[at] == '%'){ at++; break; } else {
							var += rawPath[at];
						}
					}
					path += getenv(var.c_str());
					continue;
				}
				path += rawPath[at - 1];
			}
			
			if (lua_type(L, 2) == LUA_TSTRING) {
				size_t len;
				BYTE* data = (BYTE*)lua_tolstring(L, 2, &len);
				std::ofstream myfile;
				myfile.open(path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
				if (myfile.is_open()) {
					for (int i = 0; i < len; i++) {
						myfile << (char)data[i];
					}
					myfile.close();
				}
			}

			return 0;
		}

		static int readFile(lua_State* L) {
			size_t path_len, at = 0;
			const char* rawPath = lua_tolstring(L, 1, &path_len);
			std::string path = "";
			
			while (at++ < path_len) {
				if (rawPath[at - 1] == '%') {
					std::string var = "";
					for (; at < path_len; at++) {
						if (rawPath[at] == '%'){ at++; break; } else {
							var += rawPath[at];
						}
					}
					path += getenv(var.c_str());
					continue;
				}
				path += rawPath[at - 1];
			}

			std::ifstream inFile;
			size_t size = 0;

			inFile.open(path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
			inFile.seekg(0, std::ios::end); // set the pointer to the end
			size = inFile.tellg(); // get the length of the file
			inFile.seekg(0, std::ios::beg); // set the pointer to the beginning

			char* buffer = new char[size];
			buffer[0] = '\0';
			inFile.read(buffer, size);
			lua_pushlstring(L, buffer, size);
			delete[] buffer;
			
			return 1;
		}

		// first arg should be `base` if you're
		// looking for the next code cave in .text
		static int nextCodeCave(lua_State* L) {
			DWORD start = lua_tointeger(L, 1);

			while (!(*(__int64*)(start + 0) == 0 &&
				*(__int64*)(start + 16) == 0 &&
				*(__int64*)(start + 32) == 0 &&
				*(__int64*)(start + 64) == 0)) {
				start++;
			}

			lua_pushinteger(L, start);
			return 1;
		}

		LONG WINAPI lua_exceptionHandler(PEXCEPTION_POINTERS pPointers) {
			// may help to prevent detection (from anti-
			// debugging codes that are usually in .vmp)
			if (pPointers->ContextRecord->Eip > _VMP0 && pPointers->ContextRecord->Eip < _VMP1) {
				return EXCEPTION_CONTINUE_SEARCH;
			}

			switch (pPointers->ExceptionRecord->ExceptionCode) {
			case EXCEPTION_ACCESS_VIOLATION:
				printf("[MemLua] Access Violation was detected [0x%p]\n", pPointers->ContextRecord->Eip);
				break;
			case EXCEPTION_GUARD_PAGE:

				break;
			case EXCEPTION_SINGLE_STEP:

				break;
			}

			return EXCEPTION_CONTINUE_SEARCH;
		}

		static int lua_setBreakpoint(lua_State* L) {


			return 0;
		}

		static int veh_init(lua_State* L) {
			ULONG first = 1;
			if (lua_type(L, 1) > LUA_TNIL) {
				first = static_cast<ULONG>(lua_tointeger(L, 1));
			}
			AddVectoredExceptionHandler(first, lua_exceptionHandler);
			return 0;
		}
	}

	void init(){
		L = lua_open();
		luaL_openlibs(L);

		lua_pushfunction(L, Functions::wait, "wait");
		lua_pushfunction(L, Functions::spawnThread, "spawnThread");
		lua_pushfunction(L, Functions::scanMemory, "scanMemory");
		lua_pushfunction(L, Functions::disassemble, "disassemble");
		lua_pushfunction(L, Functions::messagebox, "MessageBox");
		lua_pushfunction(L, Functions::lua_readRawHttp, "readRawHttp");
		lua_pushfunction(L, Functions::readString, "readString");
		lua_pushfunction(L, Functions::readByte, "readByte");
		lua_pushfunction(L, Functions::readBytes, "readBytes");
		lua_pushfunction(L, Functions::readWord, "readShort");
		lua_pushfunction(L, Functions::readDword, "readInt");
		lua_pushfunction(L, Functions::readQword, "readQword");
		lua_pushfunction(L, Functions::writeString, "writeString");
		lua_pushfunction(L, Functions::writeByte, "writeByte");
		lua_pushfunction(L, Functions::writeBytes, "writeBytes");
		lua_pushfunction(L, Functions::writeWord, "writeShort");
		lua_pushfunction(L, Functions::writeDword, "writeInt");
		lua_pushfunction(L, Functions::writeQword, "writeQword");
		lua_pushfunction(L, Functions::setMemoryPage, "setMemoryPage");
		lua_pushfunction(L, Functions::queryMemoryRegion, "queryMemoryRegion");
		lua_pushfunction(L, Functions::allocPageMemory, "allocPageMemory");
		lua_pushfunction(L, Functions::freePageMemory, "freePageMemory");
		lua_pushfunction(L, Functions::hex_to_str, "toString");
		lua_pushfunction(L, Functions::str_to_byte, "toByte");
		lua_pushfunction(L, Functions::str_to_addr, "toAddress");
		lua_pushfunction(L, Functions::getExport, "getExport");
		lua_pushfunction(L, Functions::debugRegister, "debugRegister");
		lua_pushfunction(L, Functions::createDetour, "createDetour_UNSAFE");
		lua_pushfunction(L, Functions::createSafeDetour, "createDetour");
		lua_pushfunction(L, Functions::createRoutine, "createRoutine");
		lua_pushfunction(L, Functions::startRoutine, "startRoutine");
		lua_pushfunction(L, Functions::cloneFunction, "cloneFunction");
		lua_pushfunction(L, Functions::lua_getPrologue, "getFuncPrologue");
		lua_pushfunction(L, Functions::isFuncPrologue, "isFuncPrologue");
		lua_pushfunction(L, Functions::isFuncEpilogue, "isFuncEpilogue");
		lua_pushfunction(L, Functions::getFuncEpilogue, "getFuncEpilogue");
		lua_pushfunction(L, Functions::getNextPrologue, "nextFuncPrologue");
		lua_pushfunction(L, Functions::getNextRef, "nextFuncRef");
		lua_pushfunction(L, Functions::getNextRefs, "nextFuncRefs");
		lua_pushfunction(L, Functions::getFunctionConvention, "getFuncConv");
		lua_pushfunction(L, Functions::getFunctionSize, "getFuncSize");
		lua_pushfunction(L, Functions::getFunctionRet, "getFuncRet");
		lua_pushfunction(L, Functions::getFunctionPointers, "getFuncPointers");
		lua_pushfunction(L, Functions::getFunctionCalls, "getFuncCalls");
		lua_pushfunction(L, Functions::getNextCall, "nextCall");
		lua_pushfunction(L, Functions::invokeFunc, "invokeFunc");
		lua_pushfunction(L, Functions::saveFile, "saveFile");
		lua_pushfunction(L, Functions::readFile, "readFile");
		lua_pushfunction(L, Functions::nextCodeCave, "nextCodeCave");

		// to-do: Add functionality for all of the most
		// commonly-used DLL exports, such as K32/Enum module functions,
		// 
		
		lua_newtable(L);
		// to-do: implement vectored exception handling api
		// use page exceptions
		lua_pushcfunction(L, Functions::lua_setBreakpoint);
		lua_setfield(L, -2, "setBreakpoint");
		lua_pushcfunction(L, Functions::veh_init);
		lua_setfield(L, -2, "init");
		//lua_pushcfunction(L, Functions::setRawHandler);
		//lua_setfield(L, -2, "setRawHandler");
		lua_setglobal(L, "veh");

		
		__asm {
			push eax;
			mov eax, fs:[0x30];
			mov eax, [eax + 8];
			mov [_BASE], eax;
			push edi;
			mov edi, eax;
		findheader1:
			inc edi;
			cmp dword ptr[edi], 0x30706D76 // "vmp0"
			jne findheader1;
			add edi, 0xB;
			mov edi, [edi];
			add edi, eax;
			mov [_VMP0], edi; // vmp0 found
			mov edi, eax;
		findheader2:
			inc edi;
			cmp dword ptr [edi], 0x31706D76 // "vmp1"
			jne findheader2;
			add edi, 0xB;
			mov edi, [edi];
			add edi, eax;
			mov [_VMP1], edi; // vmp1 found
			pop edi;
			pop eax;
		}

		addGlobalInteger(L, "base", _BASE);
		addGlobalInteger(L, "vmp0", _VMP0);
		addGlobalInteger(L, "vmp1", _VMP1);

		// Windows standard page flags
		addGlobalInteger(L, "PAGE_EXECUTE", PAGE_EXECUTE);
		addGlobalInteger(L, "PAGE_EXECUTE_READ", PAGE_EXECUTE_READ);
		addGlobalInteger(L, "PAGE_EXECUTE_READWRITE", PAGE_EXECUTE_READWRITE);
		addGlobalInteger(L, "PAGE_EXECUTE_WRITECOPY", PAGE_EXECUTE_WRITECOPY);
		addGlobalInteger(L, "PAGE_GUARD", PAGE_GUARD);
		addGlobalInteger(L, "PAGE_NOACCESS", PAGE_NOACCESS);
		addGlobalInteger(L, "PAGE_NOCACHE", PAGE_NOCACHE);
		addGlobalInteger(L, "PAGE_READONLY", PAGE_READONLY);
		addGlobalInteger(L, "PAGE_READWRITE", PAGE_READWRITE);
		addGlobalInteger(L, "MEM_COMMIT", MEM_COMMIT);
		addGlobalInteger(L, "MEM_DECOMMIT", MEM_DECOMMIT);
		addGlobalInteger(L, "MEM_RESERVE", MEM_RESERVE);
		addGlobalInteger(L, "MEM_RESET", MEM_RESET);
		addGlobalInteger(L, "MEM_RELEASE", MEM_RELEASE);
		addGlobalInteger(L, "PROCESS_ALL_ACCESS", PROCESS_ALL_ACCESS);

		// Disassembler flags
		addGlobalInteger(L, "FL_SRC_DEST", Fl_src_dest);
		addGlobalInteger(L, "FL_SRC_IMM8", Fl_src_imm8);
		addGlobalInteger(L, "FL_SRC_IMM16", Fl_src_imm16);
		addGlobalInteger(L, "FL_SRC_IMM32", Fl_src_imm32);
		addGlobalInteger(L, "FL_SRC_DISP8", Fl_src_disp8);
		addGlobalInteger(L, "FL_SRC_DISP16", Fl_src_disp16);
		addGlobalInteger(L, "FL_SRC_DISP32", Fl_src_disp32);
		addGlobalInteger(L, "FL_DEST_IMM8", Fl_dest_imm8);
		addGlobalInteger(L, "FL_DEST_IMM16", Fl_dest_imm16);
		addGlobalInteger(L, "FL_DEST_IMM32", Fl_dest_imm32);
		addGlobalInteger(L, "FL_DEST_DISP8", Fl_dest_disp8);
		addGlobalInteger(L, "FL_DEST_DISP16", Fl_dest_disp16);
		addGlobalInteger(L, "FL_DEST_DISP32", Fl_dest_disp32);
	}

	void load(std::string src) {
		src += "\n";
		if (luaL_dostring(L, src.c_str())) {
			printf("[MemLua] Error: %s\n", lua_tostring(L, -1));
		} else {
			printf("[MemLua] Success\n");
		}
	}

	void loadhttp(std::string url) {
		const char* src = readRawHttp(url.c_str(), nullptr);
		if (luaL_dostring(L, src)) {
			printf("[MemLua] Error: %s\n", lua_tostring(L, -1));
		} else {
			printf("[MemLua] Success\n");
		}
	}
};

