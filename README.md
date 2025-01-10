# Toy Compiler for the Nintendo DSi 

Based on https://github.com/PixelRifts/mini-frontend

## Testing

1. Clone the Repository
2. Run the build script (It exists for windows, but not for linux. The code should work as-is though, there's just no buildscript for it)
3. Run the exe with your code file as the first argument, this will produce an "out.asm" file.
4. Use an assembler to assemble the .asm file into a .nds. See the build_out.bat file for the command using the vasm assembler.
5. Run the nds file on any emulator. Has been tested with DESMUME and the actual Nintendo DSi.


