#include "HashTable.h"

// i32 @hash(i8* %str)
Value *Hash(Value *Str, Module *M, IRBuilder<> &B)
{
  /* unsigned long hash(const char * s) {
   *   unsigned long hash = 0;
   *   char c;
   *   while (c = *s++) {
   *     hash = c + (hash << 6) + (hash << 16) - hash;
   *   }
   *   return hash % kBucketSize;
   * }
   */
  
  /* This uses the SDBM hash (http://www.cse.yorku.ca/~oz/hash.html#sdbm) */
  
  LLVMContext &C = M->getContext();
  Function *HashF = cast<Function>(M->getOrInsertFunction("hash", Type::getInt32Ty(C),
                                                          Type::getInt8PtrTy(C),
                                                          (Type *)0));
  if (HashF->empty()) {
    
    Argument *StrArg = HashF->arg_begin();
    StrArg->setName("str");
    
    BasicBlock *HashBB = BasicBlock::Create(C, "EntryBlock", HashF);
    IRBuilder<> HashB(HashBB);
    HashB.SetInsertPoint(HashBB);
    
    Value *HPtr = HashB.CreateAlloca(Type::getInt32Ty(C));
    HPtr->setName("hashPtr");
    HashB.CreateStore(HashB.getInt32(0), HPtr);
    
    Value *IPtr = HashB.CreateAlloca(Type::getInt32Ty(C));
    IPtr->setName("indexPtr");
    HashB.CreateStore(HashB.getInt32(0), IPtr);
    
    BasicBlock *LoopBB = BasicBlock::Create(C, "Loop", HashF);
    HashB.CreateBr(LoopBB);
    
    IRBuilder<> LoopB(LoopBB);
    HashB.SetInsertPoint(LoopBB);
    
    BasicBlock *DoneBB = BasicBlock::Create(C, "Done", HashF);
    
    Value *Index = LoopB.CreateLoad(IPtr);
    Value *Char = LoopB.CreateLoad(LoopB.CreateGEP(StrArg, Index));
    
    Value *Hash = LoopB.CreateLoad(HPtr);
    Value *Char32 = LoopB.CreateIntCast(Char, Type::getInt32Ty(C), false);
    /* hash = c + (hash << 6) + (hash << 16) - hash; */
    Value *Add1 = LoopB.CreateAdd(Char32,
                                  LoopB.CreateShl(Hash, 6));
    Value *Add2 = LoopB.CreateAdd(Add1,
                                  LoopB.CreateShl(Hash, 16));
    LoopB.CreateStore(LoopB.CreateSub(Add2, Hash),
                      HPtr);
    
    // Increment Index (string pointer offset)
    Value *Add = LoopB.CreateAdd(Index,
                                 LoopB.getInt32(1));
    LoopB.CreateStore(Add, IPtr);
    
    // Loop until |Char| equals to zero
    Value *Res = LoopB.CreateICmpEQ(Char, LoopB.getInt8(0));
    LoopB.CreateCondBr(Res, DoneBB, LoopBB);
    
    IRBuilder<> DoneB(DoneBB);
    HashB.SetInsertPoint(DoneBB);
    
    // Apply modulus (|Hash| % kBucketCount)
    Value *FinalHash = DoneB.CreateURem(DoneB.CreateLoad(HPtr),
                                        DoneB.getInt32(kBucketCount));
    DoneB.CreateRet(FinalHash);
  }
  
  // Call hash function
  return B.CreateCall(HashF,
                      CastToCStr(Str, B));
}

StructType * BucketType(LLVMContext &C)
{
  static StructType *BucketType = NULL;
  if (!BucketType) {
    // struct bucket { i8 ** keys; %obj ** values; i32 size; i32 max_size; };
    BucketType = StructType::create("bucket",
                                    Type::getInt8PtrTy(C)->getPointerTo(),
                                    getObjPtrTy(C)->getPointerTo(),
                                    Type::getInt32Ty(C),
                                    Type::getInt32Ty(C), NULL);
  }
  return BucketType;
}

static GlobalVariable *__Map = NULL;

GlobalVariable * InitVarTable(Module *M)
{
  LLVMContext &C = M->getContext();
  ArrayType * Ty = ArrayType::get(BucketType(C), kBucketCount);
  vector<Constant *> constants;
  for (int i = 0; i < kBucketCount; i++) {
    Constant *Zero = ConstantInt::get(Type::getInt32Ty(C), 0);
    Constant *NullPtr = ConstantPointerNull::get(Type::getInt8PtrTy(C)->getPointerTo()); // %i8**
    Constant *NullObjPtr = ConstantPointerNull::get(getObjPtrTy(C)->getPointerTo()); // %obj**
    constants.push_back(ConstantStruct::get(BucketType(C),
                                            (Constant *[]) { NullPtr, NullObjPtr, Zero, Zero }));
  }
  ArrayRef<Constant *> Constants = ArrayRef<Constant *>(constants);
  Constant *InitPtr = ConstantArray::get(Ty, Constants);
  
  __Map = new GlobalVariable(*M, Ty, false /* non-constant */,
                             GlobalValue::WeakAnyLinkage, // Keep one copy of named function when linking (weak)
                             InitPtr, "_map");
  return __Map;
}

// void @upsize(%bucket* %b)
void Upsize(Value *BucketPtr, Module *M, IRBuilder<> &B)
{
  /* void upsize(struct str_array * arr) {
   *   arr->max_size += kArrayBaseSize;
   
   *   int size = arr->size * sizeof(char *);
   *   int new_size = arr->max_size * sizeof(char *);
   
   *   char ** new_keys = (char **)malloc(new_size);
   *   memcpy(new_keys, arr->keys, size);
   *   arr->keys = new_keys;
   
   *   char ** new_values = (char **)malloc(new_size);
   *   memcpy(new_values, arr->values, size);
   *   arr->values = new_values;
   * }
   */
  
  LLVMContext &C = M->getContext();
  
  // void @upsize(%bucket* %b)
  Function *UpsizeF = cast<Function>(M->getOrInsertFunction("upsize", Type::getVoidTy(C),
                                                            BucketType(C)->getPointerTo(),
                                                            (Type *)0));
  if (UpsizeF->empty()) {
    Argument *BArg = UpsizeF->arg_begin();
    BArg->setName("b");
    
    BasicBlock *UpBB = BasicBlock::Create(C, "EntryBlock", UpsizeF);
    IRBuilder<> UpB(UpBB);
    UpB.SetInsertPoint(UpBB);
    
    // arr->max_size += kArrayBaseSize;
    Value *MaxSizePtr = UpB.CreateStructGEP(BArg, BucketFieldMaxSize);
    Value *Add = UpB.CreateAdd(UpB.CreateLoad(MaxSizePtr),
                               UpB.getInt32(kBucketDefaultSize));
    UpB.CreateStore(Add, MaxSizePtr);
    
    // int new_size = arr->max_size * sizeof(char *);
    Value *NewSize = UpB.CreateMul(Add,
                                   UpB.getInt32(kBucketDefaultSize));
    
    // i8* @realloc(i8*, i64)
    Type* ReallocArgs[] = { Type::getInt8PtrTy(C), Type::getInt64Ty(C) };
    FunctionType *ReallocTy = FunctionType::get(Type::getInt8PtrTy(C), ReallocArgs, false);
    Function *ReallocF = cast<Function>(M->getOrInsertFunction("realloc", ReallocTy));
    
    // char ** new_keys = (char **)malloc(new_size);
    // memcpy(new_keys, arr->keys, size);
    // arr->keys = new_keys;
    Value * AllocSize = UpB.CreateMul(UpB.getInt64(8 /* sizeof(char *) */),
                                      UpB.CreateIntCast(NewSize, Type::getInt64Ty(C), false));
    Value *KeysPtr = UpB.CreateStructGEP(BArg, BucketFieldKeys); // |KeysPtr| : i8***
    
    // |NewKeysPtr| : [|NewSize| x i8*] = i8**
    Value * NewKeysPtr = UpB.CreateCall2(ReallocF,
                                         UpB.CreatePointerCast(UpB.CreateLoad(KeysPtr),
                                                               Type::getInt8PtrTy(C)),
                                         AllocSize);
    UpB.CreateStore(UpB.CreatePointerCast(NewKeysPtr, Type::getInt8PtrTy(C)->getPointerTo()),
                    KeysPtr);
    
    // obj * new_values = (obj *)malloc(new_size);
    // memcpy(new_values, arr->values, size);
    // arr->values = new_values;
    Value *ValuesPtr = UpB.CreateStructGEP(BArg, BucketFieldValues); // |ValuesPtr| : %obj***
    
    // |NewKeysPtr| : [|NewSize| x %obj*] = %obj**
    Value * NewValuesPtr = UpB.CreateCall2(ReallocF,
                                           UpB.CreatePointerCast(UpB.CreateLoad(ValuesPtr),
                                                                 Type::getInt8PtrTy(C)),
                                           AllocSize);
    UpB.CreateStore(UpB.CreatePointerCast(NewValuesPtr, getObjPtrTy(C)->getPointerTo()),
                    ValuesPtr);
    
    UpB.CreateRetVoid();
  }
  
  // Call upsize function
  B.CreateCall(UpsizeF, BucketPtr);
}

// void @insert(i8* %key, %obj* %value)
void Insert(Value *Key, Value *Val, Module *M, IRBuilder<> &B)
{
  /*
   * void insert(const char * key, const char * value) {
   *   int h = hash(key);
   *   struct str_array * arr = &_map[h];
   *
   *   if (arr->size > arr->max_size || arr->max_size == 0) {
   *     upsize(arr);
   *   }
   *
   *   int size = strlen(key) + 1;
   *   arr->keys[arr->size] = (char *)malloc(size);
   *   memcpy(arr->keys[arr->size], key, size);
   *
   *   size = strlen(value) + 1;
   *   arr->values[arr->size] = (char *)malloc(size);
   *   memcpy(arr->values[arr->size], value, size);
   *
   *   arr->size++;
   * }
   */
  
  LLVMContext &C = M->getContext();
  
  // void @insert(i8* %key, obj* %value)
  Function *InsertF = cast<Function>(M->getOrInsertFunction("insert", Type::getVoidTy(C),
                                                            Type::getInt8PtrTy(C),
                                                            getObjPtrTy(C),
                                                            (Type *)0));
  if (InsertF->empty()) {
    Function::arg_iterator it = InsertF->arg_begin();
    Argument *KArg = it;
    KArg->setName("key");
    
    Argument *VArg = ++it;
    VArg->setName("value");
    
    BasicBlock *IBB = BasicBlock::Create(C, "EntryBlock", InsertF);
    IRBuilder<> IB(IBB);
    IB.SetInsertPoint(IBB);
    
    // int h = hash(key);
    Value *HashV = Hash(KArg, M, IB);
    HashV->setName("hash");
    
    // struct str_array * arr = &_map[h];
    Value *MapPtr = IB.CreatePointerCast(__Map,
                                         BucketType(C)->getPointerTo()); // Cast "[11 x %bucket]" to "%bucket*" to compute a correct offset with GEP
    Value *BucketPtr = IB.CreateGEP(MapPtr, HashV);
    
    // if (arr->size > arr->max_size || arr->max_size == 0) {
    //   upsize(arr);
    // }
    Value *SizePtr = IB.CreateStructGEP(BucketPtr, BucketFieldSize);
    Value *MaxSizePtr = IB.CreateStructGEP(BucketPtr, BucketFieldMaxSize);
    
    Value *Cond1 = IB.CreateICmpSGT(IB.CreateLoad(SizePtr), IB.CreateLoad(MaxSizePtr));
    Value *Cond2 = IB.CreateICmpEQ(IB.CreateLoad(MaxSizePtr), IB.getInt32(0));
    
    BasicBlock *UpBB = BasicBlock::Create(C, "UpsizeBlock", InsertF);
    BasicBlock *MCBB = BasicBlock::Create(C, "MemCpyBlock", InsertF);
    IB.CreateCondBr(IB.CreateOr(Cond1, Cond2), UpBB, MCBB);
    
    /* Upsize block*/
    IRBuilder<> UpB(UpBB);
    IB.SetInsertPoint(UpBB);
    
    Upsize(BucketPtr, M, UpB);
    UpB.CreateBr(MCBB);
    
    /* Memcpy block */
    IRBuilder<> MCB(MCBB);
    IB.SetInsertPoint(MCBB);
    
    Value *Size = MCB.CreateLoad(SizePtr);
    
    // int size = strlen(key) + 1;
    // arr->keys[arr->size] = (char *)malloc(size);
    // memcpy(arr->keys[arr->size], key, size);
    Value *KeysPtr = MCB.CreateStructGEP(BucketPtr, BucketFieldKeys); // |KeysPtr| : i8***
    Value *KeyPtr = MCB.CreateGEP(MCB.CreateLoad(KeysPtr), Size); // |KeyPtr| : i8**
    MCB.CreateStore(KArg, KeyPtr);
    
    // size = strlen(value) + 1;
    // arr->values[arr->size] = (%obj *)malloc(size);
    // memcpy(arr->values[arr->size], value, size);
    Value *ValuesPtr = MCB.CreateStructGEP(BucketPtr, BucketFieldValues); // |ValuesPtr| : %obj***
    Value *ValuePtr = MCB.CreateGEP(MCB.CreateLoad(ValuesPtr), Size); // |ValuePtr| : %obj**
    MCB.CreateStore(VArg, ValuePtr);
    
    // arr->size++;
    MCB.CreateStore(MCB.CreateAdd(Size, MCB.getInt32(1)),
                    SizePtr);
    
    MCB.CreateRetVoid();
  }
  
  // Call insert function
  B.CreateCall2(InsertF, Key, Val);
}

// %obj* @getptr(i8* %key)
Value * GetPtr(Value *Key, Module *M, IRBuilder<> &B)
{
  /*
   * char ** get(const char * key)
   * {
   *   int h = hash(key);
   *   struct str_array * arr = &_map[h];
   *   int i = 0;
   *   while(1) {
   *     if (i>= arr->size)
   *       break;
   *
   *     char * s = arr->keys[i];
   *     if (strcmp(s, key) == 0)
   *       return &arr->values[i];
   *     i++;
   *   }
   *   return NULL;
   * }
   */
  
  LLVMContext &C = M->getContext();
  
  // %obj* @getptr(i8* %key)
  Function *GetF = cast<Function>(M->getOrInsertFunction("getptr", getObjPtrTy(C),
                                                         Type::getInt8PtrTy(C),
                                                         (Type *)0));
  if (GetF->empty()) {
    Argument *KArg = GetF->arg_begin();
    KArg->setName("key");
    
    BasicBlock *GetBB = BasicBlock::Create(C, "EntryBlock", GetF);
    IRBuilder<> GetB(GetBB);
    GetB.SetInsertPoint(GetBB);
    
    // int h = hash(key);
    Value *HashV = Hash(KArg, M, GetB);
    HashV->setName("hash");
    
    // struct str_array * arr = &_map[h];
    Value *MapPtr = GetB.CreatePointerCast(__Map,
                                           BucketType(C)->getPointerTo());
    Value *BucketPtr = GetB.CreateGEP(MapPtr, HashV);
    Value *SizePtr = GetB.CreateStructGEP(BucketPtr, BucketFieldSize);
    
    Value *CounterPtr = GetB.CreateAlloca(Type::getInt32Ty(C));
    CounterPtr->setName("counterPtr");
    GetB.CreateStore(GetB.getInt32(0), CounterPtr);
    
    
    BasicBlock *LoopBB = BasicBlock::Create(C, "Loop", GetF);
    GetB.CreateBr(LoopBB);
    
    BasicBlock *Loop2BB = BasicBlock::Create(C, "Loop2", GetF);
    BasicBlock *Loop3BB = BasicBlock::Create(C, "Loop3", GetF);
    
    BasicBlock *RetValueBB = BasicBlock::Create(C, "RetValue", GetF);
    BasicBlock *RetNullBB = BasicBlock::Create(C, "RetNull", GetF);
    
    /*
     * Loop:
     *   br (counter >= size), RetNull, Loop2
     *
     * Loop2:
     *   br (strcmp(s, key), 0), RetValue, Loop3
     *
     * Loop3:
     *   counter++
     *   br Loop
     *
     * RetValue:
     *   ret value
     *
     * RetNull:
     *   ret void
     */
    
    /* Loop Block */
    IRBuilder<> LoopB(LoopBB);
    GetB.SetInsertPoint(LoopBB);
    
    // if (i >= arr->size)
    //   break;
    
    Value *Counter = LoopB.CreateLoad(CounterPtr);
    Value *Cmp = LoopB.CreateICmpSGE(Counter,
                                     LoopB.CreateLoad(SizePtr));
    LoopB.CreateCondBr(Cmp, RetNullBB, Loop2BB);
    
    /* Loop2 Block */
    IRBuilder<> Loop2B(Loop2BB);
    GetB.SetInsertPoint(Loop2BB);
    
    // char * s = arr->keys[i];
    // if (strcmp(s, key) == 0)
    //   return arr->values[i];
    
    Value *KeysPtr = Loop2B.CreateStructGEP(BucketPtr, BucketFieldKeys); // |KeysPtr| : i8***
    Value *KeyPtr = Loop2B.CreateGEP(Loop2B.CreateLoad(KeysPtr),
                                     Counter); // |KeyPtr| : i8**
    
    // i32 @strcmp(i8*, i8*)
    Type* StrcmpArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
    FunctionType *StrcmpTy = FunctionType::get(Type::getInt32Ty(C), StrcmpArgs, false);
    Function *StrcmpF = cast<Function>(M->getOrInsertFunction("strcmp", StrcmpTy));
    
    Value *Ret = Loop2B.CreateCall2(StrcmpF, Loop2B.CreateLoad(KeyPtr), KArg);
    Value *Res = Loop2B.CreateICmpEQ(Ret, Loop2B.getInt32(0));
    Loop2B.CreateCondBr(Res, RetValueBB, Loop3BB);
    
    /* Loop3 Block */
    IRBuilder<> Loop3B(Loop3BB);
    GetB.SetInsertPoint(Loop3BB);
    
    // i++;
    Value *NewCounter = Loop3B.CreateAdd(Counter,
                                         Loop3B.getInt32(1));
    Loop3B.CreateStore(NewCounter, CounterPtr);
    Loop3B.CreateBr(LoopBB);
    
    // return &arr->values[i];
    IRBuilder<> RVB(RetValueBB);
    GetB.SetInsertPoint(RetValueBB);
    
    Value *ValuesPtr = RVB.CreateStructGEP(BucketPtr, BucketFieldValues); // |ValuesPtr| : %obj***
    Value *ValuePtr = RVB.CreateGEP(RVB.CreateLoad(ValuesPtr),
                                    Counter); // |ValuePtr| : %obj**
    RVB.CreateRet(RVB.CreateLoad(ValuePtr));
    
    // return (%obj *)NULL;
    IRBuilder<> RNB(RetNullBB);
    GetB.SetInsertPoint(RetNullBB);
    RNB.CreateRet(ConstantPointerNull::get(getObjPtrTy(C)));
  }
  
  // Call getptr function
  return B.CreateCall(GetF, Key);
}

// void @update(i8* %key, %obj* val)
void Update(Value *Key, Value *Val, Module *M, IRBuilder<> &B)
{
  Value *ValPtr = GetPtr(Key, M, B); // |ValPtr| : i8**
  B.CreateStore(Val, ValPtr);
}

// void @insertorupdate(i8* %key, %obj* val)
void InsertOrUpdate(Value *Key, Value *Val, Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  Function *InsOrUpF = cast<Function>(M->getOrInsertFunction("insertorupdate", Type::getVoidTy(C),
                                                             Type::getInt8PtrTy(C),
                                                             getObjPtrTy(C),
                                                             (Type *)0));
  if (InsOrUpF->empty()) {
    Function::arg_iterator it = InsOrUpF->arg_begin();
    Argument *KArg = it;
    KArg->setName("key");
    
    Argument *VArg = ++it;
    VArg->setName("value");
    
    BasicBlock *IUBB = BasicBlock::Create(C, "EntryBlock", InsOrUpF);
    IRBuilder<> IUB(IUBB);
    IUB.SetInsertPoint(IUBB);
    
    Value *ValPtr = GetPtr(KArg, M, IUB); // |ValPtr| : %obj*
    
    BasicBlock *InsertBB = BasicBlock::Create(C, "InsertBlock", InsOrUpF);
    BasicBlock *UpdateBB = BasicBlock::Create(C, "UpdateBlock", InsOrUpF);
    BasicBlock *DoneBB = BasicBlock::Create(C, "DoneBlock", InsOrUpF);
    
    Value *Cond = IUB.CreateICmpEQ(IUB.CreatePtrToInt(ValPtr, Type::getInt32Ty(C)),
                                   IUB.getInt32(0)); // |ValPtr| =?= NULL
    IUB.CreateCondBr(Cond, InsertBB, UpdateBB);
    
    /* Insert block */
    IRBuilder<> IB(InsertBB);
    IUB.SetInsertPoint(InsertBB);
    
    Insert(KArg, VArg, M, IB);
    IB.CreateBr(DoneBB);
    
    /* Update block */
    IRBuilder<> UB(UpdateBB);
    IUB.SetInsertPoint(UpdateBB);
    
    UB.CreateStore(UB.CreateLoad(VArg),
                   ValPtr);
    UB.CreateBr(DoneBB);
    
    /* Done block */
    IRBuilder<> DoneB(DoneBB);
    IUB.SetInsertPoint(DoneBB);
    
    DoneB.CreateRetVoid();
  }
  
  // Call "insertorupdate" function
  B.CreateCall2(InsOrUpF, Key, Val);
}

// %obj* @getptrorinsert(i8* %key)
Value * GetPtrOrInsert(Value *Key, Module *M, IRBuilder<> &B)
{
  LLVMContext &C = M->getContext();
  
  Function *GetOrCrF = cast<Function>(M->getOrInsertFunction("getptrorinsert", getObjPtrTy(C),
                                                             Type::getInt8PtrTy(C),
                                                             (Type *)0));
  if (GetOrCrF->empty()) {
    Argument *KArg = GetOrCrF->arg_begin();
    KArg->setName("key");
    
    BasicBlock *GCBB = BasicBlock::Create(C, "EntryBlock", GetOrCrF);
    IRBuilder<> GCB(GCBB);
    GCB.SetInsertPoint(GCBB);
    
    Value *ValPtr = GetPtr(KArg, M, GCB); // |ValPtr| : %obj*
    
    BasicBlock *GetPtrBB = BasicBlock::Create(C, "GetPtrBlock", GetOrCrF);
    BasicBlock *InsertBB = BasicBlock::Create(C, "InsertBlock", GetOrCrF);
    
    // (|ValPtr| != NULL) ? (br GetPtrBlock) : (br InsertBlock)
    Value *ICmpSGT = GCB.CreateICmpSGT(GCB.CreatePtrToInt(ValPtr, Type::getInt32Ty(C)),
                                    GCB.getInt32(0));
    GCB.CreateCondBr(ICmpSGT, GetPtrBB, InsertBB);
    
    /* Get Pointer block */
    IRBuilder<> GPB(GetPtrBB);
    GPB.SetInsertPoint(GetPtrBB);
    
    GPB.CreateRet(ValPtr);
    
    /* Insert block */
    IRBuilder<> InsB(InsertBB);
    InsB.SetInsertPoint(InsertBB);
    
    // i8* @malloc(i64)
    FunctionType *MallocTy = FunctionType::get(Type::getInt8PtrTy(C),
                                               (Type *[]) { Type::getInt64Ty(C) }, false);
    Function *MallocF = cast<Function>(M->getOrInsertFunction("malloc", MallocTy));
    Value *AllocPtr = InsB.CreateCall(MallocF,
                                    InsB.getInt64(ObjectTypeSize(C))); // |AllocPtr| : i8*
    Value *NewPtr = InsB.CreatePointerCast(AllocPtr, getObjPtrTy(C)); // |NewPtr| : %obj*
    
    // Init variable to zero
    InsB.CreateStore(InsB.getInt64(0),
                     InsB.CreateStructGEP(NewPtr, ObjectFieldData));
    InsB.CreateStore(InsB.getInt1(ObjectTypeInteger),
                     InsB.CreateStructGEP(NewPtr, ObjectFieldType));
    Insert(KArg, NewPtr, M, InsB);
    
    InsB.CreateRet(NewPtr);
    
    // @TODO: Use a "phi" for |ValPtr|/|NewPtr|
  }
  
  // Call "getptrorcreate" function
  return B.CreateCall(GetOrCrF, Key);
}
