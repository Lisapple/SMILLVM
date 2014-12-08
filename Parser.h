#ifndef SMIL_PARSER_H
#define SMIL_PARSER_H

#include <string>
#include <vector>
#include <list>

#include "llvm/Support/Casting.h"

#include "Token.h"
#include "Expr.h"
#include "CodeGen.h"
#include "Utilities.h"

class Parser {
protected:
  vector<Expr *> exprs;
  const char * input_str;
  int input_str_index;
  int input_str_length;
  
  int line, col;
  
  Expr *_lastExpr;
  
  static inline bool isskipable(char &c);
  
  Token gettok();
  Token nexttok(bool skipSkipeableCharacters = true);
  
  Expr *        parseExpr();
  Expr *        parseVar(int inversed = false);
  InputExpr *   parseInput();
  CommentExpr * parseComment();
  InitExpr *    parseInit();
  Expr *        parseOperand();
  BinOpExpr *   parseBinaryOperation(Expr *LHS, Token &op);
  PrintExpr *   parsePrint();
  HelloPrintExpr * parseHelloPrint();
  NopExpr *     parseNop();
  ExitExpr *    parseExit();
  PushExpr *    parsePush();
  PopExpr *     parsePop();
  ClearExpr *   parseClear();
  LoopExpr *    parseLoop();
  LengthFuncExpr * parseLengthFunction();
  
public:
  vector<Expr *> &getExprs() { return exprs; }
  Parser(string &s);
};

#endif // SMIL_PARSER_H