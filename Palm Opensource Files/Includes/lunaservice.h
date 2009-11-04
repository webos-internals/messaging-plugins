/* Copyright 2009 Ryan M. Hope, All rights reserved. */

typedef struct {
  int errorCode;
  char* errorMessage;
  const char* file;
  int line;
  const char* function;
  void* ctx;
  unsigned long invalid;
} LSError;

typedef struct {
  const char* method; //LSMethodFunction
  void* ptr1; //LSMethodFlags
  void* ptr2; //LSMethodFlags
} LSMethod;

typedef struct {
} LSHandle;

typedef struct {
} LSMessage;
