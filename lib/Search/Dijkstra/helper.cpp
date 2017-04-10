#include "./helper.h"
#include "klee/Config/Version.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#else
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#endif
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SourceMgr.h"

llvm::BasicBlock::iterator getIteratorOnInstruction(llvm::Instruction *inst) {
  // Construct an iterator for the start instruction
  llvm::BasicBlock *bb = inst->getParent();

  // Skip all instructions before our start instruction
  for (llvm::BasicBlock::iterator II = bb->begin(), IE = bb->end(); II != IE;
       ++II) {
    if (&*II == inst) {
      return II;
    }
  }
  // The instruction was not part of its own block ... this can never happen
  return bb->end();
}

bool isCallToFunction(llvm::Instruction *inst, llvm::StringRef funcName) {
  // Check, if it is a call instruction
  if (llvm::isa<llvm::CallInst>(inst)) {
    // Extract the called function
    llvm::CallInst *call = llvm::cast<llvm::CallInst>(inst);
    llvm::Function *called = call->getCalledFunction();
    return called != NULL && called->getName() == funcName;
  }
  return false;
}

bool isLastInstructionInFunction(llvm::Instruction *inst) {
  return llvm::isa<llvm::TerminatorInst>(inst) &&
         (llvm::isa<llvm::ReturnInst>(inst) ||
          llvm::isa<llvm::InvokeInst>(inst));
}
