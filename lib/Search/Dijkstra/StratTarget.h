#ifndef STRATTARGET_H_
#define STRATTARGET_H_

#include "./DijkstraState.h"
#include "llvm/ADT/StringRef.h"

class StratTarget {
public:
  virtual ~StratTarget(){/* empty */};
  virtual bool isTheTarget(DijkstraState state) = 0;
};

class CallToSpecificFunction : public StratTarget {
public:
  llvm::StringRef targetFunctionName;
  explicit CallToSpecificFunction(llvm::StringRef _targetFunctionName)
      : targetFunctionName(_targetFunctionName){/* empty */};
  ~CallToSpecificFunction();
  bool isTheTarget(DijkstraState state);
};

class EndOfSpecificFunction : public StratTarget {
public:
  llvm::StringRef targetFunctionName;
  explicit EndOfSpecificFunction(llvm::StringRef _targetFunctionName)
      : targetFunctionName(_targetFunctionName){/* empty */};
  ~EndOfSpecificFunction();
  bool isTheTarget(DijkstraState state);
};

class FailingAssert : public StratTarget {
public:
  ~FailingAssert();
  bool isTheTarget(DijkstraState state);
};

class FinalReturn : public StratTarget {
public:
  ~FinalReturn();
  bool isTheTarget(DijkstraState state);
};

class NoTarget : public StratTarget {
public:
  ~NoTarget();
  bool isTheTarget(DijkstraState state);
};

#endif // STRATTARGET_H_
