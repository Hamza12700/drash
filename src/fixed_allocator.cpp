#ifndef FIXED_ALLOC_H
#define FIXED_ALLOC_H

#include <sys/mman.h>

#include "./assert.c"


// NOTE:
//
// Don't add a constructor or demonstrator because the 'sub_allocator' method creates a new
// allocator which points to the memory allocated by another allocator.
//

struct Fixed_Allocator {
   size_t capacity;
   size_t size;
   void *buffer;

   // @NOTE:
   //
   // For sub-allocators add a check for if something tried to overwrite some
   // part of the memory owned by a sub-allocator. Because the sub-allocator is
   // allocated at the end of the memory region, storing the pointer/idx for the
   // sub-allocator in some kind-of array and upon new allocation checking if the
   // requested bytes overlap by doing pointer arithmetic.
   //
   // -Hamza, March 1, 2025
   //

   Fixed_Allocator sub_allocator(size_t total_size) {
      assert(total_size + size > capacity, "bump_allocator failed to create a sub-allocator because capacity is full");

      int idx = capacity - total_size;
      capacity -= total_size;

      return Fixed_Allocator {
         .capacity = total_size,
         .size = 0,
         .buffer = (char *)buffer + idx
      };
   }

   void *alloc(size_t bytes) {
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

Fixed_Allocator fixed_allocator(const uint capacity) {
   void *memory = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
   assert_err(memory == MAP_FAILED, "mmap failed");

   return Fixed_Allocator {
      .capacity = capacity,
      .size = 0,
      .buffer = memory
   };
}

#endif /* ifndef FIXED_ALLOC_H */
