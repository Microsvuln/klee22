#include "./helper.h"
#include "llvm/IR/Function.h"

bool isCallToFunction(const llvm::Instruction *inst,
                      const llvm::StringRef funcName) {
  // Check, if it is a call instruction
  if (llvm::isa<llvm::CallInst>(inst)) {
    // Extract the called function
    const llvm::CallInst *call = llvm::cast<llvm::CallInst>(inst);
    llvm::Function *called = call->getCalledFunction();
    return called != NULL && called->getName() == funcName;
  }
  return false;
}
