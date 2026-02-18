#ifndef LEXER_H
#define LEXER_H

#include "defines.h"
#include "common.h"

typedef struct Lexer {
  u8* start;
  u8* curr;
  u32 line, col, start_col;
} Lexer;

void          Lexer_Init(Lexer* lexer);
darray(Token) Lexer_Lex(Lexer* lexer, string source);
char*         Debug_TokenType_ToString(TokenType type);

#endif //LEXER_H