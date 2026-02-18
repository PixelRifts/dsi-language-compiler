/* date = December 3rd 2023 6:38 pm */

#ifndef CHECK_H
#define CHECK_H

#include "common.h"

//~ Types

// Consider moving typecache from darray to stable_table
// typedef struct ValueTypeBucket ValueTypeBucket;
// struct ValueTypeBucket {
//   ValueType* type;
//   ASTNode* key;
//
//   ValueTypeBucket* hash_next;
//   ValueTypeBucket* hash_prev;
// };

typedef enum CheckingContext {
  Checking_Type,
  Checking_Procedure,
} CheckingContext;

typedef u32 ScopeResetPoint;

typedef enum CheckingWorkType {
  Work_Decl,
  Work_SimpleCheck,
  Work_FuncCheck,
} CheckingWorkType;

typedef struct CheckingWork {
  CheckingWorkType type;
  
  ASTNode* ref;
  ASTNode* super;
  Token_list sym_names;
  ASTNode* sym_types;
  TypeIndex* to_update;
} CheckingWork;

Queue_Prototype(CheckingWork);

//~ Checker

typedef struct Checker {
  ASTNode* tree;
  M_Pool   allocator;
  M_Arena  arena;
  string   filename;
  u32      scope;
  b8       errored;
  b8       cycles_exist;
  
  CheckingContext context;
  
  u64 curr_offset;
  ASTNode* curr_func;
  dstack(ASTNodeRef) scope_stack;
  
  dqueue(CheckingWork) worklist;
  darray(ASTNodeRef_array) cycle_checker;
  
  darray(ValueTypeRef) type_cache;
  darray(Symbol) symbols;
} Checker;

void Checker_Init(Checker* checker);
void Checker_Check(Checker* checker, ASTNode* tree);
void Checker_Free(Checker* checker);

string Debug_GetTypeString(Checker* c, TypeIndex idx);

#endif //CHECK_H
