#ifndef BUMP_ALLOC_H
#define BUMP_ALLOC_H

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

#include "./assert.c"

typedef struct {
  uint16_t capacity;
  uint16_t size;
  uint8_t *data;
} bump_allocator;

bump_allocator bump_alloc_new(uint16_t capacity) {
  void *data = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  assert(data == MAP_FAILED, "mmap failed");

  bump_allocator alloc = {
    .capacity = capacity,
    .size = 0,
    .data = data
  };

  return alloc;
}

void *bump_alloc(bump_allocator* buffer_alloc, uint16_t size) {
  assert((buffer_alloc->size + size) > buffer_alloc->capacity, "bump allocator capacity full");
  void *data = &buffer_alloc->data[buffer_alloc->size];
  buffer_alloc->size += size;
  return data;
}

// Upon successful completion, munmap() shall return 0; otherwise, it return -1 and set errno to indicate the error.
int bump_free(bump_allocator *buffer_alloc) {
  int err = munmap(buffer_alloc->data, buffer_alloc->capacity);
  buffer_alloc->size = 0;
  buffer_alloc->capacity = 0;
  return err;
}

#endif /* ifndef BUMP_ALLOC_H */
