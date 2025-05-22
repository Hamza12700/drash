#ifndef ARENA_ALLOC
#define ARENA_ALLOC

#include <sys/mman.h>
#include "types.cpp"

struct Arena {
   void *buffer = NULL;
   Arena *next = NULL;

   uint pos = 0;
   uint cap = 0;

   void *alloc(int bytes);
   void free_arena();
};

void *Arena::alloc(int bytes) {
   if (pos+bytes <= cap) {
      void *mem = (char *)buffer+pos;
      pos += bytes;
      return mem;
   }

   if (!next) {
      int new_size = cap*2;
      while (new_size < bytes) new_size *= 2;
      auto mem = allocate(new_size);
      auto new_arena = (Arena *)xcalloc(sizeof(Arena), 1); // @Speed @Temporary: Instead of malloc'ing every-time a new arena is needed, allocate a contiguous memory that holds some amount of arena-allocator struct and just index into that.

      new_arena->buffer = mem;
      new_arena->cap = new_size;
      new_arena->pos += bytes;
      next = new_arena;
      return mem;
   }

   Arena *next_arena = next;
   while (true) {
      if (!next_arena->next) break;
      if (next_arena->pos+bytes <= next_arena->cap) break;
      next_arena = next_arena->next;
   }

   if (next_arena->pos+bytes > next_arena->cap) {
      int new_size = next_arena->cap*2;
      while (new_size < bytes) new_size *= 2;

      auto mem = allocate(new_size);
      auto new_arena = (Arena *)xcalloc(sizeof(Arena), 1); // @Speed @Temporary: Instead of malloc'ing every-time a new arena is needed, allocate a contiguous memory that holds some amount of arena-allocator struct and just index into that.

      new_arena->buffer = mem;
      new_arena->pos += bytes;
      new_arena->cap = new_size;

      next_arena->next = new_arena;
      return mem;
   } else {
      void *mem = (char *)next_arena->buffer+next_arena->pos;
      next_arena->pos += bytes;
      return mem;
   }
}

void Arena::free_arena() {
   unmap(buffer, cap);
   Arena *next_arena = next;
   while (true) { // @Leak: Not free'ing the malloc'd arena struct
      if (!next_arena) return;
      unmap(next_arena->buffer, next_arena->cap);
      next_arena = next_arena->next;
   }
}

Arena make_arena(int size) {
   Arena ret;
   int page_align = page_size;
   while (page_align < size) page_align *= 2;
   ret.buffer = allocate(page_align);
   ret.cap = page_align;
   return ret;
}

#endif // ARENA_ALLOC
