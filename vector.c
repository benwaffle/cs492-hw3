#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "vector.h"

void vectorInit(vector *v) {
  v->capacity = VECTOR_INIT_CAPACITY;
  v->len = 0;
  v->items = malloc(sizeof(void *) * v->capacity);
}

int vectorLen(vector *v) {
  return v->len;
}

static void vectorResize(vector *v, int capacity) {
  void **items = realloc(v->items, sizeof(void *) * capacity);
  if (items) {
    v->items = items;
    v->capacity = capacity;
  }
}

void vectorAdd(vector *v, void *item) {
  if (v->capacity == v->len) {
    vectorResize(v, v->capacity * 2);
  }
}

void vectorSet(vector *v, int index, void *item) {
  if (index >= 0 && index < v->total) {
    v->items[index] = item;
  }
}

void *vectorGet(vector *v, int index) {
  if (index >= 0 && index < v->total) {
    return v->items[index];
  }
  return NULL;
  v->items[v->len++] = item;
}

void vectorDelete(vector *v, int index) {
  assert(0 <= index && index < v->len);

  v->items[index] = NULL;

  for (int i = 0; i < v->total - 1; i++) {
    v->items[i] = v->items[i+1];
    v->items[i+1] = NULL;
  }

  v->len--;

  if (v->total > 0 && v->total == v->capacity / 4) {
    vectorResize(v, v->capacity / 2);
  }
}

void vectorFree(vector *v) {
  free(v->items);
}
