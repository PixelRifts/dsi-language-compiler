#ifndef LEXER_H
#define LEXER_H

#include "defines.h"
#include "base/str.h"
#include "base/ds.h"
#include "base/mem.h"

typedef u32 TokenType;
enum {
  TT_EOF,    TT_Error,    TT_Ident,
  TT_IntLit, TT_HexLit,   TT_FloatLit,
  TT_I8,     TT_I16,      TT_I32,
  TT_U8,     TT_U16,      TT_U32,
  TT_F32,                 TT_F64,
  TT_Func,   TT_While,    TT_If,     TT_Else,   TT_Void,   TT_Return,
  TT_Write,  TT_Lit,
  
  TT_Plus,  TT_Minus,      TT_Star,      TT_Slash,        TT_Percent,    TT_Caret,
  TT_Less,  TT_Greater,    TT_LessEqual, TT_GreaterEqual,
  TT_Bang,  TT_EqualEqual, TT_BangEqual,
  TT_Comma, TT_Colon,      TT_Semicolon, TT_Equal,        TT_ArrowRight,
  TT_Amp,   TT_Pipe,       TT_AmpAmp,    TT_PipePipe,
  
  TT_ShiftLeft, TT_ShiftRight,
  TT_Dot,       TT_Cast,
  TT_Struct,    TT_Union,
  
  TT_OpenBrace,  TT_OpenBracket,  TT_OpenParen,
  TT_CloseBrace, TT_CloseBracket, TT_CloseParen,
  
  TT_MAX,
};

typedef struct Lexer {
  u8* start;
  u8* curr;
  u32 line, col, start_col;
} Lexer;

typedef struct Token {
  TokenType type;
  string lexeme;
  u32 line, col;
} Token;

DArray_Prototype(Token);

void          Lexer_Init(Lexer* lexer);
darray(Token) Lexer_Lex(Lexer* lexer, string source);
char*         Debug_TokenType_ToString(TokenType type);


//~ Token list

typedef struct Token_node {
  Token token;
  struct Token_node* next;
} Token_node;

typedef struct Token_list {
  Token_node* first;
  Token_node* last;
  i32 node_count;
} Token_list;

void  Token_list_push_node(Token_list* list, Token_node* node);
void  Token_list_push(M_Arena* arena, Token_list* list, Token tok);


#endif //LEXER_H