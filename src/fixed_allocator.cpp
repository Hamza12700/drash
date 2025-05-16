#ifndef FIXED_ALLOC_H
#define FIXED_ALLOC_H

#include <sys/mman.h>
#include "types.cpp"

struct Fixed_Allocator {
   int capacity = 0;  // Memory capacity limit
   int size = 0;      // Used memory
   void *buffer;      // Mapped zero-initialized memory

   void *alloc(const int bytes);
   void reset();
   void free();
};

void *Fixed_Allocator::alloc(const int bytes) {
   assert((size + bytes) > capacity, "(fixed-allocator) allocation failed because capacity is full");
   void *memory = {0};
   memory = (char *)buffer + size;

   size += bytes;
   return memory;
}

// @NOTE: We have to 'memset' to zero because Null-Terminated Strings won't make your life easier!
void Fixed_Allocator::reset() {
   memset(buffer, 0, size);
   size = 0;
}

void Fixed_Allocator::free() {
   unmap(buffer, capacity);
}

Fixed_Allocator fixed_allocator(const int size) {
   int page_align_size = page_size;
   while (page_align_size < size) page_align_size *= 2;

   void *memory = mmap(NULL, page_align_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
   assert_err(memory == MAP_FAILED, "mmap failed");

   return Fixed_Allocator {
      .capacity = page_align_size,
      .buffer = memory
   };
}

#endif // FIXED_ALLOC_H
