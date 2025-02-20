#ifndef BUMP_ALLOC_H
#define BUMP_ALLOC_H

#include <stdint.h>
#include <sys/mman.h>

#include "./assert.c"

typedef struct {
  uint16_t capacity;
  uint16_t size;
  void *buffer;

} bump_allocator;

bump_allocator bump_alloc_new(uint16_t size) {
  void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  assert(memory == MAP_FAILED, "mmap failed");

  bump_allocator alloc = {
    .size = 0,
    .capacity = size,
    .buffer = memory
  };

  return alloc;
}

void *bump_alloc(bump_allocator *alloc, uint16_t size) {
  assert((alloc->size + size) > alloc->capacity, "bump allocator capacity full");
  void *memory = (char *)alloc->buffer + size;
  alloc->size += size;
  return memory;
}

void bump_alloc_free(bump_allocator *alloc) {
  assert(munmap(alloc->buffer, alloc->capacity) != 0, "munmap failed");
  alloc->size = 0;
  alloc->capacity = 0;
}

#endif /* ifndef BUMP_ALLOC_H */
