#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "vector.h"

void vectorInit(vector *v) {
  v->capacity = 10;
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
  v->items[v->len++] = item;
}

void vectorDelete(vector *v, int index) {
  assert(0 <= index && index < v->len);

  v->items[index] = NULL;

  memmove(&v->items[index], &v->items[index+1], (v->len - index - 1) * sizeof(void*));

  v->len--;

  if (v->len > 0 && v->len < v->capacity / 4) {
    vectorResize(v, v->capacity / 2);
  }
}

void vectorFree(vector *v) {
  free(v->items);
}
