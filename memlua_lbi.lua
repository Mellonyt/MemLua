local dump_bytecode = false;

print("Scanning");
local r_gettop = getFuncPrologue(scanMemory("558BEC8B??088B????2B??????????5DC3")[1]);
local r_spawn = getFuncPrologue(scanMemory("83????F20F10????F20F??????FF75")[1]);
local r_deserialize = getFuncPrologue(scanMemory("0F????83??7FD3??83??0709")[1]);

print("r_spawn: " .. toString(r_spawn));
print("r_deserialize: " .. toString(r_deserialize));
        
-- simple retcheck bypass for lua
local retcheck = {};
retcheck.functions = {};
retcheck.patch = function(func)
  local copy, copySize = cloneFunction(func);
  local retchecks = {};
            
  for at = copy, nextFuncPrologue(copy), 1 do
    if (readByte(at) == 0x72 and readByte(at + 2) == 0xA1 and readByte(at + 7) == 0x8B) then
      table.insert(retchecks, at);
    end
  end
  
  if #retchecks == 0 then
    freePageMemory(copy, copySize);
    return func;
  else
    for i,location in pairs(retchecks) do
      writeByte(location, 0xEB);
    end
  end
  
  table.insert(retcheck.functions, copy);
  return copy;
end


if (dump_bytecode) then
  MessageBox("Press OK to hook the deserializer");
  
  createDetour(r_deserialize, function(data)
    local len = readInt(data.ebp + 20);
    local str = readString(readInt(data.ebp + 16), len);
    print("bytecode size: " ..toString(len));
    print(toString(string.len(str)));
    saveFile("C:/Users/Javan/Desktop/bytecode_example.bin", str);
    endDetour();
  end)

else
  local bytecode_str = readRawHttp("https://github.com/thedoomed/MemLua/blob/master/bytecode_example.bin?raw=true");
  local bytecode_size = string.len(bytecode_str);
  local bytecode = allocPageMemory(bytecode_size);
  
  print("Writing bytecode...");
  for i=1,bytecode_size do
    writeByte(bytecode + (i - 1), bytecode_str:byte(i, i));
  end
  
  -- we must indicate that this bytecode is a const
  -- by specifying the length towards the end
  writeInt(bytecode + bytecode_size + 4 + (bytecode_size % 4), bytecode_size);
  
  local rL = debugRegister(r_gettop, "ebp", 8);
  print("Lua State: " .. toString(rL));
  
  invokeFunc(r_deserialize, rL, "@LBI", bytecode, bytecode_size);
  invokeFunc(r_spawn, rL);
end
