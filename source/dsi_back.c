#include "dsi_back.h"
#include <assert.h>

#define BackendError(c, t, f, ...)\
Statement(\
if (!c->errored) printf("%.*s:%d:%d - DSI Backend Error - " f,\
str_expand(c->filename), t.line, t.col,\
##__VA_ARGS__);\
c->errored = true;\
return;\
)

DArray_Impl(DSIBackendSymbol)

#define TypeGet(b, idx) (b->type_cache->elems[idx])

#define scope_push(o)   ((o)->scope++, (o)->symbols.len)
#define scope_pop(o, p)\
Statement(\
(o)->scope--;\
(o)->symbols.len = p;\
)

static b8 backend_symbol_lookup(darray(DSIBackendSymbol) syms, string id, u32 min_scope, u32* idx) {
  for (i32 i = syms.len-1; i >= 0; i--) {
    if (syms.elems[i].scope < min_scope) break;
    if (str_eq(syms.elems[i].ident.lexeme, id)) {
      if (idx) *idx = i;
      return true;
    }
  }
  return false;
}

static b8 is_power_of_two(uintptr_t x) {
  return (x & (x-1)) == 0;
}

static u64 align_forward_u64(u64 ptr, u64 align) {
  u64 p, a, modulo;
  
  assert(is_power_of_two(align));
  
  p = ptr;
  a = (size_t)align;
  // Same as (p % a) but faster as 'a' is a power of two
  modulo = p & (a-1);
  
  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    p += a - modulo;
  }
  return p;
}


//~ Helpers

static void write(DSIBackend* b, string s) {
  string_list_push(&b->static_arena, &b->instructions, s);
}

static void write_data(DSIBackend* b, string s) {
  string_list_push(&b->static_arena, &b->data, s);
}

static void write_formatted(DSIBackend* b, string s) {
  string_list_push(&b->static_arena, &b->instructions, str_from_format(&b->static_arena, "  %.*s\n", str_expand(s)));
}

static string get_next_label(DSIBackend* b) {
  return str_from_format(&b->static_arena, "%.*s_%d", str_expand(b->func_name), b->label_id++);
}

// Push onto the stack, the value in register reg
// For Arrays, Structs, Unions, reg will contain starting address.
static void push(DSIBackend* b, i32 reg, ValueType* typ) {
  switch (typ->type) {
    case TK_Int:
    case TK_Bool:
    case TK_Pointer:
    case TK_Func: {
      if (typ->size == 4) {
        write(b,
              str_from_format(&b->static_arena, "  str r%d, [sp], #4\n", reg));
      } else if (typ->size == 2) {
        write(b,
              str_from_format(&b->static_arena, "  strh r%d, [sp], #2\n", reg));
      } else if (typ->size == 1) {
        write(b,
              str_from_format(&b->static_arena, "  strb r%d, [sp], #1\n", reg));
      }
      b->offset += typ->size;
    } break;
    
    case TK_Void: break;
    
    case TK_Array:
    case TK_Struct:
    case TK_Union: {
      write(b, str_from_format(&b->static_arena, "  mov r1, r%d\n", reg));
      write(b, str_lit("  mov r0, sp\n"));
      write(b, str_from_format(&b->static_arena, "  mov r2, #%llu\n", typ->size));
      write(b, str_lit("  bl memcpy\n"));
      write(b, str_lit("  mov sp, r0\n")); // r0 contains incremented address
      b->offset += typ->size;
    } break;
    
    default: unreachable;
  }
}

// Push onto the stack, the value POINTED TO by register reg
// For Arrays, Structs, Unions, same as push()
static void push_indirect(DSIBackend* b, i32 reg, ValueType* typ) {
  switch (typ->type) {
    case TK_Int:
    case TK_Bool:
    case TK_Pointer:
    case TK_Func: {
      const char* extra = "";
      if (typ->type == TK_Int && typ->int_t.is_signed) extra = "s";
      
      if (typ->size == 4) {
        write(b, str_from_format(&b->static_arena, "  ldr r%d, [r%d]\n", reg, reg));
        write(b,
              str_from_format(&b->static_arena, "  str r%d, [sp], #4\n", reg));
      } else if (typ->size == 2) {
        write(b, str_from_format(&b->static_arena, "  ldr%sh r%d, [r%d]\n", extra, reg, reg));
        write(b,
              str_from_format(&b->static_arena, "  strh r%d, [sp], #2\n", reg));
      } else if (typ->size == 1) {
        write(b, str_from_format(&b->static_arena, "  ldr%sb r%d, [r%d]\n", extra, reg, reg));
        write(b,
              str_from_format(&b->static_arena, "  strb r%d, [sp], #1\n", reg));
      }
      
      b->offset += typ->size;
    } break;
    
    case TK_Void: break;
    
    case TK_Array:
    case TK_Struct:
    case TK_Union: {
      write(b, str_from_format(&b->static_arena, "  mov r1, r%d\n", reg));
      write(b, str_lit("  mov r0, sp\n"));
      write(b, str_from_format(&b->static_arena, "  mov r2, #%llu\n", typ->size));
      write(b, str_lit("  bl memcpy\n"));
      write(b, str_lit("  mov sp, r0\n")); // r0 contains incremented address
      b->offset += typ->size;
    } break;
    
    default: unreachable;
  }
}

// Puts value in 'from' register into address pointed to by 'to' register
static void put(DSIBackend* b, i32 from, i32 to, ValueType* typ) {
  switch (typ->type) {
    case TK_Int:
    case TK_Bool:
    case TK_Pointer:
    case TK_Func: {
      if (typ->size == 4) {
        write(b, str_from_format(&b->static_arena, "  str r%d, [r%d]\n", from, to));
      } else if (typ->size == 2) {
        write(b, str_from_format(&b->static_arena, "  strh r%d, [r%d]\n", from, to));
      } else if (typ->size == 1) {
        write(b, str_from_format(&b->static_arena, "  strb r%d, [r%d]\n", from, to));
      }
    } break;
    
    case TK_Void: break;
    
    case TK_Array:
    case TK_Struct:
    case TK_Union: {
      write(b, str_from_format(&b->static_arena, "  mov r11, r%d\n", from));
      write(b, str_from_format(&b->static_arena, "  mov r0,  r%d\n", to));
      write(b, str_lit("  mov r1,  r11\n"));
      write(b, str_from_format(&b->static_arena, "  mov r2,  #%llu\n", typ->size));
      write(b, str_lit("  bl memcpy\n"));
      write(b, str_lit("  mov sp, r0\n")); // r0 contains incremented address
    } break;
    
    default: unreachable;
  }
}

static void pop(DSIBackend* b, i32 reg, ValueType* typ) {
  switch (typ->type) {
    case TK_Int:
    case TK_Bool:
    case TK_Pointer:
    case TK_Func: {
      const char* extra = "";
      if (typ->type == TK_Int && typ->int_t.is_signed) extra = "s";
      
      if (typ->size == 4) {
        write(b,
              str_from_format(&b->static_arena, "  ldr r%d, [sp, #-4]!\n", reg));
      } else if (typ->size == 2) {
        write(b,
              str_from_format(&b->static_arena, "  ldr%sh r%d, [sp, #-2]!\n", extra, reg));
      } else if (typ->size == 1) {
        write(b,
              str_from_format(&b->static_arena, "  ldr%sb r%d, [sp, #-1]!\n", extra, reg));
      }
      
      b->offset -= typ->size;
    } break;
    
    case TK_Void: return;
    
    case TK_Array:
    case TK_Struct:
    case TK_Union: {
      write(b, str_from_format(&b->static_arena, "  sub sp, sp, #%llu\n", typ->size));
      write(b, str_from_format(&b->static_arena, "  mov r%d, sp\n", reg));
      b->offset -= typ->size;
    } break;
    
    default: printf("Hit %d\n", typ->type); unreachable;
  }
}

//~ Basic Stuff

static void DSIBack_PushHeader(DSIBackend* b) {
  string s = str_lit(".org 0x02000000-0x8000  ;&200 byte header - code starts at offset &8000\n"
                     "HeaderStart:\n"
                     "  ;instr                         offset  size  desc\n"
                     "  .ascii \"COMPILEDTEST\"       ;000h    12    Game Title  (Uppercase ASCII, padded with 00h)\n"
                     "  .ascii \"0000\"               ;00Ch    4     Gamecode    (Uppercase ASCII, NTR-<code>)        (0=homebrew)\n"
                     "  .ascii \"00\"                 ;010h    2     Makercode   (Uppercase ASCII, eg. \"01\"=Nintendo) (0=homebrew)\n"
                     "  .byte 3                       ;012h    1     Unitcode    (00h=NDS, 02h=NDS+DSi, 03h=DSi) (bit1=DSi)\n"
                     "  .byte 0                       ;013h    1     Encryption Seed Select (00..07h, usually 00h)\n"
                     "  .byte 0                       ;014h    1     Devicecapacity         (Chipsize = 128KB SHL nn) (eg. 7 = 16MB)\n"
                     "  .space 7                      ;015h    7     Reserved    (zero filled)  \n"
                     "  .byte 0                       ;01Ch    1     Reserved    (zero)                      (except, used on DSi)\n"
                     "  .byte 0                       ;01Dh    1     NDS Region  (00h=Normal, 80h=China, 40h=Korea) (other on DSi)\n"
                     "  .byte 0                       ;01Eh    1     ROM Version (usually 00h)\n"
                     "  .byte 4                       ;01Fh    1     Autostart (Bit2: Skip \"Press Button\" after Health and Safety)\n"
                     "  .long Arm9_Start-HeaderStart  ;020h    4     ARM9 rom_offset    (4000h and up, align 1000h)\n"
                     "  .long 0x02000000              ;024h    4     ARM9 entry_address (2000000h..23BFE00h)\n"
                     "  .long 0x02000000              ;028h    4     ARM9 ram_address   (2000000h..23BFE00h)\n"
                     "  .long Arm9_End-Arm9_Start     ;02Ch    4     ARM9 size          (max 3BFE00h) (3839.5KB)\n"
                     "  .long Arm7_Start-HeaderStart  ;030h    4     ARM7 rom_offset    (8000h and up)\n"
                     "  .long 0x03800000              ;034h    4     ARM7 entry_address (2000000h..23BFE00h, or 37F8000h..3807E00h)\n"
                     "  .long 0x03800000              ;038h    4     ARM7 ram_address   (2000000h..23BFE00h, or 37F8000h..3807E00h)\n"
                     "  .long Arm7_End-Arm7_Start     ;03Ch    4     ARM7 size          (max 3BFE00h, or FE00h) (3839.5KB, 63.5KB)\n"
                     "  .long 0                       ;040h    4     File Name Table (FNT) offset\n"
                     "  .long 0                       ;044h    4     File Name Table (FNT) size\n"
                     "  .long 0                       ;048h    4     File Allocation Table (FAT) offset\n"
                     "  .long 0                       ;04Ch    4     File Allocation Table (FAT) size\n"
                     "  .long 0                       ;050h    4     File ARM9 overlay_offset\n"
                     "  .long 0                       ;054h    4     File ARM9 overlay_size\n"
                     "  .long 0                       ;058h    4     File ARM7 overlay_offset\n"
                     "  .long 0                       ;05Ch    4     File ARM7 overlay_size\n"
                     "  .long 0x00586000              ;060h    4     Port 40001A4h setting for normal commands (usually 00586000h)\n"
                     "  .long 0x001808F8              ;064h    4     Port 40001A4h setting for KEY1 commands   (usually 001808F8h)\n"
                     "  .long 0                       ;068h    4     Icon/Title offset (0=None) (8000h and up)\n"
                     "  .word 0                       ;06Ch    2     Secure Area Checksum, CRC-16 of [[020h]..00007FFFh]\n"
                     "  .word 0                       ;06Eh    2     Secure Area Delay (in 131kHz units) (051Eh=10ms or 0D7Eh=26ms)\n"
                     "  .long 0                       ;070h    4     ARM9 Auto Load List Hook RAM Address (?) ;\endaddr of auto-load\n"
                     "  .long 0                       ;074h    4     ARM7 Auto Load List Hook RAM Address (?) ;/functions\n"
                     "  .space 8                      ;078h    8     Secure Area Disable (by encrypted \"NmMdOnly\") (usually zero)\n"
                     "  .long 0                       ;080h    4     Total Used ROM size (remaining/unused bytes usually FFh-padded)\n"
                     "  .long 0x4000                  ;084h    4     ROM Header Size (4000h)\n"
                     "  .space 0x28                   ;088h    28h   Reserved (zero filled; except, [88h..93h] used on DSi)\n"
                     "  .space 0x10                   ;0B0h    10h   Reserved (zero filled; or \"DoNotZeroFillMem\"=unlaunch fastboot)\n"
                     "  .space 0x9C                   ;0C0h    9Ch   Nintendo Logo (compressed bitmap, same as in GBA Headers)\n"
                     "  .word 0                       ;15Ch    2     Nintendo Logo Checksum, CRC-16 of [0C0h-15Bh], fixed CF56h\n"
                     "  .word 0                       ;15Eh    2     Header Checksum, CRC-16 of [000h-15Dh]\n"
                     "  .long 0                       ;160h    4     Debug rom_offset   (0=none) (8000h and up)       ;only if debug\n"
                     "  .long 0                       ;164h    4     Debug size         (0=none) (max 3BFE00h)        ;version with\n"
                     "  .long 0                       ;168h    4     Debug ram_address  (0=none) (2400000h..27BFE00h) ;SIO and 8MB\n"
                     "  .long 0                       ;16Ch    4     Reserved (zero filled) (transferred, and stored, but not used)\n"
                     "  .space 0x90                   ;170h    90h   Reserved (zero filled) (transferred, but not stored in RAM)\n"
                     "  .space 0x7E00-4\n"
                     "\n"
                     "Arm7_Start:\n"
                     "  b Arm7_Start		              ;Infloop for ARM 7\n"
                     "Arm7_End:\n"
                     "Arm9_Start:\n"
                     "  mov sp,  #0x02F00000          ;Start of RAM region\n"
                     "  mov r12, #0x02F00000          ;Static base memory pointer\n"
                     "  bl main\n"
                     "inbuilt_infloop:\n"
                     "  b inbuilt_infloop\n"
                     "memcpy:\n"
                     "  cmp     r2, #1\n"
                     "  bxlt    lr\n"
                     "memcpy_1:\n"
                     "  ldrb    r3, [r1], #1\n"
                     "  strb    r3, [r0], #1\n"
                     "  subs    r2, r2, #1\n"
                     "  bne     memcpy_1\n"
                     "  bx      lr\n");
  write(b, s);
}

static void DSIBack_WriteAST(DSIBackend* b, ASTNode* node);

static void DSIBack_WriteAddrAST(DSIBackend* b, ASTNode* node) {
  switch (node->type) {
    case NT_Error:
    case NT_Expr_IntLit:
    case NT_Expr_FloatLit:
    case NT_Expr_ArrayLit:
    case NT_Expr_Add:
    case NT_Expr_Sub:
    case NT_Expr_Mul:
    case NT_Expr_Div:
    case NT_Expr_Mod:
    case NT_Expr_ShiftLeft:
    case NT_Expr_ShiftRight:
    case NT_Expr_BitAND:
    case NT_Expr_BitOR:
    case NT_Expr_Identity:
    case NT_Expr_Negate:
    case NT_Expr_Not:
    case NT_Expr_Eq:
    case NT_Expr_Neq:
    case NT_Expr_Less:
    case NT_Expr_Greater:
    case NT_Expr_LessEq:
    case NT_Expr_GreaterEq:
    case NT_Expr_FuncProto:
    case NT_Expr_Func:
    case NT_Expr_Addr:
    case NT_Expr_Call:
    case NT_Expr_Cast:
    case NT_Type_Integer:
    case NT_Type_Float:
    case NT_Type_Void:
    case NT_Type_Func:
    case NT_Type_Struct:
    case NT_Type_Union:
    case NT_Type_Pointer:
    case NT_Type_Array:
    case NT_Stmt_Assign:
    case NT_Stmt_Expr:
    case NT_Stmt_Write:
    case NT_Stmt_Block:
    case NT_Stmt_While:
    case NT_Stmt_If:
    case NT_Stmt_Return:
    case NT_Decl: BackendError(b, node->marker, "Cannot take address\n");
    
    case NT_Expr_Ident: {
      b8 found = false;
      u32 idx = 0;
      for (u32 i = 0; i <= b->scope; i++) {
        if (backend_symbol_lookup(b->symbols, node->ident.lexeme, 0, &idx)) {
          write(b, str_from_format(&b->static_arena, "  add r0, r12, #%lld\n", b->symbols.elems[idx].offset));
          push(b, 0, TypeGet(b, Type_Index_U32));
          found = true;
          break;
        }
      }
      if (!found)
        BackendError(b, node->marker, "Did not find an identifier that was supposed to be registered... %.*s\n", str_expand(node->ident.lexeme));
    } break;
    
    case NT_Expr_Access: {
      write(b, str_lit("  ; Accessing something\n"));
      DSIBack_WriteAddrAST(b, node->access.left);
      write(b, str_lit("  ; from what came before\n"));
      pop(b, 0, TypeGet(b, Type_Index_U32));
      if (node->access.deref)
        write(b, str_lit("  ldr r0, [r0]\n"));
      
      ValueType* str_t = TypeGet(b, node->access.left->expr_type);
      if (node->access.deref) str_t = TypeGet(b, str_t->ptr_t.sub_t);
      
      u64 off = 0;
      u32 i = 0;
      b8 found = false;
      Token_node* curr = str_t->compound_t.member_names.first;
      while (curr) {
        if (str_eq(curr->token.lexeme, node->access.right.lexeme)) {
          off = str_t->compound_t.member_offsets[i];
          found = true;
          break;
        }
        curr = curr->next;
        i++;
      }
      
      if (!found) {
        BackendError(b, node->marker, "Did not find a member that was supposed exist %.*s\n", str_expand(node->access.right.lexeme));
        return;
      }
      
      write(b, str_from_format(&b->static_arena, "  add r0, r0, #%llu\n", off));
      push(b, 0, TypeGet(b, Type_Index_U32));
      
    } break;
    
    case NT_Expr_Deref: {
      DSIBack_WriteAST(b, node->deref);
    } break;
    
    case NT_Expr_Index: {
      DSIBack_WriteAddrAST(b, node->index.left);
      DSIBack_WriteAST(b, node->index.idx);
      write(b, str_from_format(&b->static_arena, "  mov r1, #%llu\n",
                               TypeGet(b, node->expr_type)->size));
      pop(b, 0, TypeGet(b, node->index.idx->expr_type));
      pop(b, 2, TypeGet(b, Type_Index_U32));
      write(b, str_lit("  mla r3, r0, r1, r2\n"));
      push(b, 3, TypeGet(b, Type_Index_U32));
    } break;
    
  }
}

static void DSIBack_WriteAST(DSIBackend* b, ASTNode* node) {
  switch (node->type) {
    case NT_Error: {
      BackendError(b, node->marker, "Got Error Node??\n");
    } break;
    
    case NT_Expr_IntLit: {
      write(b, str_from_format(&b->static_arena, "  mov r0, #%lld\n", node->constant_val.int_lit));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_FloatLit: {
      BackendError(b, node->marker, "Floats are unsupported by the Nintendo DSi");
    } break;
    
    case NT_Expr_ArrayLit: {
      if (node->is_constant) {
        if (node->constant_val.buf_lit.size) {
          write_data(b, str_lit("  .byte "));
          for (u32 i = 0; i < node->constant_val.buf_lit.size; i++) {
            if (i == node->constant_val.buf_lit.size-1) {
              write_data(b, str_from_format(&b->static_arena, "0x%X", node->constant_val.buf_lit.buf[i]));
            } else {
              write_data(b, str_from_format(&b->static_arena, "0x%X, ", node->constant_val.buf_lit.buf[i]));
            }
          }
          write_data(b, str_lit("\n"));
        }
        
      } else {
        // TODO(voxel): 
      }
    } break;
    
    case NT_Expr_Add: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("add r0, r0, r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Sub: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("sub r0, r0, r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Mul: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("mul r2, r0, r1"));
      push(b, 2, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Div: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      
      write(b, str_lit("  mov r0, #0x4000298\n"));
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      write(b, str_lit("  str r1, [r0]\n"));
      
      write(b, str_lit("  mov r0, #0x4000290\n"));
      pop(b, 1, TypeGet(b, node->binary_op.left->expr_type));
      write(b, str_lit("  str r1, [r0]\n"));
      
      write(b, str_lit("  mov r0, #0x4000280\n"));
      write(b, str_lit("  mov r1, #0x0000\n"));
      write(b, str_lit("  strh r1, [r0]\n"));
      
      string awaiting = get_next_label(b);
      
      write(b, str_from_format(&b->static_arena, "%.*s:\n", str_expand(awaiting)));
      write(b, str_lit("  ldr r1, [r0]\n"));
      write(b, str_lit("  mov r1, r1, lsr #15\n"));
      write(b, str_lit("  cmp r1, #1\n"));
      write(b, str_from_format(&b->static_arena, "  beq %.*s\n", str_expand(awaiting)));
      
      write(b, str_lit("  mov r0, #0x40002A0\n"));
      write(b, str_lit("  ldr r0, [r0]\n"));
      push(b, 0, TypeGet(b, node->binary_op.left->expr_type));
    } break;
    
    case NT_Expr_Mod: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      
      write(b, str_lit("  mov r0, #0x4000298\n"));
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      write(b, str_lit("  str r1, [r0]\n"));
      
      write(b, str_lit("  mov r0, #0x4000290\n"));
      pop(b, 1, TypeGet(b, node->binary_op.left->expr_type));
      write(b, str_lit("  str r1, [r0]\n"));
      
      write(b, str_lit("  mov r0, #0x4000280\n"));
      write(b, str_lit("  mov r1, #0x00000000\n"));
      write(b, str_lit("  str r1, [r0]\n"));
      
      string awaiting = get_next_label(b);
      write(b, str_from_format(&b->static_arena, "%.*s:\n", str_expand(awaiting)));
      write(b, str_lit("  ldr r1, [r0]\n"));
      write(b, str_lit("  mov r1, r1, lsr #15\n"));
      write(b, str_lit("  cmp r1, #1\n"));
      write(b, str_from_format(&b->static_arena, "  beq %.*s\n", str_expand(awaiting)));
      
      write(b, str_lit("  mov r0, #0x40002A8\n"));
      write(b, str_lit("  ldr r0, [r0]\n"));
      push(b, 0, TypeGet(b, node->binary_op.left->expr_type));
    } break;
    
    case NT_Expr_ShiftLeft: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("mov r0, r0, lsl r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_ShiftRight: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("mov r0, r0, lsr r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_BitAND: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("and r0, r0, r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_BitOR: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("orr r0, r0, r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Identity: {
      DSIBack_WriteAST(b, node->unary_op.operand);
      /*pop(b, 0);
      push(b, 0);*/
    } break;
    
    case NT_Expr_Negate: {
      DSIBack_WriteAST(b, node->unary_op.operand);
      pop(b, 0, TypeGet(b, node->unary_op.operand->expr_type));
      write_formatted(b, str_lit("rsb r0, r0, #0"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Not: {
      DSIBack_WriteAST(b, node->unary_op.operand);
      pop(b, 0, TypeGet(b, node->unary_op.operand->expr_type));
      write_formatted(b, str_lit("rsbs r1, r0, #0"));
      write_formatted(b, str_lit("adc r0, r0, r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Eq: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("sub r0, r0, r1"));
      write_formatted(b, str_lit("rsbs r1, r0, #0"));
      write_formatted(b, str_lit("adc r0, r0, r1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Neq: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("subs r0, r0, r1"));
      write_formatted(b, str_lit("movne r0, #1"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Less: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 2, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 1, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("mov r0, #0"));
      write_formatted(b, str_lit("cmp r1, r2"));
      write_formatted(b, str_lit("mov r1, r0"));
      write_formatted(b, str_lit("movlt r1, #1"));
      push(b, 1, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Greater: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 2, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 1, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("mov r0, #0"));
      write_formatted(b, str_lit("cmp r1, r2"));
      write_formatted(b, str_lit("mov r1, r0"));
      write_formatted(b, str_lit("movgt r1, #1"));
      push(b, 1, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_LessEq: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 2, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 1, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("mov r0, #0"));
      write_formatted(b, str_lit("cmp r1, r2"));
      write_formatted(b, str_lit("mov r1, r0"));
      write_formatted(b, str_lit("movle r1, #1"));
      push(b, 1, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_GreaterEq: {
      DSIBack_WriteAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 2, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 1, TypeGet(b, node->binary_op.left->expr_type));
      write_formatted(b, str_lit("mov r0, #0"));
      write_formatted(b, str_lit("cmp r1, r2"));
      write_formatted(b, str_lit("mov r1, r0"));
      write_formatted(b, str_lit("movge r1, #1"));
      push(b, 1, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_FuncProto: {
      ASTNode* curr_type = node->proto.arg_types;
      Token_node* curr_name = node->proto.arg_names.first;
      i64 offset = -4;
      
      curr_type = node->proto.arg_types;
      while (curr_type) {
        offset -= TypeGet(b, curr_type->constant_val.type_lit)->size;
        curr_type = curr_type->next;
      }
      
      curr_type = node->proto.arg_types;
      while (curr_type) {
        darray_add(DSIBackendSymbol, &b->symbols,
                   ((DSIBackendSymbol) {
                      .ident=curr_name->token,
                      .usage=Usage_Offset,
                      .scope=b->scope,
                      .offset=offset,
                      .type=curr_type->constant_val.type_lit
                    }));
        
        offset += TypeGet(b, curr_type->constant_val.type_lit)->size;
        curr_name = curr_name->next;
        curr_type = curr_type->next;
      }
      write(b, node->proto.name);
      write(b, str_lit(":\n"));
    } break;
    
    case NT_Expr_Func: {
      b->func_name = node->func.proto->proto.name;
      b->label_id = 0;
      DSIBack_WriteAST(b, node->func.proto);
      push(b, 14, TypeGet(b, Type_Index_U32)); // LR
      push(b, 12, TypeGet(b, Type_Index_U32)); // FP
      write(b, str_lit("  sub r12, sp, #4\n"));
      write(b, str_from_format(&b->static_arena, "  add sp, sp, #%llu ; locals\n",
                               align_forward_u64(node->func.total_local_size, 4)));
      
      u64 point = scope_push(b);
      DSIBack_WriteAST(b, node->func.body);
      scope_pop(b, point);
      
      write(b, str_from_format(&b->static_arena, "_%.*s_ret_block:\n", str_expand(node->func.proto->proto.name)));
      write(b, str_from_format(&b->static_arena, "  sub sp, sp, #%llu\n",
                               align_forward_u64(node->func.total_local_size, 4)));
      pop(b, 12, TypeGet(b, Type_Index_U32));
      pop(b, 14, TypeGet(b, Type_Index_U32));
      
      ASTNode* curr_type = node->func.proto->proto.arg_types;
      while (curr_type) {
        b->offset -= TypeGet(b, curr_type->constant_val.type_lit)->size;
        curr_type = curr_type->next;
      }
      
      // NOTE(voxel): "bx lr" pushed by decl, it sucks
      // NOTE(voxel): need to find a less weird way
    } break;
    
    case NT_Expr_Index: {
      DSIBack_WriteAST(b, node->index.left);
      ValueType* t = TypeGet(b, node->index.left->expr_type);
      ValueType* sub = TypeGet(b, (t->type == TK_Pointer ?
                                   t->ptr_t.sub_t : t->array_t.sub_t));
      DSIBack_WriteAST(b, node->index.idx);
      pop(b, 0, TypeGet(b, node->index.idx->expr_type));
      write(b, str_from_format(&b->static_arena, "  mov r1, #%llu\n", sub->size));
      pop(b, 2, TypeGet(b, node->index.left->expr_type));
      write(b, str_lit("  mla r3, r0, r1, r2\n"));
      write(b, str_lit("  ldr r0, [r3]\n"));
      
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Addr: {
      DSIBack_WriteAddrAST(b, node->addr);
    } break;
    
    case NT_Expr_Deref: {
      DSIBack_WriteAST(b, node->deref);
      pop(b, 0, TypeGet(b, node->deref->expr_type));
      write_formatted(b, str_lit("ldr r0, [r0]"));
      push(b, 0, TypeGet(b, node->expr_type));
    } break;
    
    case NT_Expr_Call: {
      ASTNode* curr = node->call.args;
      while (curr) {
        DSIBack_WriteAST(b, curr);
        curr = curr->next;
      }
      // arguments on the stack
      if (node->call.called->type == NT_Expr_Ident) {
        // Fastpath a bit
        write(b, str_from_format(&b->static_arena, "  bl %.*s\n",
                                 str_expand(node->call.called->ident.lexeme)));
      } else {
        DSIBack_WriteAST(b, node->call.called);
        pop(b, 0, TypeGet(b, node->call.called->expr_type));
        write_formatted(b, str_lit("bl r0"));
      }
      
      curr = node->call.args;
      u64 total_arg_size = 0;
      while (curr) {
        total_arg_size += TypeGet(b, curr->expr_type)->size;
        curr = curr->next;
      }
      write(b, str_from_format(&b->static_arena, "  sub sp, sp, #%llu\n", total_arg_size));
    } break;
    
    case NT_Expr_Ident: {
      b8 found = false;
      u32 idx;
      for (u32 i = 0; i <= b->scope; i++) {
        if (backend_symbol_lookup(b->symbols, node->ident.lexeme, 0, &idx)) {
          DSIBackendSymbol* sym = &b->symbols.elems[idx];
          
          switch (sym->usage) {
            case Usage_Offset: {
              write(b, str_from_format(&b->static_arena, "  add r0, r12, #%lld\n",
                                       sym->offset));
            } break;
            
            case Usage_Name: {
              write(b, str_from_format(&b->static_arena, "  ldr r0, %.*s\n",
                                       str_expand(sym->ident.lexeme)));
            } break;
          }
          
          push_indirect(b, 0, TypeGet(b, sym->type));
          found = true;
          break;
        }
      }
      if (!found)
        BackendError(b, node->marker, "Did not find an identifier that was supposed to be registered... %.*s\n", str_expand(node->ident.lexeme));
    } break;
    
    case NT_Expr_Cast: {
      // For now do basically nothing since I only want to use it for i32 -> ptr casts
      DSIBack_WriteAST(b, node->cast.casted);
      pop(b, 0, TypeGet(b, node->cast.casted->expr_type));
      push(b, 0, TypeGet(b, node->cast.type->constant_val.type_lit));
    } break;
    
    case NT_Expr_Access: {
      DSIBack_WriteAddrAST(b, node);
      
      u64 offset = 0;
      ValueType* member_type = nullptr;
      
      ValueType* type = TypeGet(b, node->access.left->expr_type);
      if (node->access.deref)
        type = TypeGet(b, type->ptr_t.sub_t);
      Token_node* curr_name = type->compound_t.member_names.first;
      for (u32 i = 0; i < type->compound_t.count; i++) {
        if (str_eq(curr_name->token.lexeme, node->access.right.lexeme)) {
          offset = type->compound_t.member_offsets[i];
          member_type = TypeGet(b, type->compound_t.member_ts[i]);
          break;
        }
        curr_name = curr_name->next;
      }
      
      pop(b, 0, TypeGet(b, Type_Index_U32));
      push_indirect(b, 0, member_type);
    } break;
    
    case NT_Type_Integer:
    case NT_Type_Float:
    case NT_Type_Void:
    case NT_Type_Func:
    case NT_Type_Struct:
    case NT_Type_Union:
    case NT_Type_Pointer:
    case NT_Type_Array: {
      BackendError(b, node->marker, "Internal: Type is not supposed to\
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      be compiled at the backend\n");
    } break;
    
    case NT_Stmt_Expr: {
      DSIBack_WriteAST(b, node->expr_stmt);
      pop(b, 0, TypeGet(b, node->expr_stmt->expr_type));
    } break;
    
    case NT_Stmt_Write: {
      DSIBack_WriteAST(b, node->write_stmt.value);
      pop(b, 1, TypeGet(b, node->write_stmt.value->expr_type));
      write(b, str_from_format(&b->static_arena, "  mov r0, #%d\n",
                               node->write_stmt.ptr));
      write(b, str_lit("  str r1, [r0]\n"));
    } break;
    
    case NT_Stmt_Assign: {
      DSIBack_WriteAddrAST(b, node->binary_op.left);
      DSIBack_WriteAST(b, node->binary_op.right);
      pop(b, 1, TypeGet(b, node->binary_op.right->expr_type));
      pop(b, 0, TypeGet(b, Type_Index_I32));
      put(b, 1, 0, TypeGet(b, node->binary_op.right->expr_type));
    } break;
    
    case NT_Stmt_Block: {
      ASTNode* curr = node->block;
      u64 point = scope_push(b);
      while (curr) {
        DSIBack_WriteAST(b, curr);
        curr = curr->next;
      }
      scope_pop(b, point);
    } break;
    
    case NT_Stmt_While: {
      string cond_lbl = get_next_label(b);
      string after_lbl = get_next_label(b);
      write(b, str_lit("  ; while condition\n"));
      write(b,
            str_from_format(&b->static_arena, "%.*s:\n", str_expand(cond_lbl)));
      DSIBack_WriteAST(b, node->while_loop.condition);
      pop(b, 0, TypeGet(b, node->while_loop.condition->expr_type));
      write(b, str_lit("  cmp r0, #0\n"));
      write(b, str_from_format(&b->static_arena, "  beq %.*s\n", str_expand(after_lbl)));
      u64 point = scope_push(b);
      DSIBack_WriteAST(b, node->while_loop.body);
      scope_pop(b, point);
      write(b, str_from_format(&b->static_arena, "  b %.*s\n", str_expand(cond_lbl)));
      write(b,
            str_from_format(&b->static_arena, "%.*s:\n", str_expand(after_lbl)));
    } break;
    
    case NT_Stmt_If: {
      string then_lbl = get_next_label(b);
      string else_lbl = get_next_label(b);
      string comb_lbl = get_next_label(b);
      write(b, str_lit("  ; if condition\n"));
      DSIBack_WriteAST(b, node->if_stmt.condition);
      pop(b, 0, TypeGet(b, node->if_stmt.condition->expr_type));
      write(b, str_lit("  cmp r0, #0\n"));
      write(b, str_from_format(&b->static_arena, "  beq %.*s\n", str_expand(else_lbl)));
      write(b,
            str_from_format(&b->static_arena, "%.*s:\n", str_expand(then_lbl)));
      
      u64 point = scope_push(b);
      DSIBack_WriteAST(b, node->if_stmt.then_body);
      scope_pop(b, point);
      
      write(b, str_from_format(&b->static_arena, "  b %.*s\n", str_expand(comb_lbl)));
      write(b,
            str_from_format(&b->static_arena, "%.*s:\n", str_expand(else_lbl)));
      
      point = scope_push(b);
      if (node->if_stmt.else_body)
        DSIBack_WriteAST(b, node->if_stmt.else_body);
      scope_pop(b, point);
      
      write(b,
            str_from_format(&b->static_arena, "%.*s:\n", str_expand(comb_lbl)));
    } break;
    
    case NT_Stmt_Return: {
      write(b, str_from_format(&b->static_arena, "  b _%.*s_ret_block\n", str_expand(b->func_name)));
    } break;
    
    case NT_Decl: {
      TypeIndex typ_idx = (node->decl.type ? node->decl.type->constant_val.type_lit :
                           node->decl.val->expr_type);
      ValueType* typ = TypeGet(b, typ_idx);
      
      if (typ->type == TK_Type) {
        return;
        
      } else if (typ->type == TK_Func) {
        
        darray_add(DSIBackendSymbol, &b->symbols,
                   ((DSIBackendSymbol) {
                      .ident=node->decl.ident,
                      .usage=Usage_Name,
                      .scope=b->scope,
                      .type=typ_idx,
                    }));
        
        if (b->scope) {
          BackendError(b, node->marker, "Nested functions are not supported for the DSI Backend");
        }
        
        DSIBack_WriteAST(b, node->decl.val);
        write(b, str_lit("  bx lr\n"));
        
      } else if (node->decl.is_constant && typ->type == TK_Array) {
        
        darray_add(DSIBackendSymbol, &b->symbols,
                   ((DSIBackendSymbol) {
                      .ident=node->decl.ident,
                      .usage=Usage_Name,
                      .scope=b->scope,
                      .type=typ_idx,
                    }));
        
        write_data(b, str_from_format(&b->static_arena, "%.*s:\n", str_expand(node->decl.ident.lexeme)));
        write_data(b, str_from_format(&b->static_arena, "  .long _%.*s_data\n", str_expand(node->decl.ident.lexeme)));
        write_data(b, str_from_format(&b->static_arena, "_%.*s_data:\n", str_expand(node->decl.ident.lexeme)));
        DSIBack_WriteAST(b, node->decl.val);
        
      } else {
        
        darray_add(DSIBackendSymbol, &b->symbols,
                   ((DSIBackendSymbol) {
                      .ident=node->decl.ident,
                      .usage=Usage_Offset,
                      .scope=b->scope,
                      .offset=node->decl.offset,
                      .type=typ_idx,
                    }));
        
        b->offset += typ->size;
        
        if (node->decl.val) {
          DSIBack_WriteAST(b, node->decl.val);
          
          switch (typ->type) {
            case TK_Int:
            case TK_Float:
            case TK_Bool:
            case TK_Pointer: {
              pop(b, 0, TypeGet(b, node->decl.val->expr_type));
              if (typ->size == 4) {
                write(b, str_from_format(&b->static_arena, "  str r0, [r12, #%llu]\n", node->decl.offset));
              } else if (typ->size == 2) {
                write(b, str_from_format(&b->static_arena, "  strh r0, [r12, #%llu]\n", node->decl.offset));
              } else if (typ->size == 1) {
                write(b, str_from_format(&b->static_arena, "  strb r0, [r12, #%llu]\n", node->decl.offset));
              }
            } break;
            
            case TK_Void: break;
            
            case TK_Array:
            case TK_Struct:
            case TK_Union: {
              pop(b, 1, TypeGet(b, node->decl.val->expr_type));
              write(b, str_from_format(&b->static_arena, "  add r0, r12, #%llu\n", node->decl.offset));
              write(b, str_from_format(&b->static_arena, "  mov r2, #%llu\n", typ->size));
              write(b, str_lit("  bl memcpy\n"));
            } break;
            
            default: break;
          }
        }
        
      }
    } break;
    
  }
}

//~ Main Functions

void DSIBack_Init(DSIBackend* b, darray(ValueTypeRef)* type_cache) {
  MemoryZeroStruct(b, DSIBackend);
  arena_init(&b->static_arena);
  b->type_cache = type_cache;
}

void DSIBack_Write(DSIBackend* b, ASTNode* tree) {
  b->tree = tree;
  DSIBack_PushHeader(b);
  ASTNode* curr = tree;
  while (curr) {
    DSIBack_WriteAST(b, curr);
    curr = curr->next;
  }
  if (b->data.first) {
    string_list_push_node(&b->instructions, b->data.first);
    b->instructions.node_count += b->data.node_count - 1;
    b->instructions.total_size += b->data.total_size - b->data.first->str.size;
    b->instructions.last = b->data.last;
  }
  write(b, str_lit("Arm9_End:"));
}

void DSIBack_Free(DSIBackend* b) {
  darray_free(DSIBackendSymbol, &b->symbols);
  arena_free(&b->static_arena);
}