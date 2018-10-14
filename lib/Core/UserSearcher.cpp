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
#include "Executor.h"

#include "klee/Internal/Support/ErrorHandling.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace klee;

namespace {
  cl::list<Searcher::CoreSearchType>
  CoreSearch("search", cl::desc("Specify the search heuristic (default=random-path interleaved with nurs:covnew)"),
	     cl::values(clEnumValN(Searcher::DFS, "dfs", "use Depth First Search (DFS)"),
			clEnumValN(Searcher::BFS, "bfs", "use Breadth First Search (BFS)"),
			clEnumValN(Searcher::RandomState, "random-state", "randomly select a state to explore"),
			clEnumValN(Searcher::RandomPath, "random-path", "use Random Path Selection (see OSDI'08 paper)"),
			clEnumValN(Searcher::NURS_CovNew, "nurs:covnew", "use Non Uniform Random Search (NURS) with Coverage-New"),
			clEnumValN(Searcher::NURS_MD2U, "nurs:md2u", "use NURS with Min-Dist-to-Uncovered"),
			clEnumValN(Searcher::NURS_Depth, "nurs:depth", "use NURS with 2^depth"),
			clEnumValN(Searcher::NURS_ICnt, "nurs:icnt", "use NURS with Instr-Count"),
			clEnumValN(Searcher::NURS_CPICnt, "nurs:cpicnt", "use NURS with CallPath-Instr-Count"),
			clEnumValN(Searcher::NURS_QC, "nurs:qc", "use NURS with Query-Cost"),
      clEnumValN(Searcher::Sonar, "sonar", "use sonar search for targeted analysis"),
			clEnumValEnd));

  cl::opt<bool>
  UseIterativeDeepeningTimeSearch("use-iterative-deepening-time-search", 
                                    cl::desc("(experimental)"));

  cl::opt<bool>
  UseBatchingSearch("use-batching-search", 
		    cl::desc("Use batching searcher (keep running selected state for N instructions/time, see --batch-instructions and --batch-time)"),
		    cl::init(false));

  cl::opt<unsigned>
  BatchInstructions("batch-instructions",
                    cl::desc("Number of instructions to batch when using --use-batching-search"),
                    cl::init(10000));
  
  cl::opt<double>
  BatchTime("batch-time",
            cl::desc("Amount of time to batch when using --use-batching-search"),
            cl::init(5.0));


  cl::opt<bool>
  UseMerge("use-merge", 
           cl::desc("Enable support for klee_merge() (experimental)"));
 
  cl::opt<bool>
  UseBumpMerge("use-bump-merge", 
           cl::desc("Enable support for klee_merge() (extra experimental)"));

  cl::list<Scanner::Target> SonarTarget(
    "sonar-target",
    cl::desc("Specify the target for the sonar search strategy"),
    cl::values(clEnumValN(Scanner::AssertFail, "assert-fail",
                                "Aim for failing assert statements (default)"),
               clEnumValN(Scanner::FunctionCall, "function-call",
                          "Aim for the call to a specific function"),
               clEnumValN(Scanner::BasicBlockID, "basic-block",
                          "Aim for the call to a specific basic-block"),
               clEnumValN(Scanner::FunctionReturn, "function-return",
                          "Aim for the return of a specific function"),
               clEnumValEnd));

  cl::list<Scanner::Distance> SonarDistance(
    "sonar-distance",
    cl::desc(
        "Specify the distance measure for the sonar search strategy"),
    cl::values(
        clEnumValN(Scanner::Decisions, "decisions",
                   "Count the number of branching decisions (default)"),
        clEnumValN(Scanner::Instructions, "instructions",
                   "Count the number of instructions"),
        clEnumValEnd));

  cl::opt<std::string>
    TargetInfo("sonar-target-info",
               llvm::cl::desc("Additional info for the target of sonar search"),
               llvm::cl::init("-"));

  cl::opt<bool> StopAtTarget(
      "sonar-stop-at-target",
      cl::desc("Do not continue the analysis with states that have passed the target"),
      cl::init(false));

  cl::opt<bool> ContinueUnreachable(
      "sonar-continue-unreachable",
      cl::desc("Continue the analysis even if the target is no longer reachable"),
      cl::init(false));

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
  case Searcher::Sonar:

    auto sonarDistance = (SonarDistance.empty()) ? Scanner::Decisions : *SonarDistance.begin();
    auto sonarTarget = (SonarTarget.empty()) ? Scanner::AssertFail : *SonarTarget.begin();
    size_t separator_location_index;
    switch (sonarTarget) {
      case Scanner::BasicBlockID:
        // must have a "function name: basic block id" in the TargetInfo
        if (TargetInfo.compare("-") == 0) {
          klee_error("sonar-target-info is needed");
        }

        // we need both a function name and a basic block id
        separator_location_index = TargetInfo.find(':');
        if ( (separator_location_index == std::string::npos) ||      // ':' not found
             (separator_location_index == 0) ||                      // function name missing
             (separator_location_index == TargetInfo.size() - 1) ) { // basic block ID missing
          klee_error("incorrect basic-block ID. Expected function_name:bb_id");
        }
        break;
      case Scanner::FunctionCall:
      case Scanner::FunctionReturn:
        // must have a function name in the TargetInfo
        if (TargetInfo.compare("-") == 0) {
          klee_error("sonar-target-info is needed");
        }
        break;
      case Scanner::AllReturns:
      case Scanner::AssertFail: // @todo: missing from the parameter list?
      default:
        // do nothing
        break;
    }

    if (StopAtTarget) {
      searcher = new SonarSearcher(executor, sonarDistance,sonarTarget, TargetInfo, ContinueUnreachable);
    } else {
      searcher = new SonarDeepSearcher(executor, sonarDistance,sonarTarget, TargetInfo, ContinueUnreachable);
    }
    break;
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
