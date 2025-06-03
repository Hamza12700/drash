#ifndef ARENA_ALLOC
#define ARENA_ALLOC

#include <sys/mman.h>
#include "types.cpp"
#include "allocator_interface.cpp"

struct Arena {
   void  *buf;
   Arena *next;

   uint cap;
   uint pos;

   void *alloc(uint size, Allocator *context = NULL);
   void reset(uint pos); // Only, set's the current allocator position to 'pos'
   void arena_free();
   Allocator  allocator();
};

void *arena_alloc(void *context, uint size) {
   auto arena = (Arena *)context;
   return arena->alloc(size);
}

void *arena_alloc(void *context, uint size, Allocator *with_context = NULL) {
   auto arena = (Arena *)context;
   return arena->alloc(size, with_context);
}

void arena_arena_reset(void *context, uint pos) {
   auto arena = (Arena *)context;
   return arena->reset(pos);
}

void Arena::reset(uint pos) {
   if (this->pos < pos) return;
   else this->pos -= pos;
}

void Arena::arena_free() {
   unmap(buf, cap);
   auto *arena = next;
   while (arena) { // @Leak: not free'ing the malloc'd 'Arena' strcut  :ArenaRemoveMalloc
      unmap(arena->buf, arena->cap);
      arena = arena->next;
   }
}

Allocator Arena::allocator() {
   Allocator ret = {};

   ret.context  = this;
   ret.fn_reset = arena_arena_reset;
   ret.fn_alloc = arena_alloc;

   ret.fn_alloc_with_context = arena_alloc;
   return ret;
}

void *Arena::alloc(uint size, Allocator *context) {
   if (pos+size <= cap) {
      void *mem = (char *)buf+pos;
      memset(mem, 0, size);
      pos += size;

      if (!context) return mem;

      *context = this->allocator();
      return mem;
   }

   if (!next) {
      uint new_size = cap * 4;
      while (new_size < size) new_size *= 2;
      auto mem = allocate(new_size);
      auto new_arena = (Arena *)xcalloc(1, sizeof(Arena)); // @Temporary: We should be using a memory-pool here, instead of calling mallo  :ArenaRemoveMalloc

      new_arena->buf = mem;
      new_arena->cap = new_size;
      new_arena->pos += size;
      next = new_arena;
      if (!context) return mem;

      *context = new_arena->allocator();
      return mem;
   }

   Arena *next_arena = next;
   while (true) {
      if (!next_arena->next) break;
      if (next_arena->pos+size <= next_arena->cap) break;
      next_arena = next_arena->next;
   }

   if (next_arena->pos+size <= next_arena->cap) {
      void *mem = (char *)next_arena->buf + next_arena->pos;
      next_arena->pos += size;
      memset(mem, 0, size);

      if (!context) return mem;
      *context = next_arena->allocator();
      return mem;
   }

   uint new_size = next_arena->cap * 4;
   while (new_size < size)
      new_size *= 2;

   auto mem = allocate(new_size);
   auto new_arena = (Arena *)xcalloc(1, sizeof(Arena)); // :ArenaRemoveMalloc

   new_arena->buf = mem;
   new_arena->pos += size;
   new_arena->cap = new_size;

   next_arena->next = new_arena;
   if (!context) return mem;

   *context = new_arena->allocator();
   return mem;
}

Arena make_arena(uint size) {
   Arena ret = {};
   auto new_size = page_align(size);
   ret.buf = allocate(new_size);
   ret.cap = new_size;
   return ret;
}

#endif // ARENA_ALLOC
