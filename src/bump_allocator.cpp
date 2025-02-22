#ifndef BUMP_ALLOC_H
#define BUMP_ALLOC_H

#include <stdint.h>
#include <sys/mman.h>

#include "./assert.c"

struct bump_allocator {
  uint16_t capacity;
  uint16_t size;
  void *buffer;

  bump_allocator(uint16_t bytes) {
    void *memory = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(memory == MAP_FAILED, "mmap failed");

    capacity = bytes;
    size = 0;
    buffer = memory;
  }

  void *alloc(uint16_t bytes) {
    assert((size + bytes) > capacity, "bump allocator capacity full");
    void *memory = (char *)buffer + bytes;
    size += bytes;
    return memory;
  }

  void free() {
    assert(munmap(buffer, capacity) != 0, "munmap failed");
    size = 0;
    capacity = 0;
    buffer = nullptr;
  }
};

#endif /* ifndef BUMP_ALLOC_H */
