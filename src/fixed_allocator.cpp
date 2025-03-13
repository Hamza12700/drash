#ifndef FIXED_ALLOC_H
#define FIXED_ALLOC_H

#include <sys/mman.h>

#include "assert.c"
#include "types.cpp"


// NOTE:
//
// Don't add a constructor or demonstrator because the 'sub_allocator' method creates a new
// allocator which points to the memory allocated by another allocator.
//

struct Fixed_Allocator {
   uint capacity;
   uint size;
   void *buffer;

   static Fixed_Allocator make(const uint size) {
      void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
      assert_err(memory == MAP_FAILED, "mmap failed");

      return Fixed_Allocator {
         .capacity = size,
         .size = 0,
         .buffer = memory
      };
   }

   Fixed_Allocator sub_allocator(uint total_size) {
      assert(total_size + size > capacity, "bump_allocator failed to create a sub-allocator because capacity is full");

      int idx = capacity - total_size;
      capacity -= total_size;

      return Fixed_Allocator {
         .capacity = total_size,
         .size = 0,
         .buffer = (char *)buffer + idx
      };
   }

   void *alloc(uint bytes) {
      assert((size + bytes) > capacity, "bump-allocator allocation failed because capacity is full");
      void *memory = {0};
      memory = (char *)buffer + size;

      size += bytes;
      return memory;
   }

   void reset() {
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

#endif /* ifndef FIXED_ALLOC_H */
