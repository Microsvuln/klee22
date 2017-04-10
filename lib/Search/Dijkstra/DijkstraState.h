#ifndef DIJKSTRASTATE_H_
#define DIJKSTRASTATE_H_

#include "./helper.h"
#include "klee/Config/Version.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#else
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#endif
#include <deque>
#include <list>
#include <queue>
#include <set>
#include <utility>
#include <vector>

/**
 * The entries on the call stack of one state in Dijkstra search. They store
 * information about the calling instruction.
 */
class DijkstraStackEntry {
public:
  llvm::BasicBlock::iterator call;
  explicit DijkstraStackEntry(llvm::BasicBlock::iterator _call)
      : call(_call) { /* empty */
  }

  friend bool operator<(const DijkstraStackEntry &l,
                        const DijkstraStackEntry &r) {
    // Very strange way for comparison, but there are no alternatives ...
    return &*l.call < &*r.call;
  }

  friend bool operator==(const DijkstraStackEntry &l,
                         const DijkstraStackEntry &r) {
    // Very strange way for comparison, but there are no alternatives ...
    return &*l.call == &*r.call;
  }
};

/**
 * A class representing the current progress of a single state in Dijkstra.
 */
class DijkstraState {
public:
  // Iterator on the instruction we are currently looking at
  llvm::BasicBlock::iterator instruction;
  // Accumulated distance from the start of the search till now
  uint distanceFromStart;
  // Stack corresponding to the call path to the current instruction
  std::deque<DijkstraStackEntry> stack;

  DijkstraState(llvm::BasicBlock::iterator _instruction,
                uint _distanceFromStart)
      : instruction(_instruction),
        distanceFromStart(_distanceFromStart) { /* empty */
  }

  DijkstraState(llvm::BasicBlock::iterator _instruction,
                uint _distanceFromStart, std::deque<DijkstraStackEntry> _stack)
      : instruction(_instruction), distanceFromStart(_distanceFromStart),
        stack(_stack) { /* empty */
  }

  DijkstraState(llvm::Instruction *_instruction, uint _distanceFromStart,
                std::list<llvm::Instruction *> _stack);

  DijkstraState(llvm::Instruction *_instruction, uint _distanceFromStart)
      : instruction(getIteratorOnInstruction(_instruction)),
        distanceFromStart(_distanceFromStart) { /* empty */
  }

  friend bool operator<(const DijkstraState &l, const DijkstraState &r) {
    return l.distanceFromStart < r.distanceFromStart;
  }

  friend bool operator>(const DijkstraState &l, const DijkstraState &r) {
    return l.distanceFromStart > r.distanceFromStart;
  }

  /**
   * Checks, if next is the start of a recursion
   */
  bool doesIntroduceRecursion(DijkstraStackEntry next);
};

#endif // DIJKSTRASTATE_H_
