#include "Utilities.h"

void Assert(string err, int line, int col, bool shouldExit)
{
  cout << "@== Assertion";
  if (line > -1 && col > -1)
    cout << "(" << line << ":" << col << ")";
  cout << ": " << err << " ==*" << "\n";
  if (shouldExit)
    exit(1);
}

void CreateInvalidBinopAssertion(Module *M, IRBuilder<> &B, int line, int col, bool shouldExit)
{
  static Value *GInvalidBinOpAssertMessage = NULL;
  if (!GInvalidBinOpAssertMessage) {
    GInvalidBinOpAssertMessage = B.CreateGlobalString("Invalid operation (SMILInvalidOperation)",
                                                      "smil.invalid.operation.assert.message");
  }
  Value *FalseV = B.getInt1(false);
  CreateAssert(FalseV, GInvalidBinOpAssertMessage,
               M, B, line, col);
}

/* Module *M = [...]; IRBuilder<> B = [...]; Value *CondV = [...];
 * CreateAssert(CondV,
 *              B.CreateGlobalString("this should be true", "SMILTrueAssertMessage"),
 *              M, B, 123, 34);
 */
void CreateAssert(Value *CondV, Value *ErrMsgV, Module *M, IRBuilder<> &B, int line, int col, bool shouldExit)
{
  LLVMContext &C = M->getContext();
  
  // @TODO: Create branch depending of |CondV|
  Function *F = B.GetInsertBlock()->getParent();
  BasicBlock *TBB = BasicBlock::Create(C, "ThrowBlock", F);
  BasicBlock *CBB = BasicBlock::Create(C, "ContinueBlock", F);
  B.CreateCondBr(CondV, CBB, TBB);
  
  B.SetInsertPoint(TBB);
  IRBuilder<> TB(TBB);
  static Value *GAssertDefaultFormat = NULL;
  if (!GAssertDefaultFormat) {
    GAssertDefaultFormat = TB.CreateGlobalString("@== Assertion (%d, %d): %s ==*\n",
                                                 "assert.default.format");
  }
  
  // i32 @printf(i8*, ...)
  Type* Args[] = { Type::getInt8PtrTy(C) };
  FunctionType *FuncTy = FunctionType::get(Type::getInt32Ty(C), Args, true);
  Function *PrintfF = cast<Function>(M->getOrInsertFunction("printf", FuncTy));
  TB.CreateCall4(PrintfF,
                 CastToCStr(GAssertDefaultFormat, TB),
                 TB.getInt32(line), TB.getInt32(col),
                 CastToCStr(ErrMsgV, TB));
  
  if (shouldExit) {
    Function *ExitF = cast<Function>(M->getOrInsertFunction("exit", Type::getVoidTy(C),
                                                            Type::getInt32Ty(C),
                                                            (Type *)0));
    Value *CodeV = TB.getInt32(1);
    TB.CreateCall(ExitF, CodeV);
    TB.CreateUnreachable();
  } else {
    TB.CreateBr(CBB);
  }
  
  B.SetInsertPoint(CBB);
}

void CreateWarning(Value *WarningMsgV, Module *M, IRBuilder<> &B, int line, int col, bool shouldExit)
{
  LLVMContext &C = M->getContext();
  
  static Value *GWarningDefaultFormat = NULL;
  if (!GWarningDefaultFormat) {
    GWarningDefaultFormat = B.CreateGlobalString("Warning (%d, %d): %s\n",
                                                 "warning.default.format");
  }
  
  // i32 @printf(i8*, ...)
  Type* Args[] = { Type::getInt8PtrTy(C) };
  FunctionType *FuncTy = FunctionType::get(Type::getInt32Ty(C), Args, true);
  Function *PrintfF = cast<Function>(M->getOrInsertFunction("printf", FuncTy));
  B.CreateCall4(PrintfF,
                CastToCStr(GWarningDefaultFormat, B),
                B.getInt32(line), B.getInt32(col),
                CastToCStr(WarningMsgV, B));
  
  if (shouldExit) {
    Function *ExitF = cast<Function>(M->getOrInsertFunction("exit", Type::getVoidTy(C),
                                                            Type::getInt32Ty(C),
                                                            (Type *)0));
    Value *CodeV = B.getInt32(1);
    B.CreateCall(ExitF, CodeV);
    B.CreateUnreachable();
  }
}

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
void MemCpy(Value *DestV, Value *SrcV, Value *Size, Module *M, IRBuilder<> &B, unsigned align)
{
  LLVMContext &C = M->getContext();
  
  CallInst * CI = B.CreateMemCpy(DestV, SrcV, Size, align);
  
  Function::arg_iterator it = CI->getCalledFunction()->arg_begin();
  Argument *SrcArg = ++it;
  
  AttributeSet AS = AttributeSet::get(C, 1, Attribute::ReadOnly);
  SrcArg->removeAttr(AS);
}

Value * Strlen(Value *StrV, Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  // i64 @strlen(i8*)
  Type* StrlenArgs[] = { Type::getInt8PtrTy(C) };
  FunctionType *StrlenTy = FunctionType::get(Type::getInt64Ty(C), StrlenArgs, false);
  Function *StrlenF = cast<Function>(M->getOrInsertFunction("strlen", StrlenTy));
  return B.CreateCall(StrlenF, CastToCStr(StrV, B));
}

Value * StrToInt64(Value *StrV, Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  // i64 @atol(i8*)
  Type* AtolArgs[] = { Type::getInt8PtrTy(C) };
  FunctionType *AtolTy = FunctionType::get(Type::getInt64Ty(C), AtolArgs, false);
  Function *AtolF = cast<Function>(M->getOrInsertFunction("atol", AtolTy));
  return B.CreateCall(AtolF, CastToCStr(StrV, B));
}

Value * StrToInt32(Value *StrV, Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  return B.CreateIntCast(StrToInt64(StrV, M, B), Type::getInt32Ty(C), false);
}

Value * Atoi(Value *StrV, Module *M, IRBuilder<> &B)
{
  return StrToInt32(StrV, M, B);
}

Value * Strxch(Value *StrV, Value *Occurence, Module *M, IRBuilder<> &B)
{
  /*
   *   int occLen = strlen(occurence);
   *   char * output = (char *)malloc(strlen(s) + 1);
   *
   *   char * outputPtr = (char *)s;
   *   while (1) {
   *     char * oldPtr = outputPtr;
   *
   *     outputPtr = strstr(outputPtr, occurence);
   *     strncat(soutput, oldPtr, (outputPtr) ? (outputPtr-oldOutputPtr) : strlen(s));
   *
   *     outputPtr += occLen;
   *     if (sstr == NULL) break;
   *   }
   */
  
  LLVMContext &C = M->getContext();
  Function *F = B.GetInsertBlock()->getParent();
  
  // int occLen = strlen(occurence);
  Value *OccLen = Strlen(Occurence, M, B);
  
  Value *StrLen = Strlen(StrV, M, B);
  
  // char * output = (char *)malloc(strlen(s) + 1);
  Value *Output = B.CreateAlloca(Type::getInt8Ty(C),
                                 B.CreateAdd(StrLen,
                                             B.getInt64(1)));
  Output->setName("Output");
  B.CreateMemSet(Output, B.getInt8(0), B.CreateAdd(StrLen, B.getInt64(1)), 8);
  
  // char * outputPtr = (char *)s;
  Value *OutputPtr = B.CreateAlloca(Type::getInt64Ty(C));
  OutputPtr->setName("OutputPtr");
  B.CreateStore(B.CreatePtrToInt(StrV, Type::getInt64Ty(C)),
                OutputPtr);
  
  // i8* @strstr(i8*, i8*)
  Type* StrstrArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
  FunctionType *StrstrTy = FunctionType::get(Type::getInt8PtrTy(C), StrstrArgs, false);
  Function *StrstrF = cast<Function>(M->getOrInsertFunction("strstr", StrstrTy));
  
  // i8* @strncat(i8*, i8*, i64)
  Type* StrncatArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C), Type::getInt64Ty(C) };
  FunctionType *StrncatTy = FunctionType::get(Type::getInt8PtrTy(C), StrncatArgs, false);
  Function *StrncatF = cast<Function>(M->getOrInsertFunction("strncat", StrncatTy));
  
  BasicBlock *LoopBB = BasicBlock::Create(C, "LoopBlock", F);
  B.CreateBr(LoopBB);
  B.SetInsertPoint(LoopBB);
  
  BasicBlock *DoneBB = BasicBlock::Create(C, "DoneBlock", F);
  
  IRBuilder<> LoopB(LoopBB);
  
  // char * oldPtr = outputPtr;
  Value *OldPtr = LoopB.CreateAlloca(Type::getInt64Ty(C));
  LoopB.CreateStore(LoopB.CreateLoad(OutputPtr), OldPtr);
  
  // outputPtr = strstr(outputPtr, occurence);
  Value *Ptr = LoopB.CreateCall2(StrstrF,
                                 LoopB.CreateIntToPtr(LoopB.CreateLoad(OutputPtr),
                                                      Type::getInt8PtrTy(C)),
                                 CastToCStr(Occurence, LoopB));
  Value *IntPtr = LoopB.CreatePtrToInt(Ptr, Type::getInt64Ty(C));
  LoopB.CreateStore(IntPtr, OutputPtr);
  
  // strncat(soutput, oldOutputPtr, (outputPtr) ? (outputPtr-oldOutputPtr) : strlen(s));
  LoopB.CreateCall3(StrncatF,
                    CastToCStr(Output, LoopB),
                    LoopB.CreateIntToPtr(LoopB.CreateLoad(OldPtr),
                                         Type::getInt8PtrTy(C)),
                    LoopB.CreateSelect(LoopB.CreateICmpEQ(IntPtr,
                                                          LoopB.getInt64(0)),
                                       StrLen,
                                       LoopB.CreateSub(LoopB.CreateLoad(OutputPtr),
                                                       LoopB.CreateLoad(OldPtr))));
  
  // outputPtr += occLen;
  Value *Offset = LoopB.CreateAdd(B.CreateLoad(OutputPtr), OccLen);
  LoopB.CreateStore(Offset, OutputPtr);
  
  // if (sstr == NULL) break;
  LoopB.CreateCondBr(LoopB.CreateICmpEQ(IntPtr,
                                        LoopB.getInt64(0)),
                     DoneBB,
                     LoopBB);
  
  B.SetInsertPoint(DoneBB);
  IRBuilder<> DoneB(DoneBB);
  
  return Output;
}

Value * CxxStrToVal(string &str, Module *M, IRBuilder<> &B)
{
  static map<string, Value *> strings;
  
  Value *V;
  if ((V = strings[str]))
    return V;
  
  V = CastToCStr(B.CreateGlobalString(str, str), B);
  strings[str] = V;
  return V;
}

Value * ObjToStr(Value *Obj, Module *M, IRBuilder<> &B)
{
  // @TODO: Create a function "@otos"
  LLVMContext &C = M->getContext();
  
  Value *TypePtr = B.CreateStructGEP(Obj, ObjectFieldType);
  Value *DataPtr = B.CreateStructGEP(Obj, ObjectFieldData);
  
  Value *CompResult = B.CreateICmpEQ(B.CreateLoad(TypePtr),
                                     B.getInt1(ObjectTypeInteger));
  
  Function *F = B.GetInsertBlock()->getParent();
  BasicBlock *IntBB = BasicBlock::Create(C, "IntegerBlock", F);
  BasicBlock *StrBB = BasicBlock::Create(C, "StringBlock", F);
  
  B.CreateCondBr(CompResult, IntBB, StrBB);
  
  BasicBlock *DoneBB = BasicBlock::Create(C, "DoneBlock", F);
  
  
  /* Integer Block */
  B.SetInsertPoint(IntBB);
  IRBuilder<> IntB(IntBB);
  Value *StrPtr = NULL;
  {
    static Value *GSprintfFormat = NULL;
    if (!GSprintfFormat) GSprintfFormat = IntB.CreateGlobalString("%lld", "sprintf.format");
    
    // i32 @sprintf(i8*, i8*, ...)
    Type* SprintfArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
    FunctionType *SprintfTy = FunctionType::get(Type::getInt32Ty(C), SprintfArgs, true);
    Function *SprintfF = cast<Function>(M->getOrInsertFunction("sprintf", SprintfTy));
    
    Value *Size = IntB.getInt32(20 /* = log10(2^64) */ + 1);
    StrPtr = IntB.CreateAlloca(Type::getInt8Ty(C), Size);
    IntB.CreateCall3(SprintfF,
                     StrPtr,
                     CastToCStr(GSprintfFormat, IntB),
                     IntB.CreateLoad(DataPtr));
  }
  IntB.CreateBr(DoneBB);
  
  /* String Block */
  B.SetInsertPoint(StrBB);
  IRBuilder<> StrB(StrBB);
  Value *AllocPtr = NULL;
  {
    Value *Str = StrB.CreateIntToPtr(StrB.CreateLoad(DataPtr), Type::getInt8PtrTy(C));
    Value *Length = Strlen(Str, M, StrB);
    Value *Size = StrB.CreateAdd(Length, StrB.getInt64(1));
    
    // i8* @malloc(i64)
    FunctionType *MallocTy = FunctionType::get(Type::getInt8PtrTy(C),
                                               (Type *[]) { Type::getInt64Ty(C) }, false);
    Function *MallocF = cast<Function>(M->getOrInsertFunction("malloc", MallocTy));
    AllocPtr = StrB.CreateCall(MallocF, Size); // |AllocPtr| : i8*
    
    // i8* @strcpy(i8*, i8*)
    Type* StrcpyArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
    FunctionType *StrcpyTy = FunctionType::get(Type::getInt8PtrTy(C), StrcpyArgs, false /* not vararg */);
    Function *StrcpyF = cast<Function>(M->getOrInsertFunction("strcpy", StrcpyTy));
    
    StrB.CreateCall2(StrcpyF, AllocPtr, Str);
  }
  StrB.CreateBr(DoneBB);
  
  /* Done Block */
  B.SetInsertPoint(DoneBB);
  IRBuilder<> DoneB(DoneBB);
  
  PHINode * PHI = DoneB.CreatePHI(Type::getInt8PtrTy(C), 2);
  PHI->addIncoming(StrPtr, IntBB);
  PHI->addIncoming(AllocPtr, StrBB);
  
  return PHI;
}

Value * ObjToInt64(Value *Obj, Module *M, IRBuilder<> &B)
{
  // @TODO: Create a function "i64 otoi64(%obj*)"
  
  LLVMContext &C = M->getContext();
  Value *IntPtr = B.CreateAlloca(Type::getInt64Ty(C));
  Value *Data = B.CreateLoad(B.CreateStructGEP(Obj, ObjectFieldData));
  
  Function *F = B.GetInsertBlock()->getParent();
  BasicBlock *IntBB = BasicBlock::Create(C, "Cast64.IntegerBlock", F);
  BasicBlock *StrBB = BasicBlock::Create(C, "Cast64.StringBlock", F);
  BasicBlock *DoneBB = BasicBlock::Create(C, "Cast64.DoneBlock", F);
  
  Value *FieldPtr = B.CreateStructGEP(Obj, ObjectFieldType);
  Value *CompResult = B.CreateICmpEQ(B.CreateLoad(FieldPtr),
                                     B.getInt1(ObjectTypeInteger));
  B.CreateCondBr(CompResult, IntBB, StrBB);
  
  /* Integer Block */
  IRBuilder<> IntB(IntBB);
  B.SetInsertPoint(IntBB);
  
  IntB.CreateStore(Data, IntPtr);
  
  IntB.CreateBr(DoneBB);
  
  /* String Block */
  IRBuilder<> StrB(StrBB);
  B.SetInsertPoint(StrBB);
  
  Value * Ptr = StrB.CreateIntToPtr(Data, Type::getInt8PtrTy(C));
  StrB.CreateStore(Strlen(Ptr, M, B), IntPtr);
  
  StrB.CreateBr(DoneBB);
  B.SetInsertPoint(DoneBB);
  return B.CreateLoad(IntPtr);
}

Value * ValToObj(Value *Val, Module *M, IRBuilder<> &B)
{
  // @TODO: Create a function "valtoobj"
  // @TODO: Save as float (and not a integer)
  
  LLVMContext &C = M->getContext();
  Value *Ptr = B.CreateAlloca(getObjTy(C));
  
  static Value *GFormat = NULL;
  if (!GFormat) GFormat = B.CreateGlobalString("%lld%s", "sscanf.format");
  
  // i32 @sscanf(i8*, i8*, ...)
  Type* SscanfArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
  FunctionType *SscanfTy = FunctionType::get(Type::getInt32Ty(C), SscanfArgs, true);
  Function *SscanfF = cast<Function>(M->getOrInsertFunction("sscanf", SscanfTy));
  
  // sscanf(s, "%lld%s", &d, &c)
  Value *PrtD = B.CreateAlloca(Type::getInt32Ty(C));
  Value *PrtC = B.CreateAlloca(Type::getInt8Ty(C));
  Value *RetV = B.CreateCall4(SscanfF,
                              CastToCStr(Val, B),
                              CastToCStr(GFormat, B),
                              CastToCStr(PrtD, B),
                              CastToCStr(PrtC, B));
  
  /* The "sscanf" function returns "1" on only integer (|d| converted and not |c|) */
  Value *CompResult = B.CreateICmpEQ(RetV, B.getInt32(1));
  
  Function *F = B.GetInsertBlock()->getParent();
  BasicBlock *IntBB = BasicBlock::Create(C, "IntegerBlock", F);
  BasicBlock *StrBB = BasicBlock::Create(C, "StringBlock", F);
  
  B.CreateCondBr(CompResult, IntBB, StrBB);
  
  BasicBlock *DoneBB = BasicBlock::Create(C, "DoneBlock", F);
  
  /* Integer Block */
  B.SetInsertPoint(IntBB);
  IRBuilder<> IntB(IntBB);
  
  Value *IntV = StrToInt64(Val, M, IntB);
  
  Value *IFPtr = IntB.CreateStructGEP(Ptr, ObjectFieldData);
  IntB.CreateStore(IntV, IFPtr);
  
  /* Update the index of the field used */
  IntB.CreateStore(IntB.getInt1(ObjectTypeInteger),
                   IntB.CreateStructGEP(Ptr, ObjectFieldType));
  
  IntB.CreateBr(DoneBB);
  
  /* String Block */
  B.SetInsertPoint(StrBB);
  IRBuilder<> StrB(StrBB);
  
  Value *CFPtr = StrB.CreateStructGEP(Ptr, ObjectFieldData);
  
  Value *Length = Strlen(Val, M, StrB);
  Value *Size = StrB.CreateAdd(Length, StrB.getInt64(1));
  Value *AllocPtr = StrB.CreateAlloca(Type::getInt8Ty(C), Size);
  
  StrB.CreateMemSet(AllocPtr, StrB.getInt8(0), Size, 8);
  MemCpy(AllocPtr, Val, Length, M, StrB);
  StrB.CreateStore(StrB.CreatePtrToInt(AllocPtr, Type::getInt64Ty(C)),
                   CFPtr);
  
  /* Update the index of the field used */
  StrB.CreateStore(StrB.getInt1(ObjectTypeString),
                   StrB.CreateStructGEP(Ptr, ObjectFieldType));
  
  StrB.CreateBr(DoneBB);
  
  /* Done Block */
  B.SetInsertPoint(DoneBB);
  
  return Ptr;
}
