#ifndef SMIL_HASH_H
#define SMIL_HASH_H

#include "llvm/Transforms/Utils/BuildLibCalls.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "ObjectType.h"

using namespace std;
using namespace llvm;

#define kBucketCount       11
#define kBucketDefaultSize 10

enum BucketField {
  BucketFieldKeys = 0, // Array of string (char *)
  BucketFieldValues, // Array of obj*
  BucketFieldSize, // Number of item in array (integer)
  BucketFieldMaxSize // Allocated size for array (integer)
};

StructType * BucketType(LLVMContext &C);

//void InitVarTable(Module *M);
GlobalVariable * InitVarTable(Module *M);

// void @insert(i8* %key, %obj* %value)
void Insert(Value *Key, Value *Val, Module *M, IRBuilder<> &B);

/// Private
// %obj* @getptr(i8* %key)
Value * GetPtr(Value *Key, Module *M, IRBuilder<> &B);

// void @update(i8* %key, %obj* val)
void Update(Value *Key, Value *Val, Module *M, IRBuilder<> &B);

// void @insertorupdate(i8* %key, %obj* val)
void InsertOrUpdate(Value *Key, Value *Val, Module *M, IRBuilder<> &B);

// %obj* @getptrorinsert(i8* %key)
Value * GetPtrOrInsert(Value *Key, Module *M, IRBuilder<> &B);

#endif // SMIL_HASH_H