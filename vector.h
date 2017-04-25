#pragma once

#define VECTOR_INIT_CAPACITY 4

typedef struct {
  void **items;
  int capacity;
  int len;
} vector;

void vectorInit(vector*);
int vectorLen(vector*);
void vectorAdd(vector*, void*);
void vectorSet(vector*, int, void*);
void *vectorGet(vector*, int);
void vectorDelete(vector*, int);
void vectorFree(vector*);
