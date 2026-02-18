.org 0x02000000-0x8000  ;&200 byte header - code starts at offset &8000
HeaderStart:
  ;instr                         offset  size  desc
  .ascii "COMPILEDTEST"       ;000h    12    Game Title  (Uppercase ASCII, padded with 00h)
  .ascii "0000"               ;00Ch    4     Gamecode    (Uppercase ASCII, NTR-<code>)        (0=homebrew)
  .ascii "00"                 ;010h    2     Makercode   (Uppercase ASCII, eg. "01"=Nintendo) (0=homebrew)
  .byte 3                       ;012h    1     Unitcode    (00h=NDS, 02h=NDS+DSi, 03h=DSi) (bit1=DSi)
  .byte 0                       ;013h    1     Encryption Seed Select (00..07h, usually 00h)
  .byte 0                       ;014h    1     Devicecapacity         (Chipsize = 128KB SHL nn) (eg. 7 = 16MB)
  .space 7                      ;015h    7     Reserved    (zero filled)  
  .byte 0                       ;01Ch    1     Reserved    (zero)                      (except, used on DSi)
  .byte 0                       ;01Dh    1     NDS Region  (00h=Normal, 80h=China, 40h=Korea) (other on DSi)
  .byte 0                       ;01Eh    1     ROM Version (usually 00h)
  .byte 4                       ;01Fh    1     Autostart (Bit2: Skip "Press Button" after Health and Safety)
  .long Arm9_Start-HeaderStart  ;020h    4     ARM9 rom_offset    (4000h and up, align 1000h)
  .long 0x02000000              ;024h    4     ARM9 entry_address (2000000h..23BFE00h)
  .long 0x02000000              ;028h    4     ARM9 ram_address   (2000000h..23BFE00h)
  .long Arm9_End-Arm9_Start     ;02Ch    4     ARM9 size          (max 3BFE00h) (3839.5KB)
  .long Arm7_Start-HeaderStart  ;030h    4     ARM7 rom_offset    (8000h and up)
  .long 0x03800000              ;034h    4     ARM7 entry_address (2000000h..23BFE00h, or 37F8000h..3807E00h)
  .long 0x03800000              ;038h    4     ARM7 ram_address   (2000000h..23BFE00h, or 37F8000h..3807E00h)
  .long Arm7_End-Arm7_Start     ;03Ch    4     ARM7 size          (max 3BFE00h, or FE00h) (3839.5KB, 63.5KB)
  .long 0                       ;040h    4     File Name Table (FNT) offset
  .long 0                       ;044h    4     File Name Table (FNT) size
  .long 0                       ;048h    4     File Allocation Table (FAT) offset
  .long 0                       ;04Ch    4     File Allocation Table (FAT) size
  .long 0                       ;050h    4     File ARM9 overlay_offset
  .long 0                       ;054h    4     File ARM9 overlay_size
  .long 0                       ;058h    4     File ARM7 overlay_offset
  .long 0                       ;05Ch    4     File ARM7 overlay_size
  .long 0x00586000              ;060h    4     Port 40001A4h setting for normal commands (usually 00586000h)
  .long 0x001808F8              ;064h    4     Port 40001A4h setting for KEY1 commands   (usually 001808F8h)
  .long 0                       ;068h    4     Icon/Title offset (0=None) (8000h and up)
  .word 0                       ;06Ch    2     Secure Area Checksum, CRC-16 of [[020h]..00007FFFh]
  .word 0                       ;06Eh    2     Secure Area Delay (in 131kHz units) (051Eh=10ms or 0D7Eh=26ms)
  .long 0                       ;070h    4     ARM9 Auto Load List Hook RAM Address (?) ;ndaddr of auto-load
  .long 0                       ;074h    4     ARM7 Auto Load List Hook RAM Address (?) ;/functions
  .space 8                      ;078h    8     Secure Area Disable (by encrypted "NmMdOnly") (usually zero)
  .long 0                       ;080h    4     Total Used ROM size (remaining/unused bytes usually FFh-padded)
  .long 0x4000                  ;084h    4     ROM Header Size (4000h)
  .space 0x28                   ;088h    28h   Reserved (zero filled; except, [88h..93h] used on DSi)
  .space 0x10                   ;0B0h    10h   Reserved (zero filled; or "DoNotZeroFillMem"=unlaunch fastboot)
  .space 0x9C                   ;0C0h    9Ch   Nintendo Logo (compressed bitmap, same as in GBA Headers)
  .word 0                       ;15Ch    2     Nintendo Logo Checksum, CRC-16 of [0C0h-15Bh], fixed CF56h
  .word 0                       ;15Eh    2     Header Checksum, CRC-16 of [000h-15Dh]
  .long 0                       ;160h    4     Debug rom_offset   (0=none) (8000h and up)       ;only if debug
  .long 0                       ;164h    4     Debug size         (0=none) (max 3BFE00h)        ;version with
  .long 0                       ;168h    4     Debug ram_address  (0=none) (2400000h..27BFE00h) ;SIO and 8MB
  .long 0                       ;16Ch    4     Reserved (zero filled) (transferred, and stored, but not used)
  .space 0x90                   ;170h    90h   Reserved (zero filled) (transferred, but not stored in RAM)
  .space 0x7E00-4

Arm7_Start:
  b Arm7_Start		              ;Infloop for ARM 7
Arm7_End:
Arm9_Start:
  mov sp,  #0x02F00000          ;Start of RAM region
  mov r12, #0x02F00000          ;Static base memory pointer
  bl main
inbuilt_infloop:
  b inbuilt_infloop
memcpy:
  cmp     r2, #1
  bxlt    lr
memcpy_1:
  ldrb    r3, [r1], #1
  strb    r3, [r0], #1
  subs    r2, r2, #1
  bne     memcpy_1
  bx      lr
main:
  str r14, [sp], #4
  str r12, [sp], #4
  sub r12, sp, #4
  add sp, sp, #8 ; locals
  mov r0, #32771
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  mov r0, #67109636
  str r1, [r0]
  mov r0, #131072
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  mov r0, #67108864
  str r1, [r0]
  mov r0, #128
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  strb r0, [sp], #1
  ldrb r1, [sp, #-1]!
  mov r0, #67109440
  str r1, [r0]
  mov r0, #109051904
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #4]
  mov r0, #256
  str r0, [sp], #4
  mov r0, #192
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  mul r2, r0, r1
  str r2, [sp], #4
  mov r0, #235
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  mov r0, #127
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #8]
  ; while condition
main_0:
  add r0, r12, #8
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  cmp r0, #0
  beq main_1
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #543
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  strh r0, [sp], #2
  ldrh r1, [sp, #-2]!
  ldr r0, [sp, #-4]!
  strh r1, [r0]
  add r0, r12, #4
  str r0, [sp], #4
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #2
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
  add r0, r12, #8
  str r0, [sp], #4
  add r0, r12, #8
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  sub r0, r0, r1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
  b main_0
main_1:
_main_ret_block:
  sub sp, sp, #8
  ldr r12, [sp, #-4]!
  ldr r14, [sp, #-4]!
  bx lr
Arm9_End: