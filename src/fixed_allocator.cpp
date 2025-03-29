#ifndef FIXED_ALLOC_H
#define FIXED_ALLOC_H

#include <sys/mman.h>

#include "types.cpp"


//
// @NOTE:
//
// Don't add a constructor or demonstrator because the 'sub_allocator' method creates a new
// allocator which points to the memory allocated by another allocator.
//

struct Fixed_Allocator {

   //
   // @NOTE: This allocator tuned for this specific CLI program and this
   // program isn't going to need more than the MAX_U16 (65536) bytes of memory.
   //

   u16 capacity = 0;
   u16 size = 0;
   void *buffer;

   Fixed_Allocator sub_allocator(const u16 total_size) {
      assert(total_size + size > capacity, "bump_allocator failed to create a sub-allocator because capacity is full");

      int idx = capacity - total_size;
      capacity -= total_size;

      return Fixed_Allocator {
         .capacity = total_size,
         .buffer = (char *)buffer + idx
      };
   }

   void *alloc(const u16 bytes) {
      assert((size + bytes) > capacity, "bump-allocator allocation failed because capacity is full");
      void *memory = {0};
      memory = (char *)buffer + size;

      size += bytes;
      return memory;
   }

   void reset() {
      // @NOTE: We have to 'memset' the memory to zero because Null-Terminated Strings won't make your life easier!
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

Fixed_Allocator fixed_allocator(const u16 size) {
   void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
   assert_err(memory == MAP_FAILED, "mmap failed");

   return Fixed_Allocator {
      .capacity = size,
      .buffer = memory
   };
}

#endif // FIXED_ALLOC_H
