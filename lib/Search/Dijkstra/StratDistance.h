#ifndef STRATDISTANCE_H_
#define STRATDISTANCE_H_

#include "klee/Config/Version.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#else
#include "llvm/BasicBlock.h"
#include "llvm/InstrTypes.h"
#endif

class StratDistance {
public:
  virtual ~StratDistance(){/* empty */};
  virtual uint distanceToPass(llvm::Instruction *instr) = 0;
};

class CountInstructions : public StratDistance {
public:
  ~CountInstructions();
  uint distanceToPass(llvm::Instruction *instr);
};

class CountDecisions : public StratDistance {
public:
  ~CountDecisions();
  uint distanceToPass(llvm::Instruction *instr);
};

#endif // STRATDISTANCE_H_
