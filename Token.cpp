#include "Token.h"

bool Token::isOperator()
{
  return (*this == tok_add || *this == tok_sub ||
          *this == tok_mod ||
          *this == tok_mul || *this == tok_div ||
          *this == tok_and || *this == tok_or);
}

int Token::getPrecedence()
{
  if /**/ (*this == tok_and || *this == tok_or) return prec_high;
  else if (*this == tok_mul || *this == tok_div ||
           *this == tok_mod) return prec_normal;
  else if (*this == tok_add || *this == tok_sub) return prec_low;
  return 0;
}

string Token::print() // DEBUG
{
  if /**/ (*this == tok_add) return "+";
  else if (*this == tok_sub) return "-";
  else if (*this == tok_mul) return "*";
  else if (*this == tok_div) return "/";
  else if (*this == tok_mod) return "%";
  else if (*this == tok_and) return "&&";
  else if (*this == tok_or ) return "||";
  
  return "unkown operator: \"" + *this + "\"";
}