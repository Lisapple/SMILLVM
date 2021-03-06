#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <stack>
#include <list>
#include <map>

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"

#define __MCJIT__ 1 // Use MCJIT (required for LLVM 3.6+, JIT is deprecated since LLVM 3.5)
#if __MCJIT__
#  include "llvm/ExecutionEngine/SectionMemoryManager.h"
#  include "llvm/ExecutionEngine/MCJIT.h"
#else
#  include "llvm/ExecutionEngine/JIT.h"
#endif

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Host.h"

#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/PassManager.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include "Parser.h"
#include "Token.h"
#include "ObjectType.h"
#include "Expr.h"
#include "CodeGen.h"
#include "HashTable.h"
#include "Utilities.h"

using namespace std;
using namespace llvm;

bool parseBoolArg(char **argv[], int *argc, const char *flag)
{
  bool flagExists = false;
  for (int i = 0; i < *argc; i++) {
    
    if (strcmp((*argv)[i], flag) == 0) {
      flagExists = true;
      
      int size = (--(*argc) * sizeof(char *));
      char ** _argv = (char **)malloc(size);
      memcpy(_argv,
             *argv,
             sizeof(char *) * i);
      memcpy(_argv + i,
             *argv + (i+1),
             sizeof(char *) * (*argc - i) );
      memcpy(*argv, _argv, sizeof(char *) * (*argc));
    }
  }
  return flagExists;
}

int main(int argc, char *argv[]) {
  
  // Active verbose mode if the "-v" flag is found
  bool verbose = parseBoolArg(&argv, &argc, "-v"); // @TODO: use "cl::ParseCommandLineOptions(...)" instead
  setOutEnabled(verbose);
  
  const char * filename = argv[1];
  ifstream file(filename, ios::in);
  
  string s, line;
  while (getline(file, line)) {
    s += line; s += "\n";
  }
  Parser p(s);
	
  LLVMContext &C = getGlobalContext();
  ErrorOr<Module *> ModuleOrErr = new Module("test", C);
  std::unique_ptr<Module> Owner = std::unique_ptr<Module>(ModuleOrErr.get());
  Module *M = Owner.get();
	
  // i32 @main(i32 %argc, i8** %argv)
  Function *MainF = cast<Function>(M->getOrInsertFunction("main", Type::getInt32Ty(C),
                                                          Type::getInt32Ty(C),
                                                          Type::getInt8PtrTy(C)->getPointerTo(),
                                                          (Type *)0));
  Function::arg_iterator it = MainF->arg_begin();
  Argument *Argc = it;
  Argc->setName("argc");
  
  Argument *Argv = ++it;
  Argv->setName("argv");
  
  BasicBlock *BB = BasicBlock::Create(C, "EntryBlock", MainF);
  IRBuilder<> B(BB);
  B.SetInsertPoint(BB);
  
  // Throw a "SMILMissingInput" exception (|Argc| < the highest input expr)
  static Value *GMissingInputsAssertMessage = NULL;
  if (!GMissingInputsAssertMessage) {
    GMissingInputsAssertMessage = B.CreateGlobalString("Missing inputs",
                                                       "assert.missing.inputs.message");
  }
  
  Value *IdxsCountV = B.getInt32(InputExpr::getIndexesCount());
  Value *CondV = B.CreateICmpSGE(Argc, IdxsCountV);
  CreateAssert(CondV, GMissingInputsAssertMessage,
               M, B, 0, 0);
  
  /* Init the hash table for variables */
  InitVarTable(M);
  
  /* Fetch input arguments */
  Value *CounterPtr = B.CreateAlloca(Type::getInt32Ty(C));
  B.CreateStore(B.getInt32(0), CounterPtr);
  
  const char * tk = tok_input;
  Value *InputToken = CastToCStr(B.CreateGlobalString(tk, "input.token"), B);
  
  // |Size| = len(|tk|) * (|argc| - 1) + 1
  Value *TotalSize = B.CreateAdd(B.CreateMul(B.getInt32(strlen(tk)), Argc),
                                 B.getInt32(1));
  
  Value *InputName = CastToCStr(B.CreateAlloca(Type::getInt8Ty(C), TotalSize), B);
  // Let string buffer |InputName| starts with '\0' (to avoid dirt on concat)
  B.CreateMemSet(InputName, B.getInt8(0), B.getInt64(1), 8);
  
  // i8* @strcat(i8*, i8*)
  Type* StrcatArgs[] = { Type::getInt8PtrTy(C), Type::getInt8PtrTy(C) };
  FunctionType *StrcatTy = FunctionType::get(Type::getInt8PtrTy(C), StrcatArgs, false);
  Function *StrcatF = cast<Function>(M->getOrInsertFunction("strcat", StrcatTy));
  
  // Concat ":$" (input) to |InputName|
  B.CreateCall(StrcatF, ArrayRef<Value *>{ InputName, InputToken });
  
  /* Loop block */
  BasicBlock *LoopBB = BasicBlock::Create(C, "LoopBlock", MainF);
  B.CreateBr(LoopBB);
  
  IRBuilder<> LoopB(LoopBB);
  B.SetInsertPoint(LoopBB);
  
  // @TODO: Use phi for |Counter|
  Value *Counter = LoopB.CreateLoad(CounterPtr);
  
  Value *Length = Strlen(InputName, M, LoopB);
  Value *Size = LoopB.CreateAdd(Length, LoopB.getInt64(1));
  Value *Name = CastToCStr(B.CreateAlloca(Type::getInt8Ty(C), Size), LoopB);
  MemCpy(Name, InputName, Size, M, LoopB);
  
  // Insert the variable
  Value *Arg = LoopB.CreateGEP(Argv, Counter);
  Value *V = ValToObj(LoopB.CreateLoad(Arg), M, LoopB);
  InsertOrUpdate(Name, V, M, LoopB);
  
  LoopB.CreateStore(LoopB.CreateAdd(Counter, LoopB.getInt32(1)),
                    CounterPtr);
  
  // Concat ":$" (input) to |InputName|
  LoopB.CreateCall(StrcatF, ArrayRef<Value *>{ InputName, InputToken });
  
  // Loop until |Counter| >= |argc|
  BasicBlock *DoneBB = BasicBlock::Create(C, "DoneBlock", MainF);
  Value * Cond = LoopB.CreateICmpSGE(LoopB.CreateLoad(CounterPtr), Argc);
  LoopB.CreateCondBr(Cond, DoneBB, LoopBB);
  
  /* Done block */
  B.SetInsertPoint(DoneBB);
  
  for (vector<Expr *>::iterator it = p.getExprs().begin();
       it != p.getExprs().end();
       it++) {
    Expr *expr = (*it);
    if (canGen(expr)) {
      out() << "Generating code for: " << expr->DebugString() << "\n";
      expr->CodeGen(M, B);
    }
  }
  
  B.CreateRet(B.getInt32(0));
  
  InitializeNativeTarget();
#if __MCJIT__
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
#endif
  
#if __MCJIT__
  std::string ErrStr;
  EngineBuilder *EB = new EngineBuilder(std::move(Owner));
  ExecutionEngine *EE = EB->setErrorStr(&ErrStr)
    .setMCJITMemoryManager(std::unique_ptr<SectionMemoryManager>(new SectionMemoryManager()))
    .create();
#else
  EngineBuilder EB = EngineBuilder(M);
  ExecutionEngine *EE = EB.create();
#endif
  
  // Add pass optimization
  const DataLayout *DL = EE->getDataLayout();
  M->setDataLayout(DL->getStringRepresentation());
  
  ModulePassManager *MPM = new ModulePassManager();
  MPM->run(*M);
	
  out() << "\n" << "=== IR Dump ===" << "\n";
  if (verbose) {
    M->dump();
  }
  
#if __MCJIT__
  EE->finalizeObject();
#endif
  
  vector<GenericValue> Args(2);
  // Skip the two first args (path of the executable and the file)
  Args[0].IntVal = APInt(32, argc-2); // argc
  Args[1].PointerVal = argv+2; // *argv[];
  
  out() << "\n" << "=== Program Output ===" << "\n";
  GenericValue gv = EE->runFunction(MainF, Args);
	
  // Clean up and shutdown
  delete EE;
  llvm_shutdown();
  
  return 0;
}