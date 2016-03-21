#ifndef SMIL_OBJECT_TYPE_H
#define SMIL_OBJECT_TYPE_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"

using namespace llvm;

enum ObjectField {
  ObjectFieldData = 0, // String or Integer Field (ptr: long int (Int64))
  ObjectFieldType // Type (type: int:1 (ObjectType))
};

enum ObjectType {
  ObjectTypeString = 0, // String data
  ObjectTypeInteger // Integer data
};

StructType * getObjTy(LLVMContext &C);
PointerType * getObjPtrTy(LLVMContext &C);

/* Return the size (in bytes) of the "obj" type */
unsigned ObjectTypeSize(LLVMContext &C);

#endif // SMIL_OBJECT_TYPE_H