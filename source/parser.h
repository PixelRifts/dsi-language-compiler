/* date = November 27th 2023 8:30 am */

#ifndef PARSER_H
#define PARSER_H

#include "defines.h"
#include "common.h"

//~ Parser

typedef struct Parser {
  M_Arena static_arena;
  M_Pool  allocator;
  u64 curr, next;
  u32 scope;
  darray(Token) tokens;
  
  string filename;
  b8 errored;
  
  string curr_func_name;
  u64 label_next;
} Parser;

void     Parser_Init(Parser* parser);
ASTNode* Parser_Parse(Parser* parser, darray(Token) tokens);
void     Parser_Free(Parser* parser);

void Debug_Dump_ASTree(ASTNode* node);

#endif //PARSER_H
