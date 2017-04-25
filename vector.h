#ifndef VECTOR_H
#define VECTOR_H

#define VECTOR_INIT_CAPACITY 4

typedef struct vector {
  void **items;
  int capacity;
  int total;
} vector;

void vectorInit(vector*);
int vectorTotal(vector*);
void vectorAdd(vector*, void*);
void vectorSet(vector*, int, void*);
void *vectorGet(vector*, int);
void vectorDelete(vector*, int);
void vectorFree(vector*);

#endif
