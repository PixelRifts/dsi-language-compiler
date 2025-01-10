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
write_char:
  str r14, [sp], #4
  str r12, [sp], #4
  sub r12, sp, #4
  add sp, sp, #20 ; locals
  mov r0, #109051904
  str r0, [sp], #4
  add r0, r12, #-16
  ldr r0, [r0]
  str r0, [sp], #4
  add r0, r12, #-12
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #256
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  mul r2, r0, r1
  str r2, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  mov r0, #2
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  mul r2, r0, r1
  str r2, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #4]
  mov r0, #63
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #8]
  add r0, r12, #-8
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #8
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  mul r2, r0, r1
  str r2, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #12]
  mov r0, #7
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #16]
  ldr r0, Font
  mov r1, r0
  mov r0, sp
  mov r2, #768
  bl memcpy
  mov sp, r0
  add r0, r12, #12
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  mov r1, #1
  sub sp, sp, #768
  mov r2, sp
  mla r3, r0, r1, r2
  ldr r0, [r3]
  strb r0, [sp], #1
  ldrb r0, [sp, #-1]!
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #20]
  ; while condition
write_char_0:
  add r0, r12, #8
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  cmp r0, #0
  beq write_char_1
  ; if condition
  add r0, r12, #20
  ldr r0, [r0]
  str r0, [sp], #4
  add r0, r12, #16
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  mov r0, r0, lsr r1
  str r0, [sp], #4
  mov r0, #1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  and r0, r0, r1
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  cmp r0, #0
  beq write_char_3
write_char_2:
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #32480
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  strh r0, [sp], #2
  ldrh r1, [sp, #-2]!
  ldr r0, [sp, #-4]!
  strh r1, [r0]
  b write_char_4
write_char_3:
write_char_4:
  ; if condition
  add r0, r12, #8
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #8
  str r0, [sp], #4
  mov r0, #0x4000298
  ldr r1, [sp, #-4]!
  str r1, [r0]
  mov r0, #0x4000290
  ldr r1, [sp, #-4]!
  str r1, [r0]
  mov r0, #0x4000280
  mov r1, #0x00000000
  str r1, [r0]
write_char_8:
  ldr r1, [r0]
  mov r1, r1, lsr #15
  cmp r1, #1
  beq write_char_8
  mov r0, #0x40002A8
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  cmp r0, #0
  beq write_char_6
write_char_5:
  add r0, r12, #4
  str r0, [sp], #4
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  mov r0, #2
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
  add r0, r12, #16
  str r0, [sp], #4
  add r0, r12, #16
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
  b write_char_7
write_char_6:
  add r0, r12, #4
  str r0, [sp], #4
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  mov r0, #498
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
  add r0, r12, #12
  str r0, [sp], #4
  add r0, r12, #12
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
  add r0, r12, #20
  str r0, [sp], #4
  ldr r0, Font
  mov r1, r0
  mov r0, sp
  mov r2, #768
  bl memcpy
  mov sp, r0
  add r0, r12, #12
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  mov r1, #1
  sub sp, sp, #768
  mov r2, sp
  mla r3, r0, r1, r2
  ldr r0, [r3]
  strb r0, [sp], #1
  ldrb r0, [sp, #-1]!
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
  add r0, r12, #16
  str r0, [sp], #4
  mov r0, #7
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
write_char_7:
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
  b write_char_0
write_char_1:
  ; if condition
  add r0, r12, #20
  ldr r0, [r0]
  str r0, [sp], #4
  add r0, r12, #16
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  mov r0, r0, lsr r1
  str r0, [sp], #4
  mov r0, #1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  and r0, r0, r1
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  cmp r0, #0
  beq write_char_10
write_char_9:
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #32480
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  strh r0, [sp], #2
  ldrh r1, [sp, #-2]!
  ldr r0, [sp, #-4]!
  strh r1, [r0]
  b write_char_11
write_char_10:
write_char_11:
_write_char_ret_block:
  sub sp, sp, #20
  ldr r12, [sp, #-4]!
  ldr r14, [sp, #-4]!
  bx lr
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
  mov r0, #0
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #4]
  mov r0, #20
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  str r0, [r12, #8]
  ; while condition
main_0:
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #11
  str r0, [sp], #4
  ldr r2, [sp, #-4]!
  ldr r1, [sp, #-4]!
  mov r0, #0
  cmp r1, r2
  mov r1, r0
  movlt r1, #1
  strb r1, [sp], #1
  ldrb r0, [sp, #-1]!
  cmp r0, #0
  beq main_1
  add r0, r12, #8
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #40
  str r0, [sp], #4
  ldr r0, disp_text
  mov r1, r0
  mov r0, sp
  mov r2, #44
  bl memcpy
  mov sp, r0
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  ldr r0, [sp, #-4]!
  mov r1, #4
  sub sp, sp, #44
  mov r2, sp
  mla r3, r0, r1, r2
  ldr r0, [r3]
  str r0, [sp], #4
  bl write_char
  sub sp, sp, #12
  add r0, r12, #8
  str r0, [sp], #4
  add r0, r12, #8
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #8
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  str r1, [r0]
  add r0, r12, #4
  str r0, [sp], #4
  add r0, r12, #4
  ldr r0, [r0]
  str r0, [sp], #4
  mov r0, #1
  str r0, [sp], #4
  ldr r1, [sp, #-4]!
  ldr r0, [sp, #-4]!
  add r0, r0, r1
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
Font:
  .long _Font_data
_Font_data:
  .byte 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x10, 0x18, 0x18, 0x18, 0x18, 0x0, 0x18, 0x0, 0x28, 0x6C, 0x28, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x28, 0x7C, 0x28, 0x7C, 0x28, 0x0, 0x0, 0x18, 0x3E, 0x48, 0x3C, 0x12, 0x7C, 0x18, 0x0, 0x2, 0xC4, 0xC8, 0x10, 0x20, 0x46, 0x86, 0x0, 0x10, 0x28, 0x28, 0x72, 0x94, 0x8C, 0x72, 0x0, 0xC, 0x1C, 0x30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x18, 0x30, 0x30, 0x30, 0x18, 0x18, 0x0, 0x18, 0x18, 0xC, 0xC, 0xC, 0x18, 0x18, 0x0, 0x8, 0x49, 0x2A, 0x1C, 0x14, 0x22, 0x41, 0x0, 0x0, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x18, 0x30, 0x0, 0x0, 0x0, 0x7E, 0x7E, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18, 0x18, 0x0, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x0, 0x7C, 0xC6, 0xD6, 0xD6, 0xD6, 0xC6, 0x7C, 0x0, 0x10, 0x18, 0x18, 0x18, 0x18, 0x18, 0x8, 0x0, 0x3C, 0x7E, 0x6, 0x3C, 0x60, 0x7E, 0x3C, 0x0, 0x3C, 0x7E, 0x6, 0x1C, 0x6, 0x7E, 0x3C, 0x0, 0x18, 0x3C, 0x64, 0xCC, 0x7C, 0xC, 0x8, 0x0, 0x3C, 0x7E, 0x60, 0x7C, 0x6, 0x7E, 0x3E, 0x0, 0x3C, 0x7E, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x0, 0x3C, 0x7E, 0x6, 0xC, 0x18, 0x18, 0x10, 0x0, 0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x0, 0x3C, 0x66, 0x66, 0x3E, 0x6, 0x7E, 0x3C, 0x0, 0x0, 0x0, 0x18, 0x18, 0x0, 0x18, 0x18, 0x0, 0x0, 0x0, 0x18, 0x18, 0x0, 0x18, 0x18, 0x30, 0xC, 0x1C, 0x38, 0x60, 0x38, 0x1C, 0xC, 0x0, 0x0, 0x0, 0x7E, 0x0, 0x0, 0x7E, 0x0, 0x0, 0x60, 0x70, 0x38, 0xC, 0x38, 0x70, 0x60, 0x0, 0x3C, 0x76, 0x6, 0x1C, 0x0, 0x18, 0x18, 0x0, 0x7C, 0xCE, 0xA6, 0xB6, 0xC6, 0xF0, 0x7C, 0x0, 0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x24, 0x0, 0x3C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x3C, 0x0, 0x38, 0x7C, 0xC0, 0xC0, 0xC0, 0x7C, 0x38, 0x0, 0x3C, 0x64, 0x66, 0x66, 0x66, 0x64, 0x38, 0x0, 0x3C, 0x7E, 0x60, 0x78, 0x60, 0x7E, 0x3C, 0x0, 0x38, 0x7C, 0x60, 0x78, 0x60, 0x60, 0x20, 0x0, 0x3C, 0x66, 0xC0, 0xC0, 0xCC, 0x66, 0x3C, 0x0, 0x24, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x24, 0x0, 0x10, 0x18, 0x18, 0x18, 0x18, 0x18, 0x8, 0x0, 0x8, 0xC, 0xC, 0xC, 0x4C, 0xFC, 0x78, 0x0, 0x24, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x24, 0x0, 0x20, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x3E, 0x0, 0x44, 0xEE, 0xFE, 0xD6, 0xD6, 0xD6, 0x44, 0x0, 0x44, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0x44, 0x0, 0x38, 0x6C, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x0, 0x38, 0x6C, 0x64, 0x7C, 0x60, 0x60, 0x20, 0x0, 0x38, 0x6C, 0xC6, 0xC6, 0xCA, 0x74, 0x3A, 0x0, 0x3C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x26, 0x0, 0x3C, 0x7E, 0x60, 0x3C, 0x6, 0x7E, 0x3C, 0x0, 0x3C, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x8, 0x0, 0x24, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0, 0x24, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x0, 0x44, 0xC6, 0xD6, 0xD6, 0xFE, 0xEE, 0x44, 0x0, 0xC6, 0x6C, 0x38, 0x38, 0x6C, 0xC6, 0x44, 0x0, 0x24, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x8, 0x0, 0x7C, 0xFC, 0xC, 0x18, 0x30, 0x7E, 0x7C, 0x0, 0x1C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1C, 0x0, 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x0, 0x38, 0xC, 0xC, 0xC, 0xC, 0xC, 0x38, 0x0, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x0, 0x0, 0x38, 0xC, 0x7C, 0xCC, 0x78, 0x0, 0x20, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x0, 0x0, 0x0, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x0, 0x8, 0xC, 0x7C, 0xCC, 0xCC, 0xCC, 0x78, 0x0, 0x0, 0x0, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x0, 0x1C, 0x36, 0x30, 0x38, 0x30, 0x30, 0x10, 0x0, 0x0, 0x0, 0x3C, 0x66, 0x66, 0x3E, 0x6, 0x3C, 0x20, 0x60, 0x6C, 0x76, 0x66, 0x66, 0x24, 0x0, 0x18, 0x0, 0x18, 0x18, 0x18, 0x18, 0x8, 0x0, 0x6, 0x0, 0x4, 0x6, 0x6, 0x26, 0x66, 0x3C, 0x20, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x26, 0x0, 0x10, 0x18, 0x18, 0x18, 0x18, 0x18, 0x8, 0x0, 0x0, 0x0, 0x6C, 0xFE, 0xD6, 0xD6, 0xC6, 0x0, 0x0, 0x0, 0x3C, 0x66, 0x66, 0x66, 0x24, 0x0, 0x0, 0x0, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x0, 0x0, 0x0, 0x3C, 0x66, 0x66, 0x7C, 0x60, 0x20, 0x0, 0x0, 0x78, 0xCC, 0xCC, 0x7C, 0xC, 0x8, 0x0, 0x0, 0x38, 0x7C, 0x60, 0x60, 0x20, 0x0, 0x0, 0x0, 0x3C, 0x60, 0x3C, 0x6, 0x7C, 0x0, 0x10, 0x30, 0x3C, 0x30, 0x30, 0x3E, 0x1C, 0x0, 0x0, 0x0, 0x24, 0x66, 0x66, 0x66, 0x3C, 0x0, 0x0, 0x0, 0x24, 0x66, 0x66, 0x3C, 0x18, 0x0, 0x0, 0x0, 0x44, 0xD6, 0xD6, 0xFE, 0x6C, 0x0, 0x0, 0x0, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x0, 0x0, 0x0, 0x24, 0x66, 0x66, 0x3E, 0x6, 0x7C, 0x0, 0x0, 0x7E, 0xC, 0x18, 0x30, 0x7E, 0x0, 0x8, 0x8, 0x8, 0x8, 0x56, 0x55, 0x57, 0x74, 0x18, 0x4, 0x8, 0x1C, 0x56, 0x55, 0x57, 0x74, 0x0, 0x0, 0x0, 0x0, 0x7E, 0x7E, 0xFF, 0xFF, 0x18, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x7E, 0xFF, 0x22, 0x77, 0x7F, 0x7F, 0x3E, 0x1C, 0x8, 0x0
disp_text:
  .long _disp_text_data
_disp_text_data:
  .byte 0x48, 0x0, 0x0, 0x0, 0x45, 0x0, 0x0, 0x0, 0x4C, 0x0, 0x0, 0x0, 0x4C, 0x0, 0x0, 0x0, 0x4F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x57, 0x0, 0x0, 0x0, 0x4F, 0x0, 0x0, 0x0, 0x52, 0x0, 0x0, 0x0, 0x4C, 0x0, 0x0, 0x0, 0x44, 0x0, 0x0, 0x0
Arm9_End: