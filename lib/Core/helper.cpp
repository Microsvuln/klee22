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

bool isInFunction(const llvm::Instruction *inst,
                  const llvm::StringRef funcName) {
  if(llvm::isa<llvm::Instruction>(inst)) {
    /* llvm 3.4 doesn not have support for directly 
     * getting function from instruction, later 
     * versions have it.
     
     * const llvm::Function *func = inst->getFunction();
     * but now first get the basicblock of the instruction
     * and getParent on BB will return Function*,
     */
    const llvm::Function *func = inst->getParent()->getParent();
    if ( func != NULL && func->getName() == funcName)
      return true;     
  }
  return false;
}

bool isInBasicBlock(const llvm::Instruction *instr,
                    const llvm::StringRef BB_stg) {
  const llvm::BasicBlock *BB = llvm::cast<llvm::BasicBlock*>(BB_stg);
  //find out the basic block of the instruction
  if(llvm::isa<llvm::Instruction>(instr)) {
    const llvm::BasicBlock *instBB = instr->getParent();
    //then check if the basic clock is same as BB
    if (BB && (BB == instBB)) 
      return true;
  }
  return false;
}
