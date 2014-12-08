#ifndef SMIL_EXPR_H
#define SMIL_EXPR_H

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
 
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "Token.h"

using namespace std;
using namespace llvm;

void setOutEnabled(bool enabled);
ostream& out();

/* Macro to conforms Expr subclasses to custom LLVM's RTTI
 * http://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
 */
typedef enum {
  ExprKindToken,
  ExprKindComment,
  ExprKindInput,
  ExprKindAssignable,
  ExprKindVar,
  ExprKindNamedVar,
  ExprKindInit,
  ExprKindBinOp,
  ExprKindPrint,
  ExprKindHelloPrint,
  ExprKindNop,
  ExprKindExit,
  ExprKindPush,
  ExprKindPop,
  ExprKindClear,
  ExprKindLoop,
  ExprKindLengthFunc,
  ExprKindUnkown
} ExprKind;

#define REGISTER_CLASSNAME(CLASSNAME) \
/**/ inline ExprKind ClassName() const { \
/*   */ return CLASSNAME; } \
/**/ static inline bool classof(const Expr *E) { \
/*   */ return (E->ClassName() == CLASSNAME); }

#define MAX(A, B) ( { __typeof__(A) _A = (A); \
/*                 */ __typeof__(B) _B = (B); \
/*                 */ _A > _B ? _A : _B; })

/*** Expression ***/
class Expr {
protected:
  int _line, _col;
  Expr(int line, int col) : _line(line), _col(col) { }
public:
  int line() { return _line; }
  int col() { return _col; }
  
  virtual ExprKind ClassName() const = 0;
  static inline bool classof(const Expr *E) { return false; }
  
  virtual Value * CodeGen(Module *M, IRBuilder<> &B) = 0;
  
  void Debug() { out() << DebugString() << endl; }
  virtual string DebugString() = 0;
};

/*** Wrapper for Token Expression ***/
class TokenExpr : public Expr {
protected:
  Token _tok;
public:
  TokenExpr(Token tok, int line = -1, int col = -1) : Expr(line, col), _tok(tok) {}
  Token token() { return _tok; }
  
  REGISTER_CLASSNAME(ExprKindToken)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Comment Expression ***/
class CommentExpr : public Expr {
protected:
  string _comment;
public:
  CommentExpr(string &comment, int line = -1, int col = -1)
  : Expr(line, col), _comment(comment) { } // @TODO: Trim |_comment|
  
  REGISTER_CLASSNAME(ExprKindComment)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Input Expression ***/
class InputExpr : public Expr {
protected:
  static int _indexesCount;
  int _index;
public:
  static int getIndexesCount() { return InputExpr::_indexesCount; }
  
  InputExpr(int index, int line = -1, int col = -1)
  : Expr(line, col), _index(index)
  { InputExpr::_indexesCount = MAX(InputExpr::_indexesCount, index + 1); }
  
  REGISTER_CLASSNAME(ExprKindInput)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Base Class for Variables ***/
class AssignableExpr : public Expr {
protected:
  AssignableExpr(int line, int col) : Expr(line, col) { }
  
public:
  inline ExprKind ClassName() const { return ExprKindAssignable; }
  static inline bool classof(const Expr *E) {
    // See http://llvm.org/docs/HowToSetUpLLVMStyleRTTI.html#concrete-bases-and-deeper-hierarchies
    return (E->ClassName() == ExprKindVar ||
            E->ClassName() == ExprKindNamedVar);
  }
  
  virtual string getName() const = 0;
  virtual bool getInversed() const  = 0;
};

/*** Variable Expression ***/
class VarExpr : public AssignableExpr {
protected:
  string _name;
  bool _inversed;
public:
  VarExpr(string &name, bool inversed = false, int line = -1, int col = -1)
  : AssignableExpr(line, col), _name(name), _inversed(inversed) { }
  
  REGISTER_CLASSNAME(ExprKindVar)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string getName() const { return _name; }
  bool getInversed() const { return _inversed; }
  
  string DebugString();
};

/*** Named Variable Expression ***/
class NamedVarExpr : public AssignableExpr {
protected:
  Expr *_expr; // VarExpr, NamedVarExpr or Input (or Binop)
public:
  NamedVarExpr(Expr *expr, int line = -1, int col = -1)
  : AssignableExpr(line, col), _expr(expr) { }
  
  REGISTER_CLASSNAME(ExprKindNamedVar)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string getName() const { return NULL; }
  bool getInversed() const { return false; }
  
  string DebugString();
};

/*** Initialisation Expression ***/
class InitExpr : public Expr {
protected:
  Expr *_LHS, *_RHS;
public:
  InitExpr(Expr *LHS, Expr *RHS, int line = -1, int col = -1)
  : Expr(line, col), _LHS(LHS), _RHS(RHS) { }
  
  REGISTER_CLASSNAME(ExprKindInit)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Binary Operator Expression ***/
class BinOpExpr : public Expr {
protected:
  Expr *_LHS, *_RHS;
  Token _op;
public:
  BinOpExpr(Expr *LHS, Token &op, Expr *RHS, int line = -1, int col = -1)
  : Expr(line, col), _LHS(LHS), _RHS(RHS), _op(op) { }
  
  REGISTER_CLASSNAME(ExprKindBinOp)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Print Expression ***/
class PrintExpr : public Expr {
protected:
  vector<Expr *> output;
public:
  PrintExpr(vector<Expr *> &_output, int line = -1, int col = -1)
  : Expr(line, col), output(_output) { }
  
  REGISTER_CLASSNAME(ExprKindPrint)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Hello Print Expression ***/
/* print "Hello, {:$}!" if has an input, "Hello, World!" else */
class HelloPrintExpr : public Expr {
public:
  HelloPrintExpr(int line = -1, int col = -1) : Expr(line, col) { }
  
  REGISTER_CLASSNAME(ExprKindHelloPrint)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** No Operation Expression ***/
class NopExpr : public Expr {
public:
  NopExpr(int line = -1, int col = -1) : Expr(line, col) { }
  
  REGISTER_CLASSNAME(ExprKindNop)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Exit (terminate) Expression ***/
class ExitExpr : public Expr {
protected:
  int code;
public:
  ExitExpr(int _code = 0, int line = -1, int col = -1)
  : Expr(line, col), code(_code) { }
  
  REGISTER_CLASSNAME(ExprKindExit)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Push (to global stack) Expression ***/
class PushExpr : public Expr {
protected:
  Expr *_expr;
public:
  PushExpr(Expr *expr, int line = -1, int col = -1)
  : Expr(line, col), _expr(expr) { }
  
  REGISTER_CLASSNAME(ExprKindPush)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Pop (from global stack) Expression ***/
class PopExpr : public Expr {
protected:
  Expr *_expr;
public:
  PopExpr(Expr *expr, int line = -1, int col = -1)
  : Expr(line, col), _expr(expr) { }
  
  REGISTER_CLASSNAME(ExprKindPop)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Clear Global Stack Expression ***/
class ClearExpr : public Expr {
public:
  ClearExpr(int line = -1, int col = -1) : Expr(line, col) { }
  
  REGISTER_CLASSNAME(ExprKindClear)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Loop Expression ***/
class LoopExpr : public Expr {
protected:
  Expr *_conditionExpr;
  vector<Expr *> _thenExprs, _thelseExprs;
public:
  LoopExpr(Expr *conditionExpr, vector<Expr *> &thenExprs, vector<Expr *> &thelseExprs,
           int line = -1, int col = -1)
  : Expr(line, col), _conditionExpr(conditionExpr),
    _thenExprs(thenExprs), _thelseExprs(thelseExprs) { }
  
  REGISTER_CLASSNAME(ExprKindLoop)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Unkown Expression ***/
class LengthFuncExpr : public Expr {
protected:
  Expr *_expr;
public:
  LengthFuncExpr(Expr *expr, int line = -1, int col = -1)
  : Expr(line, col), _expr(expr) { }
  
  REGISTER_CLASSNAME(ExprKindLengthFunc)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

/*** Unkown Expression ***/
class UnkownExpr : public Expr {
protected:
  Token _tk;
public:
  UnkownExpr(int line = -1, int col = -1) : Expr(line, col) { }
  UnkownExpr(Token &tk, int line = -1, int col = -1)
  : Expr(line, col), _tk(tk) { }
  
  REGISTER_CLASSNAME(ExprKindUnkown)
  
  Value * CodeGen(Module *M, IRBuilder<> &B);
  
  string DebugString();
};

#endif // SMIL_EXPR_H