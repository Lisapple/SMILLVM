#include "CodeGen.h"

/*** Inputs helper ***/
Value * InputAtIndex(int index /* >= 0 */, Module *M, IRBuilder<> &B)
{
  string name;
  for (int i = 0; i < (index + 1); i++)
    name += tok_input;
  Value *NameV = CxxStrToVal(name, M, B);
  return GetPtrOrInsert(NameV, M, B);
}

bool canGen(Expr *expr)
{
  return (isa<InitExpr>(expr) || isa<PrintExpr>(expr) || isa<HelloPrintExpr>(expr) ||
          isa<ExitExpr>(expr) || isa<LoopExpr>(expr) ||
          isa<PushExpr>(expr) || isa<PopExpr>(expr) || isa<ClearExpr>(expr));
}

/*** Wrapper for token Expression ***/
Value * TokenExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  return NULL;
}

/*** Comment Expression ***/
Value * CommentExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  // Nothing to do (since comments can't be added into IR)
  return NULL;
}

/*** Input Expression ***/
Value * InputExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  return InputAtIndex(_index, M, B);
}

/*** Variable Expression ***/
Value * VarExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  Value *NameV = CxxStrToVal(_name, M, B);
  return GetPtrOrInsert(NameV, M, B);
}

/*** Named Variable Expression ***/
Value * NamedVarExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  Value *NameV = ObjToStr(_expr->CodeGen(M, B), M, B);
  return GetPtrOrInsert(NameV, M, B);
}

/*** Initialisation Expression ***/
Value * InitExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  Value *RHSPtr = _RHS->CodeGen(M, B);
  Value *LHSPtr = _LHS->CodeGen(M, B);
  
  /* LHS, RHS
   * [LHS, !RHS]
   * [!LHS, RHS]
   * !LHS, !RHS
   * LHS, Input
   * [!LHS, Input]
   */
  
  AssignableExpr *LHSVar = cast<AssignableExpr>(_LHS);
  bool RHSisVar = isa<AssignableExpr>(_RHS);
  bool LHSInversed = LHSVar->getInversed();
  
  if (LHSInversed &&
      (RHSisVar && cast<VarExpr>(_RHS)->getInversed())) { // x(LHS) && x(RHS)
    
    Value *LHSDataPtr = B.CreateStructGEP(LHSPtr, ObjectFieldData);
    Value *RHSDataPtr = B.CreateStructGEP(RHSPtr, ObjectFieldData);
    
    // Inversed: set |V| to zero if |int(V)| != 1 or if |str(V)| is a ptr != NULL
    Value *ResEQZ = B.CreateICmpEQ(B.CreateLoad(RHSDataPtr), B.getInt64(0));
    Value *RHSDataNot = B.CreateSelect(ResEQZ, B.getInt64(0), B.getInt64(1));
    B.CreateStore(B.CreateIntCast(RHSDataNot, Type::getInt64Ty(C), false),
                  LHSDataPtr);
    
    /* Update the index of the field used */
    B.CreateStore(B.getInt1(ObjectTypeInteger),
                  B.CreateStructGEP(LHSPtr, ObjectFieldType));
    
  } else if ((LHSInversed
              && ((RHSisVar && !cast<VarExpr>(_RHS)->getInversed())
                  || (!RHSisVar))
              ) || (!LHSInversed
                    && (RHSisVar && cast<VarExpr>(_RHS)->getInversed()))
             ) { // ( ( x(LHS) && ( RHS || Input ) ) || ( LHS && x(RHS) ) )
    
    Value *LHSDataPtr = B.CreateStructGEP(LHSPtr, ObjectFieldData);
    Value *RHSDataPtr = B.CreateStructGEP(RHSPtr, ObjectFieldData);
    
    // Inversed: set |V| to zero if |int(V)| != 1 or if |str(V)| is a ptr != NULL
    Value *ResEQZ = B.CreateICmpEQ(B.CreateLoad(RHSDataPtr), B.getInt64(0));
    Value *RHSDataNot = B.CreateSelect(ResEQZ, B.getInt64(1), B.getInt64(0));
    B.CreateStore(B.CreateIntCast(RHSDataNot, Type::getInt64Ty(C), false),
                  LHSDataPtr);
    
    // Update the index of the field used
    B.CreateStore(B.getInt1(ObjectTypeInteger),
                  B.CreateStructGEP(LHSPtr, ObjectFieldType));
    
  } else {
    Value *RHSDataPtr = B.CreateStructGEP(RHSPtr, ObjectFieldData);
    B.CreateStore(B.CreateLoad(RHSDataPtr),
                  B.CreateStructGEP(LHSPtr, ObjectFieldData));
    
    Value *RHSTypePtr = B.CreateStructGEP(RHSPtr, ObjectFieldType);
    B.CreateStore(B.CreateLoad(RHSTypePtr),
                  B.CreateStructGEP(LHSPtr, ObjectFieldType));
  }
  
  return LHSPtr;
}

/*** Binary Operator Expression ***/
Value * BinOpExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  if (_LHS == NULL) {
    out() << "|LHS| == NULL" << "\n";
    exit(1);
  }
  
  if (_RHS == NULL) {
    out() << "|RHS| == NULL" << "\n";
    exit(1);
  }
  
  Value *LHSV = _LHS->CodeGen(M, B);
  Value *LHSFieldPtr = B.CreateStructGEP(LHSV, ObjectFieldType);
  Value *LHSisInt = B.CreateICmpEQ(B.CreateLoad(LHSFieldPtr),
                                   B.getInt1(ObjectTypeInteger));
  Value *LHSDataPtr = B.CreateStructGEP(LHSV, ObjectFieldData);
  LHSDataPtr->setName("LHSDataPtr");
  
  Value *RHSV = _RHS->CodeGen(M, B);
  Value *RHSFieldPtr = B.CreateStructGEP(RHSV, ObjectFieldType);
  Value *RHSisInt = B.CreateICmpEQ(B.CreateLoad(RHSFieldPtr),
                                   B.getInt1(ObjectTypeInteger));
  Value *RHSDataPtr = B.CreateStructGEP(RHSV, ObjectFieldData);
  LHSDataPtr->setName("RHSDataPtr");
  
  Value *ObjPtr = B.CreateAlloca(getObjTy(C));
  ObjPtr->setName("objPtr");
  
  Function *F = B.GetInsertBlock()->getParent();
  BasicBlock *IntBB = BasicBlock::Create(C, "IntegerBlock", F);
  BasicBlock *StrBB = BasicBlock::Create(C, "StringBlock", F);
  
  B.CreateCondBr(B.CreateAnd(LHSisInt, RHSisInt),
                 IntBB,
                 StrBB);
  
  /* Only integers block */
  IRBuilder<> IntB(IntBB);
  B.SetInsertPoint(IntBB);
  
  Instruction::BinaryOps Op = (_op == tok_add) ? Instruction::Add :
  /*                       */ (_op == tok_sub) ? Instruction::Sub :
  /*                       */ (_op == tok_mul) ? Instruction::Mul :
  /*                       */ (_op == tok_div) ? Instruction::UDiv : // @TODO: Or signed "SDiv"?
  /*                       */ (_op == tok_mod) ? Instruction::URem : // @TODO: Or signed "SRem"?
  /*                       */ (_op == tok_and) ? Instruction::And :
  /*                                          */ Instruction::Or;
  
  Value *Result = IntB.CreateBinOp(Op,
                                   IntB.CreateLoad(LHSDataPtr),
                                   IntB.CreateLoad(RHSDataPtr));
  
  IntB.CreateStore(Result,
                   IntB.CreateStructGEP(ObjPtr, ObjectFieldData));
  
  IntB.CreateStore(IntB.getInt1(ObjectTypeInteger),
                   IntB.CreateStructGEP(ObjPtr, ObjectFieldType));
  
  BasicBlock *EndBB = BasicBlock::Create(C, "EndBlock", F);
  IntB.CreateBr(EndBB);
  
  /*** String and (string or integer) block ***/
  IRBuilder<> StrB(StrBB);
  B.SetInsertPoint(StrBB);
  
  StrB.CreateStore(StrB.getInt1(ObjectTypeString),
                   StrB.CreateStructGEP(ObjPtr, ObjectFieldType));
  
  // const char *sOutput = [...];
  // const char *sInput1 = [...];
  // const char *sInput2 = [...];
  // int Input2 = [...];
  
  if /**/ (_op == tok_add) { // string and (string or integer)
    
    // sOutput = strcat(sInput1, sInput2)
    // or:
    /*
     sInput = [...]
     sprintf(sInput, "%lld", Input2)
     sOutput = strcat(sInput1, sInput)
     */
    
    /*
     * StrB -> [LHSBlock]
     *   LHSBlock -> [LHSisIntegerBlock | LHSisStringBlock]
     *     .LHSisIntegerBlock -> [LHSDoneBlock]
     *     .LHSisStringBlock -> [LHSDoneBlock]
     *     .LHSDoneBlock -> [RHSBlock]
     *   RHSBlock -> [RHSisIntegerBlock | RHSisStringBlock]
     *     .RHSisIntegerBlock -> [RHSDoneBlock]
     *     .RHSisStringBlock -> [RHSDoneBlock]
     *     .RHSDoneBlock -> [DoneBlock]
     *   DoneBlock -> [EndBlock]
     * EndBlock
     */
    
    BasicBlock *LHSBB = BasicBlock::Create(C, "_LHSBlock", F);
    IRBuilder<> LHSB(LHSBB);
    
    BasicBlock *RHSBB = BasicBlock::Create(C, "_RHSBlock", F);
    IRBuilder<> RHSB(RHSBB);
    
    StrB.CreateBr(LHSBB);
    
    StrB.SetInsertPoint(LHSBB);
    StrB.SetInsertPoint(RHSBB);
    
    /** Left Hand Side **/
    Value *LHSPtrPtr = LHSB.CreateAlloca(Type::getInt8PtrTy(C));
    LHSPtrPtr->setName("LHSPtrPtr");
    // @TODO: Use Phi for |LHSPtrPtr|
    
    BasicBlock *LHSisIntBB = BasicBlock::Create(C, "_LHSBlock.LHSisIntegerBlock", F);
    BasicBlock *LHSisStrBB = BasicBlock::Create(C, "_LHSBlock.LHSisStringBlock", F);
    BasicBlock *LHSDoneBB = BasicBlock::Create(C, "_LHSBlock.LHSDoneBlock", F);
    
    LHSB.CreateCondBr(LHSisInt, LHSisIntBB, LHSisStrBB);
    
    /* LHS is Integer Block */
    LHSB.SetInsertPoint(LHSisIntBB);
    IRBuilder<> LHSisIntB(LHSisIntBB);
    
    // i32 @sprintf(i8*, i8*, ...)
    Type* SprintfArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
    FunctionType *SprintfTy = FunctionType::get(Type::getInt32Ty(C), SprintfArgs, true);
    Function *SprintfF = cast<Function>(M->getOrInsertFunction("sprintf", SprintfTy));
    
    static Value *GSprintfFormat = NULL;
    if (!GSprintfFormat) GSprintfFormat = B.CreateGlobalString("%lld", "sprintf.format");
    
    Value *LHSSize = LHSisIntB.getInt32(20 /* = log10(2^64) */ + 1);
    Value *LHSStrPtr = LHSisIntB.CreateAlloca(Type::getInt8Ty(C), LHSSize);
    LHSisIntB.CreateCall3(SprintfF,
                          LHSStrPtr,
                          CastToCStr(GSprintfFormat, LHSisIntB),
                          LHSisIntB.CreateLoad(LHSDataPtr));
    LHSisIntB.CreateStore(LHSStrPtr, LHSPtrPtr);
    
    LHSisIntB.CreateBr(LHSDoneBB);
    
    /* LHS is String Block */
    LHSB.SetInsertPoint(LHSisStrBB);
    IRBuilder<> LHSisStrB(LHSisStrBB);
    
    Value *LHSPtr = LHSisStrB.CreateIntToPtr(LHSisStrB.CreateLoad(LHSDataPtr),
                                             Type::getInt8PtrTy(C));
    LHSisStrB.CreateStore(LHSPtr, LHSPtrPtr);
    
    LHSisStrB.CreateBr(LHSDoneBB);
    
    /* LHS Done Block */
    IRBuilder<> LHSDoneB(LHSDoneBB);
    LHSDoneB.CreateBr(RHSBB);
    LHSB.SetInsertPoint(LHSDoneBB);
    
    /** Right Hand Side **/
    Value *RHSPtrPtr = RHSB.CreateAlloca(Type::getInt8PtrTy(C));
    RHSPtrPtr->setName("RHSPtrPtr");
    // @TODO: Use Phi for |RHSPtrPtr|
    
    BasicBlock *RHSisIntBB = BasicBlock::Create(C, "_RHSBlock.RHSisIntegerBlock", F);
    BasicBlock *RHSisStrBB = BasicBlock::Create(C, "_RHSBlock.RHSisStringBlock", F);
    BasicBlock *RHSDoneBB = BasicBlock::Create(C, "_RHSBlock.RHSDoneBlock", F);
    
    RHSB.CreateCondBr(RHSisInt, RHSisIntBB, RHSisStrBB);
    
    /* RHS is Integer Block */
    RHSB.SetInsertPoint(RHSisIntBB);
    IRBuilder<> RHSisIntB(RHSisIntBB);
    
    // Convert RHS from int to str (to concat)
    Value *RHSSize = RHSisIntB.getInt32(20 /* = log10(2^64) */ + 1);
    Value *RHSStrPtr = RHSisIntB.CreateAlloca(Type::getInt8Ty(C), RHSSize);
    RHSisIntB.CreateCall3(SprintfF,
                          RHSStrPtr,
                          CastToCStr(GSprintfFormat, LHSisIntB),
                          RHSisIntB.CreateLoad(RHSDataPtr));
    RHSisIntB.CreateStore(RHSStrPtr, RHSPtrPtr);
    
    RHSisIntB.CreateBr(RHSDoneBB);
    
    /* RHS is String Block */
    RHSB.SetInsertPoint(RHSisStrBB);
    IRBuilder<> RHSisStrB(RHSisStrBB);
    
    Value *RHSPtr = RHSisStrB.CreateIntToPtr(RHSisStrB.CreateLoad(RHSDataPtr),
                                             Type::getInt8PtrTy(C));
    RHSisStrB.CreateStore(RHSPtr, RHSPtrPtr);
    
    RHSisStrB.CreateBr(RHSDoneBB);
    
    BasicBlock *DoneBB = BasicBlock::Create(C, "_DoneBlock__", F);
    IRBuilder<> DoneB(DoneBB);
    
    /* RHS Done Block */
    IRBuilder<> RHSDoneB(RHSDoneBB);
    RHSDoneB.CreateBr(DoneBB);
    RHSB.SetInsertPoint(RHSDoneBB);
    
    StrB.SetInsertPoint(DoneBB);
    
    // i8* @strcat(i8*, i8*)
    Type* StrcatArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
    FunctionType *StrcatTy = FunctionType::get(Type::getInt8PtrTy(C), StrcatArgs, false);
    Function *StrcatF = cast<Function>(M->getOrInsertFunction("strcat", StrcatTy));
    
    Value *LHSPtrV = DoneB.CreateLoad(LHSPtrPtr);
    Value *RHSPtrV = DoneB.CreateLoad(RHSPtrPtr);
    
    Value *Length = DoneB.CreateAdd(Strlen(LHSPtrV, M, DoneB),
                                    Strlen(RHSPtrV, M, DoneB));
    Value *Size = DoneB.CreateAdd(Length, DoneB.getInt64(1));
    Value *StrPtr = DoneB.CreateAlloca(Type::getInt8Ty(C), Size);
    
    // Set '\0' to the buffer string |StrPtr| (only at [0] to get an empty string)
    DoneB.CreateMemSet(StrPtr, DoneB.getInt8(0), DoneB.getInt64(1), 8);
    DoneB.CreateCall2(StrcatF, StrPtr, LHSPtrV);
    DoneB.CreateCall2(StrcatF, StrPtr, RHSPtrV);
    
    DoneB.CreateStore(DoneB.CreatePtrToInt(StrPtr, Type::getInt64Ty(C)),
                      DoneB.CreateStructGEP(ObjPtr, ObjectFieldData));
    DoneB.CreateBr(EndBB);
  }
  else if (_op == tok_sub) { // string and (string or integer)
    
    // strncpy(sOutput, sInput1, strlen(sInput1) - Input2)
    
    BasicBlock *SIBB = BasicBlock::Create(C, "StringAndInteger", F);
    BasicBlock *SSBB = BasicBlock::Create(C, "StringAndString", F);
    
    BasicBlock *DoneBB = BasicBlock::Create(C, "DoneBlock", F);
    
    StrB.CreateCondBr(StrB.CreateXor(LHSisInt, RHSisInt),
                      SIBB,
                      SSBB);
    
    // if (string and integer)
    IRBuilder<> SIB(SIBB);
    StrB.SetInsertPoint(SIBB);
    
    // strncpy([output], [StrV], [StrLen] - [IntV])
    
    Value *IntV = SIB.CreateLoad(SIB.CreateSelect(RHSisInt,
                                                  RHSDataPtr,
                                                  LHSDataPtr,
                                                  "IntV"));
    
    Value *StrVPtr = SIB.CreateLoad(SIB.CreateSelect(RHSisInt,
                                                     LHSDataPtr,
                                                     RHSDataPtr,
                                                     "StrV"));
    Value *StrV = CastToCStr(SIB.CreateIntToPtr(StrVPtr,
                                                Type::getInt64Ty(C)->getPointerTo()),
                             SIB);
    
    // i8* @strncpy(i8*, i8*, i64)
    Type* StrncpyArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C), Type::getInt64Ty(C) };
    FunctionType *StrncpyTy = FunctionType::get(Type::getInt8PtrTy(C), StrncpyArgs, false);
    Function *StrncpyF = cast<Function>(M->getOrInsertFunction("strncpy", StrncpyTy));
    
    Value *StrLen = Strlen(StrV, M, SIB);
    Value *Length = SIB.CreateSub(StrLen, IntV, "Length"); // @TODO: Be sure that 0 <= |Length| <= |StrLen|
    Value *Size = SIB.CreateAdd(Length, SIB.getInt64(1));
    Value *StrPtr = SIB.CreateAlloca(Type::getInt8Ty(C), Size);
    SIB.CreateMemSet(StrPtr, SIB.getInt8(0), Size, 8);
    SIB.CreateCall3(StrncpyF, StrPtr, StrV, Length);
    
    SIB.CreateStore(SIB.CreatePtrToInt(StrPtr, Type::getInt64Ty(C)),
                    SIB.CreateStructGEP(ObjPtr, ObjectFieldData));
    
    SIB.CreateBr(DoneBB);
    
    // else if (string and string)
    IRBuilder<> SSB(SSBB);
    StrB.SetInsertPoint(SSBB);
    
    Value *RetPtr = Strxch(SSB.CreateIntToPtr(SSB.CreateLoad(LHSDataPtr),
                                              Type::getInt64Ty(C)->getPointerTo()),
                           SSB.CreateIntToPtr(SSB.CreateLoad(RHSDataPtr),
                                              Type::getInt64Ty(C)->getPointerTo()),
                           M, SSB);
    SSB.CreateStore(SSB.CreatePtrToInt(RetPtr, Type::getInt64Ty(C)),
                    SSB.CreateStructGEP(ObjPtr, ObjectFieldData));
    
    SSB.CreateBr(DoneBB);
    
    IRBuilder<> DoneB(DoneBB);
    StrB.SetInsertPoint(DoneBB);
    DoneB.CreateBr(EndBB);
  }
  else {
    
    /* (|LHSisInt| xor |RHSisInt|) must be true (a string *and* an integer) */
    BasicBlock *VTBB = BasicBlock::Create(C, "ValidTypesBlock", F);
    BasicBlock *ITBB = BasicBlock::Create(C, "InvalidTypesBlock", F);
    StrB.CreateCondBr(StrB.CreateXor(LHSisInt, RHSisInt), VTBB, ITBB);
    
    // Invalid operation (string and string)
    IRBuilder<> ITB(ITBB);
    StrB.SetInsertPoint(ITBB);
    
    // Throw a "SMILInvalidOperation" exception
    CreateInvalidBinopAssertion(M, ITB, this->line(), this->col());
    ITB.CreateBr(EndBB);
    
    // Valid operation
    IRBuilder<> VTB(VTBB);
    StrB.SetInsertPoint(VTBB);
    
    // %IntV = (|RHSisInt|) ? |RHSDataPtr| : |LHSDataPtr|
    Value *IntV = VTB.CreateLoad(VTB.CreateSelect(RHSisInt, RHSDataPtr, LHSDataPtr,
                                                  "IntV"));
    
    // %StrV = (|RHSisInt|) ? |LHSDataPtr| : |RHSDataPtr|
    Value *StrVPtr = VTB.CreateLoad(VTB.CreateSelect(RHSisInt, LHSDataPtr, RHSDataPtr,
                                                     "StrV"));
    Value *StrV = CastToCStr(VTB.CreateIntToPtr(StrVPtr,
                                                Type::getInt64Ty(C)->getPointerTo()),
                             VTB);
    
    Value *StrLen = Strlen(StrV, M, VTB);
    
    if (_op == tok_mul) { // string and integer
      
      /*
       * int rep = [IntV];
       * int l = [StrLen];
       * const char * s = [StrPtr];
       * int size = l * rep + 1;
       * char * souput = (char *)malloc(size);
       *
       * for (int i = 0; i < rep; i++) {
       * ~~strcpy(souput + (i * l), s);~~
       *   strcat(souput, s);
       * }
       */
      
      Value *TotalLen = VTB.CreateMul(StrLen, IntV, "TotalLen");
      Value *Size = VTB.CreateAdd(TotalLen, VTB.getInt64(1));
      Value *StrPtr = VTB.CreateAlloca(Type::getInt8Ty(C), Size);
      
      // i8* @strcpy(i8*, i8*)
      Type* StrcpyArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
      FunctionType *StrcpyTy = FunctionType::get(Type::getInt8PtrTy(C), StrcpyArgs, false /* not vararg */);
      Function *StrcpyF = cast<Function>(M->getOrInsertFunction("strcpy", StrcpyTy));
      
      /* Loop for concatenation */
      BasicBlock *LoopBB = BasicBlock::Create(C, "Loop", F);
      VTB.CreateBr(LoopBB);
      
      BasicBlock *DoneBB = BasicBlock::Create(C, "Done", F);
      
      VTB.SetInsertPoint(LoopBB);
      IRBuilder<> LoopB(LoopBB);
      
      PHINode *CounterPHI = LoopB.CreatePHI(Type::getInt64Ty(C), 2);
      CounterPHI->setName("counter");
      CounterPHI->addIncoming(LoopB.getInt64(0), VTBB);
      
      Value *OffsetV = LoopB.CreateAdd(LoopB.CreatePtrToInt(StrPtr, Type::getInt64Ty(C)),
                                       LoopB.CreateMul(CounterPHI, StrLen));
      LoopB.CreateCall2(StrcpyF,
                        LoopB.CreateIntToPtr(OffsetV, Type::getInt8PtrTy(C)),
                        LoopB.CreatePointerCast(StrV, Type::getInt8PtrTy(C)));
      
      Value *NextCounter = LoopB.CreateAdd(CounterPHI, LoopB.getInt64(1));
      NextCounter->setName("nextCounter");
      CounterPHI->addIncoming(NextCounter, LoopBB);
      
      Value *CompResult = LoopB.CreateICmpSLT(NextCounter, IntV);
      LoopB.CreateCondBr(CompResult, LoopBB, DoneBB);
      
      VTB.SetInsertPoint(DoneBB);
      IRBuilder<> DoneB(DoneBB);
      
      VTB.CreateStore(VTB.CreatePtrToInt(StrPtr, Type::getInt64Ty(C)),
                      VTB.CreateStructGEP(ObjPtr, ObjectFieldData));
      
      DoneB.CreateBr(EndBB);
    }
    else if (_op == tok_div) { // string and integer
      
      // Throw a "SMILDividedByZero" exception if |IntV| == 0
      static Value *GDiviseByZeroAssertMessage = NULL;
      if (!GDiviseByZeroAssertMessage) {
        GDiviseByZeroAssertMessage = VTB.CreateGlobalString("Can not divise by zero (SMILDividedByZero)",
                                                            "smil.divise.by.zero.assert.message");
      }
      Value *NEqZeroV = VTB.CreateICmpNE(IntV, VTB.getInt64(0)); // Assert(|IntV| != 0)
      CreateAssert(NEqZeroV, GDiviseByZeroAssertMessage,
                   M, VTB, this->line(), this->col());
      
      // i8* @strncpy(i8*, i8*, i64)
      Type* StrncpyArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C), Type::getInt64Ty(C) };
      FunctionType *StrncpyTy = FunctionType::get(Type::getInt8PtrTy(C), StrncpyArgs, false);
      Function *StrncpyF = cast<Function>(M->getOrInsertFunction("strncpy", StrncpyTy));
      
      // |Length| = ceil( len(|StrLen|) / |IntV| )
      Value *Length = VTB.CreateSDiv(StrLen, IntV, "Length"); // @TODO: Be sure that |IntV| > 0
      Value *Size = VTB.CreateAdd(Length, VTB.getInt64(1));
      Value *StrPtr = VTB.CreateAlloca(Type::getInt8Ty(C), Size);
      VTB.CreateMemSet(StrPtr, VTB.getInt8(0), Size, 8);
      VTB.CreateCall3(StrncpyF, StrPtr, StrV, Length);
      
      VTB.CreateStore(VTB.CreatePtrToInt(StrPtr, Type::getInt64Ty(C)),
                      VTB.CreateStructGEP(ObjPtr, ObjectFieldData));
      
      VTB.CreateBr(EndBB);
    }
    else if (_op == tok_mod) { // string and integer
      // @TODO: Add rotating to the left if |IntV| is negative
      /*
       int l = [StrLen];
       int offset = [IntV] % l;
       
       const char * sInput = [StrV];
       char * sOutput = [StrPtr];
       
       int len = (l - offset);
       memcpy(sOutput+offset, sInput, len);
       memcpy(sOutput, sInput+len, offset);
       */
      
      Value *Size = VTB.CreateAdd(StrLen, VTB.getInt64(1));
      Value *StrPtr = VTB.CreateAlloca(Type::getInt8Ty(C), Size);
      
      Value *OffsetV = VTB.CreateSRem(IntV, StrLen, "Offset");
      Value *LenV = VTB.CreateSub(StrLen, OffsetV, "Len");
      
      Value *StrVToInt = VTB.CreatePtrToInt(StrV, Type::getInt64Ty(C));
      Value *StrPtrToInt = VTB.CreatePtrToInt(StrPtr, Type::getInt64Ty(C));
      
      MemCpy(VTB.CreateIntToPtr(VTB.CreateAdd(StrPtrToInt, OffsetV),
                                Type::getInt8PtrTy(C)),
             StrV, LenV,
             M, VTB);
      MemCpy(StrPtr,
             VTB.CreateIntToPtr(VTB.CreateAdd(StrVToInt, LenV),
                                Type::getInt8PtrTy(C)),
             OffsetV,
             M, VTB);
      VTB.CreateStore(VTB.CreatePtrToInt(StrPtr, Type::getInt64Ty(C)),
                      VTB.CreateStructGEP(ObjPtr, ObjectFieldData));
      
      VTB.CreateBr(EndBB);
    }
    else {
      // Throw a "SMILInvalidOperation" exception
      CreateInvalidBinopAssertion(M, VTB, this->line(), this->col());
    }
  }
  
  B.SetInsertPoint(EndBB);
  return ObjPtr;
}

/*** Print Expression ***/
Value * PrintExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  vector<Type *> argsF;
  for (vector<Expr *>::iterator it = output.begin(); it != output.end(); it++) {
    argsF.push_back(getObjPtrTy(C));
  }
  // void @printN(%obj* [, %obj*]+)
  ArrayRef<Type *> ArgsRef = ArrayRef<Type *>(argsF);
  FunctionType *PrintTy = FunctionType::get(Type::getVoidTy(C), ArgsRef, false);
  
  // Generate the function name
  ostringstream ostr;
  ostr << "print" << (output.end() - output.begin());
  Function *PrintF = cast<Function>(M->getOrInsertFunction(ostr.str(), PrintTy));
  
  if (PrintF->empty()) {
    
    BasicBlock *FBB = BasicBlock::Create(C, "EntryBlock", PrintF);
    IRBuilder<> FB(FBB);
    FB.SetInsertPoint(FBB);
    
    // i32 @printf(i8*, ...)
    Type* PrintfArgs[] = { Type::getInt8PtrTy(C) };
    FunctionType *PrintfTy = FunctionType::get(Type::getInt32Ty(C), PrintfArgs, true);
    Function *PrintfF = cast<Function>(M->getOrInsertFunction("printf", PrintfTy));
    
    // i8* @strcat(i8*, i8*)
    Type* StrcatArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
    FunctionType *StrcatTy = FunctionType::get(Type::getInt8PtrTy(C), StrcatArgs, false);
    Function *StrcatF = cast<Function>(M->getOrInsertFunction("strcat", StrcatTy));
    
    static Value *GIntArgFormat = NULL;
    if (!GIntArgFormat) GIntArgFormat = B.CreateGlobalString("%lld ", "printf.format.arg.integer"); // DEBUG (remove whitespace)
    static Value *GStrArgFormat = NULL;
    if (!GStrArgFormat) GStrArgFormat = B.CreateGlobalString("\"%s\" ", "printf.format.arg.string"); // DEBUG (remove quotes and whitespace)
    static Value *GLBArgFormat = NULL;
    if (!GLBArgFormat) GLBArgFormat = B.CreateGlobalString("\n", "printf.format.arg.line-break");
    
    Value *Size = FB.getInt32(5 /* "%lld " */ * output.size() + 2 /* for "\n" */);
    Value *FormatPtr = FB.CreateAlloca(Type::getInt8Ty(C), Size, "printf.format");
    // Set the first byte to '\0' for the buffer string |FormatPtr|
    FB.CreateMemSet(FormatPtr, FB.getInt8(0), FB.getInt64(1), 8);
    
    vector<Value *> printfParams;
    for (Function::arg_iterator it = PrintF->arg_begin(); it != PrintF->arg_end(); it++) {
      
      Value *Arg = it;
      Value *FieldPtr = FB.CreateStructGEP(Arg, ObjectFieldType);
      Value *CompResult = FB.CreateICmpEQ(FB.CreateLoad(FieldPtr),
                                          FB.getInt1(ObjectTypeString));
      Value *ArgFormat = FB.CreateSelect(CompResult,
                                         CastToCStr(GStrArgFormat, FB),
                                         CastToCStr(GIntArgFormat, FB),
                                         "printf.format.arg");
      Value* Params[] = { FormatPtr, ArgFormat };
      FB.CreateCall(StrcatF, Params);
      
      Value *DataPtr = FB.CreateStructGEP(Arg, ObjectFieldData);
      printfParams.push_back(FB.CreateLoad(DataPtr));
    }
    
    /* Add "\n" at the end of |FormatPtr| */
    Value* StrcatParams[] = { FormatPtr, CastToCStr(GLBArgFormat, FB) };
    FB.CreateCall(StrcatF, StrcatParams);
    
    printfParams.insert(printfParams.begin(), FormatPtr);
    ArrayRef<Value *> PrintfParamsRef = ArrayRef<Value *>(printfParams);
    FB.CreateCall(PrintfF, PrintfParamsRef);
    
    FB.CreateRetVoid();
  }
  
  /* Call the function "print()" */
  vector<Value *> printParams;
  for (vector<Expr *>::iterator it = output.begin(); it != output.end(); it++) {
    Expr *E = cast<Expr>(*it); // @TODO: Check that |E| is an input, a variable or a binop
    Value *Ptr = E->CodeGen(M, B);
    printParams.push_back(Ptr);
  }
  ArrayRef<Value *> PrintParamsRef = ArrayRef<Value *>(printParams);
  B.CreateCall(PrintF, PrintParamsRef);
  
  return NULL;
}

/*** Hello Print Expression (print "Hello, {:$}!" if has an input, "Hello, World!" else) ***/
Value * HelloPrintExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  // i32 @printf(i8*, ...)
  Type* Args[] = { Type::getInt8PtrTy(C) };
  FunctionType *Type = FunctionType::get(Type::getInt32Ty(C), Args, true);
  Function *PrintfF = cast<Function>(M->getOrInsertFunction("printf", Type));
  
  static Value *GIntArgHelloFormat = NULL;
  if (!GIntArgHelloFormat) GIntArgHelloFormat = B.CreateGlobalString("Hello, %lld!\n", "hello.format.arg.integer");
  static Value *GStrArgHelloFormat = NULL;
  if (!GStrArgHelloFormat) GStrArgHelloFormat = B.CreateGlobalString("Hello, %s!\n", "hello.format.arg.string");
  
  Value *Input = InputAtIndex(0, M, B);
  if (Input) {
    
    Value *FieldPtr = B.CreateStructGEP(Input, ObjectFieldType);
    Value *CompResult = B.CreateICmpEQ(B.CreateLoad(FieldPtr),
                                       B.getInt1(ObjectTypeString));
    Value *ArgFormat = B.CreateSelect(CompResult,
                                      CastToCStr(GStrArgHelloFormat, B),
                                      CastToCStr(GIntArgHelloFormat, B),
                                      "printf.format.arg");
    B.CreateCall2(PrintfF, ArgFormat, B.CreateLoad(Input));
    
  } else {
    static Value *GHelloWorld = NULL;
    if (!GHelloWorld) GHelloWorld = B.CreateGlobalString("world", "hello.world");
    
    B.CreateCall2(PrintfF,
                  CastToCStr(GStrArgHelloFormat, B),
                  CastToCStr(GHelloWorld, B));
  }
  
  return NULL;
}

/*** No Operation Expression ***/
Value * NopExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  // @TODO: Use "@llvm.donothing":
  /* Function *F = Intrinsic::getDeclaration(M, Intrinsic::donothing);
   * InvokeInst::Create(F, II->getNormalDest(), II->getUnwindDest(), None, "", II->getParent());
   */
  
  return NULL;
}

/*** Exit (terminate) Expression ***/
Value * ExitExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  // void @exit(i32)
  Function *ExitF = cast<Function>(M->getOrInsertFunction("exit", Type::getVoidTy(C),
                                                          Type::getInt32Ty(C),
                                                          (Type *)0));
  Value *Zero = B.getInt32(code);
  return B.CreateCall(ExitF, Zero);
}

/*** Global Stack Variables ***/
static Value *__Stack = NULL; // Ptr to a stack of ptr to obj (cast as i64), i.e. i64**
static Value *__StackIdx = NULL;
static Value *__StackSize = NULL;

/*** Push (to global stack) Expression ***/
Value * PushExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  if (__Stack == NULL) {
#define kDefaultStackSize 16
    
    __Stack = B.CreateAlloca(Type::getInt64Ty(C)->getPointerTo());
    __Stack->setName("stack");
    Value *Stack = B.CreateAlloca(Type::getInt64Ty(C),
                                  B.getInt64(kDefaultStackSize));
    B.CreateStore(Stack, __Stack);
    
    __StackSize = B.CreateAlloca(Type::getInt64Ty(C));
    __StackSize->setName("stack.size");
    B.CreateStore(B.getInt64(kDefaultStackSize), __StackSize);
  }
  
  if (__StackIdx == NULL) {
    __StackIdx = B.CreateAlloca(Type::getInt64Ty(C));
    __StackIdx->setName("stack.index");
    B.CreateStore(B.getInt64(0), __StackIdx);
  }
  
  Value *Idx = B.CreateLoad(__StackIdx);
  
  Function *F = B.GetInsertBlock()->getParent();
  BasicBlock *RSBB = BasicBlock::Create(C, "ResizeStackBlock", F);
  BasicBlock *DoneBB = BasicBlock::Create(C, "DoneBlock", F);
  
  Value *ResGT = B.CreateICmpSGE(Idx, B.CreateLoad(__StackSize)); // If |Idx| >= |__StackSize|
  B.CreateCondBr(ResGT, RSBB, DoneBB);
  
  B.SetInsertPoint(RSBB);
  IRBuilder<> RSB(RSBB);
  
  // Add |kDefaultStackSize| to the stack size
  Value *NewSize = RSB.CreateAdd(Idx, RSB.getInt64(kDefaultStackSize));
  
#undef kDefaultStackSize
  
  // Alloc a new stack and copy from the old stack
  Value *NewStack = RSB.CreateAlloca(Type::getInt64Ty(C),
                                     NewSize);
  MemCpy(NewStack, RSB.CreateLoad(__Stack),
         RSB.CreateMul(RSB.CreateLoad(__StackSize), B.getInt64(8)), // Copy |__StackSize| * 8 bytes of memory
         M, RSB);
  RSB.CreateStore(NewStack, __Stack);
  
  RSB.CreateStore(NewSize, __StackSize);
  
  RSB.CreateBr(DoneBB);
  B.SetInsertPoint(DoneBB);
  
  
  Value *Ptr = B.CreateLoad(__Stack);// |_Stack| -> (i64*)*, |Ptr| -> i64*
  Value *Addr = B.CreateAdd(B.CreatePtrToInt(Ptr, Type::getInt64Ty(C)),
                            B.CreateMul(Idx, B.getInt64(8 /* 64 bits */)));
  Value *FinalPtr = B.CreateIntToPtr(Addr,
                                     Type::getInt64Ty(C)->getPointerTo());
  
  Value *V = _expr->CodeGen(M, B);
  B.CreateStore(B.CreatePtrToInt(V, Type::getInt64Ty(C)),
                FinalPtr);
  
  // Increment |__StackIdx|
  Value *NewIdx = B.CreateAdd(Idx, B.getInt64(1));
  B.CreateStore(NewIdx, __StackIdx);
  
  return NULL;
}

/*** Pop (from global stack) Expression ***/
Value * PopExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  Value *Idx = B.CreateLoad(__StackIdx);
  Value *NewIdx = B.CreateSub(Idx, B.getInt64(1));
  B.CreateStore(NewIdx, __StackIdx);
  
  LLVMContext &C = M->getContext();
  
  Value *Ptr = B.CreateLoad(__Stack);
  Value *Addr = B.CreateAdd(B.CreatePtrToInt(Ptr,
                                             Type::getInt64Ty(C)),
                            B.CreateMul(NewIdx, B.getInt64(8)));
  Value *FinalPtr = B.CreateIntToPtr(Addr,
                                     Type::getInt64Ty(C)->getPointerTo());
  
  Value *V = B.CreateIntToPtr(B.CreateLoad(FinalPtr),
                              getObjPtrTy(C));
  
  string name = (cast<VarExpr>(_expr))->getName();
  InsertOrUpdate(CxxStrToVal(name, M, B),
                 V, M, B);
  
  return V;
}

/*** Clear Global Stack Expression ***/
Value * ClearExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  B.CreateStore(B.getInt64(0), __StackIdx);
  // @TODO: Free stack (?)
  return NULL;
}

/*** Loop Expression ***/
Value * LoopExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  Function *F = B.GetInsertBlock()->getParent();
  
  /* Condition Block */
  BasicBlock *ConditionBB = BasicBlock::Create(C, "ConditionBlock", F);
  B.CreateBr(ConditionBB);
  IRBuilder<> ConditionB(ConditionBB);
  
  BasicBlock *ThenBB = BasicBlock::Create(C, "ThenBlock", F);
  BasicBlock *ThelseBB = BasicBlock::Create(C, "ThelseBlock", F);
  BasicBlock *EndBB = BasicBlock::Create(C, "EndBlock", F);
  
  // @TODO: Compare with |CreateFCmp[O|U]GT()|
  Value *ICond = ObjToInt64(_conditionExpr->CodeGen(M, ConditionB), M, ConditionB);
  ICond->setName("ICond");
  Value *CompResult = ConditionB.CreateICmpSGT(ICond, ConditionB.getInt64(0)); // Signed Int Comp Greater Than
  CompResult->setName("CompResult");
  ConditionB.CreateCondBr(CompResult, ThenBB, ThelseBB);
  
  /* Then Block */
  B.SetInsertPoint(ThenBB);
  IRBuilder<> ThenB(ThenBB);
  for (vector<Expr *>::iterator it = _thenExprs.begin(); it != _thenExprs.end(); it++) {
    Expr *expr = (*it);
    if (canGen(expr)) {
      out() << "Generating code into then block for: " << expr->DebugString() << "\n";
      expr->CodeGen(M, ThenB);
    }
  }
  Value *ThenICond = ObjToInt64(_conditionExpr->CodeGen(M, ThenB), M, ThenB);
  ThenICond->setName("ThenICond");
  Value *ThenCompResult = ThenB.CreateICmpSGT(ThenICond, ThenB.getInt64(0)); // Signed Int Comp Greater Than
  ThenCompResult->setName("ThenCompResult");
  ThenB.CreateCondBr(ThenCompResult, ThenBB, EndBB);
  
  /* Thelse Block */
  B.SetInsertPoint(ThelseBB);
  IRBuilder<> ThelseB(ThelseBB);
  for (vector<Expr *>::iterator it = _thelseExprs.begin(); it != _thelseExprs.end(); it++) {
    Expr *expr = (*it);
    if (canGen(expr)) {
      out() << "Generating code into thelse block for: " << expr->DebugString() << "\n";
      expr->CodeGen(M, ThelseB);
    }
  }
  ThelseB.CreateBr(EndBB);
  
  B.SetInsertPoint(EndBB);
  return NULL;
}

/*** Unkown Expression ***/
Value * LengthFuncExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  Value *V = _expr->CodeGen(M, B);
  Value *Str = ObjToStr(V, M, B);
  
  Value *NewPtr = B.CreateAlloca(getObjTy(C));
  B.CreateStore(Strlen(Str, M, B),
                B.CreateStructGEP(NewPtr, ObjectFieldData));
  B.CreateStore(B.getInt1(ObjectTypeInteger),
                B.CreateStructGEP(NewPtr, ObjectFieldType));
  return NewPtr;
}

/*** Unkown Expression ***/
Value * UnkownExpr::CodeGen(Module *M, IRBuilder<> &B)
{
  return NULL;
}
