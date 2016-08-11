#include "Decisions2TargetCallSearcher.h"
#include "llvm/IR/Instructions.h"


bool Decisions2TargetCallSearcher::isTheTarget(BFSearchState state) {
  // Check, if it is a call instruction
  if (llvm::isa<llvm::CallInst>(state.instruction)) {
    // Extract the called function
    llvm::CallInst* call = llvm::cast<llvm::CallInst>(state.instruction);
    llvm::Function* called = call->getCalledFunction();

    // Check, if the called function actually exists
    if (called == NULL) {
      return false;
    }

    // Check, if it calls our target or one of the klee internals used
    // be the prepend error modification to actually check for these errors
    return called->getName().str() == this->targetFunctionName ||
           called->getName() == "klee_silent_exit" ||
           called->getName() == "klee_report_error";
  }
  return false;
}

uint Decisions2TargetCallSearcher::distanceToPass(llvm::Instruction* instr) {
  // Check, if it is a terminator instruction
  if (llvm::isa<llvm::TerminatorInst>(instr)) {
    // Get it as an terminator instruction
    // Direct cast generates memory erros, so we take a view indirections here
    llvm::TerminatorInst* term = instr->getParent()->getTerminator();

    // Check, if have more than one successor
    return (term->getNumSuccessors() > 1) ? 1 : 0;
  }
  return 0;
}
