#pragma once
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instructions.h"
#include <initializer_list>
#include <limits>
#include <stdint.h>

inline constexpr uint64_t sumOrMax(uint64_t a, uint64_t b) {
  return (std::numeric_limits<uint64_t>::max() - a < b)
             ? std::numeric_limits<uint64_t>::max()
             : a + b;
}

template <typename... Args>
inline constexpr uint64_t sumOrMax(uint64_t a, Args... args) {
  return sumOrMax(a, sumOrMax(args...));
}

bool isCallToFunction(const llvm::Instruction *inst,
                      const llvm::StringRef funcName);

bool isInFunction(const llvm::Instruction *inst,
                  const llvm::StringRef funcName);

bool isInBasicBlock(const llvm::Instruction *instr,
                    const llvm::StringRef BB_stg);

