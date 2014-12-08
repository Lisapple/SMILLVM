#include "ObjectType.h"

StructType * getObjTy(LLVMContext &C)
{
  static StructType *Ty = NULL;
  if (!Ty) {
    /* struct obj { long int data; int type:1; }; */
    Ty = StructType::create("obj",
                            Type::getInt64Ty(C),
                            Type::getInt1Ty(C), NULL);
  }
  return Ty;
}

PointerType * getObjPtrTy(LLVMContext &C)
{
  static PointerType *PtrTy = NULL;
  if (!PtrTy) {
    PtrTy = getObjTy(C)->getPointerTo();
  }
  return PtrTy;
}

/* Return the size (in bytes) of the "obj" type */
unsigned ObjectTypeSize(LLVMContext &C)
{
  static unsigned Size = 0;
  if (Size == 0) {
    StructType *STy = getObjTy(C);
    int NumElements = STy->getNumElements();
    for (int i = 0; i < NumElements; i++)
      Size += STy->getElementType(i)->getScalarSizeInBits();
    Size = ceilf(Size / 8.);
  }
  return Size;
}