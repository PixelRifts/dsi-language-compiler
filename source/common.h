/* date = February 18th 2026 4:18 pm */

#ifndef COMMON_H
#define COMMON_H

#include "defines.h"
#include "base/str.h"
#include "base/mem.h"
#include "base/ds.h"

//~ Fwd Decls for cyclic pointers

typedef struct ASTNode ASTNode;
typedef u64 TypeIndex;

//~ Tokens

typedef u32 TokenType;
enum TokenType {
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

typedef struct Token Token;
struct Token {
  TokenType type;
  string lexeme;
  u32 line, col;
};

DArray_Prototype(Token);

typedef struct Token_node Token_node;
struct Token_node {
  Token token;
  Token_node* next;
};

typedef struct Token_list Token_list;
struct Token_list {
  Token_node* first;
  Token_node* last;
  i32 node_count;
};

void  Token_list_push_node(Token_list* list, Token_node* node);
void  Token_list_push(M_Arena* arena, Token_list* list, Token tok);


//~ Constants

typedef u32 ConstantValueType;
enum ConstantValueType {
  CVT_None,
  CVT_Int,
  CVT_Float,
  CVT_Type,
  CVT_Buffer,
};

typedef struct ConstantValue ConstantValue;
struct ConstantValue {
  ConstantValueType type;
  
  union {
    i64 int_lit;
    f64 float_lit;
    TypeIndex type_lit;
    struct { u8* buf; u64 size; } buf_lit;
  };
};


//~ Types

// Some Default Type Indices
enum DefaultTypeIndex {
  Type_Index_None,
  
  Type_Index_I8,
  Type_Index_I16,
  Type_Index_I32,
  Type_Index_U8,
  Type_Index_U16,
  Type_Index_U32,
  
  Type_Index_F32,
  Type_Index_F64,
  Type_Index_Type,
  Type_Index_Void,
  Type_Index_Bool,
  
  Type_Index_Count,
};


typedef struct ValueType ValueType;

typedef u32 TypeKind;
enum TypeKind {
  TK_None,
  TK_Int,
  TK_Float,
  TK_Func,
  TK_Type,
  TK_Void,
  TK_Bool,
  TK_Pointer,
  TK_Array,
  TK_Struct,
  TK_Union,
  
  TK_MAX,
};

typedef struct TypeInt TypeInt;
struct TypeInt {
  u32 size;
  b8 is_signed;
};

typedef struct TypeFloat TypeFloat;
struct TypeFloat {
  u32 size;
};

typedef struct TypeFunc TypeFunc;
struct TypeFunc {
  TypeIndex ret_t;
  u32 arity;
  TypeIndex* arg_ts;
};

typedef struct TypePointer TypePointer;
struct TypePointer {
  TypeIndex sub_t;
};

typedef struct TypeArray TypeArray;
struct TypeArray {
  TypeIndex sub_t;
  u64 count;
};

typedef struct TypeCompound TypeCompound;
struct TypeCompound {
  string name;
  TypeIndex* member_ts;
  Token_list member_names;
  u64* member_offsets;
  u64 count;
};

struct ValueType {
  TypeKind type;
  u64 size;
  
  union {
    TypeInt int_t;
    TypeFloat float_t;
    TypeFunc func_t;
    TypePointer ptr_t;
    TypeArray array_t;
    TypeCompound compound_t;
  };
};

typedef ValueType* ValueTypeRef;
DArray_Prototype(ValueTypeRef);

typedef struct Symbol Symbol;
struct Symbol {
  Token ident;
  ASTNode* node;
  TypeIndex type;
  
  b8 is_constant;
  ConstantValue constant_val;
  
  u32 scope;
  u64 offset;
};

DArray_Prototype(Symbol);



//~ ASTNodes

typedef enum NodeStatus NodeStatus;
enum NodeStatus {
  Status_Ready         = 0x1,
  Status_ProtoReady    = 0x2,
  Status_Resolved      = 0x4,
  Status_ProtoResolved = 0x8,
  Status_DepsBuilt     = 0x10,
  Status_Waiting       = 0x20,
  Status_Tried         = 0x40,
};


typedef enum NodeType NodeType;
enum NodeType {
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
};


typedef enum OpPrecedence OpPrecedence;
enum OpPrecedence {
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

typedef struct BinaryOpNode BinaryOpNode;
struct BinaryOpNode {
  ASTNode* left;
  ASTNode* right;
};

typedef struct UnaryOpNode UnaryOpNode;
struct UnaryOpNode {
  ASTNode* operand;
};

typedef struct ArrayIndexNode ArrayIndexNode;
struct ArrayIndexNode {
  ASTNode* left;
  ASTNode* idx;
};

typedef struct FuncCallNode FuncCallNode;
struct FuncCallNode {
  ASTNode* called;
  ASTNode* args;
  u32 arity;
};

typedef struct FuncProtoNode FuncProtoNode;
struct FuncProtoNode {
  string name;
  ASTNode* return_type;
  ASTNode* arg_types;
  Token_list arg_names;
  u32 arity;
};

typedef struct FuncNode FuncNode;
struct FuncNode {
  ASTNode* proto;
  ASTNode* body;
  u64 total_local_size;
  u64 tracking_local_size;
};

typedef struct CastNode CastNode;
struct CastNode {
  ASTNode* casted;
  ASTNode* type;
};

typedef struct AccessNode AccessNode;
struct AccessNode {
  ASTNode* left;
  Token right;
  b8 deref;
};

typedef struct ArrayLitNode ArrayLitNode;
struct ArrayLitNode {
  ASTNode* type;
  ASTNode* values;
  u32 count;
};

typedef struct IntegerTypeNode IntegerTypeNode;
struct IntegerTypeNode {
  u32 size;
  b8 is_signed;
};

typedef struct FloatTypeNode FloatTypeNode;
struct FloatTypeNode {
  u32 size;
};

typedef struct PointerTypeNode PointerTypeNode;
struct PointerTypeNode {
  ASTNode* sub;
};

typedef struct ArrayTypeNode ArrayTypeNode;
struct ArrayTypeNode {
  ASTNode* count;
  ASTNode* sub;
};

typedef struct FuncTypeNode FuncTypeNode;
struct FuncTypeNode {
  ASTNode* return_type;
  ASTNode* arg_types;
  u32      arity;
};

typedef struct CompoundTypeNode CompoundTypeNode;
struct CompoundTypeNode {
  string     name;
  u64        member_count;
  Token_list member_names;
  ASTNode*   member_types;
};

typedef struct WhileLoopNode WhileLoopNode;
struct WhileLoopNode {
  ASTNode* condition;
  ASTNode* body;
};

typedef struct IfStmtNode IfStmtNode;
struct IfStmtNode {
  ASTNode* condition;
  ASTNode* then_body;
  ASTNode* else_body;
};

typedef struct WriteStmtNode WriteStmtNode;
struct WriteStmtNode {
  i32 ptr;
  ASTNode* value;
};

typedef struct DeclNode DeclNode;
struct DeclNode {
  Token ident;
  ASTNode* type;
  ASTNode* val;
  
  i32 parent;
  u16 color;
  
  b8 is_constant;
  u64 offset;
};



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

typedef ASTNode* ASTNodeRef;

Queue_Prototype(ASTNodeRef);
Stack_Prototype(ASTNodeRef);
DArray_Prototype(ASTNodeRef);
DArray_Prototype(ASTNodeRef_array);

#endif //COMMON_H
