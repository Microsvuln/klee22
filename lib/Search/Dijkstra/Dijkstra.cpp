#include "./Dijkstra.h"
#include "./DijkstraState.h"
#include "./helper.h"
#include "klee/Config/Version.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#else
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Support/CFG.h"
#include "llvm/Value.h"
#endif
#include <deque>
#include <list>

Dijkstra::Dijkstra(StratDistance *stratDistance, StratTarget *stratTarget,
                   llvm::Instruction *start) {
  // Store the strategyies
  this->stratDistance = stratDistance;
  this->stratTarget = stratTarget;

  // Add the start instruction to the search queue with 0 distance so far
  addToSearchQueue(DijkstraState(start, 0));

  // Initialize the iteration counter
  this->iterationCounter = 0;
}

Dijkstra::Dijkstra(StratDistance *stratDistance, StratTarget *stratTarget,
                   llvm::Instruction *start,
                   std::list<llvm::Instruction *> stack) {
  // Store the strategyies
  this->stratDistance = stratDistance;
  this->stratTarget = stratTarget;

  // Add the start instruction to the search queue with 0 distance so far
  // and everything that was stored on the stack so far
  addToSearchQueue(DijkstraState(start, 0, stack));

  // Initialize the iteration counter
  this->iterationCounter = 0;
}

uint Dijkstra::searchForMinimalDistance() {
  while (!searchqueue.empty() &&
         searchqueue.top().distanceFromStart < maxDistance &&
         iterationCounter < maxIterations) {
    // Check, if we already hit the target
    if (isTheTarget(searchqueue.top())) {
      return searchqueue.top().distanceFromStart;
    }
    doSingleSearchIteration();
    iterationCounter++;
  }
  // Empty search queue and still not found -> target not reachable
  return -1;
}

void Dijkstra::addToSearchQueue(DijkstraState state) {
  if (!wasAddedEarlier(state) && searchqueue.size() <= maxQueueLength) {
    searchqueue.push(state);
    this->rememberAsAdded(state);
  }
}

void Dijkstra::enqueueInSearchQueue(DijkstraState oldState,
                                    llvm::BasicBlock::iterator next,
                                    std::deque<DijkstraStackEntry> newStack) {
  addToSearchQueue(DijkstraState(
      next, oldState.distanceFromStart + distanceToPass(&*oldState.instruction),
      newStack));
}

DijkstraState Dijkstra::popFromSeachQueue() {
  DijkstraState result = searchqueue.top();
  searchqueue.pop();
  return result;
}

bool Dijkstra::wasAddedEarlier(DijkstraState state) {
  // Only lookup states, that are the first in their basic block
  if (&(state.instruction->getParent()->front()) == &*state.instruction) {
    return duplicateFilter.count(
        std::make_pair(&*state.instruction, state.stack));
  }
  return false;
}

void Dijkstra::rememberAsAdded(DijkstraState state) {
  // Only store instructions, that are the first in their basic block
  if (&(state.instruction->getParent()->front()) == &*state.instruction) {
    duplicateFilter.insert(std::make_pair(&*state.instruction, state.stack));
  }
}

void Dijkstra::doSingleSearchIteration() {
  // Remove the first state from the queue
  DijkstraState curr = this->popFromSeachQueue();

  // nullptr indicates an invalid instruction
  assert(&*curr.instruction != NULL);

  if (llvm::isa<llvm::CallInst>(curr.instruction)) {
    // If call, increase stack and add to search queue

    // Try to resolve the function call
    llvm::BasicBlock::iterator succ = resolveCall(curr.instruction);

    // If the resolve was successfull
    if (succ != curr.instruction) {
      // Avoid recursions
      DijkstraStackEntry next(curr.instruction);
      if (!curr.doesIntroduceRecursion(next)) {
        // Add the current call instruction to the stack
        curr.stack.push_back(next);

        // Add everything to the search queue
        enqueueInSearchQueue(curr, succ, curr.stack);
      }
    } else {
      // Just skip the called function and treat it as an normal instruction
      enqueueInSearchQueue(curr, (++(curr.instruction))--, curr.stack);
    }

  } else if (llvm::isa<llvm::ReturnInst>(curr.instruction)) {
    // If return, add last entry from stack

    // Check, if we have any point to return to
    if (!curr.stack.empty()) {
      // Extract the top stack frame
      DijkstraStackEntry gobackto = curr.stack.back();
      // and remove it from the stack
      curr.stack.pop_back();

      // Add everything to the search queue
      enqueueInSearchQueue(curr, ++(gobackto.call), curr.stack);
    }

  } else if (llvm::isa<llvm::TerminatorInst>(curr.instruction)) {
    // If terminal instruction, add all successor

    // Get access to the current block
    llvm::BasicBlock *currblock = curr.instruction->getParent();

    // Iterate over all the successors
    for (llvm::succ_iterator sit = llvm::succ_begin(currblock),
                             et = llvm::succ_end(currblock);
         sit != et; sit++) {
      // And add their first instruction to the search queue
      enqueueInSearchQueue(curr, sit->begin(), curr.stack);
    }

  } else {
    // All other instructions just add their successor
    enqueueInSearchQueue(curr, (++(curr.instruction))--, curr.stack);
  }
}

/**
* Resolves a llvm call operation and returns the first instruction executed
* after the call. If the call cannot be resolved, it returns the call itself
*/
llvm::BasicBlock::iterator resolveCall(llvm::BasicBlock::iterator iterOnCall) {
  // Check if the iterator is a valid call
  if (!llvm::isa<llvm::CallInst>(iterOnCall)) {
    // Non-Calls cannot be resolved
    return iterOnCall;
  }
  // Extract the called function
  llvm::CallInst *call = llvm::cast<llvm::CallInst>(iterOnCall);
  llvm::Function *called = call->getCalledFunction();

  // Check if the function is an external call
  if (called && !called->isIntrinsic() && !called->empty()) {
    // llvm::outs() << "Correctly resolved: " << called->getName() << '\n';
    return called->front().begin();
  }

  // Manually resolve the function pointer to the original main function used
  // in __uClibc_main of KLEE's uClibc implementation
  if (llvm::isa<llvm::LoadInst>(call->getCalledValue()) &&
      call->getParent()->getParent()->getName() == "__uClibc_main") {
    // Extract the type of the function allocated in the called pointer
    llvm::LoadInst *load = llvm::cast<llvm::LoadInst>(call->getCalledValue());
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
      // Extract actual content of the parameter for __uClibc_main

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
          // And return a jump to the first instruction of the user main
          llvm::Function *usermain = llvm::cast<llvm::Function>(probe);
          return usermain->front().begin();
        }
      }
    }
  }

  return iterOnCall;
}
