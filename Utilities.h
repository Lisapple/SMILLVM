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

/* Module *M = [...]; IRBuilder<> B = [...]; Value *CondV = [...];
 * CreateAssert(CondV,
 *              B.CreateGlobalString("this should be true", "SMILTrueAssertMessage"),
 *              M, B, 123, 34);
 */
void CreateAssert(Value *CondV, Value *ErrMsgV, Module *M, IRBuilder<> &B, int line, int col, bool shouldExit = true);

void CreateWarning(Value *WarningMsgV, Module *M, IRBuilder<> &B, int line, int col, bool shouldExit = false);

/*
 * Call "IRBuilder::CreateMemCpy()" but remove the "readonly" attribute
 *   for the second argument (src), clang don't like it when creating a.out (tested on 3.3)
 *   ("error: invalid use of function-only attribute: readonly")
 *                                                    ^
 * The default declaration:
 *   void @llvm.memcpy.xxx(i8* nocapture, i8* nocapture readonly, i64, i32, i1)
 * becomes this one:
 *   void @llvm.memcpy.xxx(i8* nocapture, i8* nocapture , i64, i32, i1)
 */
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