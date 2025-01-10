#include "lexer.h"
#include <string.h>
#include <stdio.h>

//~ Helpers

static u8 get_next(Lexer* lexer) {
  if (*lexer->curr == '\0') return '\0';
  return lexer->curr[1];
}

static b8 is_digit(u8 c) {
  return c >= '0' && c <= '9';
}

static b8 is_whitespace(u8 c) {
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

static b8 is_alpha(u8 c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static Token error_token(Lexer* lexer, string message) {
  Token ret;
  ret.type = TT_Error;
  ret.lexeme = message;
  ret.line = lexer->line;
  ret.col  = lexer->col;
  return ret;
}

static Token make_token(Lexer* lexer, TokenType type) {
  Token ret;
  ret.type = type;
  ret.lexeme = (string) {
    .str  = lexer->start,
    .size = (u32) (lexer->curr - lexer->start)
  };
  ret.line = lexer->line;
  ret.col  = lexer->start_col;
  return ret;
}

#define match_str(lexer, start, needle, yes)\
match_str_def(lexer, start, needle, yes, TT_Ident)

static TokenType match_str_def(Lexer* lexer, u32 start, string needle, TokenType yes, TokenType no) {
  if (lexer->curr - lexer->start == start + needle.size &&
      memcmp(lexer->start + start, needle.str, needle.size) == 0) {
    return yes;
  }
  return no;
}

static TokenType ident_token_type(Lexer* lexer) {
  u32 sz = (u32) (lexer->curr - lexer->start);
  switch (lexer->start[0]) {
    case 'i': {
      if (sz == 3) {
        if (lexer->start[1] == '1')
          return match_str(lexer, 2, str_lit("6"), TT_I16);
        else if (lexer->start[1] == '3')
          return match_str(lexer, 2, str_lit("2"), TT_I32);
      } else if (sz == 2) {
        if (lexer->start[1] == '8')
          return TT_I8;
        return match_str(lexer, 1, str_lit("f"), TT_If);
      }
    } break;
    
    case 'u': {
      if (sz == 2) {
        if (lexer->start[1] == '8')
          return TT_U8;
      } else if (sz == 3) {
        if (lexer->start[1] == '1')
          return match_str(lexer, 2, str_lit("6"), TT_U16);
        else if (lexer->start[1] == '3')
          return match_str(lexer, 2, str_lit("2"), TT_U32);
      } else if (sz == 5) {
        if (lexer->start[1] == 'n')
          return match_str(lexer, 2, str_lit("ion"), TT_Union);;
      }
    } break;
    
    case 'f': {
      if (sz == 3) {
        if (lexer->start[1] == '3')
          return match_str(lexer, 2, str_lit("2"), TT_F32);
        else if (lexer->start[1] == '6')
          return match_str(lexer, 2, str_lit("4"), TT_F64);
        
      } else if (sz == 4) {
        return match_str(lexer, 1, str_lit("unc"), TT_Func);
      }
    } break;
    
    case 'c': {
      return match_str(lexer, 1, str_lit("ast"), TT_Cast);
    } break;
    
    case 'v': {
      return match_str(lexer, 1, str_lit("oid"), TT_Void);
    } break;
    
    case 'w': {
      if (sz == 5) {
        if (lexer->start[1] == 'h')
          return match_str(lexer, 2, str_lit("ile"), TT_While);
        else if (lexer->start[1] == 'r')
          return match_str(lexer, 2, str_lit("ite"), TT_Write);
      }
    } break;
    
    case 'e': {
      return match_str(lexer, 1, str_lit("lse"), TT_Else);
    } break;
    
    case 'r': {
      return match_str(lexer, 1, str_lit("eturn"), TT_Return);
    } break;
    
    case 's': {
      return match_str(lexer, 1, str_lit("truct"), TT_Struct);
    } break;
    
    case 'l': {
      return match_str(lexer, 1, str_lit("it"), TT_Lit);
    } break;
  }
  return TT_Ident;
}

DArray_Impl(Token);

//~ Main Stuff

void Lexer_Init(Lexer* lexer) {
  lexer->line = 1;
  lexer->col = 1;
}

Token_array Lexer_Lex(Lexer* lexer, string source) {
  Token_array tokens = {0};
  lexer->start = source.str;
  lexer->curr = source.str;
  
  while (true) {
    while (is_whitespace(*lexer->curr)) {
      if (*lexer->curr == '\n') {
        lexer->curr++;
        lexer->col = 0;
        lexer->line++;
      } else {
        lexer->curr++;
        lexer->col++;
      }
    }
    lexer->start = lexer->curr;
    lexer->start_col = lexer->col;
    
    if (!*lexer->curr) {
      darray_add(Token, &tokens, make_token(lexer, TT_EOF));
      break;
    }
    u8 c = *lexer->curr++;
    lexer->col++;
    
    switch (c) {
      
      case '+': darray_add(Token, &tokens, make_token(lexer, TT_Plus)); break;
      case '*': darray_add(Token, &tokens, make_token(lexer, TT_Star)); break;
      case '%': darray_add(Token, &tokens, make_token(lexer, TT_Percent)); break;
      case '^': darray_add(Token, &tokens, make_token(lexer, TT_Caret)); break;
      case '{': darray_add(Token, &tokens, make_token(lexer, TT_OpenBrace)); break;
      case '}': darray_add(Token, &tokens, make_token(lexer, TT_CloseBrace)); break;
      case '[': darray_add(Token, &tokens, make_token(lexer, TT_OpenBracket)); break;
      case ']': darray_add(Token, &tokens, make_token(lexer, TT_CloseBracket)); break;
      case '(': darray_add(Token, &tokens, make_token(lexer, TT_OpenParen)); break;
      case ')': darray_add(Token, &tokens, make_token(lexer, TT_CloseParen)); break;
      case ':': darray_add(Token, &tokens, make_token(lexer, TT_Colon)); break;
      case ';': darray_add(Token, &tokens, make_token(lexer, TT_Semicolon)); break;
      case ',': darray_add(Token, &tokens, make_token(lexer, TT_Comma)); break;
      case '.': darray_add(Token, &tokens, make_token(lexer, TT_Dot)); break;
      
      case '/': {
        if (*lexer->curr == '/') {
          while (*lexer->curr != '\n' && *lexer->curr != '\0') {
            lexer->curr++;
            lexer->col++;
          }
          if (*lexer->curr != '\0') lexer->curr++;
          lexer->col = 0;
          lexer->line++;
          continue;
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Slash)); break;
      } break;
      
      case '-': {
        if (*lexer->curr == '>') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_ArrowRight));
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Minus));
      } break;
      
      case '<': {
        if (*lexer->curr == '=') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_LessEqual));
        } else if (*lexer->curr == '<') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_ShiftLeft));
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Less));
      } break;
      
      case '>': {
        if (*lexer->curr == '=') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_GreaterEqual));
        } else if (*lexer->curr == '>') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_ShiftRight));
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Greater));
      } break;
      
      case '=': {
        if (*lexer->curr == '=') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_EqualEqual));
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Equal));
      } break;
      
      case '!': {
        if (*lexer->curr == '=') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_BangEqual));
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Bang));
      } break;
      
      case '&': {
        if (*lexer->curr == '&') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_AmpAmp));
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Amp));
      } break;
      
      case '|': {
        if (*lexer->curr == '|') {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, make_token(lexer, TT_PipePipe));
        } else
          darray_add(Token, &tokens, make_token(lexer, TT_Pipe));
      } break;
      
      case '1': case '2': case '3': case '4': case '5':
      case '6': case '7': case '8': case '9': case '0': {
        if (c == '0' && (*lexer->curr == 'x' || *lexer->curr == 'X')) {
          lexer->curr++;
          lexer->col++;
          // Hex number
          while (is_digit(*lexer->curr) ||
                 (*lexer->curr >= 'a' && *lexer->curr <= 'f') ||
                 (*lexer->curr >= 'A' && *lexer->curr <= 'F')) {
            lexer->curr++;
            lexer->col++;
          }
          
          darray_add(Token, &tokens, make_token(lexer, TT_HexLit));
        } else {
          while (is_digit(*lexer->curr)) {
            lexer->curr++;
            lexer->col++;
          }
          
          if (*lexer->curr == '.') {
            lexer->curr++;
            lexer->col++;
            while (is_digit(*lexer->curr)) {
              lexer->curr++;
              lexer->col++;
            }
            darray_add(Token, &tokens, make_token(lexer, TT_FloatLit));
          } else {
            darray_add(Token, &tokens, make_token(lexer, TT_IntLit));
          }
        }
      } break;
      
      default: {
        if (!is_alpha(c)) {
          lexer->curr++;
          lexer->col++;
          darray_add(Token, &tokens, error_token(lexer, str_lit("Unexpected Character")));
          
        } else {
          while (is_alpha(*lexer->curr) || is_digit(*lexer->curr)) {
            lexer->curr++;
            lexer->col++;
          }
          
          darray_add(Token, &tokens, make_token(lexer, ident_token_type(lexer)));
        }
        
      } break;
    }
    
  }
  
  return tokens;
}


//~ Debug

char* tokentype_to_string_table[TT_MAX] = {
  [TT_EOF] = "EOF",
  [TT_Error] = "Error",
  [TT_Ident] = "Identifier",
  [TT_IntLit] = "Integer Literal",
  [TT_FloatLit] = "Float Literal",
  [TT_I32] = "I32",
  [TT_I16] = "I16",
  [TT_I8] = "I8",
  [TT_U32] = "U32",
  [TT_U16] = "U16",
  [TT_U8] = "U8",
  [TT_F32] = "F32",
  [TT_F64] = "F64",
  [TT_Plus] = "+",
  [TT_Minus] = "-",
  [TT_Star] = "*",
  [TT_Slash] = "/",
  [TT_Percent] = "%",
  [TT_Less] = "<",
  [TT_Greater] = ">",
  [TT_LessEqual] = "<=",
  [TT_GreaterEqual] = ">=",
  [TT_Bang] = "!",
  [TT_EqualEqual] = "==",
  [TT_BangEqual] = "!=",
  [TT_Colon] = ":",
  [TT_Semicolon] = ";",
  [TT_Equal] = "=",
  [TT_OpenBrace] = "{",
  [TT_OpenBracket] = "[",
  [TT_OpenParen] = "(",
  [TT_CloseBrace] = "}",
  [TT_CloseBracket] = "]",
  [TT_CloseParen] = ")",
};

char* Debug_TokenType_ToString(TokenType type) {
  if (type >= TT_MAX) return " Bad ";
  return tokentype_to_string_table[type];
}

//~ Token list
void Token_list_push_node(Token_list* list, Token_node* node) {
  if (!list->first && !list->last) {
    list->first = node;
    list->last = node;
  } else {
    list->last->next = node;
    list->last = node;
  }
  list->node_count += 1;
}

void Token_list_push(M_Arena* arena, Token_list* list, Token tok) {
  Token_node* node = (Token_node*) arena_alloc(arena, sizeof(Token_node));
  node->token = tok;
  Token_list_push_node(list, node);
}

