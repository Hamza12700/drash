#ifndef FIXED_ALLOC_H
#define FIXED_ALLOC_H

#include <sys/mman.h>

#include "types.cpp"

struct Fixed_Allocator {
   uint capacity = 0; // Memory capacity limit
   uint size = 0;     // Used memory
   void *buffer;      // Mapped zero-initialized memory

   void *alloc(const uint bytes);
   void reset();
   void free();
};

void * Fixed_Allocator::alloc(const uint bytes) {
   assert((size + bytes) > capacity, "bump-allocator allocation failed because capacity is full");
   void *memory = {0};
   memory = (char *)buffer + size;

   size += bytes;
   return memory;
}

// @NOTE: We have to 'memset' the memory to zero because Null-Terminated Strings won't make your life easier!
void Fixed_Allocator::reset() {
   memset(buffer, 0, size);
   size = 0;
}

void Fixed_Allocator::free() {
   assert_err(munmap(buffer, capacity) != 0, "munmap failed");
   size = 0;
   capacity = 0;
   buffer = NULL;
}

Fixed_Allocator fixed_allocator(const uint size) {
   void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
   assert_err(memory == MAP_FAILED, "mmap failed");

   return Fixed_Allocator {
      .capacity = size,
      .buffer = memory
   };
}

#endif // FIXED_ALLOC_H
