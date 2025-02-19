#ifndef BUMP_ALLOC_H
#define BUMP_ALLOC_H

#include <stdint.h>
#include <sys/mman.h>

#include "./assert.c"

struct bump_allocator {
  uint16_t capacity;
  uint16_t size;
  void *buffer;

  bump_allocator(uint16_t size_bytes) {
    void *memory = mmap(nullptr, size_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(memory == MAP_FAILED, "mmap failed");

    buffer = memory;
    capacity = size_bytes;
    size = 0;
  }

  void *alloc(uint16_t bytes) {
    assert((size + bytes) > capacity, "bump allocator capacity full");
    void *memory = (char *)buffer + size;
    size += bytes;
    return memory;
  }

  // Upon successful completion, munmap() shall return 0; otherwise, it return -1 and set errno to indicate the error.
  void free() {
    assert(munmap(buffer, capacity) != 0, "munmap failed");
    size = 0;
    capacity = 0;
  }
};

#endif /* ifndef BUMP_ALLOC_H */
