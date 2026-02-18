#include "tac.h"

//~ Helpers
#define TacGenError(p, t, f, ...)\
Statement(\
if (!p->errored) printf("%.*s:%d:%d - Internal Compiler Error (TAC GEN) - " f,\
str_expand(p->filename), t.line, t.col,\
##__VA_ARGS__);\
p->errored = true;\
return;\
)

TacFunction* Tac_AddFunction(TacProgram* p, ASTNode* n) {
  TacFunction* fn = pool_alloc(&p->function_allocator);
  MemoryZeroStruct(fn, TacFunction);
  fn->symbol = n;
  
  // DLL_Add
  if (!p->functions_first && !p->functions_last) {
    p->functions_first = fn;
    p->functions_last = fn;
  } else {
    p->functions_last->next = fn;
    fn->prev = p->functions_last;
    p->functions_last = fn;
  }
  
  return fn;
}

//~ Emission

static void Tac_EmitFunction(TacProgram* p, TacFunction* func, ASTNode* body) {
  dstack(ASTNodeRef) worklist = {0};
  dstack_push(ASTNodeRef, &worklist, body);
  
  while (worklist.len) {
    ASTNode* curr = dstack_pop(ASTNodeRef, &worklist);
    switch (curr->type) {
      case NT_Error: TacGenError(p, curr->marker, "Error node detected\n");
      case NT_Expr_IntLit: {
      } break;
      
      case NT_Expr_FloatLit: {} break;
      case NT_Expr_Add: {} break;
      case NT_Expr_Sub: {} break;
      case NT_Expr_Mul: {} break;
      case NT_Expr_Div: {} break;
      case NT_Expr_Mod: {} break;
      case NT_Expr_Identity: {} break;
      case NT_Expr_Negate: {} break;
      case NT_Expr_Not: {} break;
      case NT_Expr_Eq: {} break;
      case NT_Expr_Neq: {} break;
      case NT_Expr_Less: {} break;
      case NT_Expr_Greater: {} break;
      case NT_Expr_LessEq: {} break;
      case NT_Expr_GreaterEq: {} break;
      case NT_Expr_FuncProto: {} break;
      case NT_Expr_Func: {} break;
      case NT_Expr_ShiftLeft: {} break;
      case NT_Expr_ShiftRight: {} break;
      case NT_Expr_BitAND: {} break;
      case NT_Expr_BitOR: {} break;
      case NT_Expr_Index: {} break;
      case NT_Expr_Addr: {} break;
      case NT_Expr_Deref: {} break;
      case NT_Expr_Call: {} break;
      case NT_Expr_Ident: {} break;
      case NT_Expr_Cast: {} break;
      case NT_Expr_Access: {} break;
      case NT_Expr_ArrayLit: {} break;
      case NT_Type_Integer: {} break;
      case NT_Type_Float: {} break;
      case NT_Type_Void: {} break;
      case NT_Type_Func: {} break;
      case NT_Type_Struct: {} break;
      case NT_Type_Union: {} break;
      case NT_Type_Pointer: {} break;
      case NT_Type_Array: {} break;
      case NT_Stmt_Assign: {} break;
      case NT_Stmt_Expr: {} break;
      case NT_Stmt_Block: {} break;
      case NT_Stmt_While: {} break;
      case NT_Stmt_If: {} break;
      case NT_Stmt_Return: {} break;
      case NT_Stmt_Write: {} break;
      case NT_Decl: {} break;
    }
  }
  
  dstack_free(ASTNodeRef, &worklist);
}


//~ Main Function

void Tac_Init(TacProgram* p) {
  MemoryZeroStruct(p, TacProgram);
  arena_init(&p->misc_allocator);
  pool_init(&p->function_allocator, sizeof(TacFunction));
  pool_init(&p->bb_allocator, sizeof(TacBasicBlock));
  pool_init(&p->inst_allocator, sizeof(TacInst));
}

void Tac_Write(TacProgram* p, ASTNode* tree) {
  ASTNode* curr = tree;
  while (curr) {
    if (curr->type != NT_Decl) {
      TacGenError(p, curr->marker, "Top-Level node is not a declaration!!\n");
    }
    
    if (curr->decl.val && curr->decl.val->type == NT_Expr_Func) {
      // Generate code for this please
      TacFunction* fn = Tac_AddFunction(p, curr);
      Tac_EmitFunction(p, fn, curr->decl.val);
    }
    
    curr = curr->next;
  }
}

void Tac_Free(TacProgram* p) {
  pool_free(&p->inst_allocator);
  pool_free(&p->bb_allocator);
  pool_free(&p->function_allocator);
  arena_free(&p->misc_allocator);
}