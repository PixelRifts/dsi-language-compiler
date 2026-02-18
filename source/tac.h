/* date = January 22nd 2025 11:07 am */

#ifndef TAC_H
#define TAC_H

#include "defines.h"
#include "common.h"

typedef enum TacOperandType {
  TacOperand_Constant,
  TacOperand_Temporary,
  TacOperand_Symbol,
  TacOperand_Count,
} TacOperandType;

typedef struct TacOperand TacOperand;
struct TacOperand {
  TacOperandType type;
  union {
    ConstantValue constant;
    int temp_id;
    Symbol* symbol;
  };
};

typedef u32 TacOp;
enum TacOp {
  TacOp_Add,
  TacOp_Mul,
  TacOp_Sub,
  TacOp_Div,
  TacOp_Mod,
  TacOp_Negate,
  
  TacOp_Not,
  TacOp_Eq,
  TacOp_Neq,
  TacOp_Less,
  TacOp_LessEq,
  //TacOp_Greater,   // Less   but swapped ops
  //TacOp_GreaterEq, // LessEq but swapped ops
  
  TacOp_ShiftLeft,
  TacOp_ShiftRight,
  TacOp_BitAND,
  TacOp_BitOR,
  
  //TacOp_Index, // This is just add and deref
  TacOp_Copy,
  TacOp_Addr,
  TacOp_Load,
  TacOp_Store,
  
  TacOp_Ifz,
  TacOp_Goto,
  TacOp_Return,
  
  TacOp_Param,
  TacOp_Call,
};

typedef struct TacInst TacInst;
struct TacInst {
  TacOp op;
  TacOperand result;
  TacOperand a;
  TacOperand b;
  
  TacInst* next;
  TacInst* prev;
};

typedef struct TacBasicBlock TacBasicBlock;
struct TacBasicBlock {
  int id;
  
  TacInst* first;
  TacInst* last;
  
  TacBasicBlock** pred;
  int pred_count;
  
  TacBasicBlock* succ[2];
};

typedef struct TacFunction TacFunction;
struct TacFunction {
  ASTNode* symbol;
  TacBasicBlock* entry;
  
  TacBasicBlock* current;
  
  TacFunction* next;
  TacFunction* prev;
};

typedef struct TacProgram TacProgram;
struct TacProgram {
  M_Arena misc_allocator;
  M_Pool  function_allocator;
  M_Pool  bb_allocator;
  M_Pool  inst_allocator;
  
  TacFunction* functions_first;
  TacFunction* functions_last;
  
  string filename;
  b8 errored;
};

void Tac_Init(TacProgram* p);
void Tac_Write(TacProgram* p, ASTNode* tree);
void Tac_Free(TacProgram* p);


#endif //TAC_H
