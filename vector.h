#pragma once

typedef struct {
  void **items;
  int capacity;
  int len;
} vector;

void vectorInit(vector*);
int vectorLen(vector*);
void vectorAdd(vector*, void*);
void vectorDelete(vector*, int);
void vectorFree(vector*);
