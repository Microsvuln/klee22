#include "./Scanner.h"
#include "./Annotation.h"
#include "./helper.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstdint>


bool Scanner::isTarget(const llvm::Instruction *instr) {
  switch (this->target) {
  case AllReturns:
    return llvm::isa<llvm::ReturnInst>(instr);
    break;
  case AssertFail:
    return isCallToFunction(instr, "__assert_fail");
    break;
  case FunctionCall:
    return isCallToFunction(instr, targetinfo);
    break;
  case BasicBlockID: {
    //return llvm::isa<llvm::BasicBlock>(instr);
    /*
       func, label = stripBasicBlockID();
       return isInFunction(instr, func) && isInBlock(instr, label);
    */
      const std::pair<llvm::StringRef, llvm::StringRef> funcLabel = 
        llvm::cast<llvm::StringRef>(targetinfo).split(':');
      return isInFunction(instr, funcLabel.first) && isInBasicBlock(instr, funcLabel.second); 
      break;
    }
  case FunctionReturn:
    if (llvm::isa<llvm::ReturnInst>(instr)) {
      const llvm::Function *func = instr->getParent()->getParent();
      return func && func->getName() == targetinfo;
    }
    return false;
    break;
  }
  return false;
}

uint64_t Scanner::distance2Pass(const llvm::Instruction *instr) {
  switch (this->distance) {
  case Instructions:
    return 1;
    break;
  case Decisions:
    // Check, if it is a terminator instruction
    if (llvm::isa<llvm::TerminatorInst>(instr)) {
      // Get it as an terminator instruction
      // Direct cast generates memory erros, so a few indirections here
      const llvm::TerminatorInst *term = instr->getParent()->getTerminator();

      // Check, if have more than one successor
      return (term->getNumSuccessors() > 1) ? 1 : 0;
    }
    return 0;
    break;
  }
  return std::numeric_limits<uint64_t>::max();
}

void Scanner::scan() {
  llvm::CallGraph cg{};
  cg.runOnModule(*this->module);

  auto testSCC = llvm::scc_begin(&cg);

  for (auto cgnSCC = llvm::scc_begin<llvm::CallGraph *>(&cg),
            cgnSCCe = llvm::scc_end(&cg);
       cgnSCC != cgnSCCe; ++cgnSCC) {
    bool cgnSCCchanged;
    do {
      cgnSCCchanged = false;
      for (const auto &cgn : *cgnSCC) {
        auto func = cgn->getFunction();
        if (!func || func->isIntrinsic() || func->empty()) {
          continue;
        }

        for (auto bbSCC = llvm::scc_begin<llvm::Function *>(func),
                  bbSCCe = llvm::scc_end(func);
             bbSCC != bbSCCe; ++bbSCC) {
          bool bbSCCchanged;
          do {
            bbSCCchanged = false;

            for (const auto &bb : *bbSCC) {

              // Get the shortest distance from all successor block
              uint64_t prevDist = std::numeric_limits<uint64_t>::max();
              for (llvm::succ_iterator SI = succ_begin(bb), SE = succ_end(bb); SI != SE; ++SI) {
                prevDist = std::min(prevDist, anno[*SI]);
              }

              for (auto inst = bb->rbegin(), inste = bb->rend(); inst != inste;
                   ++inst) {
                uint64_t newDist = std::numeric_limits<uint64_t>::max();
                if (isTarget(&*inst)) {
                  newDist = 0;
                } else {
                  // Calls require a different logic
                  if (llvm::isa<llvm::CallInst>(*inst)) {
                    newDist = this->getDistanceForCall(
                        prevDist, llvm::cast<llvm::CallInst>(&*inst));
                  } else {
                    // Just pass normal instructions
                    newDist = sumOrMax(prevDist, distance2Pass(&*inst));
                  }
                }

                if (newDist < anno[&*inst]) {
                  cgnSCCchanged = true;
                  bbSCCchanged = true;
                  anno[&*inst] = newDist;
                }
                prevDist = anno[&*inst];
              }
            }
          } while (bbSCCchanged);
        }
      }
    } while (cgnSCCchanged);
  }
  // this->dump();
}

uint64_t &Scanner::operator[](const llvm::Instruction *instr) {
  return anno[instr];
}
uint64_t &Scanner::operator[](const llvm::BasicBlock *bb) { return anno[bb]; }
uint64_t &Scanner::operator[](const llvm::Function *func) { return anno[func]; }

size_t Scanner::size() { return anno.size(); }
void Scanner::dump() { this->anno.dump(); }
void Scanner::test() { this->anno.test(); }

uint64_t Scanner4Return::getDistanceForCall(uint64_t prevDist,
                                            const llvm::CallInst *call) {
  uint64_t callDist = 0;
  llvm::Function *called = call->getCalledFunction();
  if (called && !called->isIntrinsic() && !called->empty()) {
    callDist = anno[called];
  }
  return sumOrMax(prevDist, callDist, distance2Pass(call));
}

uint64_t Scanner4Target::getDistanceForCall(uint64_t prevDist,
                                            const llvm::CallInst *call) {
  llvm::Function *called = call->getCalledFunction();

  // Manually resolve the function pointer to the original main function used
  // in __uClibc_main of KLEE's uClibc implementation
  if (llvm::isa<llvm::LoadInst>(call->getCalledValue()) &&
      call->getParent()->getParent()->getName() == "__uClibc_main") {

    // Extract the type of the function allocated in the called pointer
    const llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(call->getCalledValue());
    const llvm::AllocaInst *alloca =
        llvm::cast<llvm::AllocaInst>(load->getPointerOperand());
    const llvm::PointerType *inptr =
        llvm::cast<llvm::PointerType>(alloca->getAllocatedType());
    const llvm::FunctionType *infnc =
        llvm::cast<llvm::FunctionType>(inptr->getElementType());

    // Check, if the inner function has the signature of a main function
    if (infnc->getNumParams() == 3 && infnc->getReturnType()->isIntegerTy() &&
        infnc->getParamType(0)->isIntegerTy() &&
        infnc->getParamType(1)->isPointerTy() &&
        infnc->getParamType(2)->isPointerTy()) {

      // Find the call to uclibc inside the main function
      llvm::CallInst *call2uclibc =
          llvm::cast<llvm::CallInst>(call->getParent()
                                         ->getParent()
                                         ->getParent()
                                         ->getFunction("main")
                                         ->getEntryBlock()
                                         .begin());
       if (call2uclibc->getNumArgOperands() == 7) {
        llvm::Value *probe = call2uclibc->getArgOperand(0)->stripPointerCasts();
        if (llvm::isa<llvm::Function>(probe)) {
          // Extract actual content of the parameter for the __uClibc_main call
          called = llvm::cast<llvm::Function>(probe);
        }
      }
    }
  }

  if (called && !called->isIntrinsic() && !called->empty()) {
    // we take the shortest of two choices:
    // 1) Go to the target in the called function
    // 2) Return from the call and get to the target in the current function
    return std::min(
        sumOrMax(anno[called], distance2Pass(call)),
        sumOrMax(prevDist, dist2return[called], distance2Pass(call)));
  } else {
    // If it is an external function
    return sumOrMax(prevDist, distance2Pass(call));
  }
}

uint64_t Scanner4Target::getDistance2Target(const klee::ExecutionState * state) {
  // Initialize the distances from the current position without considering the stack
  uint64_t minDist = anno[state->pc->inst];
  uint64_t prevPassed = dist2return[state->pc->inst];

  // --state->...rend() Skips the first caller (i.e. void -> main)
  for (auto it = state->stack.rbegin(); it != --(state->stack.rend()); ++it) {
    // Get the instruction after the call
    klee::KInstIterator next = it->caller;
    ++next;

    // Either we have found the shortest way earlier
    // or we must consider the shortest way to go up the stack till here
    // and go directly to the target from here
    minDist = std::min(minDist, sumOrMax(prevPassed, anno[next->inst]));
    prevPassed += dist2return[next->inst];

    // Small short-cut: prevPassed increases monotonically
    if (prevPassed > minDist) {
      break;
    }
  }
  return minDist;
}
