#include "llvm/Support/raw_ostream.h"

#include "Annotation.h"

Annotation::Annotation(const llvm::Module *_module) : module(_module) {
  for (llvm::Module::const_iterator func = module->begin(), funcE = module->end(); func != funcE; ++func) {
    for (const auto &bb : *func) {
      for (const auto &inst : bb) {
        info[&inst] = std::numeric_limits<uint64_t>::max();
      }
    }
  }
}

size_t Annotation::size() { return info.size(); }

void Annotation::dump() {
  for (llvm::Module::const_iterator func = module->begin(), funcE = module->end(); func != funcE; ++func) {
    if (func->isIntrinsic() || func->empty()) {
      continue;
    }

    func->dump();
    for (auto &bb : *func) {
      llvm::outs() << "> ";
      for (auto &inst : bb) {
        if (info[&inst] == std::numeric_limits<uint64_t>::max()) {
          llvm::outs() << "\u221E"
                       << " ";
        } else {
          llvm::outs() << info[&inst] << " ";
        }
      }
      llvm::outs() << '\n';
    }
    llvm::outs() << '\n';
  }
}

void Annotation::test() {
  for (llvm::Module::const_iterator func = module->begin(), funcE = module->end(); func != funcE; ++func) {
    if (func->isIntrinsic() || func->empty()) {
      continue;
    }
    llvm::outs() << func->getName() << '\n';

    for (const auto &bb : *func) {
      llvm::outs() << "> ";
      for (const auto &inst : bb) {
        if (info[&inst] == std::numeric_limits<uint64_t>::max()) {
          llvm::outs() << "\u221E"
                       << " ";
        } else {
          llvm::outs() << info[&inst] << " ";
        }
      }
      llvm::outs() << '\n';
    }
    llvm::outs() << '\n';
  }
}
