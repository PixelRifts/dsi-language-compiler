
#include "parser.h"
#include "base/log.h"

#define ParserError(p, t, f, ...) Statement(\
printf("%.*s:%d:%d - Parse Error - " f,\
str_expand(p->filename), t.line, t.col,\
##__VA_ARGS__);\
p->errored = true;\
)

DArray_Impl(ASTNodeRef);
DArray_Impl(ASTNodeRef_array);
Queue_Impl(ASTNodeRef);
Stack_Impl(ASTNodeRef);

//~ Helpers

static Token current_token(Parser* parser) {
  return parser->tokens.elems[parser->curr];
}
static TokenType current_token_type(Parser* parser) {
  return parser->tokens.elems[parser->curr].type;
}

static Token next_token(Parser* parser) {
  if (parser->next >= parser->tokens.len) return (Token) { .type=TT_EOF };
  return parser->tokens.elems[parser->next];
}
static TokenType next_token_type(Parser* parser) {
  if (parser->next >= parser->tokens.len) return TT_EOF;
  return parser->tokens.elems[parser->next].type;
}


static void parser_advance(Parser* parser) {
  parser->curr++;
  parser->next++;
}

static Token parser_eat(Parser* parser, TokenType type) {
  if (current_token_type(parser) == type) {
    Token ret = current_token(parser);
    parser_advance(parser);
    return ret;
  }
  ParserError(parser, current_token(parser), "Expected token %s, but got %s\n",
              Debug_TokenType_ToString(type),
              Debug_TokenType_ToString(current_token_type(parser)));
  return (Token) { .type=TT_Error };
}

static b8 parser_match(Parser* parser, TokenType type) {
  if (current_token_type(parser) == type) {
    parser_advance(parser);
    return true;
  }
  return false;
}

//~ Data

OpPrecedence precedence_lookup[TT_MAX] = {
  [TT_Plus]         = Prec_Term,
  [TT_Minus]        = Prec_Term,
  [TT_Star]         = Prec_Factor,
  [TT_Slash]        = Prec_Factor,
  [TT_Percent]      = Prec_Factor,
  [TT_BangEqual]    = Prec_Eq,
  [TT_EqualEqual]   = Prec_Eq,
  [TT_ShiftLeft]    = Prec_Shift,
  [TT_ShiftRight]   = Prec_Shift,
  [TT_Amp]          = Prec_BitAND,
  [TT_Pipe]         = Prec_BitOR,
  [TT_Less]         = Prec_Cmp,
  [TT_Greater]      = Prec_Cmp,
  [TT_LessEqual]    = Prec_Cmp,
  [TT_GreaterEqual] = Prec_Cmp,
  [TT_OpenParen]    = Prec_Call,
  [TT_OpenBracket]  = Prec_Call,
  [TT_Caret]        = Prec_Call,
  [TT_Dot]          = Prec_Call,
  [TT_ArrowRight]   = Prec_Call,
};

//~ Node Creation

ASTNode* error_node = nullptr;
static ASTNode* make_error_node(Parser* parser, Token marker) {
  if (!error_node) {
    error_node = pool_alloc(&parser->allocator);
    error_node->type = NT_Error;
    error_node->marker = marker;
  }
  return error_node;
}

static ASTNode* make_integer_node(Parser* parser, Token marker, i64 val) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_IntLit;
  node->marker = marker;
  node->is_constant = true;
  node->constant_val.type = CVT_Int;
  node->constant_val.int_lit = val;
  return node;
}

static ASTNode* make_float_node(Parser* parser, Token marker, f64 val) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_FloatLit;
  node->marker = marker;
  node->is_constant = true;
  node->constant_val.type = CVT_Float;
  node->constant_val.float_lit = val;
  return node;
}

static ASTNode* make_array_lit_node(Parser* parser, Token marker, ASTNode* type, ASTNode* elems, u32 count) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_ArrayLit;
  node->marker = marker;
  node->array_lit.type = type;
  node->array_lit.values = elems;
  node->array_lit.count = count;
  return node;
}

static ASTNode* make_ident_node(Parser* parser, Token ident) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Ident;
  node->marker = ident;
  node->ident = ident;
  return node;
}

static ASTNode* make_binary_node(Parser* parser, ASTNode* left, ASTNode* right, Token tok) {
  ASTNode* node = pool_alloc(&parser->allocator);
  NodeType type = NT_Error;
  switch (tok.type) {
    case TT_Plus:         type = NT_Expr_Add;        break;
    case TT_Minus:        type = NT_Expr_Sub;        break;
    case TT_Star:         type = NT_Expr_Mul;        break;
    case TT_Slash:        type = NT_Expr_Div;        break;
    case TT_Percent:      type = NT_Expr_Mod;        break;
    case TT_EqualEqual:   type = NT_Expr_Eq;         break;
    case TT_BangEqual:    type = NT_Expr_Neq;        break;
    case TT_Less:         type = NT_Expr_Less;       break;
    case TT_Greater:      type = NT_Expr_Greater;    break;
    case TT_LessEqual:    type = NT_Expr_LessEq;     break;
    case TT_GreaterEqual: type = NT_Expr_GreaterEq;  break;
    case TT_ShiftLeft:    type = NT_Expr_ShiftLeft;  break;
    case TT_ShiftRight:   type = NT_Expr_ShiftRight; break;
    case TT_Amp:          type = NT_Expr_BitAND;     break;
    case TT_Pipe:         type = NT_Expr_BitOR;      break;
  }
  node->type = type;
  node->marker = tok;
  node->binary_op.left = left;
  node->binary_op.right = right;
  return node;
}

static ASTNode* make_assign_node(Parser* parser, Token marker, ASTNode* left, ASTNode* right) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Stmt_Assign;
  node->marker = marker;
  node->binary_op.left = left;
  node->binary_op.right = right;
  return node;
}

static ASTNode* make_unary_node(Parser* parser, ASTNode* operand, Token tok) {
  ASTNode* node = pool_alloc(&parser->allocator);
  NodeType type = NT_Error;
  switch (tok.type) {
    case TT_Plus:  type = NT_Expr_Identity; break;
    case TT_Minus: type = NT_Expr_Negate;   break;
    case TT_Bang:  type = NT_Expr_Not;      break;
  }
  node->type = type;
  node->marker = tok;
  node->unary_op.operand = operand;
  return node;
}

static ASTNode* make_func_proto_node(Parser* parser, Token marker, string name,
                                     ASTNode* return_type, Token_list arg_names,
                                     ASTNode* arg_types, u32 arity) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_FuncProto;
  node->marker = marker;
  node->proto.name = name;
  node->proto.return_type = return_type;
  node->proto.arg_types = arg_types;
  node->proto.arg_names = arg_names;
  node->proto.arity = arity;
  return node;
}

static ASTNode* make_func_node(Parser* parser, Token marker, string name,
                               ASTNode* return_type, Token_list arg_names,
                               ASTNode* arg_types, ASTNode* body, u32 arity) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Func;
  node->marker = marker;
  node->func.proto = make_func_proto_node(parser, marker, name, return_type, arg_names, arg_types, arity);
  node->func.body = body;
  node->is_constant = true;
  node->constant_val.type = 0;
  return node;
}

static ASTNode* make_array_index_node(Parser* parser, Token marker, ASTNode* left, ASTNode* idx) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Index;
  node->marker = marker;
  node->index.left = left;
  node->index.idx = idx;
  return node;
}

static ASTNode* make_addr_node(Parser* parser, Token tok, ASTNode* operand) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Addr;
  node->marker = tok;
  node->addr = operand;
  return node;
}

static ASTNode* make_deref_node(Parser* parser, Token marker, ASTNode* operand) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Deref;
  node->marker = marker;
  node->deref = operand;
  return node;
}

static ASTNode* make_cast_node(Parser* parser, Token marker, ASTNode* casted, ASTNode* type) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Cast;
  node->marker = marker;
  node->cast.casted = casted;
  node->cast.type = type;
  return node;
}

static ASTNode* make_access_node(Parser* parser, Token marker, ASTNode* left, Token right, b8 deref) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Access;
  node->marker = marker;
  node->access.left  = left;
  node->access.right = right;
  node->access.deref = deref;
  return node;
}

static ASTNode* make_func_call_node(Parser* parser, Token marker, ASTNode* called,
                                    ASTNode* args, u32 arity) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Expr_Call;
  node->marker = marker;
  node->call.called = called;
  node->call.args = args;
  node->call.arity = arity;
  return node;
}

static ASTNode* make_void_type_node(Parser* parser, Token marker) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Type_Void;
  node->is_constant = true;
  node->constant_val.type = CVT_Type;
  node->marker = marker;
  return node;
}

static ASTNode* make_int_type_node(Parser* parser, Token tok) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Type_Integer;
  node->marker = tok;
  if (tok.type == TT_I32 || tok.type == TT_U32)
    node->int_type.size = 32;
  else if (tok.type == TT_I16 || tok.type == TT_U16)
    node->int_type.size = 16;
  else
    node->int_type.size = 8;
  node->is_constant = true;
  node->constant_val.type = CVT_Type;
  node->int_type.is_signed = (tok.type == TT_I32) || (tok.type == TT_I16) || (tok.type == TT_I8);
  return node;
}

static ASTNode* make_float_type_node(Parser* parser, Token tok) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Type_Float;
  node->marker = tok;
  if (tok.type == TT_F32)
    node->float_type.size = 32;
  else
    node->float_type.size = 64;
  node->is_constant = true;
  node->constant_val.type = CVT_Type;
  return node;
}

static ASTNode* make_pointer_type_node(Parser* parser, Token marker, ASTNode* sub) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Type_Pointer;
  node->pointer_type.sub = sub;
  node->is_constant = true;
  node->constant_val.type = CVT_Type;
  return node;
}

static ASTNode* make_array_type_node(Parser* parser, Token marker, ASTNode* count,
                                     ASTNode* sub) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Type_Array;
  node->array_type.sub = sub;
  node->array_type.count = count;
  node->is_constant = true;
  node->constant_val.type = CVT_Type;
  return node;
}

static ASTNode* make_func_type_node(Parser* parser, Token marker, ASTNode* return_type,
                                    ASTNode* arg_types, u32 arity) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Type_Func;
  node->marker = marker;
  node->func_type.return_type = return_type;
  node->func_type.arg_types = arg_types;
  node->func_type.arity = arity;
  node->is_constant = true;
  node->constant_val.type = CVT_Type;
  return node;
}

static ASTNode* make_compound_type_node(Parser* parser, Token marker, string name,
                                        u64 member_count, Token_list member_names, ASTNode* member_types) {
  ASTNode* node = pool_alloc(&parser->allocator);
  NodeType type = NT_Error;
  switch (marker.type) {
    case TT_Struct: type = NT_Type_Struct; break;
    case TT_Union:  type = NT_Type_Union;  break;
  }
  node->type = type;
  node->marker = marker;
  node->compound_type.name = name;
  node->compound_type.member_count = member_count;
  node->compound_type.member_names = member_names;
  node->compound_type.member_types = member_types;
  node->is_constant = true;
  node->constant_val.type = CVT_Type;
  return node;
}

static ASTNode* make_expr_stmt_node(Parser* parser, Token marker, ASTNode* expr) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Stmt_Expr;
  node->marker = marker;
  node->expr_stmt = expr;
  return node;
}

static ASTNode* make_write_stmt_node(Parser* parser, Token marker, i32 ptr, ASTNode* value) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Stmt_Write;
  node->marker = marker;
  node->write_stmt.ptr = ptr;
  node->write_stmt.value = value;
  return node;
}

static ASTNode* make_block_node(Parser* parser, Token marker, ASTNode* block) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Stmt_Block;
  node->marker = marker;
  node->block = block;
  return node;
}

static ASTNode* make_while_node(Parser* parser, Token marker, ASTNode* condition, ASTNode* body) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Stmt_While;
  node->marker = marker;
  node->while_loop.condition = condition;
  node->while_loop.body = body;
  return node;
}

static ASTNode* make_if_node(Parser* parser, Token marker, ASTNode* condition, ASTNode* then_body,
                             ASTNode* else_body) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Stmt_If;
  node->marker = marker;
  node->if_stmt.condition = condition;
  node->if_stmt.then_body = then_body;
  node->if_stmt.else_body = else_body;
  return node;
}

static ASTNode* make_return_node(Parser* parser, Token marker, ASTNode* ret) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Stmt_Return;
  node->marker = marker;
  node->return_stmt = ret;
  return node;
}

static ASTNode* make_decl_node(Parser* parser, Token ident, ASTNode* type,
                               ASTNode* val, b8 is_constant) {
  ASTNode* node = pool_alloc(&parser->allocator);
  node->type = NT_Decl;
  node->marker = ident;
  node->decl.ident = ident;
  node->decl.type = type;
  node->decl.val = val;
  node->decl.is_constant = is_constant;
  node->decl.color = 0;
  node->decl.parent = -1;
  return node;
}

//~ Expression Parsing Helpers

#define Parser_ParseExpr(p, op) Parser_ParseExpr_N(p, op, (string){0})
#define Parser_ParsePrefixExpr(p) Parser_ParsePrefixExpr_N(p, (string){0})

static ASTNode* Parser_ParseExpr_N(Parser* parser, OpPrecedence curr_op_prec, string decl_name);
static ASTNode* Parser_ParseStmt(Parser* parser);

static ASTNode* Parser_ParsePrefixExpr_N(Parser* parser, string decl_name) {
  switch (current_token_type(parser)) {
    case TT_IntLit: {
      Token curr = current_token(parser);
      i64 value = strtoll((const char*) curr.lexeme.str, nullptr, 10);
      parser_advance(parser);
      return make_integer_node(parser, curr, value);
    } break;
    
    case TT_HexLit: {
      Token curr = current_token(parser);
      i64 value = strtoll((const char*) curr.lexeme.str, nullptr, 16);
      parser_advance(parser);
      return make_integer_node(parser, curr, value);
    } break;
    
    case TT_FloatLit: {
      Token curr = current_token(parser);
      f64 value = strtod((const char*) curr.lexeme.str, nullptr);
      parser_advance(parser);
      return make_float_node(parser, curr, value);
    } break;
    
    case TT_Lit: {
      Token curr = current_token(parser);
      parser_advance(parser);
      parser_eat(parser, TT_OpenParen);
      ASTNode* type = Parser_ParseExpr(parser, Prec_None);
      parser_eat(parser, TT_CloseParen);
      
      parser_eat(parser, TT_OpenBracket);
      u32 count = 0;
      ASTNode* cn = nullptr;
      ASTNode* elems = nullptr;
      while (!parser_match(parser, TT_CloseBracket)) {
        if (!cn) {
          cn = Parser_ParseExpr(parser, Prec_None);
          elems = cn;
        } else {
          cn->next = Parser_ParseExpr(parser, Prec_None);
          cn = cn->next;
        }
        count += 1;
        if (!parser_match(parser, TT_Comma)) {
          parser_eat(parser, TT_CloseBracket);
          break;
        }
      }
      return make_array_lit_node(parser, curr, type, elems, count);
    } break;
    
    case TT_Plus:
    case TT_Minus:
    case TT_Bang: {
      Token curr = current_token(parser);
      parser_advance(parser);
      ASTNode* operand = Parser_ParseExpr(parser, Prec_Cast);
      return make_unary_node(parser, operand, curr);
    } break;
    
    case TT_OpenParen: {
      parser_advance(parser);
      ASTNode* ret = Parser_ParseExpr(parser, Prec_None);
      parser_eat(parser, TT_CloseParen);
      return ret;
    } break;
    
    case TT_Amp: {
      Token op = current_token(parser);
      parser_advance(parser);
      ASTNode* left = Parser_ParseExpr(parser, Prec_Cast);
      return make_addr_node(parser, op, left);
    } break;
    
    case TT_Ident: {
      Token str = current_token(parser);
      parser_advance(parser);
      return make_ident_node(parser, str);
    } break;
    
    case TT_I8: case TT_I16: case TT_I32:
    case TT_U8: case TT_U16: case TT_U32: {
      Token curr = current_token(parser);
      parser_advance(parser);
      return make_int_type_node(parser, curr);
    } break;
    
    case TT_F32: case TT_F64: {
      Token curr = current_token(parser);
      parser_advance(parser);
      return make_float_type_node(parser, curr);
    } break;
    
    case TT_Void: {
      Token curr = current_token(parser);
      parser_advance(parser);
      return make_void_type_node(parser, curr);
    } break;
    
    case TT_Caret: {
      Token mark = current_token(parser);
      parser_advance(parser);
      ASTNode* sub_type = Parser_ParsePrefixExpr(parser);
      return make_pointer_type_node(parser, mark, sub_type);
    } break;
    
    case TT_Cast: {
      Token mark = current_token(parser);
      parser_advance(parser);
      parser_eat(parser, TT_OpenParen);
      ASTNode* type = Parser_ParseExpr(parser, Prec_None);
      parser_eat(parser, TT_CloseParen);
      ASTNode* casted = Parser_ParseExpr(parser, Prec_Cast);
      return make_cast_node(parser, mark, casted, type);
    } break;
    
    case TT_OpenBracket: {
      Token mark = current_token(parser);
      parser_advance(parser);
      ASTNode* count = Parser_ParseExpr(parser, Prec_None);
      parser_eat(parser, TT_CloseBracket);
      ASTNode* sub_type = Parser_ParsePrefixExpr(parser);
      return make_array_type_node(parser, mark, count, sub_type);
    } break;
    
    case TT_Struct:
    case TT_Union: {
      Token mark = current_token(parser);
      parser_advance(parser);
      parser_eat(parser, TT_OpenBrace);
      u32 member_count = 0;
      Token_list member_names = {0};
      ASTNode* member_types = nullptr;
      ASTNode* curr = member_types;
      while (!parser_match(parser, TT_CloseBrace)) {
        member_count += 1;
        
        Token ident = parser_eat(parser, TT_Ident);
        parser_eat(parser, TT_Colon);
        Token_list_push(&parser->static_arena, &member_names, ident);
        
        if (!member_types) {
          member_types = Parser_ParseExpr(parser, Prec_None);
          curr = member_types;
        } else {
          curr->next = Parser_ParseExpr(parser, Prec_None);
          curr = curr->next;
        }
        
        parser_eat(parser, TT_Semicolon);
      }
      return make_compound_type_node(parser, mark, decl_name, member_count,
                                     member_names, member_types);
    } break;
    
    case TT_Func: {
      Token mark = current_token(parser);
      parser_advance(parser);
      parser_eat(parser, TT_OpenParen);
      ASTNode* params = nullptr;
      ASTNode* curr = params;
      ASTNode* ret = nullptr;
      Token_list arg_names = {0};
      u32 arity = 0;
      
      while (!parser_match(parser, TT_CloseParen)) {
        arity += 1;
        
        if (next_token_type(parser) == TT_Colon) {
          Token ident = parser_eat(parser, TT_Ident);
          parser_eat(parser, TT_Colon);
          Token_list_push(&parser->static_arena, &arg_names, ident);
        }
        
        if (!params) {
          params = Parser_ParseExpr(parser, Prec_None);
          curr = params;
        } else {
          curr->next = Parser_ParseExpr(parser, Prec_None);
          curr = curr->next;
        }
        
        if (!parser_match(parser, TT_Comma)) {
          parser_eat(parser, TT_CloseParen);
          break;
        }
      }
      if (parser_match(parser, TT_ArrowRight))
        ret = Parser_ParseExpr(parser, Prec_None);
      else
        ret = make_void_type_node(parser, mark);
      
      if (current_token_type(parser) == TT_OpenBrace) {
        if (arity != arg_names.node_count) {
          ParserError(parser, mark, "For Function Expressions, names are required for"
                      " every argument.\nIf you wanted a function type, "
                      "do not add curly braces\n");
          
          return make_error_node(parser, mark);
        }
        
        ASTNode* body = Parser_ParseStmt(parser);
        
        return make_func_node(parser, mark, decl_name, ret, arg_names, params, body, arity);
      }
      
      return make_func_type_node(parser, mark, ret, params, arity);
    } break;
  }
  
  Token mark = current_token(parser);
  parser_advance(parser);
  ParserError(parser, mark, "Expected an expression but got an invalid token\n");
  
  return make_error_node(parser, mark);
}

static ASTNode* Parser_ParseInfixExpr(Parser* parser, ASTNode* left, Token op) {
  switch (op.type) {
    case TT_Plus: case TT_Minus:
    case TT_Star: case TT_Slash: case TT_Percent:
    case TT_EqualEqual: case TT_BangEqual:
    case TT_Less: case TT_Greater:
    case TT_ShiftLeft: case TT_ShiftRight:
    case TT_Amp: case TT_Pipe:
    case TT_LessEqual: case TT_GreaterEqual: {
      ASTNode* right = Parser_ParseExpr(parser, precedence_lookup[op.type]);
      return make_binary_node(parser, left, right, op);
    } break;
    
    case TT_OpenBracket: {
      ASTNode* right = Parser_ParseExpr(parser, Prec_None);
      parser_eat(parser, TT_CloseBracket);
      return make_array_index_node(parser, op, left, right);
    } break;
    
    case TT_Caret: {
      return make_deref_node(parser, op, left);
    } break;
    
    case TT_Dot:
    case TT_ArrowRight: {
      Token name = parser_eat(parser, TT_Ident);
      return make_access_node(parser, op, left, name,
                              op.type == TT_Dot ? false : true);
    } break;
    
    case TT_OpenParen: {
      ASTNode* args = nullptr;
      ASTNode* curr = nullptr;
      u32 arity = 0;
      while (!parser_match(parser, TT_CloseParen)) {
        if (!args) {
          args = Parser_ParseExpr(parser, Prec_None);
          curr = args;
        } else {
          curr->next = Parser_ParseExpr(parser, Prec_None);
          curr = curr->next;
        }
        arity++;
        
        if (!parser_match(parser, TT_Comma)) {
          parser_eat(parser, TT_CloseParen);
          break;
        }
      }
      
      return make_func_call_node(parser, op, left, args, arity);
    } break;
  }
  
  Token mark = current_token(parser);
  parser_advance(parser);
  ParserError(parser, mark, "Expected a valid infix expression operator but got an"
              "invalid token '%s'\n", Debug_TokenType_ToString(op.type));
  
  return make_error_node(parser, mark);
}


//~ Node Parsing

static ASTNode* Parser_ParseExpr_N(Parser* parser, OpPrecedence curr_op_prec, string decl_name) {
  ASTNode* left = Parser_ParsePrefixExpr_N(parser, decl_name);
  Token next_op = current_token(parser);
  OpPrecedence next_op_prec = precedence_lookup[next_op.type];
  
  while (next_op_prec != Prec_None) {
    if (curr_op_prec >= next_op_prec) {
      break;
    } else {
      parser_advance(parser);
      left = Parser_ParseInfixExpr(parser, left, next_op);
      next_op = current_token(parser);
      next_op_prec = precedence_lookup[next_op.type];
    }
  }
  
  return left;
}

static ASTNode* Parser_ParseStmt(Parser* parser) {
  
  if (current_token_type(parser) == TT_OpenBrace) {
    Token mark = current_token(parser);
    parser_advance(parser);
    
    ASTNode* ret = nullptr;
    ASTNode* curr = nullptr;
    while (current_token_type(parser) != TT_CloseBrace) {
      if (!ret) {
        ret = Parser_ParseStmt(parser);
        curr = ret;
      } else {
        curr->next = Parser_ParseStmt(parser);
        curr = curr->next;
      }
    }
    
    parser_eat(parser, TT_CloseBrace);
    return make_block_node(parser, mark, ret);
    
  } else if (current_token_type(parser) == TT_While) {
    Token mark = current_token(parser);
    parser_advance(parser);
    ASTNode* condition = Parser_ParseExpr(parser, Prec_None);
    ASTNode* body = Parser_ParseStmt(parser);
    
    return make_while_node(parser, mark, condition, body);
    
  } else if (current_token_type(parser) == TT_If) {
    Token mark = current_token(parser);
    parser_advance(parser);
    ASTNode* condition = Parser_ParseExpr(parser, Prec_None);
    ASTNode* then_body = Parser_ParseStmt(parser);
    
    ASTNode* else_body = nullptr;
    if (parser_match(parser, TT_Else))
      else_body = Parser_ParseStmt(parser);
    
    return make_if_node(parser, mark, condition, then_body, else_body);
    
  } else if (current_token_type(parser) == TT_Return) {
    Token mark = current_token(parser);
    parser_advance(parser);
    ASTNode* ret = nullptr;
    if (!parser_match(parser, TT_Semicolon)) {
      ret = Parser_ParseExpr(parser, Prec_None);
      parser_eat(parser, TT_Semicolon);
    }
    return make_return_node(parser, mark, ret);
  } else if (current_token_type(parser) == TT_Write) {
    Token mark = current_token(parser);
    parser_advance(parser);
    i32 ptr;
    
    if (current_token_type(parser) == TT_IntLit) {
      Token curr = parser_eat(parser, TT_IntLit);
      ptr = strtoll((const char*) curr.lexeme.str, nullptr, 10);
    } else if (current_token_type(parser) == TT_HexLit) {
      Token curr = parser_eat(parser, TT_HexLit);
      ptr = strtoll((const char*) curr.lexeme.str, nullptr, 16);
    } else {
      ParserError(parser, mark, "Expected an int literal (pointer) after write\n");
      ptr = -1;
    }
    
    parser_eat(parser, TT_Comma);
    ASTNode* value = Parser_ParseExpr(parser, Prec_None);
    
    parser_eat(parser, TT_Semicolon);
    return make_write_stmt_node(parser, mark, ptr, value);
    
  } else if (next_token_type(parser) == TT_Colon) {
    Token ident = parser_eat(parser, TT_Ident);
    parser_eat(parser, TT_Colon);
    
    if (parser_match(parser, TT_Colon)) {
      // Typeless compile-time constant decl
      ASTNode* value = Parser_ParseExpr_N(parser, Prec_None, ident.lexeme);
      parser_eat(parser, TT_Semicolon);
      return make_decl_node(parser, ident, nullptr, value, true);
      
    } else if (parser_match(parser, TT_Equal)) {
      // Typeless variable decl
      ASTNode* value = Parser_ParseExpr_N(parser, Prec_None, ident.lexeme);
      parser_eat(parser, TT_Semicolon);
      return make_decl_node(parser, ident, nullptr, value, false);
      
    } else {
      // Type
      ASTNode* type = Parser_ParseExpr(parser, Prec_None);
      
      if (parser_match(parser, TT_Semicolon)) {
        // Valueless typed decl
        return make_decl_node(parser, ident, type, nullptr, false);
      }
      
      if (parser_match(parser, TT_Colon)) {
        // Typed compile-time constant decl
        ASTNode* value = Parser_ParseExpr_N(parser, Prec_None, ident.lexeme);
        parser_eat(parser, TT_Semicolon);
        return make_decl_node(parser, ident, type, value, true);
        
      } else if (parser_match(parser, TT_Equal)) {
        // Typed variable decl
        ASTNode* value = Parser_ParseExpr_N(parser, Prec_None, ident.lexeme);
        parser_eat(parser, TT_Semicolon);
        return make_decl_node(parser, ident, type, value, false);
      }
    }
    
  } else {
    
    ASTNode* expr = Parser_ParseExpr(parser, Prec_None);
    if (current_token_type(parser) == TT_Equal) {
      Token mark = current_token(parser);
      parser_advance(parser);
      ASTNode* val = Parser_ParseExpr(parser, Prec_None);
      parser_eat(parser, TT_Semicolon);
      return make_assign_node(parser, mark, expr, val);
    }
    parser_eat(parser, TT_Semicolon);
    return make_expr_stmt_node(parser, expr->marker, expr);
    
  }
  
  
  Token mark = current_token(parser);
  parser_advance(parser);
  ParserError(parser, mark, "Expected a statement but got an invalid token '%s'\n",
              Debug_TokenType_ToString(current_token_type(parser)));
  return make_error_node(parser, mark);
}

//~ Parser

void Parser_Init(Parser* parser) {
  MemoryZeroStruct(parser, Parser);
  parser->curr = 0; parser->next = 1;
  arena_init(&parser->static_arena);
  pool_init(&parser->allocator, sizeof(ASTNode));
}

ASTNode* Parser_Parse(Parser* parser, Token_array tokens) {
  parser->tokens = tokens;
  
  ASTNode* ret = nullptr;
  ASTNode* curr = nullptr;
  while (current_token_type(parser) != TT_EOF) {
    if (!ret) {
      ret = Parser_ParseStmt(parser);
      curr = ret;
    } else {
      curr->next = Parser_ParseStmt(parser);
      curr = curr->next;
    }
  }
  
  return ret;
}

void Parser_Free(Parser* parser) {
  pool_free(&parser->allocator);
  arena_free(&parser->static_arena);
}

//~ Debug
static void Debug_Dump_Tree_Indented(ASTNode* node, u32 indent) {
  for (u32 i = 0; i < indent; i++)
    printf("  ");
  
  if (!node) {
    printf("None\n");
    return;
  }
  
  switch (node->type) {
    case NT_Error: printf("Error\n"); break;
    case NT_Expr_IntLit: printf("%lld\n", node->constant_val.int_lit); break;
    case NT_Expr_FloatLit: printf("%f\n", node->constant_val.float_lit); break;
    case NT_Expr_Ident: printf("%.*s\n", str_expand(node->ident.lexeme)); break;
    case NT_Type_Void: printf("Void\n"); break;
    
    case NT_Expr_ArrayLit: {
      printf("Array:\n");
      for (u32 i = 0; i < indent+1; i++)
        printf("  ");
      printf("Type:\n");
      Debug_Dump_Tree_Indented(node->array_lit.type, indent+2);
      for (u32 i = 0; i < indent+1; i++)
        printf("  ");
      printf("Values:\n");
      ASTNode* ce = node->array_lit.values;
      u64 i = 0;
      while (ce) {
        for (u32 i = 0; i < indent+2; i++)
          printf("  ");
        
        printf("[%llu]:\n", i);
        Debug_Dump_Tree_Indented(ce, indent+3);
        ce = ce->next;
        i++;
      }
    } break;
    
    case NT_Expr_Add: {
      printf("+\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_Sub: {
      printf("-\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_Mul: {
      printf("*\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_Div: {
      printf("/\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_Mod: {
      printf("%%\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_ShiftLeft: {
      printf("<<\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_ShiftRight: {
      printf(">>\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_BitAND: {
      printf("&\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_BitOR: {
      printf("|\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    
    case NT_Expr_Identity: {
      printf("+\n");
      Debug_Dump_Tree_Indented(node->unary_op.operand, indent+1);
    } break;
    case NT_Expr_Negate: {
      printf("-\n");
      Debug_Dump_Tree_Indented(node->unary_op.operand, indent+1);
    } break;
    case NT_Expr_Not: {
      printf("!\n");
      Debug_Dump_Tree_Indented(node->unary_op.operand, indent+1);
    } break;
    
    case NT_Expr_Eq: {
      printf("==\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_Neq: {
      printf("!=\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_Less: {
      printf("<\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_Greater: {
      printf(">\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_LessEq: {
      printf("<=\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    case NT_Expr_GreaterEq: {
      printf(">=\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+1);
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+1);
    } break;
    
    case NT_Expr_Deref: {
      printf("Deref:\n");
      Debug_Dump_Tree_Indented(node->deref, indent+1);
    } break;
    case NT_Expr_Addr: {
      printf("Address of:\n");
      Debug_Dump_Tree_Indented(node->addr, indent+1);
    } break;
    case NT_Expr_Cast: {
      printf("Cast:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Left:\n");
      Debug_Dump_Tree_Indented(node->cast.casted, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Type:\n");
      Debug_Dump_Tree_Indented(node->cast.type, indent+2);
    } break;
    
    case NT_Expr_FuncProto: {
      printf("Func Prototype:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Return:\n");
      Debug_Dump_Tree_Indented(node->proto.return_type, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Args:\n");
      ASTNode* curr = node->proto.arg_types;
      Token_node* curr_name = node->proto.arg_names.first;
      u32 i = 1;
      while (curr) {
        for (u32 i = 0; i < (indent+2); i++) printf("  ");
        printf("Arg %d:\n", i);
        for (u32 i = 0; i < (indent+3); i++) printf("  ");
        printf("Name: %.*s\n", str_expand(curr_name->token.lexeme));
        for (u32 i = 0; i < (indent+3); i++) printf("  ");
        printf("Type:\n");
        Debug_Dump_Tree_Indented(curr, indent+4);
        curr = curr->next;
        curr_name = curr_name->next;
        i++;
      }
    } break;
    case NT_Expr_Func: {
      printf("Func Expr:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Proto:\n");
      Debug_Dump_Tree_Indented(node->func.proto, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Body:\n");
      Debug_Dump_Tree_Indented(node->func.body, indent+2);
    } break;
    case NT_Expr_Call: {
      printf("Func Call:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Called Function:\n");
      Debug_Dump_Tree_Indented(node->call.called, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Args:\n");
      ASTNode* curr = node->call.args;
      while (curr) {
        Debug_Dump_Tree_Indented(curr, indent+2);
        curr = curr->next;
      }
    } break;
    case NT_Expr_Index: {
      printf("Array Index:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Indexee:\n");
      Debug_Dump_Tree_Indented(node->index.left, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Idx:\n");
      Debug_Dump_Tree_Indented(node->index.idx, indent+2);
    } break;
    case NT_Expr_Access: {
      printf("Member Access:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Left:\n");
      Debug_Dump_Tree_Indented(node->access.left, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Member: %.*s\n", str_expand(node->access.right.lexeme));
    } break;
    
    case NT_Type_Integer: {
      printf("Integer:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Size: %u\n", node->int_type.size);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Is signed: %s\n", node->int_type.is_signed?"true":"false");
    } break;
    case NT_Type_Float: {
      printf("Float:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Size: %u\n", node->float_type.size);
    } break;
    case NT_Type_Pointer: {
      printf("Pointer to:\n");
      Debug_Dump_Tree_Indented(node->pointer_type.sub, indent+1);
    } break;
    case NT_Type_Array: {
      printf("Array:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Count:\n");
      Debug_Dump_Tree_Indented(node->array_type.count, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Type:\n");
      Debug_Dump_Tree_Indented(node->array_type.sub, indent+2);
    } break;
    case NT_Type_Func: {
      printf("Func Type:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Return:\n");
      Debug_Dump_Tree_Indented(node->func_type.return_type, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Args:\n");
      ASTNode* curr = node->func_type.arg_types;
      while (curr) {
        Debug_Dump_Tree_Indented(curr, indent+2);
        curr = curr->next;
      }
    } break;
    case NT_Type_Struct:
    case NT_Type_Union: {
      printf("%s Type:\n", node->type == NT_Type_Struct ? "Struct" : "Union");
      ASTNode* curr = node->compound_type.member_types;
      while (curr) {
        Debug_Dump_Tree_Indented(curr, indent+1);
        curr = curr->next;
      }
    } break;
    
    case NT_Stmt_If: {
      printf("If:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Condition:\n");
      Debug_Dump_Tree_Indented(node->if_stmt.condition, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Then:\n");
      Debug_Dump_Tree_Indented(node->if_stmt.then_body, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Else:\n");
      Debug_Dump_Tree_Indented(node->if_stmt.else_body, indent+2);
    } break;
    case NT_Stmt_While: {
      printf("While:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Condition:\n");
      Debug_Dump_Tree_Indented(node->while_loop.condition, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Body:\n");
      Debug_Dump_Tree_Indented(node->while_loop.body, indent+2);
    } break;
    case NT_Stmt_Block: {
      printf("Block:\n");
      ASTNode* curr = node->block;
      while (curr) {
        Debug_Dump_Tree_Indented(curr, indent+1);
        curr = curr->next;
      }
    } break;
    case NT_Stmt_Assign: {
      printf("Assign:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Left:\n");
      Debug_Dump_Tree_Indented(node->binary_op.left, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Value:\n");
      Debug_Dump_Tree_Indented(node->binary_op.right, indent+2);
    } break;
    case NT_Stmt_Return: {
      printf("Return:\n");
      Debug_Dump_Tree_Indented(node->return_stmt, indent+2);
    } break;
    case NT_Stmt_Expr: {
      printf("Expression:\n");
      Debug_Dump_Tree_Indented(node->expr_stmt, indent+1);
    } break;
    case NT_Stmt_Write: {
      printf("Write to %d\n", node->write_stmt.ptr);
      Debug_Dump_Tree_Indented(node->write_stmt.value, indent+1);
    } break;
    
    case NT_Decl: {
      printf("Declaration:\n");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Identifier: %.*s\n", str_expand(node->decl.ident.lexeme));
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Is constant: %s\n", node->decl.is_constant?"true":"false");
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Type:\n");
      Debug_Dump_Tree_Indented(node->decl.type, indent+2);
      for (u32 i = 0; i < (indent+1); i++) printf("  ");
      printf("Value:\n");
      Debug_Dump_Tree_Indented(node->decl.val, indent+2);
    } break;
  }
}

void Debug_Dump_ASTree(ASTNode* node) {
  Debug_Dump_Tree_Indented(node, 0);
}

