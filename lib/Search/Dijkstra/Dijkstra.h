#ifndef DIJKSTRA_H_
#define DIJKSTRA_H_

#include "./DijkstraState.h"
#include "./StratDistance.h"
#include "./StratTarget.h"

#include "klee/Config/Version.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/BasicBlock.h"
#else
#include "llvm/BasicBlock.h"
#endif
#include <deque>
#include <list>
#include <queue>
#include <set>
#include <utility>
#include <vector>

/**
 * Dijkstra's algorithm traversing the whole llvm bitcode looking for the
 * shortest distance to a target. Functions for counting the distance of an
 * instruction and to determine the target are virtual, so this is a base class
 * implementing the strategy for a lot of different search types.
 */
class Dijkstra {
private:
  StratDistance *stratDistance;
  StratTarget *stratTarget;

  std::priority_queue<DijkstraState, std::vector<DijkstraState>,
                      std::greater<DijkstraState> >
      searchqueue;

  // This data structure is ugly as hell, but is there any better way?
  std::set<std::pair<llvm::Instruction *, std::deque<DijkstraStackEntry> > >
      duplicateFilter;

  // Some variables to avoid extreme long search runs
  static const uint maxDistance = 100000;
  static const uint maxIterations = 10000000;
  static const uint maxQueueLength = 10000;

  /**
   * Add the state to search queue, if some sanity checks are passed
   */
  void addToSearchQueue(DijkstraState state);

  /**
   * Small helper function to add a new search state to the search queue. The
   * new distance is calculated internally.
   */
  void enqueueInSearchQueue(DijkstraState oldState,
                            llvm::BasicBlock::iterator next,
                            std::deque<DijkstraStackEntry> newStack);

  /**
   * Get and remove the next entry from the search queue
   */
  DijkstraState popFromSeachQueue();

  /**
   * Check, if the given state was added earlier to the search queue
   */
  bool wasAddedEarlier(DijkstraState state);

  /**
   * Store a state for a later wasAddedEarlier check
   */
  void rememberAsAdded(DijkstraState state);

  /**
   * Do one step in the search. Select the next state and add all its
   * successors to the search queue.
   */
  void doSingleSearchIteration();

  /**
   * Check, if the current state is the target of our search
   */
  bool isTheTarget(DijkstraState state) {
    return stratTarget->isTheTarget(state);
  };

  /**
   * Determine the weight of the current instruction, that is added to the
   * overall distance, when passing this instruction.
   */
  uint distanceToPass(llvm::Instruction *instr) {
    return stratDistance->distanceToPass(instr);
  };

public:
  uint iterationCounter;

  Dijkstra(StratDistance *stratDistance, StratTarget *stratTarget,
           llvm::Instruction *start);
  Dijkstra(StratDistance *stratDistance, StratTarget *stratTarget,
           llvm::Instruction *start, std::list<llvm::Instruction *> stack);

  /**
   * Runs a search for the minimal distance to the target. If the target is
   * reachable, it returns the distance to it. Otherwise -1 is returned.
   */
  uint searchForMinimalDistance();
};

llvm::BasicBlock::iterator resolveCall(llvm::BasicBlock::iterator iterOnCall);

#endif // DIJKSTRA_H_
