#ifndef SMIL_TOKEN_H
#define SMIL_TOKEN_H

#include <string>

using namespace std;

#include "Tokens.def"

/*** Token ***/
/* Note: all tokens (except the optional "</3" for the end of a script)
 *       have a length of 2 characters.
 */
class Token : public string {
private:
  string ctos(char c1, char c2) {
    string s;
    return s + c1 + c2;
  }
  
public:
  Token() : string() { }
  Token(char c1, char c2) : string( ctos(c1, c2) ) { }
  
  operator bool() {
    return (this->length() > 0);
  }
  
  bool isOperator();
  int getPrecedence();
  
  string print(); // DEBUG
};

static Token tok_unkown;

#endif // SMIL_TOKEN_H