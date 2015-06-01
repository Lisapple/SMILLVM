#include "Parser.h"

inline bool Parser::isskipable(char &c)
{
  /* Skip whitespaces, tabs and line breaks */
  return (c == ' ' || c == '\n' || c == '\t');
}

Token Parser::gettok()
{
  char c;
  while ( (c = input_str[input_str_index]) && isskipable(c) ) {
    input_str_index++;
    
    if (c == '\n') { // On new lines...
      line++; // ...increment the line number
      col = 0; // ...reset the column number
    } else {
      col++;
    }
  }
  
  col += 2;
  
  if (input_str_index < input_str_length) {
    Token tok(input_str[input_str_index], input_str[input_str_index+1]);
    input_str_index += 2;
    return tok;
  }
  return tok_unkown;
}

Token Parser::nexttok(bool skipSkipeableCharacters)
{
  int tmp_int = input_str_index;
  char c;
  while ( (c = input_str[tmp_int])
         && (skipSkipeableCharacters && isskipable(c)) ) { tmp_int++; }
  
  if (input_str_index < input_str_length) {
    Token tok(input_str[tmp_int], input_str[tmp_int+1]);
    return tok;
  }
  return tok_unkown;
}

Expr /* VarExpr or NamedVarExpr */ * Parser::parseVar(int inversed)
{
  string name;
  Token tok;
  while ( (tok = nexttok()) ) {
    
    if (tok == tok_var_end) { // End of the variable
      gettok(); // Eat var end token
      break;
      
    } else if (tok == tok_var_start && name.length() == 0) { // Named variable, like `:( :( :$ :) :)'
      gettok(); // Eat var begin token
      Expr *expr = parseVar();
      Token tok = gettok(); // Eat the |tok_var_end| after the variable
      // @TODO: Check that |tok| is |tok_var_end|
      return new NamedVarExpr(expr, line, col);
      
    } else { // Appends symbol to |name|
      name += input_str[input_str_index++];
    }
  }
  
  return new VarExpr(name, inversed, line, col);
}

InputExpr * Parser::parseInput()
{
  int index = 0; // First input by default (index = 0), (because the first ':$' has already been found to call this method)
  while (nexttok(false /* Don't skip skipable characters */) == tok_input) {
    index++;
    gettok(); // Eat the token
  }
  
  return new InputExpr(index, line, col);
}

CommentExpr * Parser::parseComment()
{
  string str;
  char c;
  while ( (c = input_str[input_str_index++]) && c != '\n' ) { str += c; }
  
  return new CommentExpr(str, line++, col);
}

InitExpr * Parser::parseInit()
{
  Expr *LHS = _lastExpr;
  if (!isa<AssignableExpr>(LHS)) {
    /* Show an error if |LHS| is not a variable */
    Assert("parseInit: LHS is not assignable (" + LHS->DebugString() + ")", LHS->line(), LHS->col());
  }
  
  Expr *RHS = parseExpr();
  return new InitExpr(LHS, RHS, line, col);
}

/* Used in |parseBinaryOperation()| function to get next expression from a while loop */
Expr * Parser::parseOperand()
{
  Token tok = gettok();
  Expr *expr = NULL;
  if /**/ (tok == tok_var_start || tok == tok_var_not_start) // Parse variable
    expr = parseVar(tok == tok_var_not_start);
  else if (tok == tok_input) // Parse input
    expr = parseInput();
  else if (tok == tok_str_length) // Length function
    expr = parseLengthFunction();
  
  return expr;
}

BinOpExpr * Parser::parseBinaryOperation(Expr *LHS, Token &op)
{
  /* This uses the Shunting Yard Algorithm: http://en.wikipedia.org/wiki/Shunting_yard_algorithm */
  
  list<Token> ops;
  list<Expr *> outputRPN;
  
  /* Build the RPN representation of tokens */
  
  outputRPN.push_back(LHS);
  ops.push_back(op);
  
  Expr *RHS;
  while ( (RHS = parseOperand()) && (op = nexttok()) && op.isOperator() ) { // Until we have comming operations
    
    gettok(); // Eat the operator
    
    outputRPN.push_back(RHS);
    
    Token &lastOp = ops.back();
    
    /* If the precedence of the last operator of the stack if lower (or equal) than the operator to push... */
    if (lastOp.getPrecedence() >= op.getPrecedence()) {
      
      /* ...pop all operator until a lower (or equal) precedence is found */
      list<Token> ops_copy(ops.begin(), ops.end()); // Copy |ops| to keep sync even after a pop
      for (list<Token>::reverse_iterator it = ops_copy.rbegin(); it != ops_copy.rend(); it++) {
        
        Token &lastOp = (*it);
        if (op.getPrecedence() > lastOp.getPrecedence()) break;
        
        Expr *tkExpr = new TokenExpr(*it);
        outputRPN.push_back(tkExpr);
        ops.pop_back();
      }
    }
    ops.push_back(op);
  }
  /* Push the remaining RHS (out of the while loop) */
  outputRPN.push_back(RHS);
  
  /* Add remaining operators at the end of the RPN list (in reversed order) */
  for (list<Token>::reverse_iterator it = ops.rbegin(); it != ops.rend(); it++) {
    Expr *tkExpr = new TokenExpr((*it));
    outputRPN.push_back(tkExpr);
  }
  
  /* Transform RPN to binary operators */
  list<Expr *> binops;
  for (list<Expr *>::iterator it = outputRPN.begin(); it != outputRPN.end(); it++) {
    
    if (!isa<TokenExpr>(*it)) { // If it's an input or variable...
      binops.push_back(*it); // ... push it to stack
      
    } else { // Else, it's an operator (as token), create a binary expression
      
      Token tk = (cast<TokenExpr>(*it))->token();
      
      /* Pop the two last items of the stack */
      Expr *m = binops.back();
      binops.pop_back();
      Expr *n = binops.back();
      binops.pop_back();
      
      BinOpExpr *expr = new BinOpExpr(n, tk, m, line, col);
      binops.push_back(expr);
    }
  }
  
  return cast<BinOpExpr>(binops.front() /* There *should* be only one item */); // @TODO: Check that is there only one binop into the list
}

PrintExpr * Parser::parsePrint()
{
  vector<Expr *> output;
  Token tok;
  while ( (tok = nexttok()) && tok != tok_print_end ) {
    output.push_back(parseExpr());
  }
  gettok(); // Eat the |tok_print_end|
  
  // @TODO: Show an error if |output| is not printable
  return new PrintExpr(output, line, col);
}

HelloPrintExpr * Parser::parseHelloPrint()
{
  return new HelloPrintExpr(line, col);
}

NopExpr * Parser::parseNop()
{
  return new NopExpr(line, col);
}

ExitExpr * Parser::parseExit()
{
  return new ExitExpr(0, line, col);
}

PushExpr * Parser::parsePush()
{
  Expr *expr = parseExpr();
  return new PushExpr(expr, line, col);
}

PopExpr * Parser::parsePop()
{
  Expr *expr = parseExpr();
  if (!isa<AssignableExpr>(expr)) {
    // @TODO: Show an error if |expr| is nor assignable (variable)
    Assert("parsePop: Destination expr is not assignable (" + expr->DebugString() + ")", expr->line(), expr->col());
  }
  return new PopExpr(expr, line, col);
}

ClearExpr * Parser::parseClear()
{
  return new ClearExpr(line, col);
}

LoopExpr * Parser::parseLoop()
{
  Expr *condition;
  vector<Expr *> thenExprs, thelseExprs;
  
  Token tok;
  /* Get the (only) condition expression */
  while ( (tok = nexttok()) && tok != tok_loop_then ) {
    if (condition)
      _lastExpr = condition;
    
    condition = parseExpr();
    // @TODO: Show a warning if multiple condition (that can not be appended)
  }
  gettok(); // Eat the |tok_loop_then| token
  
  /* Get *then* expressions */
  while ( (tok = nexttok()) && tok != tok_loop_thelse ) {
    Expr *expr = parseExpr();
    if (canGen(expr))
      thenExprs.push_back(expr);
    else
      _lastExpr = expr;
  }
  gettok(); // Eat the |tok_loop_thelse| token
  
  /* Get *thelse* expressions */
  while ( (tok = nexttok()) && tok != tok_loop_end ) {
    Expr *expr = parseExpr();
    if (canGen(expr))
      thelseExprs.push_back(expr);
    else
      _lastExpr = expr;
  }
  gettok(); // Eat the |tok_loop_end| token
  
  // @TODO: Show an error if |condition| is not a condition (binary op, variable or input)
  return new LoopExpr(condition, thenExprs, thelseExprs, line, col);
}

LengthFuncExpr * Parser::parseLengthFunction()
{
  Expr *expr = parseExpr();
  // @TODO: Show an error if [expr] is not a string
  return new LengthFuncExpr(expr, line, col);
}

Expr * Parser::parseExpr()
{
  Expr *expr;
  Token tok = gettok();
  
  // @TODO: Re-order depending frequency
  if (tok == tok_var_start || tok == tok_var_not_start) { // Parse variable
    expr = parseVar(tok == tok_var_not_start);
    
  } else if (tok == tok_input) { // Parse input
    expr = parseInput();
    
  } else if (tok == tok_equal) { // Parse initialization
    return parseInit(); // @TODO: Explain why this return now
    
  } else if (tok == tok_comment) { // Parse comment
    expr = parseComment();
    
  } else if (tok == tok_print_start) { // Parse print
    expr = parsePrint();
    
  } else if (tok == tok_print_hello) { // Parse hello print
    expr = parseHelloPrint();
    
  } else if (tok == tok_nop) { // Parse nop
    expr = parseNop();
    
  } else if (tok == tok_exit) { // Parse exit
    expr = parseExit();
    
  } else if (tok == tok_stack_push) { // Parse push
    expr = parsePush();
    
  } else if (tok == tok_stack_pop) { // Parse pop
    expr = parsePop();
    
  } else if (tok == tok_stack_clear) { // Parse clear
    expr = parseClear();
    
  } else if (tok == tok_loop_start) { // Parse loop
    expr = parseLoop();
    
  } else if (tok == tok_str_length) { // Parse length function
    expr = parseLengthFunction();
    
  } else {
    expr = new UnkownExpr(tok, line, col);
  }
  
  Token op;
  if ( (op = nexttok()) && op.isOperator() ) {
    gettok(); // Eat the operator
    
    expr = parseBinaryOperation(expr, op);
  }
  
  return expr;
}

Parser::Parser(string &s)
{
  input_str = s.c_str();
  input_str_length = strlen(input_str);
  
  Token tok;
  
  if ( (tok = nexttok()) && (tok == tok_prog_start))
    gettok();
  
  while ( (tok = nexttok()) && (tok != tok_prog_end) ) {
    
    Expr *expr = parseExpr();
    if (!canGen(expr)) {
      _lastExpr = expr;
    }
    if (isa<UnkownExpr>(expr)) {
      // @TODO: Show an error if |error| is unknown
      Assert(expr->DebugString(), expr->line(), expr->col());
    }
    
    if (expr) {
      exprs.push_back(expr);
      expr = NULL;
    }
  }
  
  /*** Uncomment this block to get more debugging information ***
   * // This prints all expressions found by the parser
   * list<string> lines;
   * string line;
   * stringstream ss(s);
   *
   * while ( getline(ss, line, '\n') ) {
   *   lines.push_back(line);
   * }
   *
   * for (vector<Expr *>::iterator it = exprs.begin(); it != exprs.end(); it++) {
   *   Expr *expr = *it;
   *   cout << expr->line() << ":" << expr->col() << ": " << expr->DebugString() << "\n";
   *
   *   list<string>::iterator lines_it = lines.begin();
   *   advance(lines_it, expr->line());
   *   cout << *(lines_it) << "\n";
   *   cout.width(expr->col());
   *   cout << "^" << "\n";
   *   cout << "\n";
   * }
   */
}