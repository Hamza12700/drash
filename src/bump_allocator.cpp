#ifndef BUMP_ALLOC_H
#define BUMP_ALLOC_H

#include <sys/mman.h>

#include "./assert.c"

struct bump_allocator {
  size_t capacity;
  size_t size;
  void *buffer;

  bump_allocator(size_t bytes) {
    void *memory = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert_err(memory == MAP_FAILED, "mmap failed");

    capacity = bytes;
    size = 0;
    buffer = memory;
  }

  ~bump_allocator() {
    assert_err(munmap(buffer, capacity) != 0, "munmap failed");
    size = 0;
    capacity = 0;
    buffer = nullptr;
  }

  void *alloc(size_t bytes) {
    assert((size + bytes) > capacity, "bump allocator capacity full");
    void *memory = {0};

    if (size == 0) memory = buffer;
    else memory = (char *)buffer + bytes;

    size += bytes;
    return memory;
  }

  // Set the entire buffer to zero with MEMSET
  void reset() {
    memset(buffer, 0, size);
    size = 0;
  }

  void free() {
    assert_err(munmap(buffer, capacity) != 0, "munmap failed");
    size = 0;
    capacity = 0;
    buffer = nullptr;
  }
};

#endif /* ifndef BUMP_ALLOC_H */
