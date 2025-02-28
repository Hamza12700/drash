#ifndef BUMP_ALLOC_H
#define BUMP_ALLOC_H

#include <sys/mman.h>

#include "./assert.c"

struct bump_allocator {
  size_t capacity;
  size_t size;
  void *buffer;

  bump_allocator sub_allocator(size_t total_size) {
    assert(total_size + size > capacity, "bump_allocator failed to create a sub-allocator because capacity is full");
    void *memory = {0};

    if (size == 0) memory = buffer;
    else memory = (char *)buffer + total_size;

    size += total_size;

    return bump_allocator {
      .capacity = total_size,
      .size = 0,
      .buffer = memory,
    };
  }

  void *alloc(size_t bytes) {
    assert((size + bytes) > capacity, "bump-allocator allocation failed because capacity is full");
    void *memory = {0};

    if (size == 0) memory = buffer;
    else memory = (char *)buffer + bytes;

    size += bytes;
    return memory;
  }

  // Set the entire buffer to zero with MEMSET
  void reset() {
    // @NOTE: Is memset necessary or just setting the size to 0 is good enough?
    memset(buffer, 0, size);
    size = 0;
  }

  void free() {
    assert_err(munmap(buffer, capacity) != 0, "munmap failed");
    size = 0;
    capacity = 0;
    buffer = NULL;
  }
};

bump_allocator new_bump_allocator(size_t capacity) {
  void *memory = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  assert_err(memory == MAP_FAILED, "mmap failed");

  return bump_allocator {
    .capacity = capacity,
    .size = 0,
    .buffer = memory
  };
}

#endif /* ifndef BUMP_ALLOC_H */
