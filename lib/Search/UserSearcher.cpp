//===-- UserSearcher.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "UserSearcher.h"

#include "Searcher.h"
#include "../Core/Executor.h"

#include "klee/CommandLine.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"

#include <string>

using namespace llvm;
using namespace klee;

namespace {
cl::list<Searcher::CoreSearchType> CoreSearch(
    "search", cl::desc("Specify the search heuristic (default=random-path "
                       "interleaved with nurs:covnew)"),
    cl::values(
        clEnumValN(Searcher::DFS, "dfs", "use Depth First Search (DFS)"),
        clEnumValN(Searcher::BFS, "bfs", "use Breadth First Search (BFS)"),
        clEnumValN(Searcher::RandomState, "random-state",
                   "randomly select a state to explore"),
        clEnumValN(Searcher::RandomPath, "random-path",
                   "use Random Path Selection (see OSDI'08 paper)"),
        clEnumValN(Searcher::NURS_CovNew, "nurs:covnew",
                   "use Non Uniform Random Search (NURS) with Coverage-New"),
        clEnumValN(Searcher::NURS_MD2U, "nurs:md2u",
                   "use NURS with Min-Dist-to-Uncovered"),
        clEnumValN(Searcher::NURS_Depth, "nurs:depth", "use NURS with 2^depth"),
        clEnumValN(Searcher::NURS_ICnt, "nurs:icnt",
                   "use NURS with Instr-Count"),
        clEnumValN(Searcher::NURS_CPICnt, "nurs:cpicnt",
                   "use NURS with CallPath-Instr-Count"),
        clEnumValN(Searcher::NURS_QC, "nurs:qc", "use NURS with Query-Cost"),
        clEnumValN(Searcher::Dijkstra, "dijkstra",
                   "use dijkstra algorithm for targeted analysis"),
        clEnumValN(
            Searcher::AfterCall, "after-call",
            "focus the explorations on branches calling a specific function"),
        clEnumValEnd));

cl::opt<bool>
    UseIterativeDeepeningTimeSearch("use-iterative-deepening-time-search",
                                    cl::desc("(experimental)"));

cl::opt<bool> UseBatchingSearch(
    "use-batching-search",
    cl::desc("Use batching searcher (keep running selected state for N "
             "instructions/time, see --batch-instructions and --batch-time)"),
    cl::init(false));

cl::opt<unsigned> BatchInstructions(
    "batch-instructions",
    cl::desc(
        "Number of instructions to batch when using --use-batching-search"),
    cl::init(10000));

cl::opt<double> BatchTime(
    "batch-time",
    cl::desc("Amount of time to batch when using --use-batching-search"),
    cl::init(5.0));

cl::opt<bool>
    UseMerge("use-merge",
             cl::desc("Enable support for klee_merge() (experimental)"));

cl::opt<bool> UseBumpMerge(
    "use-bump-merge",
    cl::desc("Enable support for klee_merge() (extra experimental)"));

cl::opt<bool> ContinueUnreachable(
    "dij-continue-unreachable",
    cl::desc("Continue the analysis even if the target is no longer reachable"),
    cl::init(false));

cl::list<DijkstraSearcher::Target> DijkstraTarget(
    "dij-target",
    cl::desc("Specify the target for the dijkstra search strategy"),
    cl::values(clEnumValN(DijkstraSearcher::AssertFail, "assert-fail",
                          "Aim for failing assert statements (default)"),
               clEnumValN(DijkstraSearcher::FunctionCall, "function-call",
                          "Aim for the call to a specific function"),
               clEnumValN(DijkstraSearcher::FunctionEnd, "function-end",
                          "Aim for the end of a specific function"),
               clEnumValN(DijkstraSearcher::FinalReturn, "final-return",
                          "Aim for the final return statement in the program"),
               clEnumValEnd));

cl::list<DijkstraSearcher::Distance> DijkstraDistance(
    "dij-distance",
    cl::desc("Specify the distance measure for the dijkstra search strategy"),
    cl::values(clEnumValN(DijkstraSearcher::Decisions, "decisions",
                          "Count the number of branching decisions (default)"),
               clEnumValN(DijkstraSearcher::Instructions, "instructions",
                          "Count the number of instructions"),
               clEnumValEnd));

cl::opt<std::string>
    TargetInfo("dij-target-info",
               cl::desc("Additional info for the target of dijkstra search"),
               cl::init("-"));
}


bool klee::userSearcherRequiresMD2U() {
  return (std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_MD2U) != CoreSearch.end() ||
	  std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_CovNew) != CoreSearch.end() ||
	  std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_ICnt) != CoreSearch.end() ||
	  std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_CPICnt) != CoreSearch.end() ||
	  std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::NURS_QC) != CoreSearch.end());
}


Searcher *getNewSearcher(Searcher::CoreSearchType type, Executor &executor) {
  Searcher *searcher = NULL;
  switch (type) {
  case Searcher::DFS: searcher = new DFSSearcher(); break;
  case Searcher::BFS: searcher = new BFSSearcher(); break;
  case Searcher::RandomState: searcher = new RandomSearcher(); break;
  case Searcher::RandomPath: searcher = new RandomPathSearcher(executor); break;
  case Searcher::NURS_CovNew: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::CoveringNew); break;
  case Searcher::NURS_MD2U: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::MinDistToUncovered); break;
  case Searcher::NURS_Depth: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::Depth); break;
  case Searcher::NURS_ICnt: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::InstCount); break;
  case Searcher::NURS_CPICnt: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::CPInstCount); break;
  case Searcher::NURS_QC: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::QueryCost); break;
  case Searcher::Dijkstra:
    // Parse the distance
    DijkstraSearcher::Distance selectedDistance;
    if (DijkstraDistance.size() == 0) {
      selectedDistance = DijkstraSearcher::Distance::Decisions;
    } else {
      selectedDistance = *DijkstraDistance.begin();
    }

    // Parse the target
    DijkstraSearcher::Target selectedTarget;
    if (DijkstraTarget.size() == 0) {
      selectedTarget = DijkstraSearcher::Target::AssertFail;
    } else {
      selectedTarget = *DijkstraTarget.begin();
    }

    // Check for target information
    if ((selectedTarget == DijkstraSearcher::Target::FunctionCall ||
         selectedTarget == DijkstraSearcher::Target::FunctionEnd) &&
        TargetInfo == "-") {
      llvm::errs()
          << "This mode of DijkstraSearcher requires target information \n";
      llvm::errs() << " please add --dij-target-info=... to your parameters\n";
      exit(1);
    }

    searcher = new DijkstraSearcher(executor, selectedDistance, selectedTarget,
                                    TargetInfo, ContinueUnreachable);
    break;
  case Searcher::AfterCall:
    if (AfterFunctionName == "-") {
      llvm::errs() << "after-call search mode requires a target function";
      llvm::errs() << " please add --after-function=... to your parameters\n";
      exit(1);
    }

    searcher = new AfterCallSearcher(
        executor, DijkstraSearcher::Distance::Decisions,
        DijkstraSearcher::Target::FunctionCall, AfterFunctionName, false);
  }

  return searcher;
}

Searcher *klee::constructUserSearcher(Executor &executor) {

  // default values
  if (CoreSearch.size() == 0) {
    CoreSearch.push_back(Searcher::RandomPath);
    CoreSearch.push_back(Searcher::NURS_CovNew);
  }

  Searcher *searcher = getNewSearcher(CoreSearch[0], executor);
  
  if (CoreSearch.size() > 1) {
    std::vector<Searcher *> s;
    s.push_back(searcher);

    for (unsigned i=1; i<CoreSearch.size(); i++)
      s.push_back(getNewSearcher(CoreSearch[i], executor));
    
    searcher = new InterleavedSearcher(s);
  }

  if (UseBatchingSearch) {
    searcher = new BatchingSearcher(searcher, BatchTime, BatchInstructions);
  }

  // merge support is experimental
  if (UseMerge) {
    assert(!UseBumpMerge);
    assert(std::find(CoreSearch.begin(), CoreSearch.end(), Searcher::RandomPath) == CoreSearch.end()); // XXX: needs further debugging: test/Features/Searchers.c fails with this searcher
    searcher = new MergingSearcher(executor, searcher);
  } else if (UseBumpMerge) {
    searcher = new BumpMergingSearcher(executor, searcher);
  }
  
  if (UseIterativeDeepeningTimeSearch) {
    searcher = new IterativeDeepeningTimeSearcher(searcher);
  }

  llvm::raw_ostream &os = executor.getHandler().getInfoStream();

  os << "BEGIN searcher description\n";
  searcher->printName(os);
  os << "END searcher description\n";

  return searcher;
}
