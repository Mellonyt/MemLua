// Here is how we find the deserializer, and then hook it to obtain any bytecode from the game!
// 
// Steps:
//   1. Simply create a new LocalScript, write your Lua code in it, and put it in Lighting.
//      Make sure the Disabled property is set.
//      Now, hit File -> Publish to Roblox.
//      
//   2. Build your Bytecode dumping DLL using this source code by
//      going into Visual Studios, creating a new, non-empty DLL project.
//      Under `Source Files`, delete the CPP file named after this project's
//      name, and paste this entire source code into dllmain.cpp (replacing
//      any existing code).
//      Build the project under Release mode, x86. Do not set it to Debug or run in x64.
//      
//   3. Join your game, now that theres a LocalScript in Lighting.
//      Press F9 to open the Dev Console, /after/ you inject the DLL.
//      Follow what the console says that will pop up.
//      We're going to use a little script
//      
//   4. When the DLL is waiting for a bytecode,
//      we have to be quick to run the LocalScript we wrote in our game
//      so that it can trace the bytecode of it, /before/ memcheck disconnects us.
//      We have just enough time however, so immediately run this script in
//      the Dev Console when the DLL is ready/waiting for a bytecode to run:
//      x=game.Lighting.LOCALSCRIPT_NAME:Clone() x.Parent=workspace.YOURUSERNAME x.Disabled=false
//      
//   5. copy and paste the entire bytecode displayed on the DLL console.
//      
//   6. check out my second project, `BytecodeRunner.cpp` which is used in a new DLL project
//      to run the bytecode! Also, don't forget to subscribe! :sunglasses:
//      
// anyways, now for the source code:
//
