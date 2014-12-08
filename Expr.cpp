#include "Expr.h"

/*** Debug print functions ***/
static ostream * _out = NULL;

void setOutEnabled(bool enabled)
{
  _out = new ostream((enabled) ? cout.rdbuf() : 0);
}

// usage: "out() << "string" << "\n";" like "std::cout"
ostream& out()
{
  if (!_out)
    _out = new ostream(cout.rdbuf());
  
	return *_out;
}

/*** Wrapper for Token Expression ***/
string TokenExpr::DebugString()
{
  return _tok;
}

/*** Comment Expression ***/
string CommentExpr::DebugString()
{
  return "comment found: \"" + _comment + "\"";
}

/*** Input Expression ***/

int InputExpr::_indexesCount = 0;

string InputExpr::DebugString()
{
  ostringstream ostr;
  ostr << "input " << _index;
  return ostr.str();
}

/*** Variable Expression ***/
string VarExpr::DebugString()
{
  string s = ((_inversed) ? "inversed " : "");
  return s + "var \"" + _name + "\"";
}

/*** Named Variable Expression ***/
string NamedVarExpr::DebugString()
{
  return "named var from " + _expr->DebugString();
}

/*** Initialisation Expression ***/
string InitExpr::DebugString()
{
  return _LHS->DebugString() + " = " + _RHS->DebugString();
}

/*** Binary Operator Expression ***/
string BinOpExpr::DebugString()
{
  return "Binop(" + _LHS->DebugString() + " " + _op.print() + " " + _RHS->DebugString() + ")";
}

/*** Print Expression ***/
string PrintExpr::DebugString()
{
  ostringstream ostr;
  ostr << "print: \"";
  for (vector<Expr *>::iterator it = output.begin(); it != output.end(); it++) {
    ostr << (*it)->DebugString();
    if (*it != output.back()) // Add "," between each expression (and not for the last item)
      ostr << ", ";
  }
  ostr << "\"";
  return ostr.str();
}

/*** Hello Print Expression (print "Hello, {:$}!" if has an input, "Hello, World!" else) ***/
string HelloPrintExpr::DebugString()
{
  return "Hello, World!";
}

/*** No Operation Expression ***/
string NopExpr::DebugString()
{
  return "nop;";
}

/*** Exit (terminate) Expression ***/
string ExitExpr::DebugString()
{
  ostringstream ostr;
  ostr << "exit(" << code << ")";
  return ostr.str();
}

/*** Push (to global stack) Expression ***/
string PushExpr::DebugString()
{
  return "push: " + _expr->DebugString();
}

/*** Pop (from global stack) Expression ***/
string PopExpr::DebugString()
{
  return "pop to: " + _expr->DebugString();
}

/*** Clear Global Stack Expression ***/
string ClearExpr::DebugString()
{
  return "clear stack";
}

/*** Loop Expression ***/
string LoopExpr::DebugString()
{
  ostringstream ostr;
  ostr << "loop: | " << _conditionExpr->DebugString();
  ostr << " | then | ";
  
  vector<Expr *>::iterator it;
  for (it = _thenExprs.begin(); it != _thenExprs.end(); it++) {
    ostr << (*it)->DebugString();
    if (*it != _thenExprs.back()) // Add "," between each expression
      ostr << ", ";
  }
  
  ostr << " | else | ";
  for (it = _thelseExprs.begin(); it != _thelseExprs.end(); it++) {
    ostr << (*it)->DebugString();
    if (*it != _thelseExprs.back()) // Add "," between each expression
      ostr << ", ";
  }
  ostr << " |";
  
  return ostr.str();
}

/*** Unkown Expression ***/
string LengthFuncExpr::DebugString()
{
  return "get length of: " + _expr->DebugString();
}

/*** Unkown Expression ***/
string UnkownExpr::DebugString()
{
  return "unkown expression" + ((_tk) ? (": \"" + _tk + "\"") : "");
}