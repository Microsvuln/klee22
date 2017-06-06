// Unoptimized posix uses a function pointer to user main that must be resolved correctly
// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t.bc
// RUN: rm -rf %t.klee-out

// RUN: %klee --output-dir=%t.klee-out -search=sonar -sonar-target=assert-fail --posix-runtime --libc=uclibc %t.bc --sym-arg 3 >%t.log
// RUN: test -f %t.klee-out/test000001.assert.err
// RUN: test ! -f %t.klee-out/test000002.ktest


#include <assert.h>

int main(int argc, char** argv) {
  assert(argc == 2 && argv[1][0] != 'x');
}
