#include <stdio.h>

#include "os/os.h"

#include "lexer.h"
#include "parser.h"
#include "check.h"
#include "dsi_back.h"

extern M_Arena* global_arena_t;

static string read_entire_file(M_Arena* arena, const char* path) {
  FILE* file = fopen(path, "rb");
  
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);
  
  char* buffer = (char*) arena_alloc(arena, fileSize + 1);
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';
  fclose(file);
  
  return (string) { .str = (u8*) buffer, .size = fileSize };
}

int main(int arc, char** argv) {
  OS_Init();
  ThreadContext tctx = {0};
  tctx_init(&tctx);
  
  M_Arena arena = {0};
  arena_init(&arena);
  
  string fp = str_make(argv[1]);
  string code = read_entire_file(&arena, argv[1]);
  global_arena_t = &arena;
  
  Lexer lexer = {0};
  Lexer_Init(&lexer);
  Token_array tokens = Lexer_Lex(&lexer, code);
  
  Parser parser = {0};
  Parser_Init(&parser);
  parser.filename = fp;
  ASTNode* tree = Parser_Parse(&parser, tokens);
  
  if (parser.errored) {
    Parser_Free(&parser);
    arena_free(&arena);
    tctx_free(&tctx);
    return 1;
  }
  
  ASTNode* curr = tree;
  while (curr) {
    Debug_Dump_ASTree(curr);
    curr = curr->next;
  }
  
  Checker checker = {0};
  Checker_Init(&checker);
  checker.filename = fp;
  Checker_Check(&checker, tree);
  
  if (checker.errored) {
    Checker_Free(&checker);
    Parser_Free(&parser);
    arena_free(&arena);
    tctx_free(&tctx);
    return 1;
  }
  
  DSIBackend backend = {0};
  backend.filename = fp;
  DSIBack_Init(&backend, &checker.type_cache);
  DSIBack_Write(&backend, tree);
  string outcode = string_list_flatten(&arena, &backend.instructions);
  OS_FileCreateWrite(str_lit("out.asm"), outcode);
  
  if (backend.errored) {
    DSIBack_Free(&backend);
    Checker_Free(&checker);
    Parser_Free(&parser);
    arena_free(&arena);
    tctx_free(&tctx);
    return 1;
  }
  
  
  DSIBack_Free(&backend);
  Checker_Free(&checker);
  Parser_Free(&parser);
  
  arena_free(&arena);
  tctx_free(&tctx);
  return 0;
}