#ifndef ARENA_ALLOC
#define ARENA_ALLOC

#include <sys/mman.h>
#include "types.cpp"

struct Arena_Allocator {
   void *buffer = NULL;
   Arena_Allocator *next_arena = NULL;

   i32 size = 0;
   i32 capacity = 0;

   void *alloc(int bytes);
   void free_arena();
};

void *Arena_Allocator::alloc(int bytes) {
   if (!next_arena) {
      if (size+bytes > capacity) {
         int page_align = page_size;
         while (page_align < bytes) page_align *= 2;
         auto mem = allocate(page_align);
         auto new_arena = (Arena_Allocator *)xmalloc(sizeof(Arena_Allocator)); // @Speed @Temporary: Instead of malloc'ing every-time a new arena is needed, allocate a contiguous memory that holds some amount of arena-allocator struct and just index into that.

         new_arena->buffer = mem;
         new_arena->size += bytes;
         new_arena->capacity = page_align;
         next_arena = new_arena;
         return mem;
      }

      void *mem = (char *)buffer+size;
      size += bytes;
      return mem;
   }

   Arena_Allocator *next = next_arena;
   while (true) {
      if (next->next_arena == NULL) break;
      next = next->next_arena;
   }

   if (next->size+bytes > next->capacity) {
      int page_align = page_size;
      while (page_align < bytes) page_align *= 2;
      auto mem = allocate(page_align);
      auto new_arena = (Arena_Allocator *)xmalloc(sizeof(Arena_Allocator)); // @Speed @Temporary: Instead of malloc'ing every-time a new arena is needed, allocate a contiguous memory that holds some amount of arena-allocator struct and just index into that.

      new_arena->buffer = mem;
      new_arena->size += bytes;
      new_arena->capacity = page_align;

      next->next_arena = new_arena;
      return mem;
   }

   void *mem = (char *)next->buffer+size;
   next->size += bytes;
   return mem;
}

void Arena_Allocator::free_arena() {
   unmap(buffer, capacity);

   for (auto next = next_arena; next;) { // @Leak: Not free'ing the malloc'd arena struct
      unmap(next->buffer, next->capacity);
      next = next->next_arena;
   }
}

Arena_Allocator make_arena(int size) {
   Arena_Allocator ret;
   int page_align = page_size;
   while (page_align < size) page_align *= 2;
   ret.buffer = allocate(page_align);
   ret.capacity = page_align;
   return ret;
}

#endif // ARENA_ALLOC
