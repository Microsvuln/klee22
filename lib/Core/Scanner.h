#pragma once
#include "./Annotation.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"
#include <string>
#include <vector>

class Scanner {
public:
  enum Distance { Decisions, Instructions };
  enum Target { AllReturns, AssertFail, FunctionCall, BasicBlockID, FunctionReturn };

protected:
  llvm::Module *const module;
  Distance distance;
  Target target;
  const std::string targetinfo;
  Annotation anno;

  virtual uint64_t getDistanceForCall(uint64_t prevDist,
                                      const llvm::CallInst *call) = 0;

  uint64_t distance2Pass(const llvm::Instruction *instr);

public:
  Scanner(llvm::Module *_module, Distance _distance, Target _target,
          const llvm::StringRef _targetinfo="")
      : module(_module), distance(_distance), target(_target),
        targetinfo(_targetinfo), anno(module){/* empty */};

  ~Scanner(){/*empty*/};
  void scan();

  uint64_t &operator[](const llvm::Instruction *instr);
  uint64_t &operator[](const llvm::BasicBlock *bb);
  uint64_t &operator[](const llvm::Function *func);

  bool isTarget(const llvm::Instruction *instr);
  void dump();
  size_t size();
  void test();
};

class Scanner4Return : public Scanner {
protected:
  uint64_t getDistanceForCall(uint64_t prevDist,
                              const llvm::CallInst *call) override;

public:
  Scanner4Return(llvm::Module *module, Distance distance)
      : Scanner(module, distance, AllReturns, "") {
    this->scan();
  };
  ~Scanner4Return(){/*empty*/};
};

class Scanner4Target : public Scanner {
protected:
  Scanner4Return dist2return;

  uint64_t getDistanceForCall(uint64_t prevDist,
                              const llvm::CallInst *call) override;

public:
  Scanner4Target(llvm::Module *module, Distance distance,
                 Target target, const std::string targetinfo)
      : Scanner(module, distance, target, targetinfo),
        dist2return(module, distance) {
    this->scan();
  };
  Scanner4Target(llvm::Module *module, Distance distance,
                 Target target)
      : Scanner(module, distance, target, ""),
        dist2return(module, distance){/*empty*/};

  ~Scanner4Target(){/*empty*/};
  uint64_t getDistance2Target(const klee::ExecutionState * state);
};
