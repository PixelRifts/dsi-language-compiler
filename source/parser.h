/* date = November 27th 2023 8:30 am */

#ifndef PARSER_H
#define PARSER_H

#include "defines.h"
#include "base/str.h"
#include "base/mem.h"
#include "base/ds.h"
#include "lexer.h"

//~ The AST

typedef struct ASTNode ASTNode;
typedef ASTNode* ASTNodeRef;
DArray_Prototype(ASTNodeRef);
DArray_Prototype(ASTNodeRef_array);

typedef u64 TypeIndex;

typedef enum NodeType {
  NT_Error,
  
  // Expressions
  NT_Expr_IntLit, NT_Expr_FloatLit,
  NT_Expr_Add, NT_Expr_Sub, NT_Expr_Mul, NT_Expr_Div,
  NT_Expr_Mod, NT_Expr_Identity, NT_Expr_Negate, NT_Expr_Not,
  NT_Expr_Eq,  NT_Expr_Neq, NT_Expr_Less, NT_Expr_Greater,
  NT_Expr_LessEq, NT_Expr_GreaterEq, NT_Expr_FuncProto, NT_Expr_Func,
  NT_Expr_ShiftLeft, NT_Expr_ShiftRight, NT_Expr_BitAND, NT_Expr_BitOR,
  NT_Expr_Index, NT_Expr_Addr, NT_Expr_Deref, NT_Expr_Call,
  NT_Expr_Ident, NT_Expr_Cast, NT_Expr_Access, NT_Expr_ArrayLit,
  
  // Types
  NT_Type_Integer, NT_Type_Float, NT_Type_Void,
  NT_Type_Func, NT_Type_Struct, NT_Type_Union,
  NT_Type_Pointer, NT_Type_Array,
  
  // Statements
  NT_Stmt_Assign, NT_Stmt_Expr, NT_Stmt_Block,
  NT_Stmt_While, NT_Stmt_If, NT_Stmt_Return, NT_Stmt_Write,
  
  // Declaration
  NT_Decl,
} NodeType;

typedef enum NodeStatus {
  Status_Ready         = 0x1,
  Status_ProtoReady    = 0x2,
  Status_Resolved      = 0x4,
  Status_ProtoResolved = 0x8,
  Status_DepsBuilt     = 0x10,
  Status_Waiting       = 0x20,
  Status_Tried         = 0x40,
} NodeStatus;

typedef u8 OpPrecedence;
enum {
  Prec_None,
  Prec_BitOR,   // |
  Prec_BitAND,  // &
  Prec_Eq,      // ==  !=
  Prec_Cmp,     // <   >   <=   >=
  Prec_Shift,   // <<  >>
  Prec_Term,    // +   -
  Prec_Factor,  // /   *   %
  Prec_Cast,    // cast(x)
  Prec_Call,    // ()  ^(deref)
  Prec_MAX,
};

typedef struct BinaryOpNode {
  ASTNode* left;
  ASTNode* right;
} BinaryOpNode;

typedef struct UnaryOpNode {
  ASTNode* operand;
} UnaryOpNode;

typedef struct ArrayIndexNode {
  ASTNode* left;
  ASTNode* idx;
} ArrayIndexNode;

typedef struct FuncCallNode {
  ASTNode* called;
  ASTNode* args;
  u32 arity;
} FuncCallNode;

typedef struct FuncProtoNode {
  string name;
  ASTNode* return_type;
  ASTNode* arg_types;
  Token_list arg_names;
  u32 arity;
} FuncProtoNode;

typedef struct FuncNode {
  ASTNode* proto;
  ASTNode* body;
  u64 total_local_size;
  u64 tracking_local_size;
} FuncNode;

typedef struct CastNode {
  ASTNode* casted;
  ASTNode* type;
} CastNode;

typedef struct AccessNode {
  ASTNode* left;
  Token right;
  b8 deref;
} AccessNode;

typedef struct ArrayLitNode {
  ASTNode* type;
  ASTNode* values;
  u32 count;
} ArrayLitNode;

typedef struct IntegerTypeNode {
  u32 size;
  b8 is_signed;
} IntegerTypeNode;

typedef struct FloatTypeNode {
  u32 size;
} FloatTypeNode;

typedef struct PointerTypeNode {
  ASTNode* sub;
} PointerTypeNode;

typedef struct ArrayTypeNode {
  ASTNode* count;
  ASTNode* sub;
} ArrayTypeNode;

typedef struct FuncTypeNode {
  ASTNode* return_type;
  ASTNode* arg_types;
  u32      arity;
} FuncTypeNode;

typedef struct CompoundTypeNode {
  string     name;
  u64        member_count;
  Token_list member_names;
  ASTNode*   member_types;
} CompoundTypeNode;

typedef struct WhileLoopNode {
  ASTNode* condition;
  ASTNode* body;
} WhileLoopNode;

typedef struct IfStmtNode {
  ASTNode* condition;
  ASTNode* then_body;
  ASTNode* else_body;
} IfStmtNode;

typedef struct WriteStmtNode {
  i32 ptr;
  ASTNode* value;
} WriteStmtNode;

typedef struct DeclNode {
  Token ident;
  ASTNode* type;
  ASTNode* val;
  
  i32 parent;
  u16 color;
  
  b8 is_constant;
  u64 offset;
} DeclNode;


typedef enum ConstantValueType {
  CVT_None,
  CVT_Int,
  CVT_Float,
  CVT_Type,
  CVT_Buffer,
} ConstantValueType;

typedef struct ConstantValue {
  ConstantValueType type;
  
  union {
    i64 int_lit;
    f64 float_lit;
    TypeIndex type_lit;
    struct { u8* buf; u64 size; } buf_lit;
  };
} ConstantValue;

struct ASTNode {
  // Basic Things
  NodeType type;
  ASTNode* next;
  Token marker;
  TypeIndex expr_type;
  NodeStatus status;
  
  // Constant Values
  b8 is_constant;
  ConstantValue constant_val;
  
  // Subtypes
  union {
    Token ident;
    ArrayLitNode array_lit;
    BinaryOpNode binary_op;
    UnaryOpNode  unary_op;
    FuncProtoNode proto;
    FuncNode func;
    ArrayIndexNode index;
    FuncCallNode call;
    ASTNode* addr;
    ASTNode* deref;
    CastNode cast;
    AccessNode access;
    
    IntegerTypeNode int_type;
    FloatTypeNode float_type;
    FuncTypeNode func_type;
    CompoundTypeNode compound_type;
    ArrayTypeNode array_type;
    PointerTypeNode pointer_type;
    
    ASTNode* expr_stmt;
    ASTNode* return_stmt;
    WriteStmtNode write_stmt;
    
    struct {
      union {
        WhileLoopNode while_loop;
        IfStmtNode if_stmt;
        ASTNode* block;
        DeclNode decl;
      };
      
      u64 local_size;
    };
  };
};

Queue_Prototype(ASTNodeRef);
Stack_Prototype(ASTNodeRef);

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
