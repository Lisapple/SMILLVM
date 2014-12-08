#ifndef SMIL_UTILITIES_H
#define SMIL_UTILITIES_H

#include <iostream>

#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "ObjectType.h"

using namespace std;
using namespace llvm;

void Assert(string err, int line, int col, bool shouldExit = true);

void CreateInvalidBinopAssertion(Module *M, IRBuilder<> &B, int line, int col, bool shouldExit = true);

void CreateAssert(Value *CondV, Value *ErrMsgV, Module *M, IRBuilder<> &B, int line, int col, bool shouldExit = true);

void CreateWarning(Value *WarningMsgV, Module *M, IRBuilder<> &B, int line, int col, bool shouldExit = false);

void MemCpy(Value *DestV, Value *SrcV, Value *Size, Module *M, IRBuilder<> &B, unsigned align = 8);

Value * Strlen(Value *StrV, Module *M, IRBuilder<> &B);

Value * StrToInt64(Value *StrV, Module *M, IRBuilder<> &B);

Value * StrToInt32(Value *StrV, Module *M, IRBuilder<> &B);

Value * Atoi(Value *StrV, Module *M, IRBuilder<> &B);

Value * Strxch(Value *StrV, Value *Occurence, Module *M, IRBuilder<> &B);

Value * CxxStrToVal(string &str, Module *M, IRBuilder<> &B);

Value * ObjToStr(Value *Obj, Module *M, IRBuilder<> &B);

Value * ObjToInt64(Value *Obj, Module *M, IRBuilder<> &B);

Value * ValToObj(Value *Val, Module *M, IRBuilder<> &B);

#endif // SMIL_UTILITIES_H