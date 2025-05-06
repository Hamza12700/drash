#ifndef ARENA_ALLOC
#define ARENA_ALLOC

#include <sys/mman.h>

#include "types.cpp"

//
// @Temporary: Each arena is 'page_size' long and it will not handle memory size large than that.
//
struct Arena_Allocator {
   void *buffer = NULL;
   int size = 0;
   Arena_Allocator *next_arena = NULL;

   void *alloc(int bytes);
   void reset_with_memset();
   void free();
};

void *Arena_Allocator::alloc(int bytes) {
   assert(bytes > page_size, "(arena-allocator) - requested memory is larger than page_size"); // @Temporary: Reject requested memory size if larger than 'page_size'

   if (!next_arena) {
      if (size + bytes > page_size) {
         auto mem = allocate(page_size);
         next_arena->buffer = mem;
         next_arena->size += bytes;
         return next_arena->buffer;
      }

      void *mem = (char *)buffer+size;
      size += bytes;
      return mem;
   }

   Arena_Allocator *next;
   while ((next = next_arena) != NULL);

   if (next->size + bytes > page_size) {
      auto mem = allocate(page_size);
      next->next_arena->buffer = mem;
      next->next_arena->size += bytes;
      return next->next_arena->buffer;
   }

   void *mem = (char *)next->buffer+size;
   next->size += bytes;
   return mem;
}

void Arena_Allocator::reset_with_memset() {
   memset(buffer, 0, page_size);
   size = 0;

   Arena_Allocator *next;
   while ((next = next_arena) != NULL) {
      memset(next->buffer, 0, page_size);
      next->size = 0;
   }
}

void Arena_Allocator::free() {
   unmap(buffer, page_size);

   Arena_Allocator *next;
   while ((next = next_arena) != NULL) {
      unmap(next->buffer, page_size);
   }
}

#endif // ARENA_ALLOC
