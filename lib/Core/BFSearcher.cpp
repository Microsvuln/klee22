#include "BFSearcher.h"
#include <deque>
#include <list>
#include <stack>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CFG.h"


llvm::BasicBlock::iterator getIteratorOnInstruction(llvm::Instruction* inst) {
  // Construct an iterator for the start instruction
  llvm::BasicBlock* bb = inst->getParent();

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


BFSearchState::BFSearchState(llvm::Instruction* _instruction,
                             uint _distanceFromStart,
                             std::list<llvm::Instruction*> _stack)
    : instruction(getIteratorOnInstruction(_instruction)),
      distanceFromStart(_distanceFromStart),
      stack() {
  for (std::list<llvm::Instruction*>::iterator it = _stack.begin();
       it != _stack.end(); it++) {
    this->stack.push(BFStackEntry(getIteratorOnInstruction(*it)));
  }
}

bool BFSearchState::doesIntroduceRecursion(BFStackEntry next) {
  // Empty stacks do not contain recursions
  if (this->stack.empty()) {
    return false;
  }

  // Copy the stack
  std::stack<BFStackEntry> stackCopy(this->stack);

  // Extract the function called by the next stack entry
  llvm::CallInst* nextcall = llvm::cast<llvm::CallInst>(next.call);
  llvm::Function* nextcalled = nextcall->getCalledFunction();

  // check if last call in the stack exists twice
  while (!stackCopy.empty()) {
    BFStackEntry probe = stackCopy.top();

    // Extract the function called by the current stack entry
    llvm::CallInst* probecall = llvm::cast<llvm::CallInst>(probe.call);
    llvm::Function* probecalled = probecall->getCalledFunction();

    if (nextcalled == probecalled) {
      return true;
    }
    stackCopy.pop();
  }

  // If no duplicates are found -> no recursion added
  return false;
}


BFSearcher::BFSearcher(llvm::Instruction* start) {
  // Add the start instruction to the search queue with 0 distance so far
  addToSearchQueue(BFSearchState(start, 0));

  // Initialize the iteration counter
  this->iterationCounter = 0;
}

BFSearcher::BFSearcher(llvm::Instruction* start,
                       std::list<llvm::Instruction*> stack) {
  // Add the start instruction to the search queue with 0 distance so far
  // and everything that was stored on the stack so far
  addToSearchQueue(BFSearchState(start, 0, stack));

  // Initialize the iteration counter
  this->iterationCounter = 0;
}

uint BFSearcher::searchForMinimalDistance() {
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

void BFSearcher::addToSearchQueue(BFSearchState state) {
  if (!wasAddedEarlier(state) && searchqueue.size() <= maxQueueLength) {
    searchqueue.push(state);
    this->rememberAsAdded(state);
  }
}

void BFSearcher::enqueueInSearchQueue(BFSearchState oldState,
                                      llvm::BasicBlock::iterator next,
                                      std::stack<BFStackEntry> newStack) {
  addToSearchQueue(BFSearchState(
      next, oldState.distanceFromStart + distanceToPass(oldState.instruction),
      newStack));
}

BFSearchState BFSearcher::popFromSeachQueue() {
  BFSearchState result = searchqueue.top();
  searchqueue.pop();
  return result;
}

bool BFSearcher::wasAddedEarlier(BFSearchState state) {
  // Only lookup states, that are the first in their basic block
  if (&(state.instruction->getParent()->front()) == &*state.instruction) {
    return duplicateFilter.count(
        std::make_pair(&*state.instruction, state.stack));
  }
  return false;
}

void BFSearcher::rememberAsAdded(BFSearchState state) {
  // Only store instructions, that are the first in their basic block
  if (&(state.instruction->getParent()->front()) == &*state.instruction) {
    duplicateFilter.insert(std::make_pair(&*state.instruction, state.stack));
  }
}

void BFSearcher::doSingleSearchIteration() {
  // Remove the first state from the queue
  BFSearchState curr = this->popFromSeachQueue();

  // nullptr indicates an invalid instruction
  assert(&*curr.instruction != NULL);

  if (llvm::isa<llvm::CallInst>(curr.instruction)) {
    // If call, increase stack and add to search queue

    // Extract the called function
    llvm::CallInst* call = llvm::cast<llvm::CallInst>(curr.instruction);
    llvm::Function* called = call->getCalledFunction();

    // Check if the function is an external call
    if (called && !called->isIntrinsic() && !called->empty()) {
      // It is a call to an defined function

      // Avoid recursions
      BFStackEntry next(curr.instruction);
      if (!curr.doesIntroduceRecursion(next)) {
        // Add the current call instruction to the stack
        curr.stack.push(next);

        // Add everything to the search queue
        enqueueInSearchQueue(curr, called->front().begin(), curr.stack);
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
      BFStackEntry gobackto = curr.stack.top();
      // and remove it from the stack
      curr.stack.pop();

      // Add everything to the search queue
      enqueueInSearchQueue(curr, ++(gobackto.call), curr.stack);
    }

  } else if (llvm::isa<llvm::TerminatorInst>(curr.instruction)) {
    // If terminal instruction, add all successor

    // Get access to the current block
    llvm::BasicBlock* currblock = curr.instruction->getParent();

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
