#ifndef SMIL_CODE_GEN_H
#define SMIL_CODE_GEN_H

#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/IR/Intrinsics.h" // For nop expr
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "ObjectType.h"
#include "Expr.h"
#include "Utilities.h"
#include "HashTable.h"

Value * InputAtIndex(int index /* >= 0 */, Module *M, IRBuilder<> &B);

bool canGen(Expr *expr);

#endif // SMIL_CODE_GEN_H