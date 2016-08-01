#ifndef BFSEARCHER_H_
#define BFSEARCHER_H_

#include <deque>
#include <list>
#include <queue>
#include <set>
#include <stack>
#include <utility>
#include <vector>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"


llvm::BasicBlock::iterator getIteratorOnInstruction(llvm::Instruction* inst);


/**
 * The entries on the call stack of one state in BFSearcher. They store
 * information about the calling instruction.
 */
class BFStackEntry {
 public:
  llvm::BasicBlock::iterator call;
  explicit BFStackEntry(llvm::BasicBlock::iterator _call)
      : call(_call) { /* empty */
  }

  friend bool operator<(const BFStackEntry& l, const BFStackEntry& r) {
    // Very strange way for comparison, but there are no alternatives ...
    return &*l.call < &*r.call;
  }
};

/**
 * A class representing the current progress of a single state in BFSearcher.
 */
class BFSearchState {
 public:
  // Iterator on the instruction we are currently looking at
  llvm::BasicBlock::iterator instruction;
  // Accumulated distance from the start of the search till now
  uint distanceFromStart;
  // Stack corresponding to the call path to the current instruction
  std::stack<BFStackEntry> stack;

  BFSearchState(llvm::BasicBlock::iterator _instruction,
                uint _distanceFromStart)
      : instruction(_instruction),
        distanceFromStart(_distanceFromStart) { /* empty */
  }

  BFSearchState(llvm::BasicBlock::iterator _instruction,
                uint _distanceFromStart, std::stack<BFStackEntry> _stack)
      : instruction(_instruction),
        distanceFromStart(_distanceFromStart),
        stack(_stack) { /* empty */
  }

  BFSearchState(llvm::Instruction* _instruction, uint _distanceFromStart,
                std::list<llvm::Instruction*> _stack);

  BFSearchState(llvm::Instruction* _instruction, uint _distanceFromStart)
      : instruction(getIteratorOnInstruction(_instruction)),
        distanceFromStart(_distanceFromStart) { /* empty */
  }

  friend bool operator<(const BFSearchState& l, const BFSearchState& r) {
    return l.distanceFromStart < r.distanceFromStart;
  }

  friend bool operator>(const BFSearchState& l, const BFSearchState& r) {
    return l.distanceFromStart > r.distanceFromStart;
  }

  /**
   * Checks, if next is the start of a recursion
   */
  bool doesIntroduceRecursion(BFStackEntry next);
};

/**
 * Breadth first search strategy traversing the whole llvm bitcode looking
 * for the shortest distance to a target. Functions for counting the distance
 * of an instruction and to determine the target are virtual, so this is
 * a base class implementing the strategy for a lot of different search types.
 */
class BFSearcher {
 private:
  std::priority_queue<BFSearchState, std::vector<BFSearchState>,
                      std::greater<BFSearchState> >
      searchqueue;

  // This data structure is ugly as hell, but is there any better way?
  std::set<std::pair<llvm::Instruction*, std::stack<BFStackEntry> > >
      duplicateFilter;

  // Some variables to avoid extreme long search runs
  static const uint maxDistance = 100000;
  static const uint maxIterations = 10000000;
  static const uint maxQueueLength = 10000;

  /**
   * Add the state to search queue, if some sanity checks are passed
   */
  void addToSearchQueue(BFSearchState state);

  /**
   * Small helper function to add a new search state to the search queue. The
   * new distance is calculated internally.
   */
  void enqueueInSearchQueue(BFSearchState oldState,
                            llvm::BasicBlock::iterator next,
                            std::stack<BFStackEntry> newStack);

  /**
   * Get and remove the next entry from the search queue
   */
  BFSearchState popFromSeachQueue();

  /**
   * Check, if the given state was added earlier to the search queue
   */
  bool wasAddedEarlier(BFSearchState state);

  /**
   * Store a state for a later wasAddedEarlier check
   */
  void rememberAsAdded(BFSearchState state);

  /**
   * Do one step in the search. Select the next state and add all its
   * successors to the search queue.
   */
  void doSingleSearchIteration();

  /**
   * Check, if the current state is the target of our search
   */
  virtual bool isTheTarget(BFSearchState state) = 0;

  /**
   * Determine the weight of the current instruction, that is added to the
   * overall distance, when passing this instruction.
   */
  virtual uint distanceToPass(llvm::Instruction* instr) = 0;

 public:
  uint iterationCounter;

  explicit BFSearcher(llvm::Instruction* start);
  BFSearcher(llvm::Instruction* start, std::list<llvm::Instruction*> stack);

  /**
   * Runs a search for the minimal distance to the target. If the target is
   * reachable, it returns the distance to it. Otherwise -1 is returned.
   */
  uint searchForMinimalDistance();
};

#endif  // BFSEARCHER_H_
