// RUN: %llvmgcc %s -emit-llvm -g %O0opt -c -o %t.bc
// RUN: rm -rf %t.klee-out-2
// RUN: %klee --output-dir=%t.klee-out-1 --afl-seed-out-dir=%S/afl-results --zero-seed-extension --libc=uclibc --posix-runtime %t.bc --sym-files 1 10 --max-fail 1
// RUN: ls %t.klee-out-1 | grep -c assert | grep 4

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

int main(int argc, char** argv) {
  char buf[32];
  
  int fd = open(argv[1], O_RDONLY, S_IRUSR);
  size_t nbytes = read(fd, buf, sizeof("Hello"));
  if(buf[0]=='H') {
    if(buf[1]=='e') {
        if(buf[2]=='l') {
            if(buf[3]=='l') {
                if(buf[4]=='o') {
                    printf("Hello!\n");
                }
                else {
                    printf("Sorry, I failed you\n");
                }
            }
            else {
                printf("Sorry, I failed you\n");
            }
        }
        else {
            printf("Sorry, I failed you\n");
        }
    }
    else {
        printf("Sorry, I failed you\n");
    }
  }
  else {
      printf("Sorry, I failed you\n");
  }
  
  return 0;
}
