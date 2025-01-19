# Toy Compiler for the Nintendo DSi 

Based on https://github.com/PixelRifts/mini-frontend

## Testing

1. Clone the Repository
2. Run the build script (It exists for windows, but not for linux. The code should work as-is though, there's just no buildscript for it)
3. Run the exe with your code file as the first argument, this will produce an "out.asm" file.
4. Use an assembler to assemble the .asm file into a .nds. See the build_out.bat file for the command using the vasm assembler.
5. Run the nds file on any emulator. Has been tested with DESMUME and the actual Nintendo DSi.


## Benchmarking
I am no expert when it comes to benchmarking, so if you have any obvious improvements to this system, I am open to hear them. That being said, here's what we have for now.  

To test the roms created by the compiler, I use DesMuMe, which has it's own LUA [Scripting API](https://fceux.com/web/help/LuaFunctionsList.html).  
The default.lua script can be run on DesMuMe and it will create a "temp.txt" file which contains frametimes for 4 runs.
Every run will capture the average time for 1000 frames.


## Current measurements
Running on my laptop with no FrameTime limit and Advanced Bus Timing enabled.

### Laptop Specs:
Processor: 12th Gen Intel(R) Core(TM) i5-12450H   2.00 GHz  (12 Cores)  
RAM:       16.0 GB  
GPU:       NVIDIA GeForce RTX 2050  

I don't think anything else is required for reproducing these results


### Stage 0
Unoptimised Output (Just a dumb stack machine)  
  
Frame 0 Average: 0.095 s  
Frametime Average: 0.00739675 s  
