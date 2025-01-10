/* date = February 25th 2024 10:43 am */

#ifndef DSI_BACK_H
#define DSI_BACK_H

#include "base/ds.h"
#include "parser.h"
#include "check.h"

typedef enum SymbolUsage {
  Usage_Offset,
  Usage_Name,
} SymbolUsage;

typedef struct DSIBackendSymbol {
  Token ident;
  SymbolUsage usage;
  u32 scope;
  i64 offset;
  TypeIndex type;
} DSIBackendSymbol;
DArray_Prototype(DSIBackendSymbol);

#define ALLOCATION_SIZE Kilobytes(1)

typedef struct DSIBackend {
  M_Arena static_arena;
  string_list instructions;
  string_list data;
  
  u32 scope;
  
  u64 offset;
  string filename;
  string func_name;
  u64 label_id;
  b8 errored;
  
  ASTNode* tree;
  darray(ValueTypeRef)* type_cache;
  darray(DSIBackendSymbol) symbols;
} DSIBackend;

void DSIBack_Init(DSIBackend* b, darray(ValueTypeRef)* type_cache);
void DSIBack_Write(DSIBackend* b, ASTNode* tree);
void DSIBack_Free(DSIBackend* b);

#endif //DSI_BACK_H
