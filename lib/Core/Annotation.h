#pragma once
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include <unordered_map>

class Annotation {
private:
  const llvm::Module *module;
  std::unordered_map<const llvm::Instruction *, uint64_t> info;

public:
  explicit Annotation(const llvm::Module *_module);

  uint64_t &operator[](const llvm::Instruction *instr) { return info[instr]; }

  uint64_t &operator[](const llvm::BasicBlock *bb) {
    return this->operator[](&(bb->front()));
  }

  uint64_t &operator[](const llvm::Function *func) {
    return this->operator[](&(func->front()));
  }

  size_t size();
  void dump();
  void test();
};
