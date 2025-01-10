#include "check.h"
#include "base/log.h"
#include <assert.h>

//~ Preproc Params

#define PRINT_DEPENDENCIES 1
#define POINTER_SIZE 4

// Required members: 'scope u32', 'symbols darray(T)'
//                    and 'scope_stack darray(ASTNodeRef)'
#define scope_push(o, n) ((o)->scope++,\
dstack_push(ASTNodeRef, &(o)->scope_stack, n),\
(o)->symbols.len)
#define scope_pop(o, p)\
Statement((o)->scope--;\
ASTNode* r = dstack_pop(ASTNodeRef, &(o)->scope_stack);\
if ((o)->curr_func) (o)->curr_func->func.tracking_local_size -= r->local_size;\
(o)->symbols.len = p;)

//~ Uber-Helpers

#define CheckerError(c, t, f, ...)\
Statement(\
if (!c->errored) printf("%.*s:%d:%d - Check Error - " f,\
str_expand(c->filename), t.line, t.col,\
##__VA_ARGS__);\
c->errored = true;\
return; /* Maybe this should go */\
)

#define CheckerErrorNoRet(c, t, f, ...)\
Statement(\
printf("%.*s:%d:%d - Check Error - " f,\
str_expand(c->filename), t.line, t.col,\
##__VA_ARGS__);\
c->errored = true;\
)

Queue_Impl(CheckingWork);
DArray_Impl(ValueTypeRef);

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

//~ Type Helpers

#define FNV_OFFSET_BASIS 14695981039346656037ull
#define FNV_PRIME 1099511628211llu
M_Arena* global_arena_t;

#define TypeGet(c, idx) (c->type_cache.elems[idx])

b8 CheckAbsolute(Checker* c, TypeIndex a, TypeIndex b) {
  if (a == b) return true;
  ValueType* at = TypeGet(c, a);
  ValueType* bt = TypeGet(c, b);
  if (at->type != bt->type) return false;
  
  switch (at->type) {
    case TK_None:
    case TK_Int:
    case TK_Float:
    case TK_Void:
    case TK_Bool: {
      return false;
    }
    
    case TK_MAX:
    case TK_Type: {
      unreachable;
      return false;
    }
    
    case TK_Func: {
      if (!CheckAbsolute(c, at->func_t.ret_t, bt->func_t.ret_t)) return false;
      if (at->func_t.arity != bt->func_t.arity) return false;
      for (u32 i = 0; i < at->func_t.arity; i++) {
        if (!CheckAbsolute(c, at->func_t.arg_ts[i], bt->func_t.arg_ts[i]))
          return false;
      }
      return true;
    }
    
    case TK_Pointer: {
      return CheckAbsolute(c, at->ptr_t.sub_t, bt->ptr_t.sub_t);
    }
    
    case TK_Array: {
      return at->array_t.count == bt->array_t.count &&
        CheckAbsolute(c, at->array_t.sub_t, bt->array_t.sub_t);
    }
    
    case TK_Struct:
    case TK_Union: {
      if (at->compound_t.count != bt->compound_t.count) return false;
      Token_node* acn = at->compound_t.member_names.first;
      Token_node* bcn = bt->compound_t.member_names.first;
      for (u64 i = 0; i < at->compound_t.count; i++) {
        if (!str_eq(acn->token.lexeme, bcn->token.lexeme)) return false;
        if (!CheckAbsolute(c, at->compound_t.member_ts[i], bt->compound_t.member_ts[i])) return false;
        acn = acn->next;
        bcn = bcn->next;
      }
      return true;
    }
  }
  return false;
}

#define IsInteger(c, t) (TypeGet(c, t)->type == TK_Int)
#define IsFloat(c, t) (TypeGet(c, t)->type == TK_Float)
#define IsFunc(c, t) (TypeGet(c, t)->type == TK_Func)
#define IsType(c, t) (TypeGet(c, t)->type == TK_Type)
#define IsVoid(c, t) (TypeGet(c, t)->type == TK_Void)
#define IsBool(c, t) (TypeGet(c, t)->type == TK_Bool)
#define IsPointer(c, t) (TypeGet(c, t)->type == TK_Pointer)
#define IsArray(c, t) (TypeGet(c, t)->type == TK_Array)
#define IsStruct(c, t) (TypeGet(c, t)->type == TK_Struct)
#define IsUnion(c, t) (TypeGet(c, t)->type == TK_Union)

#define IsNotInteger(c, t) (TypeGet(c, t)->type != TK_Int)
#define IsNotFloat(c, t) (TypeGet(c, t)->type != TK_Float)
#define IsNotFunc(c, t) (TypeGet(c, t)->type != TK_Func)
#define IsNotType(c, t) (TypeGet(c, t)->type != TK_Type)
#define IsNotVoid(c, t) (TypeGet(c, t)->type != TK_Void)
#define IsNotBool(c, t) (TypeGet(c, t)->type != TK_Bool)
#define IsNotPointer(c, t) (TypeGet(c, t)->type != TK_Pointer)
#define IsNotArray(c, t) (TypeGet(c, t)->type != TK_Array)
#define IsNotStruct(c, t) (TypeGet(c, t)->type != TK_Struct)
#define IsNotUnion(c, t) (TypeGet(c, t)->type != TK_Union)

#define IsConstant(c, n) (n->is_constant)
#define IsConstantInt(c, n) (IsConstant(c, n) && n->constant_val.type == CVT_Int)
#define IsConstantFloat(c, n) (IsConstant(c, n) && n->constant_val.type == CVT_Float)

#define GetConstantInt(n) ((n)->constant_val.int_lit)
#define GetConstantFloat(n) ((n)->constant_val.float_lit)
#define GetConstantType(n) ((n)->constant_val.type_lit)

#define IsLValue(n) (n->type == NT_Expr_Deref ||\
n->type == NT_Expr_Index ||\
n->type == NT_Expr_Access ||\
n->type == NT_Expr_Ident)

static b8 is_castable(Checker* c, ValueType* a, ValueType* b) {
  switch (b->type) {
    case TK_Void:
    case TK_Type: return false;
    
    case TK_None:
    case TK_MAX:  unreachable; return false;
    
    case TK_Int:
    case TK_Float: {
      return a->type == TK_Int || a->type == TK_Float || a->type == TK_Pointer;
    } break;
    
    case TK_Func: {
      if (a->type != TK_Func) return false;
      // Do more here maybe
      return true;
    } break;
    
    case TK_Bool: {
      return a->type == TK_Bool;
    } break;
    
    case TK_Pointer: {
      return a->type == TK_Pointer || a->type == TK_Int;
    } break;
    
    case TK_Array: {
      return false;
    } break;
    
    case TK_Struct:
    case TK_Union: {
      if (a->type != b->type) return false;
      if (a->compound_t.count != b->compound_t.count) return false;
      for (u64 i = 0; i < a->compound_t.count; i++) {
        if (!CheckAbsolute(c, a->compound_t.member_ts[i], b->compound_t.member_ts[i]))
          return false;
      }
      return true;
    } break;
  }
  
  return false;
}

static TypeIndex type_reduce(Checker* c, TypeIndex t) {
  ValueType* ty = TypeGet(c, t);
  if (ty->type == TK_Pointer) {
    return ty->ptr_t.sub_t;
  } else if (ty->type == TK_Array) {
    return ty->array_t.sub_t;
  } else if (ty->type == TK_Func) {
    return ty->func_t.ret_t;
  }
  unreachable;
  return -1;
}

static TypeIndex type_push_pointer(Checker* c, TypeIndex t) {
  ValueType* reg = pool_alloc(&c->allocator);
  reg->type = TK_Pointer;
  reg->size = POINTER_SIZE;
  reg->ptr_t.sub_t = t;
  darray_add(ValueTypeRef, &c->type_cache, reg);
  return c->type_cache.len-1;
}

static TypeIndex type_push_array(Checker* c, TypeIndex t, u64 count) {
  ValueType* reg = pool_alloc(&c->allocator);
  reg->type = TK_Array;
  reg->size = POINTER_SIZE;
  reg->array_t.sub_t = t;
  reg->array_t.count = count;
  darray_add(ValueTypeRef, &c->type_cache, reg);
  return c->type_cache.len-1;
}

DArray_Impl(Symbol);

static b8 symbol_lookup(darray(Symbol) syms, string id, u32 min_scope, u32* idx) {
  for (i32 i = syms.len-1; i >= 0; i--) {
    if (syms.elems[i].scope < min_scope) break;
    if (str_eq(syms.elems[i].ident.lexeme, id)) {
      if (idx) *idx = i;
      return true;
    }
  }
  return false;
}

static u8* transfer_constant(u8* to, ConstantValue* val, ValueType* type) {
  // TODO(voxel): Make sure endianness is fine...
  
  switch (val->type) {
    
    case CVT_None:
    case CVT_Type:  unreachable;
    
    case CVT_Int: {
      if (type->size == 1) {
        *to         = (u8)  val->int_lit;
      } else if (type->size == 2) {
        *((u16*)to) = (u16) val->int_lit;
      } else if (type->size == 4) {
        *((u32*)to) = (u32) val->int_lit;
      }
    } break;
    
    case CVT_Float: {
      if (type->size == 4) {
        *((f32*)to) = (f32) val->float_lit;
      } else if (type->size == 8) {
        *((f64*)to) = (f64) val->float_lit;
      }
    } break;
    
    case CVT_Buffer: {
      memmove(to, val->buf_lit.buf, val->buf_lit.size);
    } break;
  }
  return to + type->size;
}

static void allocate_and_fill_constant_buffer(Checker* c, ASTNode* node) {
  switch (node->type) {
    case NT_Expr_ArrayLit: {
      ValueType* elem_type = TypeGet(c, GetConstantType(node->array_lit.type));
      u64 size = node->array_lit.count * elem_type->size;
      u8* buffer = arena_alloc(&c->arena, size);
      
      ASTNode* ce = node->array_lit.values;
      u8* write_ptr = buffer;
      while (ce) {
        write_ptr = transfer_constant(write_ptr, &ce->constant_val, elem_type);
        ce = ce->next;
      }
      
      node->constant_val.buf_lit.buf = buffer;
      node->constant_val.buf_lit.size = size;
    } break;
    
    default: unreachable;
  }
}

//~ Error Stuff

static b8 Checker_RunDFS(darray(ASTNodeRef_array)* adj, u32 i, i32* start, i32* end) {
  adj->elems[i].elems[0]->decl.color = 1;
  for (u32 j = 1; j < adj->elems[i].len; j++) {
    i32 curr_dep = -1;
    ASTNode* theone = adj->elems[i].elems[j];
    for (u32 k = 0; k < adj->len; k++) {
      if (adj->elems[k].elems[0] == theone) {
        curr_dep = k;
        break;
      }
    }
    if (curr_dep == -1) return false;
    
    if (adj->elems[curr_dep].elems[0]->decl.color == 0) {
      adj->elems[curr_dep].elems[0]->decl.parent = i;
      if (Checker_RunDFS(adj, curr_dep, start, end))
        return true;
    } else if (adj->elems[curr_dep].elems[0]->decl.color == 1) {
      *start = curr_dep;
      *end = i;
      return true;
    }
  }
  adj->elems[i].elems[0]->decl.color = 2;
  return false;
}

static void Checker_DetectCycles(Checker* c, darray(ASTNodeRef_array)* adj) {
  i32 cy_start = -1;
  i32 cy_end = -1;
  IteratePtr (adj, i) {
    if (!adj->elems[i].elems[0]->decl.color &&
        Checker_RunDFS(adj, i, &cy_start, &cy_end))
      break;
  }
  
  if (cy_start == -1 || cy_end == -1) return;
  
  CheckerErrorNoRet(c, ((Token){0}), "Found a Dependency Cycle:\n");
  ASTNode* curr = adj->elems[cy_end].elems[0];
  u32 index = 0;
  ASTNode* prev = adj->elems[cy_end].elems[0];
  while (true) {
    if (curr != prev)
      CheckerErrorNoRet(c, curr->marker, "%u) %.*s -> %.*s\n", index, str_expand(prev->decl.ident.lexeme), str_expand(curr->decl.ident.lexeme));
    else
      CheckerErrorNoRet(c, curr->marker, "%u) Starting here: %.*s\n", index,
                        str_expand(curr->decl.ident.lexeme));
    
    index += 1;
    if (curr->decl.parent == -1) break;
    
    prev = curr;
    curr = adj->elems[curr->decl.parent].elems[0];
  }
  CheckerErrorNoRet(c, curr->marker, "%u) %.*s -> %.*s\n", index,
                    str_expand(curr->decl.ident.lexeme),
                    str_expand(adj->elems[cy_end].elems[0]->decl.ident.lexeme));
}

static void Checker_ErrorsBuild(Checker* c, ASTNode* base, ASTNode* node) {
  if (!node) return;
  
  switch (node->type) {
    case NT_Error:
    case NT_Expr_IntLit:
    case NT_Expr_FloatLit: {} break;
    
    case NT_Expr_ArrayLit: {
      Checker_ErrorsBuild(c, base, node->array_lit.type);
      ASTNode* ce = node->array_lit.values;
      while (ce) {
        Checker_ErrorsBuild(c, base, ce);
        ce = ce->next;
      }
    } break;
    
    case NT_Expr_Add:
    case NT_Expr_Sub:
    case NT_Expr_Mul:
    case NT_Expr_Div:
    case NT_Expr_ShiftLeft:
    case NT_Expr_ShiftRight:
    case NT_Expr_BitAND:
    case NT_Expr_BitOR:
    case NT_Expr_Mod: {
      Checker_ErrorsBuild(c, base, node->binary_op.left);
      Checker_ErrorsBuild(c, base, node->binary_op.right);
    } break;
    
    
    
    case NT_Expr_Identity:
    case NT_Expr_Negate:
    case NT_Expr_Not: {
      Checker_ErrorsBuild(c, base, node->unary_op.operand);
    } break;
    
    case NT_Expr_Eq:
    case NT_Expr_Neq:
    case NT_Expr_Less:
    case NT_Expr_Greater:
    case NT_Expr_LessEq:
    case NT_Expr_GreaterEq: {
      Checker_ErrorsBuild(c, base, node->binary_op.left);
      Checker_ErrorsBuild(c, base, node->binary_op.right);
    } break;
    
    case NT_Expr_FuncProto: {
      Checker_ErrorsBuild(c, base, node->proto.return_type);
      ASTNode* ct = node->proto.arg_types;
      while (ct) {
        Checker_ErrorsBuild(c, base, ct);
        ct = ct->next;
      }
    } break;
    
    case NT_Expr_Func: {
      Checker_ErrorsBuild(c, base, node->func.proto);
      Checker_ErrorsBuild(c, base, node->func.body);
    } break;
    
    case NT_Expr_Index: {
      Checker_ErrorsBuild(c, base, node->index.left);
      Checker_ErrorsBuild(c, base, node->index.idx);
    } break;
    
    case NT_Expr_Addr: {
      Checker_ErrorsBuild(c, base, node->addr);
    } break;
    
    case NT_Expr_Deref: {
      Checker_ErrorsBuild(c, base, node->deref);
    } break;
    
    case NT_Expr_Call: {
      Checker_ErrorsBuild(c, base, node->call.called);
      ASTNode* ca = node->call.args;
      while (ca) {
        Checker_ErrorsBuild(c, base, ca);
        ca = ca->next;
      }
    } break;
    
    case NT_Expr_Ident: {
      if (node->status & Status_Waiting) {
        // Failed Identifier Lookup
        ASTNode* top_level = c->tree;
        b8 exists = false;
        while (top_level) {
          if (str_eq(top_level->decl.ident.lexeme, node->ident.lexeme)) {
            exists = true;
            break;
          }
          top_level = top_level->next;
        }
        
        if (exists) {
          // Since this exists in the top level, there HAS to be a cycle.
          b8 did_something = false;
          
          Iterate (c->cycle_checker, i) {
            darray(ASTNodeRef)* curr_cycle = &c->cycle_checker.elems[i];
            
            if (curr_cycle->elems[0] == base) {
              darray_add(ASTNodeRef, curr_cycle, top_level);
              did_something = true;
            }
          }
          
          if (!did_something) {
            darray(ASTNodeRef) newguy = {0};
            darray_add(ASTNodeRef, &newguy, base);
            darray_add(ASTNodeRef, &newguy, top_level);
            darray_add(ASTNodeRef_array, &c->cycle_checker, newguy);
          }
          
          c->cycles_exist = true;
        } else {
          CheckerErrorNoRet(c, node->marker, "Undeclared Identifier '%.*s'\n",
                            str_expand(node->ident.lexeme));
        }
      }
    } break;
    
    case NT_Expr_Cast: {
      Checker_ErrorsBuild(c, base, node->cast.casted);
      Checker_ErrorsBuild(c, base, node->cast.type);
    } break;
    
    case NT_Expr_Access: {
      Checker_ErrorsBuild(c, base, node->access.left);
    } break;
    
    case NT_Type_Integer:
    case NT_Type_Float:
    case NT_Type_Void: {} break;
    
    case NT_Type_Func: {
      Checker_ErrorsBuild(c, base, node->func_type.return_type);
      ASTNode* ct = node->func_type.arg_types;
      while (ct) {
        Checker_ErrorsBuild(c, base, ct);
        ct = ct->next;
      }
    } break;
    
    case NT_Type_Struct:
    case NT_Type_Union: {
      ASTNode* ct = node->compound_type.member_types;
      while (ct) {
        Checker_ErrorsBuild(c, base, ct);
        ct = ct->next;
      }
    } break;
    
    case NT_Type_Pointer: {
      Checker_ErrorsBuild(c, base, node->pointer_type.sub);
    } break;
    
    case NT_Type_Array: {
      Checker_ErrorsBuild(c, base, node->array_type.count);
      Checker_ErrorsBuild(c, base, node->array_type.sub);
    } break;
    
    case NT_Stmt_Assign: {
      Checker_ErrorsBuild(c, base, node->binary_op.left);
      Checker_ErrorsBuild(c, base, node->binary_op.right);
    } break;
    
    case NT_Stmt_Expr: {
      Checker_ErrorsBuild(c, base, node->expr_stmt);
    } break;
    
    case NT_Stmt_Write: {
      Checker_ErrorsBuild(c, base, node->write_stmt.value);
    } break;
    
    case NT_Stmt_Block: {
      ASTNode* in = node->block;
      while (in) {
        Checker_ErrorsBuild(c, base, in);
        in = in->next;
      }
    } break;
    
    case NT_Stmt_While: {
      Checker_ErrorsBuild(c, base, node->while_loop.condition);
      Checker_ErrorsBuild(c, base, node->while_loop.body);
    } break;
    
    case NT_Stmt_If: {
      Checker_ErrorsBuild(c, base, node->if_stmt.condition);
      Checker_ErrorsBuild(c, base, node->if_stmt.then_body);
      Checker_ErrorsBuild(c, base, node->if_stmt.else_body);
    } break;
    
    case NT_Stmt_Return: {
      Checker_ErrorsBuild(c, base, node->return_stmt);
    } break;
    
    case NT_Decl: {
      Checker_ErrorsBuild(c, base, node->decl.type);
      Checker_ErrorsBuild(c, base, node->decl.val);
    } break;
  }
}


//~ Main Functions

static void Checker_Transfer(Checker* c, ASTNode* node);

#define CheckerIf(c, cond) if (cond)
#define ReturnIfError(c) if (c->errored) return;
#define ReadyStat(cond) ((cond) ? Status_Ready : 0)
#define ProtoReadyStat(cond) ((cond) ? Status_ProtoReady : 0)

static b8 Checker_IsReady(Checker* c, ASTNode* base, ASTNode* curr) {
  if (!curr) return true;
  if (curr->status & Status_Ready) return true;
  
  switch (curr->type) {
    case NT_Error:
    case NT_Expr_IntLit:
    case NT_Expr_FloatLit: {
      curr->status |= Status_Ready;
      curr->status |= Status_ProtoReady;
    } break;
    
    case NT_Expr_ArrayLit: {
      b8 stat = Checker_IsReady(c, base, curr->array_lit.type);
      ASTNode* ce = curr->array_lit.values;
      while (ce) {
        stat &= Checker_IsReady(c, base, ce);
        ce = ce->next;
      }
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Add:
    case NT_Expr_Sub:
    case NT_Expr_Mul:
    case NT_Expr_Div:
    case NT_Expr_ShiftLeft:
    case NT_Expr_ShiftRight:
    case NT_Expr_BitAND:
    case NT_Expr_BitOR:
    case NT_Expr_Mod: {
      b8 stat =
        Checker_IsReady(c, base, curr->binary_op.left) &
        Checker_IsReady(c, base, curr->binary_op.right);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Identity:
    case NT_Expr_Negate:
    case NT_Expr_Not: {
      b8 stat = Checker_IsReady(c, base, curr->unary_op.operand);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Eq:
    case NT_Expr_Neq:
    case NT_Expr_Less:
    case NT_Expr_Greater:
    case NT_Expr_LessEq:
    case NT_Expr_GreaterEq: {
      b8 stat =
        Checker_IsReady(c, base, curr->binary_op.left) &
        Checker_IsReady(c, base, curr->binary_op.right);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_FuncProto: {
      b8 stat = Checker_IsReady(c, base, curr->proto.return_type);
      ASTNode* ct = curr->proto.arg_types;
      Token_node* cn = curr->proto.arg_names.first;
      while (ct) {
        stat &= Checker_IsReady(c, base, ct);
        
        Symbol reg = (Symbol) {
          .ident = cn->token,
          .scope = c->scope,
          .node = curr,
        };
        darray_add(Symbol, &c->symbols, reg);
        
        ct = ct->next;
        cn = cn->next;
      }
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Func: {
      b8 protostat = Checker_IsReady(c, base, curr->func.proto);
      b8 stat = Checker_IsReady(c, base, curr->func.body);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(protostat);
    } break;
    
    case NT_Expr_Index: {
      b8 stat =
        Checker_IsReady(c, base, curr->index.left) &
        Checker_IsReady(c, base, curr->index.idx);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Addr: {
      b8 stat = Checker_IsReady(c, base, curr->addr);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Deref: {
      b8 stat = Checker_IsReady(c, base, curr->deref);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Call: {
      Checker_IsReady(c, base, curr->call.called);
      b8 stat = (curr->call.called->status & Status_ProtoReady) ? true : false;
      
      ASTNode* ca = curr->call.args;
      while (ca) {
        stat &= Checker_IsReady(c, base, ca);
        ca = ca->next;
      }
      
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Ident: {
      u32 idx;
      if (!symbol_lookup(c->symbols, curr->ident.lexeme, 0, &idx)) {
        curr->status |= Status_Waiting;
        break;
      }
      
      curr->status |= (c->symbols.elems[idx].node->status & Status_Ready);
      curr->status |= (c->symbols.elems[idx].node->status & Status_ProtoReady);
      if (!(curr->status & Status_Ready || curr->status & Status_ProtoReady))
        curr->status |= Status_Waiting;
    } break;
    
    case NT_Expr_Cast: {
      b8 stat =
        Checker_IsReady(c, base, curr->cast.casted) &
        Checker_IsReady(c, base, curr->cast.type);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Expr_Access: {
      Checker_IsReady(c, base, curr->access.left);
      b8 stat = curr->access.left->status & Status_ProtoReady ? true : false;
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Type_Integer:
    case NT_Type_Float:
    case NT_Type_Void: {
      b8 stat = true;
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Type_Func: {
      b8 stat = Checker_IsReady(c, base, curr->func_type.return_type);
      ASTNode* ct = curr->func_type.arg_types;
      while (ct) {
        stat &= Checker_IsReady(c, base, ct);
        ct = ct->next;
      }
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Type_Struct:
    case NT_Type_Union: {
      b8 stat = true;
      ASTNode* ct = curr->compound_type.member_types;
      while (ct) {
        stat &= Checker_IsReady(c, base, ct);
        ct = ct->next;
      }
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Type_Pointer: {
      if (c->context == Checking_Procedure) {
        b8 stat = Checker_IsReady(c, base, curr->pointer_type.sub);
        curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
      } else if (c->context == Checking_Type) {
        curr->status |= Status_Ready | Status_ProtoReady;
      }
    } break;
    
    case NT_Type_Array: {
      b8 stat =
        Checker_IsReady(c, base, curr->array_type.count) &
        Checker_IsReady(c, base, curr->array_type.sub);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Stmt_Assign: {
      b8 stat =
        Checker_IsReady(c, base, curr->binary_op.left) &
        Checker_IsReady(c, base, curr->binary_op.right);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Stmt_Expr: {
      b8 stat = Checker_IsReady(c, base, curr->expr_stmt);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Stmt_Write: {
      b8 stat = Checker_IsReady(c, base, curr->write_stmt.value);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Stmt_Block: {
      b8 stat = true;
      ASTNode* in = curr->block;
      ScopeResetPoint p = scope_push(c, curr);
      while (in) {
        stat &= Checker_IsReady(c, base, in);
        in = in->next;
      }
      scope_pop(c, p);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Stmt_While: {
      b8 stat = Checker_IsReady(c, base, curr->while_loop.condition);
      ScopeResetPoint p = scope_push(c, curr);
      stat &= Checker_IsReady(c, base, curr->while_loop.body);
      scope_pop(c, p);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Stmt_If: {
      b8 stat = Checker_IsReady(c, base, curr->if_stmt.condition);
      
      ScopeResetPoint p = scope_push(c, curr);
      stat &= Checker_IsReady(c, base, curr->if_stmt.then_body);
      scope_pop(c, p);
      
      p = scope_push(c, curr);
      stat &= Checker_IsReady(c, base, curr->if_stmt.else_body);
      scope_pop(c, p);
      
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Stmt_Return: {
      b8 stat = Checker_IsReady(c, base, curr->return_stmt);
      curr->status |= ReadyStat(stat) | ProtoReadyStat(stat);
    } break;
    
    case NT_Decl: {
      b8 stat = Checker_IsReady(c, base, curr->decl.type);
      b8 protostat = stat;
      
      CheckingContext old = c->context;
      if (curr->decl.val) {
        if (curr->decl.val->type == NT_Expr_Func) {
          c->context = Checking_Procedure;
        } else if (curr->decl.val->type == NT_Type_Struct ||
                   curr->decl.val->type == NT_Type_Union ||
                   curr->decl.val->type == NT_Type_Array ||
                   curr->decl.val->type == NT_Type_Pointer) {
          c->context = Checking_Type;
        }
      }
      
      Symbol reg = (Symbol) {
        .ident = curr->decl.ident,
        .scope = c->scope,
        .node = curr,
      };
      darray_add(Symbol, &c->symbols, reg);
      
      ScopeResetPoint p = scope_push(c, curr);
      stat &= Checker_IsReady(c, base, curr->decl.val);
      scope_pop(c, p);
      
      c->context = old;
      
      if (curr->decl.val) {
        protostat &= (curr->decl.val->status & Status_ProtoReady ? true : false);
      }
      
      curr->status |= ReadyStat(stat) | ProtoReadyStat(protostat);
    } break;
  }
  
  return !!(curr->status & Status_Ready);
}


static void Checker_ProtoTransfer(Checker* c, ASTNode* node) {
  if (!node) return;
  if ((node->status & Status_Resolved) ||
      (node->status & Status_ProtoResolved)) return;
  
  switch (node->type) {
    case NT_Expr_FuncProto: {
      Checker_Transfer(c, node->proto.return_type);
      ASTNode* curr = node->proto.arg_types;
      while (curr) {
        Checker_Transfer(c, curr);
        curr = curr->next;
      }
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotType(c, node->proto.return_type->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->proto.return_type->expr_type);
        CheckerError(c, node->marker,
                     "Return type must actually be a type, but we got '%.*s'\n",
                     str_expand(t1));
      }
      
      curr = node->proto.arg_types;
      Token_node* curr_name = node->proto.arg_names.first;
      u32 i = 0;
      while (curr) {
        CheckerIf (c,
                   IsNotType(c, curr->expr_type)) {
          string t1 =
            Debug_GetTypeString(c, curr->expr_type);
          CheckerError(c, node->marker,
                       "Argument %u must be a type, but we got '%.*s'\n",
                       i, str_expand(t1));
        }
        
        Symbol reg = (Symbol) {
          .ident = curr_name->token,
          .node = node,
          .type = curr->constant_val.type_lit,
          .scope = c->scope,
        };
        darray_add(Symbol, &c->symbols, reg);
        
        i++;
        curr = curr->next;
        curr_name = curr_name->next;
      }
      
      ValueType* reg = pool_alloc(&c->allocator);
      reg->type = TK_Func;
      reg->size = POINTER_SIZE;
      reg->func_t.ret_t = node->proto.return_type->expr_type;
      reg->func_t.arity = node->proto.arity;
      reg->func_t.arg_ts = arena_alloc(&c->arena, sizeof(TypeIndex) * node->proto.arity);
      curr = node->proto.arg_types;
      i = 0;
      while (curr) {
        reg->func_t.arg_ts[i] = curr->constant_val.type_lit;
        i++;
        curr = curr->next;
      }
      darray_add(ValueTypeRef, &c->type_cache, reg);
      
      node->expr_type = c->type_cache.len-1;
      node->status |= Status_Resolved;
    } break;
    
    case NT_Expr_Func: {
      Checker_Transfer(c, node->func.proto);
      
      ReturnIfError(c);
      
      node->expr_type = node->func.proto->expr_type;
    } break;
    
    case NT_Decl: {
      Checker_Transfer(c, node->decl.type);
      Checker_ProtoTransfer(c, node->decl.val);
      
      if (symbol_lookup(c->symbols, node->decl.ident.lexeme, c->scope, nullptr)) {
        CheckerError(c, node->decl.ident,
                     "Redeclared Identifier '%.*s'\n",
                     str_expand(node->decl.ident.lexeme));
      }
      
      Symbol reg = (Symbol) {
        .ident = node->decl.ident,
        .node = node,
        .scope = c->scope,
      };
      
      if (node->decl.type) {
        reg.type = node->decl.type->constant_val.type_lit;
      } else {
        reg.type = node->decl.val->expr_type;
      }
      
      darray_add(Symbol, &c->symbols, reg);
    } break;
    
    default: break;
  }
  
  node->status |= Status_ProtoResolved;
}

static void Checker_Transfer(Checker* c, ASTNode* node) {
  if (!node) return;
  if (node->status & Status_Resolved) return;
  
  switch (node->type) {
    case NT_Error: {
      node->expr_type = Type_Index_None;
    } break;
    
    case NT_Expr_IntLit: {
      node->expr_type = Type_Index_I32;
    } break;
    
    case NT_Expr_FloatLit: {
      node->expr_type = Type_Index_F32;
    } break;
    
    case NT_Expr_ArrayLit: {
      Checker_Transfer(c, node->array_lit.type);
      ASTNode* ce = node->array_lit.values;
      b8 constantness = true;
      while (ce) {
        Checker_Transfer(c, ce);
        constantness &= ce->is_constant;
        ce = ce->next;
      }
      
      ReturnIfError(c);
      
      if (constantness) {
        node->is_constant = true;
        node->constant_val.type = CVT_Buffer;
        allocate_and_fill_constant_buffer(c, node);
      }
      
      ValueType* reg = pool_alloc(&c->allocator);
      reg->type = TK_Array;
      reg->size =
        TypeGet(c, GetConstantType(node->array_lit.type))->size *
        node->array_lit.count;
      reg->array_t.sub_t = GetConstantType(node->array_lit.type);
      reg->array_t.count = node->array_lit.count;
      darray_add(ValueTypeRef, &c->type_cache, reg);
      
      node->expr_type = c->type_cache.len-1;
    } break;
    
    case NT_Expr_Add:
    case NT_Expr_Sub:
    case NT_Expr_Mul:
    case NT_Expr_Div: {
      Checker_Transfer(c, node->binary_op.left);
      Checker_Transfer(c, node->binary_op.right);
      ReturnIfError(c);
      
      // TODO(voxel): Check If you can widen implicitly
      CheckerIf (c,
                 !CheckAbsolute(c, node->binary_op.left->expr_type,
                                node->binary_op.right->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->binary_op.left->expr_type);
        string t2 =
          Debug_GetTypeString(c, node->binary_op.right->expr_type);
        CheckerError(c, node->marker,
                     "LHS and RHS of the binary operator should be the same type, but we got '%.*s' and '%.*s'\n",
                     str_expand(t1), str_expand(t2));
      }
      
      node->expr_type = node->binary_op.left->expr_type;
    } break;
    
    case NT_Expr_ShiftLeft:
    case NT_Expr_ShiftRight: {
      Checker_Transfer(c, node->binary_op.left);
      Checker_Transfer(c, node->binary_op.right);
      ReturnIfError(c);
      
      CheckerIf(c,
                IsNotInteger(c, node->binary_op.left->expr_type) ||
                IsNotInteger(c, node->binary_op.right->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->binary_op.left->expr_type);
        string t2 =
          Debug_GetTypeString(c, node->binary_op.right->expr_type);
        CheckerError(c, node->marker,
                     "LHS and RHS of the shift operator should be integers, but we got '%.*s' and '%.*s'\n",
                     str_expand(t1), str_expand(t2));
      }
      
      node->expr_type = node->binary_op.left->expr_type;
    } break;
    
    case NT_Expr_BitAND:
    case NT_Expr_BitOR: {
      Checker_Transfer(c, node->binary_op.left);
      Checker_Transfer(c, node->binary_op.right);
      ReturnIfError(c);
      
      CheckerIf(c,
                (IsNotInteger(c, node->binary_op.left->expr_type) &&
                 IsNotBool(c, node->binary_op.left->expr_type)) ||
                (IsNotInteger(c, node->binary_op.right->expr_type) &&
                 IsNotBool(c, node->binary_op.right->expr_type))) {
        string t1 =
          Debug_GetTypeString(c, node->binary_op.left->expr_type);
        string t2 =
          Debug_GetTypeString(c, node->binary_op.right->expr_type);
        CheckerError(c, node->marker,
                     "LHS and RHS of the bitwise operator should be integers, but we got '%.*s' and '%.*s'\n",
                     str_expand(t1), str_expand(t2));
      }
      
      node->expr_type = node->binary_op.left->expr_type;
    } break;
    
    case NT_Expr_Mod: {
      Checker_Transfer(c, node->binary_op.left);
      Checker_Transfer(c, node->binary_op.right);
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotInteger(c, node->binary_op.left->expr_type) ||
                 IsNotInteger(c, node->binary_op.right->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->binary_op.left->expr_type);
        string t2 =
          Debug_GetTypeString(c, node->binary_op.right->expr_type);
        CheckerError(c, node->marker,
                     "LHS and RHS of %% should be integers but we got '%.*s' and '%.*s'\n",
                     str_expand(t1), str_expand(t2));
      }
      
      node->expr_type = node->binary_op.left->expr_type;
    } break;
    
    case NT_Expr_Identity:
    case NT_Expr_Negate: {
      Checker_Transfer(c, node->unary_op.operand);
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotInteger(c, node->unary_op.operand->expr_type) &&
                 IsNotFloat(c, node->unary_op.operand->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->unary_op.operand->expr_type);
        CheckerError(c, node->marker,
                     "This operator is not supported by the type '%.*s'\n",
                     str_expand(t1));
      }
      
      node->expr_type = node->unary_op.operand->expr_type;
    } break;
    
    case NT_Expr_Not: {
      Checker_Transfer(c, node->unary_op.operand);
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotInteger(c, node->unary_op.operand->expr_type) &&
                 IsNotFloat(c, node->unary_op.operand->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->unary_op.operand->expr_type);
        CheckerError(c, node->marker,
                     "This operator is not supported by the type '%.*s'\n",
                     str_expand(t1));
      }
      
      node->expr_type = node->unary_op.operand->expr_type;
    } break;
    
    case NT_Expr_Eq:
    case NT_Expr_Neq:
    case NT_Expr_Less:
    case NT_Expr_Greater:
    case NT_Expr_LessEq:
    case NT_Expr_GreaterEq: {
      Checker_Transfer(c, node->binary_op.left);
      Checker_Transfer(c, node->binary_op.right);
      ReturnIfError(c);
      
      CheckerIf (c,
                 !CheckAbsolute(c, node->binary_op.left->expr_type,
                                node->binary_op.right->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->binary_op.left->expr_type);
        string t2 =
          Debug_GetTypeString(c, node->binary_op.right->expr_type);
        CheckerError(c, node->marker,
                     "LHS and RHS of the binary operator should be the same type, but we got '%.*s' and '%.*s'\n",
                     str_expand(t1), str_expand(t2));
      }
      
      node->expr_type = Type_Index_Bool;
    } break;
    
    case NT_Expr_Index: {
      Checker_Transfer(c, node->index.left);
      Checker_Transfer(c, node->index.idx);
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotPointer(c, node->index.left->expr_type) &&
                 IsNotArray(c, node->index.left->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->index.left->expr_type);
        CheckerError(c, node->marker,
                     "Only pointers and arrays can be indexed, but we got a '%.*s'\n",
                     str_expand(t1));
      }
      
      CheckerIf (c,
                 IsNotInteger(c, node->index.idx->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->index.idx->expr_type);
        CheckerError(c, node->marker,
                     "Only pointers and arrays can be indexed, but we got a '%.*s'\n",
                     str_expand(t1));
      }
      
      node->expr_type = type_reduce(c, node->index.left->expr_type);
    } break;
    
    case NT_Expr_Addr: {
      Checker_Transfer(c, node->addr);
      ReturnIfError(c);
      
      CheckerIf (c,
                 !IsLValue(node->addr)) {
        CheckerError(c, node->marker,
                     "Can only take address of an l-value\n");
      }
      
      node->expr_type = type_push_pointer(c, node->addr->expr_type);
    } break;
    
    case NT_Expr_FuncProto: {
      Checker_Transfer(c, node->proto.return_type);
      ASTNode* curr = node->proto.arg_types;
      while (curr) {
        Checker_Transfer(c, curr);
        curr = curr->next;
      }
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotType(c, node->proto.return_type->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->proto.return_type->expr_type);
        CheckerError(c, node->marker,
                     "Return type must actually be a type, but we got '%.*s'\n",
                     str_expand(t1));
      }
      
      curr = node->proto.arg_types;
      Token_node* curr_name = node->proto.arg_names.first;
      u32 i = 0;
      while (curr) {
        CheckerIf (c,
                   IsNotType(c, curr->expr_type)) {
          string t1 =
            Debug_GetTypeString(c, curr->expr_type);
          CheckerError(c, node->marker,
                       "Argument %u must be a type, but we got '%.*s'\n",
                       i, str_expand(t1));
        }
        
        Symbol reg = (Symbol) {
          .ident = curr_name->token,
          .node = node,
          .type = curr->constant_val.type_lit,
          .scope = c->scope,
        };
        darray_add(Symbol, &c->symbols, reg);
        
        i++;
        curr = curr->next;
        curr_name = curr_name->next;
      }
      
      ValueType* reg = pool_alloc(&c->allocator);
      reg->type = TK_Func;
      reg->size = POINTER_SIZE;
      reg->func_t.ret_t = node->proto.return_type->constant_val.type_lit;
      reg->func_t.arity = node->proto.arity;
      reg->func_t.arg_ts = arena_alloc(&c->arena, sizeof(TypeIndex) * node->proto.arity);
      curr = node->proto.arg_types;
      i = 0;
      while (curr) {
        reg->func_t.arg_ts[i] = curr->constant_val.type_lit;
        i++;
        curr = curr->next;
      }
      darray_add(ValueTypeRef, &c->type_cache, reg);
      
      node->expr_type = c->type_cache.len-1;
    } break;
    
    case NT_Expr_Func: {
      Checker_Transfer(c, node->func.proto);
      /*Checker_Transfer(c, node->func.body);*/
      
      CheckingWork work = {
        .type = Work_FuncCheck,
        .super = node,
        .sym_names = node->func.proto->proto.arg_names,
        .sym_types = node->func.proto->proto.arg_types,
        .ref = node->func.body,
      };
      dqueue_push(CheckingWork, &c->worklist, work);
      
      ReturnIfError(c);
      
      node->expr_type = node->func.proto->expr_type;
    } break;
    
    case NT_Expr_Deref: {
      Checker_Transfer(c, node->deref);
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotPointer(c, node->deref->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->deref->expr_type);
        CheckerError(c, node->marker,
                     "Only pointers can be dereferenced, but we got a '%.*s'\n",
                     str_expand(t1));
      }
      
      node->expr_type = type_reduce(c, node->deref->expr_type);
    } break;
    
    case NT_Expr_Call: {
      Checker_Transfer(c, node->call.called);
      ASTNode* curr = node->call.args;
      while (curr) {
        Checker_Transfer(c, curr);
        curr = curr->next;
      }
      ReturnIfError(c);
      
      CheckerIf (c,
                 IsNotFunc(c, node->call.called->expr_type)) {
        string t1 =
          Debug_GetTypeString(c, node->call.called->expr_type);
        CheckerError(c, node->marker,
                     "Only functions can be called, but we got a '%.*s'\n",
                     str_expand(t1));
      }
      
      ValueType* called_type = TypeGet(c, node->call.called->expr_type);
      CheckerIf (c,
                 called_type->func_t.arity != node->call.arity) {
        CheckerError(c, node->marker,
                     "Function argument count mismatch, expected '%u' args, but we got '%u' of them\n",
                     called_type->func_t.arity, node->call.arity);
      }
      
      curr = node->call.args;
      u32 i = 0;
      while (curr) {
        CheckerIf (c,
                   !CheckAbsolute(c, called_type->func_t.arg_ts[i], curr->expr_type)) {
          string t1 = Debug_GetTypeString(c, called_type->func_t.arg_ts[i]);
          string t2 = Debug_GetTypeString(c, curr->expr_type);
          CheckerError(c, curr->marker,
                       "Argument %u of the function call should be a '%.*s', but we got a '%.*s'\n", i, str_expand(t1), str_expand(t2));
        }
        curr = curr->next;
        i++;
      }
      
      node->expr_type = type_reduce(c, node->call.called->expr_type);
    } break;
    
    case NT_Expr_Ident: {
      u32 idx;
      CheckerIf (c, !symbol_lookup(c->symbols, node->ident.lexeme, 0, &idx)) {
        CheckerError(c, node->marker,
                     "Undeclared Identifier '%.*s'\n",
                     str_expand(node->ident.lexeme));
      }
      
      Symbol found = c->symbols.elems[idx];
      node->is_constant = found.is_constant;
      node->constant_val = found.constant_val;
      node->expr_type = found.type;
    } break;
    
    case NT_Expr_Cast: {
      Checker_Transfer(c, node->cast.casted);
      Checker_Transfer(c, node->cast.type);
      ReturnIfError(c);
      
      CheckerIf (c,
                 !is_castable(c, TypeGet(c, node->cast.casted->expr_type), TypeGet(c, node->cast.type->constant_val.type_lit))) {
        string t1 =
          Debug_GetTypeString(c, node->cast.casted->expr_type);
        string t2 =
          Debug_GetTypeString(c, node->cast.type->constant_val.type_lit);
        CheckerError(c, node->marker,
                     "The type '%.*s' cannot be casted to '%.*s'\n",
                     str_expand(t1), str_expand(t2));
      }
      
      node->expr_type = node->cast.type->constant_val.type_lit;
    } break;
    
    case NT_Expr_Access: {
      Checker_Transfer(c, node->access.left);
      ReturnIfError(c);
      
      TypeIndex inner = Type_Index_None;
      if (node->access.deref) {
        CheckerIf (c,
                   IsNotPointer(c, node->access.left->expr_type)) {
          string t1 = Debug_GetTypeString(c, node->access.left->expr_type);
          CheckerError(c, node->marker,
                       "The expression being accessed is not a pointer to a struct or a union, but it is a '%.*s'\n",
                       str_expand(t1));
        }
        inner = TypeGet(c, node->access.left->expr_type)->ptr_t.sub_t;
        CheckerIf(c,
                  IsNotStruct(c, inner) && IsNotUnion(c, inner)) {
          string t1 = Debug_GetTypeString(c, inner);
          CheckerError(c, node->marker,
                       "The expression being accessed is not a pointer to a struct or a union, but it is a '^%.*s'\n",
                       str_expand(t1));
        }
      } else {
        CheckerIf (c,
                   IsNotStruct(c, node->access.left->expr_type) &&
                   IsNotUnion(c, node->access.left->expr_type)) {
          string t1 = Debug_GetTypeString(c, node->access.left->expr_type);
          CheckerError(c, node->marker,
                       "The expression being accessed is not a struct or a union, but it is a '%.*s'\n",
                       str_expand(t1));
        }
        inner = node->access.left->expr_type;
      }
      
      ValueType* comp = TypeGet(c, inner);
      Token_node* curr_name = comp->compound_t.member_names.first;
      b8 found = false;
      TypeIndex found_type = Type_Index_None;
      u64 i = 0;
      while (curr_name) {
        if (str_eq(node->access.right.lexeme, curr_name->token.lexeme)) {
          found = true;
          found_type = comp->compound_t.member_ts[i];
          break;
        }
        i++;
        curr_name = curr_name->next;
      }
      
      CheckerIf (c, !found) {
        string t1 = Debug_GetTypeString(c, inner);
        CheckerError(c, node->marker,
                     "No member '%.*s' in the left struct/union '%.*s'\n",
                     str_expand(node->access.right.lexeme), str_expand(t1));
      }
      
      node->expr_type = found_type;
    } break;
    
    
    
    
    
    case NT_Type_Integer: {
      if (node->int_type.size == 8) {
        node->constant_val.type_lit = node->int_type.is_signed ? Type_Index_I8 : Type_Index_U8;
      } else if (node->int_type.size == 16) {
        node->constant_val.type_lit = node->int_type.is_signed ? Type_Index_I16 : Type_Index_U16;
      } else if (node->int_type.size == 32) {
        node->constant_val.type_lit = node->int_type.is_signed ? Type_Index_I32 : Type_Index_U32;
      }
      
      node->expr_type = Type_Index_Type;
    } break;
    
    case NT_Type_Float: {
      if (node->float_type.size == 64) {
        node->constant_val.type_lit = Type_Index_F64;
      } else if (node->float_type.size == 32) {
        node->constant_val.type_lit = Type_Index_F32;
      }
      
      node->expr_type = Type_Index_Type;
    } break;
    
    case NT_Type_Void: {
      node->constant_val.type_lit = Type_Index_Void;
      
      node->expr_type = Type_Index_Type;
    } break;
    
    case NT_Type_Func: {
      Checker_Transfer(c, node->func_type.return_type);
      ASTNode* curr = node->compound_type.member_types;
      while (curr) {
        Checker_Transfer(c, curr);
        curr = curr->next;
      }
      
      CheckerIf (c,
                 IsNotType(c, node->func_type.return_type->expr_type)) {
        string t1 = Debug_GetTypeString(c, node->func_type.return_type->expr_type);
        CheckerError(c, node->marker,
                     "The return type should actually be a type, but we got a '%.*s'\n",
                     str_expand(t1));
      }
      
      curr = node->func_type.arg_types;
      u64 i = 0;
      while (curr) {
        CheckerIf (c,
                   IsNotType(c, curr->expr_type)) {
          string t1 = Debug_GetTypeString(c, curr->expr_type);
          CheckerError(c, node->marker,
                       "The %llu-th arg type should actually be a type, but we got a '%.*s'\n",
                       i, str_expand(t1));
        }
        i++;
        curr = curr->next;
      }
      
      ValueType* reg = pool_alloc(&c->allocator);
      reg->type = TK_Func;
      reg->size = POINTER_SIZE;
      reg->func_t.ret_t = node->func_type.return_type->expr_type;
      reg->func_t.arity = node->func_type.arity;
      reg->func_t.arg_ts = arena_alloc(&c->arena, sizeof(TypeIndex) * node->func_type.arity);
      curr = node->func_type.arg_types;
      i = 0;
      while (curr) {
        reg->func_t.arg_ts[i] = curr->constant_val.type_lit;
        i++;
        curr = curr->next;
      }
      darray_add(ValueTypeRef, &c->type_cache, reg);
      node->constant_val.type_lit = c->type_cache.len-1;
      
      node->expr_type = Type_Index_Type;
    } break;
    
    case NT_Type_Struct:
    case NT_Type_Union: {
      ASTNode* curr = node->compound_type.member_types;
      while (curr) {
        Checker_Transfer(c, curr);
        curr = curr->next;
      }
      
      curr = node->compound_type.member_types;
      u64 i = 0;
      while (curr) {
        CheckerIf (c,
                   IsNotType(c, curr->expr_type)) {
          string t1 = Debug_GetTypeString(c, curr->expr_type);
          CheckerError(c, node->marker,
                       "The %llu-th member type should actually be a type, but we got a '%.*s'\n",
                       i, str_expand(t1));
        }
        i++;
        curr = curr->next;
      }
      
      ValueType* reg = pool_alloc(&c->allocator);
      reg->type = node->type == NT_Type_Struct ? TK_Struct : TK_Union;
      reg->compound_t.name         = node->compound_type.name;
      reg->compound_t.member_names = node->compound_type.member_names;
      reg->compound_t.count        = node->compound_type.member_count;
      reg->compound_t.member_offsets = arena_alloc(&c->arena, sizeof(u64) * reg->compound_t.count);
      reg->compound_t.member_ts = arena_alloc(&c->arena, sizeof(TypeIndex) * reg->compound_t.count);
      curr = node->compound_type.member_types;
      i = 0;
      
      while (curr) {
        reg->compound_t.member_ts[i] = curr->constant_val.type_lit;
        
        u64 curr_size = TypeGet(c, curr->constant_val.type_lit)->size;
        if (node->type == NT_Type_Struct) {
          if (align_forward_u64(reg->size, 4) !=
              align_forward_u64(reg->size + curr_size, 4)) {
            reg->size = align_forward_u64(reg->size, 4);
          }
          reg->compound_t.member_offsets[i] = reg->size;
          reg->size += curr_size;
        } else {
          reg->size = Max(reg->size, curr_size);
        }
        
        i++;
        curr = curr->next;
      }
      reg->size = align_forward_u64(reg->size, 4);
      
      darray_add(ValueTypeRef, &c->type_cache, reg);
      node->constant_val.type_lit = c->type_cache.len-1;
      
      node->expr_type = Type_Index_Type;
    } break;
    
    case NT_Type_Pointer: {
      ValueType* reg = pool_alloc(&c->allocator);
      reg->type = TK_Pointer;
      reg->size = POINTER_SIZE;
      darray_add(ValueTypeRef, &c->type_cache, reg);
      node->constant_val.type_lit = c->type_cache.len-1;
      
      if (c->context == Checking_Type) {
        CheckingWork work = {
          .type = Work_SimpleCheck,
          .ref = node->pointer_type.sub,
          .to_update = &reg->ptr_t.sub_t,
        };
        dqueue_push(CheckingWork, &c->worklist, work);
      } else if (c->context == Checking_Procedure) {
        Checker_Transfer(c, node->pointer_type.sub);
        reg->ptr_t.sub_t = GetConstantType(node->pointer_type.sub);
      }
      node->expr_type = Type_Index_Type;
    } break;
    
    case NT_Type_Array: {
      Checker_Transfer(c, node->array_type.count);
      Checker_Transfer(c, node->array_type.sub);
      
      CheckerIf (c, !IsConstantInt(c, node->array_type.count)) {
        CheckerError(c, node->array_type.count->marker,
                     "Array Type count should be a constant integer\n");
      }
      CheckerIf (c, IsNotType(c, node->array_type.sub->expr_type)) {
        CheckerError(c, node->array_type.sub->marker,
                     "Array Modifier only applies to types\n");
      }
      
      ValueType* reg = pool_alloc(&c->allocator);
      reg->type = TK_Array;
      reg->size = GetConstantInt(node->array_type.count) * TypeGet(c,GetConstantType(node->array_type.sub))->size;
      
      darray_add(ValueTypeRef, &c->type_cache, reg);
      node->constant_val.type_lit = c->type_cache.len-1;
      
      node->expr_type = Type_Index_Type;
    } break;
    
    
    
    
    
    
    case NT_Stmt_Assign: {
      Checker_Transfer(c, node->binary_op.left);
      Checker_Transfer(c, node->binary_op.right);
      
      CheckerIf (c, !IsLValue(node->binary_op.left)) {
        CheckerError(c, node->marker,
                     "The left expression cannot be assigned to\n");
      }
      CheckerIf (c,
                 !CheckAbsolute(c, node->binary_op.left->expr_type, node->binary_op.right->expr_type)) {
        string t1 = Debug_GetTypeString(c, node->binary_op.left->expr_type);
        string t2 = Debug_GetTypeString(c, node->binary_op.right->expr_type);
        CheckerError(c, node->marker,
                     "The left and right expression types do not match: Got '%.*s' and '%.*s'\n", str_expand(t1), str_expand(t2));
      }
      
      node->expr_type = Type_Index_None;
    } break;
    
    case NT_Stmt_Expr: {
      Checker_Transfer(c, node->expr_stmt);
      
      node->expr_type = Type_Index_None;
    } break;
    
    
    case NT_Stmt_Write: {
      Checker_Transfer(c, node->write_stmt.value);
      
      node->expr_type = Type_Index_None;
    } break;
    
    case NT_Stmt_Block: {
      ASTNode* curr = node->block;
      
      ScopeResetPoint p = scope_push(c, node);
      while (curr) {
        Checker_Transfer(c, curr);
        curr = curr->next;
      }
      scope_pop(c, p);
      
      node->expr_type = Type_Index_None;
    } break;
    
    case NT_Stmt_While: {
      Checker_Transfer(c, node->while_loop.condition);
      ScopeResetPoint p = scope_push(c, node);
      Checker_Transfer(c, node->while_loop.body);
      scope_pop(c, p);
      
      node->expr_type = Type_Index_None;
    } break;
    
    case NT_Stmt_If: {
      Checker_Transfer(c, node->if_stmt.condition);
      ScopeResetPoint p = scope_push(c, node);
      Checker_Transfer(c, node->if_stmt.then_body);
      scope_pop(c, p);
      
      if (node->if_stmt.else_body) {
        ScopeResetPoint p = scope_push(c, node);
        Checker_Transfer(c, node->if_stmt.else_body);
        scope_pop(c, p);
      }
      
      node->expr_type = Type_Index_None;
    } break;
    
    case NT_Stmt_Return: {
      Checker_Transfer(c, node->return_stmt);
      
      node->expr_type = Type_Index_None;
    } break;
    
    case NT_Decl: {
      ScopeResetPoint p = scope_push(c, node);
      Checker_Transfer(c, node->decl.type);
      
      CheckingContext old = c->context;
      if (node->decl.val) {
        if (node->decl.val->type == NT_Expr_Func) {
          c->context = Checking_Procedure;
        } else if (node->decl.val->type == NT_Type_Struct ||
                   node->decl.val->type == NT_Type_Union ||
                   node->decl.val->type == NT_Type_Array ||
                   node->decl.val->type == NT_Type_Pointer) {
          c->context = Checking_Type;
        }
      }
      Checker_Transfer(c, node->decl.val);
      c->context = old;
      
      scope_pop(c, p);
      
      ReturnIfError(c);
      
      if (node->decl.is_constant) {
        CheckerIf (c, !node->decl.val) {
          CheckerError(c, node->marker, "This should not happen? Unreachable?\n");
        }
        
        CheckerIf (c, !node->decl.val->is_constant) {
          CheckerError(c, node->marker, "Value of const declaration is non-constant\n");
        }
      }
      
      TypeIndex typ = Type_Index_None;
      if (node->decl.type) {
        CheckerIf (c, IsNotType(c, node->decl.type->expr_type)) {
          string t1 = Debug_GetTypeString(c, node->decl.type->expr_type);
          CheckerError(c, node->decl.type->marker,
                       "Declaration type should actually be a type, but we got a '%.*s'\n",
                       str_expand(t1));
        }
        
        if (node->decl.val) {
          CheckerIf (c,
                     !CheckAbsolute(c, GetConstantType(node->decl.type), node->decl.val->expr_type)) {
            string t1 = Debug_GetTypeString(c, GetConstantType(node->decl.type));
            string t2 = Debug_GetTypeString(c, node->decl.val->expr_type);
            CheckerError(c, node->marker,
                         "Declaration type and type of the value assigned do not match, The type is '%.*s', and the value is a '%.*s'\n",
                         str_expand(t1), str_expand(t2));
          }
        }
        
        typ = GetConstantType(node->decl.type);
      } else {
        typ = node->decl.val->expr_type;
      }
      
      if (c->curr_func) {
        u64 edited_size = align_forward_u64(TypeGet(c, typ)->size, 4);
        node->decl.offset = 4 + c->curr_func->func.tracking_local_size;
        c->curr_func->func.tracking_local_size += edited_size;
        
        c->curr_func->func.total_local_size =
          Max(c->curr_func->func.tracking_local_size, c->curr_func->func.total_local_size);
        if (c->scope_stack.len) {
          (c->scope_stack.elems[c->scope_stack.len-1])->local_size += edited_size;
        }
      }
      
      if (!(node->status & Status_ProtoResolved)) {
        // If the prototype hasn't been transferred, then register the symbol
        if (symbol_lookup(c->symbols, node->decl.ident.lexeme, c->scope, nullptr)) {
          CheckerError(c, node->decl.ident,
                       "Redeclared Identifier '%.*s'\n",
                       str_expand(node->decl.ident.lexeme));
        }
        
        Symbol reg = (Symbol) {
          .ident = node->decl.ident,
          .node = node,
          .type = typ,
          .scope = c->scope,
        };
        if (node->decl.val) {
          reg.is_constant  = node->decl.val->is_constant;
          reg.constant_val = node->decl.val->constant_val;
        }
        darray_add(Symbol, &c->symbols, reg);
      } else {
        // If the prototype has been transferred already, update old stuff
        u32 idx;
        if (!symbol_lookup(c->symbols, node->decl.ident.lexeme, c->scope, &idx)) {
          unreachable;
        }
        
        if (node->decl.val) {
          c->symbols.elems[idx].is_constant  = node->decl.val->is_constant;
          c->symbols.elems[idx].constant_val = node->decl.val->constant_val;
        }
      }
      
      node->expr_type = Type_Index_None;
    } break;
  }
  
  node->status |= Status_Resolved | Status_ProtoResolved;
}

#undef CheckerIf
#undef ReturnIfError
#undef ReadyStat
#undef ProtoReadyStat

//~ Checker

void Checker_Init(Checker* checker) {
  MemoryZeroStruct(checker, Checker);
  pool_init(&checker->allocator, sizeof(ValueType));
  arena_init(&checker->arena);
  darray_reserve(ValueTypeRef, &checker->type_cache, 32);
  
  ValueType* type_none = pool_alloc(&checker->allocator);
  type_none->type = TK_None;
  type_none->size = 0;
  darray_add(ValueTypeRef, &checker->type_cache, type_none);
  
  ValueType* type_i8 = pool_alloc(&checker->allocator);
  type_i8->type = TK_Int;
  type_i8->size = 1;
  type_i8->int_t.size = 8;
  type_i8->int_t.is_signed = true;
  darray_add(ValueTypeRef, &checker->type_cache, type_i8);
  
  ValueType* type_i16 = pool_alloc(&checker->allocator);
  type_i16->type = TK_Int;
  type_i16->size = 2;
  type_i16->int_t.size = 16;
  type_i16->int_t.is_signed = true;
  darray_add(ValueTypeRef, &checker->type_cache, type_i16);
  
  ValueType* type_i32 = pool_alloc(&checker->allocator);
  type_i32->type = TK_Int;
  type_i32->size = 4;
  type_i32->int_t.size = 32;
  type_i32->int_t.is_signed = true;
  darray_add(ValueTypeRef, &checker->type_cache, type_i32);
  
  ValueType* type_u8 = pool_alloc(&checker->allocator);
  type_u8->type = TK_Int;
  type_u8->size = 1;
  type_u8->int_t.size = 8;
  type_u8->int_t.is_signed = false;
  darray_add(ValueTypeRef, &checker->type_cache, type_u8);
  
  ValueType* type_u16 = pool_alloc(&checker->allocator);
  type_u16->type = TK_Int;
  type_u16->size = 2;
  type_u16->int_t.size = 16;
  type_u16->int_t.is_signed = false;
  darray_add(ValueTypeRef, &checker->type_cache, type_u16);
  
  ValueType* type_u32 = pool_alloc(&checker->allocator);
  type_u32->type = TK_Int;
  type_u32->size = 4;
  type_u32->int_t.size = 32;
  type_u32->int_t.is_signed = false;
  darray_add(ValueTypeRef, &checker->type_cache, type_u32);
  
  ValueType* type_f32 = pool_alloc(&checker->allocator);
  type_f32->type = TK_Float;
  type_f32->size = 4;
  type_f32->float_t.size = 32;
  darray_add(ValueTypeRef, &checker->type_cache, type_f32);
  
  ValueType* type_f64 = pool_alloc(&checker->allocator);
  type_f64->type = TK_Float;
  type_f32->size = 8;
  type_f64->float_t.size = 32;
  darray_add(ValueTypeRef, &checker->type_cache, type_f64);
  
  ValueType* type_type = pool_alloc(&checker->allocator);
  type_type->type = TK_Type;
  type_type->size = 0; // No Runtime Size
  darray_add(ValueTypeRef, &checker->type_cache, type_type);
  
  ValueType* type_void = pool_alloc(&checker->allocator);
  type_void->type = TK_Void;
  type_void->size = 0;
  darray_add(ValueTypeRef, &checker->type_cache, type_void);
  
  ValueType* type_bool = pool_alloc(&checker->allocator);
  type_bool->type = TK_Bool;
  type_bool->size = 1;
  darray_add(ValueTypeRef, &checker->type_cache, type_bool);
}

void Checker_Check(Checker* checker, ASTNode* tree) {
  checker->tree = tree;
  checker->worklist = (dqueue(CheckingWork)) {0};
  ASTNode* curr = checker->tree;
  while (curr) {
    dqueue_push(CheckingWork, &checker->worklist, ((CheckingWork) {
                                                     .type = Work_Decl,
                                                     .ref = curr,
                                                   }));
    curr = curr->next;
  }
  
  CheckingWork current_work = {0};
  u32 unresolved_nodes = 0;
  while (dqueue_pop(CheckingWork, &checker->worklist, &current_work)) {
    ScopeResetPoint p = scope_push(checker, nullptr);
    Checker_IsReady(checker, current_work.ref, current_work.ref);
    scope_pop(checker, p);
    
    if (current_work.ref->status & Status_Ready &&
        !(current_work.ref->status & Status_Resolved)) {
      ASTNode* prev_super = checker->curr_func;
      if (current_work.super)
        checker->curr_func = current_work.super;
      
      if (current_work.type == Work_FuncCheck) {
        p = scope_push(checker, current_work.ref);
        ASTNode* ct = current_work.sym_types;
        Token_node* cn = current_work.sym_names.first;
        
        while (ct) {
          Symbol reg = (Symbol) {
            .ident = cn->token,
            .node = ct,
            .type = ct->constant_val.type_lit,
            .scope = checker->scope,
          };
          darray_add(Symbol, &checker->symbols, reg);
          
          ct = ct->next;
          cn = cn->next;
        }
        checker->context = Checking_Procedure;
      }
      
      Checker_Transfer(checker, current_work.ref);
      
      if (current_work.type == Work_FuncCheck) {
        scope_pop(checker, p);
        checker->context = Checking_Type;
      }
      
      checker->curr_func = prev_super;
      
      if (current_work.to_update)
        *current_work.to_update = current_work.ref->constant_val.type_lit;
      
      unresolved_nodes = 0;
    } else if (current_work.ref->status & Status_ProtoReady &&
               !(current_work.ref->status & Status_ProtoResolved)) {
      ASTNode* prev_super = checker->curr_func;
      if (current_work.super)
        checker->curr_func = current_work.super;
      Checker_ProtoTransfer(checker, current_work.ref);
      checker->curr_func = prev_super;
      
      dqueue_push(CheckingWork, &checker->worklist, current_work);
    } else {
      dqueue_push(CheckingWork, &checker->worklist, current_work);
      current_work.ref->status |= Status_Tried;
      
      if (unresolved_nodes == (checker->worklist.back-checker->worklist.front)) {
        
        // Dependency Cycle or Undeclared Identifier
        // CheckerErrorNoRet(checker, (Token) {0}, "Dependency Cycle or Undeclared Identifier Detected\n");
        
        // Build Adjacency List
        while (dqueue_pop(CheckingWork, &checker->worklist, &current_work)) {
          if (current_work.type == Work_Decl)
            Checker_ErrorsBuild(checker, current_work.ref, current_work.ref);
        }
        
        // Detect Cycles
        if (checker->cycles_exist)
          Checker_DetectCycles(checker, &checker->cycle_checker);
        
        // Free Everything
        Iterate (checker->cycle_checker, i)
          darray_free(ASTNodeRef, &checker->cycle_checker.elems[i]);
        darray_free(ASTNodeRef_array, &checker->cycle_checker);
        
        dqueue_free(CheckingWork, &checker->worklist);
        return;
      }
      
      unresolved_nodes += 1;
    }
  }
  
  dqueue_free(CheckingWork, &checker->worklist);
  printf("Done Checking\n");
}

void Checker_Free(Checker* checker) {
  pool_free(&checker->allocator);
  arena_init(&checker->arena);
  darray_free(Symbol, &checker->symbols);
}

//~ Debug

string Debug_GetTypeString(Checker* c, TypeIndex idx) {
  ValueType* type = TypeGet(c, idx);
  M_Arena* arena = &c->arena;
  
  if (!type) {
    return str_lit("void");
  }
  
  switch (type->type) {
    case TK_None: return str_lit("none");
    case TK_Void: return str_lit("void");
    case TK_Bool: return str_lit("bool");
    case TK_Int:  return str_from_format(arena,
                                         "%s%d", type->int_t.is_signed ? "i" : "u",
                                         type->int_t.size);
    case TK_Float: return str_from_format(arena, "f%d", type->float_t.size);
    case TK_Type: return str_lit("type"); break;
    
    case TK_Pointer: return str_cat(arena, str_lit("^"), Debug_GetTypeString(c, type->ptr_t.sub_t));
    case TK_Array: {
      string subtype = Debug_GetTypeString(c, type->ptr_t.sub_t);
      return str_from_format(arena, "[%llu]%.*s", type->array_t.count, str_expand(subtype));
    } break;
    
    case TK_Struct:
    case TK_Union: {
      if (type->compound_t.name.size) {
        return type->compound_t.name;
      } else {
        string_list sl = {0};
        string_list_push(arena, &sl, type->type == TK_Struct
                         ? str_lit("<anon> struct { ")
                         : str_lit("<anon> union { "));
        for (u64 i = 0; i < type->compound_t.count; i++) {
          string_list_push(arena, &sl, Debug_GetTypeString(c, type->compound_t.member_ts[i]));
          string_list_push(arena, &sl, str_lit("; "));
        }
        string_list_push(arena, &sl, str_lit("}"));
        
        return string_list_flatten(arena, &sl);
      }
    } break;
    
    case TK_Func: {
      string_list sl = {0};
      string_list_push(arena, &sl, str_lit("func ("));
      for (u32 i = 0; i < type->func_t.arity; i++) {
        string_list_push(arena, &sl, Debug_GetTypeString(c, type->func_t.arg_ts[i]));
        if (i != type->func_t.arity-1) string_list_push(arena, &sl, str_lit(","));
      }
      string_list_push(arena, &sl, str_lit(") -> "));
      string_list_push(arena, &sl, Debug_GetTypeString(c, type->func_t.ret_t));
      return string_list_flatten(arena, &sl);
    } break;
    
    default: {
      return str_from_format(arena, "Got something weird: %u\n", type->type);
    } break;
  }
}
