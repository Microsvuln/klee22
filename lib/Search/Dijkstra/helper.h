#ifndef HELPER_H_
#define HELPER_H_

#include "klee/Config/Version.h"
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#else
#include "llvm/BasicBlock.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#endif
#include <string>

llvm::BasicBlock::iterator getIteratorOnInstruction(llvm::Instruction *inst);

/**
* Checks, whether an instruction is a call to a function with the given name
*/
bool isCallToFunction(llvm::Instruction *inst, llvm::StringRef funcName);

/**
* Checks, whether an instruction is the last instruction in its function
*/
bool isLastInstructionInFunction(llvm::Instruction *inst);

#endif // HELPER_H_
