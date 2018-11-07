//===-- KTest.cpp ---------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/ADT/KTest.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <assert.h>
#include <iostream>


#define KTEST_VERSION 3
#define KTEST_MAGIC_SIZE 5
#define KTEST_MAGIC "KTEST"

// for compatibility reasons
#define BOUT_MAGIC "BOUT\n"

/***/
using namespace std;

static int read_uint32(FILE *f, unsigned *value_out) {
  unsigned char data[4];
  if (fread(data, 4, 1, f)!=1)
    return 0;
  *value_out = (((((data[0]<<8) + data[1])<<8) + data[2])<<8) + data[3];
  return 1;
}

static int write_uint32(FILE *f, unsigned value) {
  unsigned char data[4];
  data[0] = value>>24;
  data[1] = value>>16;
  data[2] = value>> 8;
  data[3] = value>> 0;
  return fwrite(data, 1, 4, f)==4;
}

static int read_string(FILE *f, char **value_out) {
  unsigned len;
  if (!read_uint32(f, &len))
    return 0;
  *value_out = (char*) malloc(len+1);
  if (!*value_out)
    return 0;
  if (fread(*value_out, len, 1, f)!=1)
    return 0;
  (*value_out)[len] = 0;
  return 1;
}

static int write_string(FILE *f, const char *value) {
  unsigned len = strlen(value);
  if (!write_uint32(f, len))
    return 0;
  if (fwrite(value, len, 1, f)!=1)
    return 0;
  return 1;
}

/***/


unsigned kTest_getCurrentVersion() {
  return KTEST_VERSION;
}


static int kTest_checkHeader(FILE *f) {
  char header[KTEST_MAGIC_SIZE];
  if (fread(header, KTEST_MAGIC_SIZE, 1, f)!=1)
    return 0;
  if (memcmp(header, KTEST_MAGIC, KTEST_MAGIC_SIZE) &&
      memcmp(header, BOUT_MAGIC, KTEST_MAGIC_SIZE))
    return 0;
  return 1;
}

int kTest_isKTestFile(const char *path) {
  FILE *f = fopen(path, "rb");
  int res;

  if (!f)
    return 0;
  res = kTest_checkHeader(f);
  fclose(f);
  
  return res;
}

KTest *kTest_fromFile(const char *path) {
  FILE *f = fopen(path, "rb");
  KTest *res = 0;
  unsigned i, version;

  if (!f) 
    goto error;
  if (!kTest_checkHeader(f)) 
    goto error;

  res = (KTest*) calloc(1, sizeof(*res));
  if (!res) 
    goto error;

  if (!read_uint32(f, &version)) 
    goto error;
  
  if (version > kTest_getCurrentVersion())
    goto error;

  res->version = version;

  if (!read_uint32(f, &res->numArgs)) 
    goto error;
  res->args = (char**) calloc(res->numArgs, sizeof(*res->args));
  if (!res->args) 
    goto error;
  
  for (i=0; i<res->numArgs; i++)
    if (!read_string(f, &res->args[i]))
      goto error;

  if (version >= 2) {
    if (!read_uint32(f, &res->symArgvs)) 
      goto error;
    if (!read_uint32(f, &res->symArgvLen)) 
      goto error;
  }

  if (!read_uint32(f, &res->numObjects))
    goto error;
  res->objects = (KTestObject*) calloc(res->numObjects, sizeof(*res->objects));
  if (!res->objects)
    goto error;
  for (i=0; i<res->numObjects; i++) {
    KTestObject *o = &res->objects[i];
    if (!read_string(f, &o->name))
      goto error;
    if (!read_uint32(f, &o->numBytes))
      goto error;
    o->bytes = (unsigned char*) malloc(o->numBytes);
    if (fread(o->bytes, o->numBytes, 1, f)!=1)
      goto error;
  }

  fclose(f);

  return res;
 error:
  if (res) {
    if (res->args) {
      for (i=0; i<res->numArgs; i++)
        if (res->args[i])
          free(res->args[i]);
      free(res->args);
    }
    if (res->objects) {
      for (i=0; i<res->numObjects; i++) {
        KTestObject *bo = &res->objects[i];
        if (bo->name)
          free(bo->name);
        if (bo->bytes)
          free(bo->bytes);
      }
      free(res->objects);
    }
    free(res);
  }

  if (f) fclose(f);

  return 0;
}

KTest *kTest_fromAFLFile(const char* path, const char* bc, std::vector<std::string> argv) {
  // assert(!strcmp(inputType, "stdin") || !strcmp(inputType, "file"));
  
  /*
  FILE *f = fopen(path, "rb");
  unsigned i, version;

  if (!f) {
    fclose(f);
    return 0;
  }
  */
  /* Pre-initialization so goto doesn't complain*/
  KTestObject* obj;
  size_t inputSize;
  std::string inputBuffer;
  std::string inputStat;
  int argInd;
  int *dum;
  /* TODO: Remove this and take as a parameter */
  // std::vector<std::string> argv(2, "-a");
  
  /* Create and initialize the ktest object */
  KTest* newKTest = (KTest*)calloc(1, sizeof(KTest));
  if(!newKTest) {
    return 0;
  }

  newKTest->version = kTest_getCurrentVersion();

  /* Read the input file into buffer */
  {
    std::ifstream instream_input(path);
    std::stringstream input_stream;
    input_stream << instream_input.rdbuf();
    inputBuffer = input_stream.str();
  }
  
  inputSize = inputBuffer.size();
  
  inputStat = "\xff\xff\xff\xff\xff\xff\xff\xff\x01\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
  
  /* Now say what the arguments to KLEE should be */
  /* dummy.bc A --sym-files 1 1 <file_size> --sym-stdin <stdin_size>  */
  newKTest->numArgs = 8;

  newKTest->args = (char**)calloc(newKTest->numArgs, sizeof(char*));
  if(!newKTest->args)
    goto error;
  
  argInd = 0;

  newKTest->args[argInd] = (char*)calloc(1, strlen(bc)+1); // args[0]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], bc);
  argInd++;

  newKTest->args[argInd] = (char*)calloc(1, strlen("A")+1); // args[1]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], "A");
  argInd++;
  
  newKTest->args[argInd] = (char*)calloc(1, strlen("--sym-files")+1); // args[2]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], "--sym-files");
  argInd++;
  
  newKTest->args[argInd] = (char*)calloc(1, strlen("1")+1); // args[3]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], "1");
  argInd++;
  
  newKTest->args[argInd] = (char*)calloc(1, strlen("1")+1); // args[4]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], "1");
  argInd++;
 
  newKTest->args[argInd] = (char*)calloc(1, std::to_string(inputSize).size()+1); // args[5]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], std::to_string(inputSize).c_str());
  argInd++;

  newKTest->args[argInd] = (char*)calloc(1, strlen("--sym-stdin")+1); // args[6]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], "--sym-stdin");
  argInd++;
  
  newKTest->args[argInd] = (char*)calloc(1, std::to_string(inputSize).size()+1); // args[7]
  if(!newKTest->args[argInd])
    goto error;

  strcpy(newKTest->args[argInd], std::to_string(inputSize).c_str());
  argInd++;
  
 
  /* These are zero because the input is either stdin or file */
  /* No, actually they are zero because I don't know what the heck they do*/
  newKTest->symArgvs = 0;
  newKTest->symArgvLen = 0;
  
  /* Allocate array for KTestObjects */
  /* Objects: A-data, A-data-stat, stdin, stdin-stat, model_version */
  newKTest->numObjects = 5;

  if(argv.size()>0) {
    newKTest->numObjects += 1; // for "n_args"
    newKTest->numObjects += static_cast<int>(argv.size()); // for arg0 to argN
  }

  newKTest->objects = (KTestObject*)calloc(newKTest->numObjects, sizeof(*newKTest->objects));
  if(!newKTest->objects)
    goto error;

  obj = newKTest->objects;

  /* Are there any command line args? If so then make them symbolic */
  if(static_cast<int>(argv.size())>0) {
    obj->name = (char*)calloc(1, strlen("n_args") + 1);
    if(!obj->name)
      goto error;
    strcpy(obj->name, "n_args");
    obj->numBytes = 4;
    obj->bytes = (unsigned char*)calloc(1, sizeof(int));
    if(!obj->bytes)
      goto error;
    dum = (int*)malloc(sizeof(int));
    *dum = static_cast<int>(argv.size());
    memcpy(obj->bytes, dum, obj->numBytes);
    obj++;

    int n_args = static_cast<int>(argv.size());
    for(int i=0; i<n_args; i++) {
      obj->name = (char*)calloc(1, strlen("arg0")+1);
      if(!obj->name)
        goto error;
      strcpy(obj->name, ("arg"+std::to_string(i)).c_str());
      obj->numBytes = argv[i].size();
      obj->bytes = (unsigned char*)calloc(1, argv[i].size()+1);
      if(!obj->bytes)
        goto error;
      memcpy(obj->bytes, const_cast<char*>(argv[i].c_str()), obj->numBytes);
      obj->bytes[obj->numBytes] = 0;
      obj++;
    }
  }

  /* Finally add the data from AFL testcases */
  obj->name = (char*)calloc(1, strlen("A-data")+1);
  if(!obj->name)
    goto error;
  strcpy(obj->name, "A-data");
  obj->numBytes = inputSize;
  obj->bytes = (unsigned char*)calloc(1, obj->numBytes+1);
  if(!obj->bytes)
    goto error;
  memcpy(obj->bytes, const_cast<char*>(inputBuffer.c_str()), obj->numBytes);
  obj->bytes[obj->numBytes] = 0;
  obj++;

  obj->name = (char*)calloc(1, strlen("A-data-stat")+1);
  if(!obj->name)
    goto error;
  strcpy(obj->name, "A-data-stat");
  obj->numBytes = 144;
  obj->bytes = (unsigned char*)calloc(1, 144);
  if(!obj->bytes)
    goto error;
  memcpy(obj->bytes, inputStat.c_str(), inputStat.size());
  // obj->bytes[obj->numBytes] = 0;
  obj++;
  
  obj->name = (char*)calloc(1, strlen("stdin")+1);
  if(!obj->name)
    goto error;
  strcpy(obj->name, "stdin");
  obj->numBytes = inputSize;
  obj->bytes = (unsigned char*)calloc(1, obj->numBytes+1);
  if(!obj->bytes)
    goto error;
  memcpy(obj->bytes, const_cast<char*>(inputBuffer.c_str()), obj->numBytes);
  obj->bytes[obj->numBytes] = 0;
  obj++;

  obj->name = (char*)calloc(1, strlen("stdin-stat")+1);
  if(!obj->name)
    goto error;
  strcpy(obj->name, "stdin-stat");
  obj->numBytes = 144;
  obj->bytes = (unsigned char*)calloc(1, 144);
  if(!obj->bytes)
    goto error;
  memcpy(obj->bytes, inputStat.c_str(), inputStat.size());
  // obj->bytes[obj->numBytes] = 0;
  obj++;
  
  obj->name = (char*)calloc(1, strlen("model_version")+1);
  if(!obj->name)
    goto error;
  strcpy(obj->name, "model_version");
  obj->numBytes = 4;
  obj->bytes = (unsigned char*)calloc(1, sizeof(int));
  if(!obj->bytes)
    goto error;
  dum = (int*)malloc(sizeof(int));
  *dum = 1;
  memcpy(obj->bytes, dum, obj->numBytes);
  
  kTest_toFile(newKTest, "/var/tmp/intermediate.ktest");
  return newKTest;
 error:
  if (newKTest) {
    if (newKTest->args) {
      for (unsigned i=0; i<newKTest->numArgs; i++)
        if (newKTest->args[i])
          free(newKTest->args[i]);
      free(newKTest->args);
    }
    if (newKTest->objects) {
      for (unsigned i=0; i<newKTest->numObjects; i++) {
        KTestObject *bo = &newKTest->objects[i];
        if (bo->name)
          free(bo->name);
        if (bo->bytes)
          free(bo->bytes);
      }
      free(newKTest->objects);
    }
    free(newKTest);
  }
  return 0;
}

int kTest_toFile(KTest *bo, const char *path) {
  FILE *f = fopen(path, "wb");
  unsigned i;

  if (!f) 
    goto error;
  if (fwrite(KTEST_MAGIC, strlen(KTEST_MAGIC), 1, f)!=1)
    goto error;
  if (!write_uint32(f, KTEST_VERSION))
    goto error;
      
  if (!write_uint32(f, bo->numArgs))
    goto error;
  for (i=0; i<bo->numArgs; i++) {
    if (!write_string(f, bo->args[i]))
      goto error;
  }

  if (!write_uint32(f, bo->symArgvs))
    goto error;
  if (!write_uint32(f, bo->symArgvLen))
    goto error;
  
  if (!write_uint32(f, bo->numObjects))
    goto error;
  for (i=0; i<bo->numObjects; i++) {
    KTestObject *o = &bo->objects[i];
    if (!write_string(f, o->name))
      goto error;
    if (!write_uint32(f, o->numBytes))
      goto error;
    if (fwrite(o->bytes, o->numBytes, 1, f)!=1)
      goto error;
  }

  fclose(f);

  return 1;
 error:
  if (f) fclose(f);
  
  return 0;
}

unsigned kTest_numBytes(KTest *bo) {
  unsigned i, res = 0;
  for (i=0; i<bo->numObjects; i++)
    res += bo->objects[i].numBytes;
  return res;
}

void kTest_free(KTest *bo) {
  unsigned i;
  for (i=0; i<bo->numArgs; i++)
    free(bo->args[i]);
  free(bo->args);
  for (i=0; i<bo->numObjects; i++) {
    free(bo->objects[i].name);
    free(bo->objects[i].bytes);
  }
  free(bo->objects);
  free(bo);
}
