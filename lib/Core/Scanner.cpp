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

bool Scanner::isTheTarget(const llvm::Instruction *instr) {
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
  return -1;
}

void Scanner::scan() {
  llvm::CallGraph cg{};
  cg.runOnModule(*this->module);

  llvm::outs() << "After runON" << '\n';

  auto testSCC = llvm::scc_begin(&cg);

  llvm::outs() << "Testing" << '\n';

  for (auto cgnSCC = llvm::scc_begin<llvm::CallGraph *>(&cg),
            cgnSCCe = llvm::scc_end(&cg);
       cgnSCC != cgnSCCe; ++cgnSCC) {
    llvm::outs() << "Don't work" << '\n';
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
                if (isTheTarget(&*inst)) {
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

  return 0;
}

uint64_t Scanner4Target::getDistance2Target(
    const llvm::Instruction *pos,
    std::vector<const llvm::Instruction *> &stack) {
  uint64_t minDist = anno[pos];
  uint64_t prevPassed = dist2return[pos];

  for (auto instr = stack.rbegin(); instr != stack.rend(); ++instr) {
    minDist = std::min(minDist, sumOrMax(prevPassed, anno[*instr]));
    prevPassed += dist2return[*instr];
  }
  return minDist;
}

uint64_t Scanner4Target::getDistance2Target(const klee::ExecutionState * state) {
  uint64_t minDist = anno[state->pc->inst];
  uint64_t prevPassed = dist2return[state->pc->inst];

  for (auto it = (++state->stack.begin()); it != state->stack.end(); it++) {
    minDist = std::min(minDist, sumOrMax(prevPassed, anno[it->caller->inst]));
    prevPassed += dist2return[it->caller->inst];
  }
  return minDist;
}
