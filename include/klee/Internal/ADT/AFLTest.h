//===-- AFLTest.h --------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//#ifndef __COMMON_KTEST_H__
//#define __COMMON_KTEST_H__

#include <vector>
#include <string>

/* reads AFL testcase as KTest file */
/* returns NULL on (unspecified) error */
KTest* kTest_fromAFLFile(const char *path, const char *bc, std::vector<std::string> argv);
