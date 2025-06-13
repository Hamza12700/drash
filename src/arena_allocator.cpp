#ifndef ARENA_ALLOC
#define ARENA_ALLOC

#include <sys/mman.h>
#include "types.cpp"

struct Arena_Checkpoint {
  // Because, every arena is 4x times bigger in size of its parent arena,
  // we can simply check if the capacity of the arena matches and because the
  // linked-list of arena's is in ascending order if can simply traverse the list to find the match.

  uint cap;
  uint mark;
};

struct Arena {
   void  *buf;
   Arena *next;
   Arena *prev;
   Arena *last; // The last one to allocate memory. If null then the last one is the current arena

   uint cap;
   uint pos;

   Arena_Checkpoint checkpoint();
   void restore(Arena_Checkpoint checkpoint); // Sets the position to checkpoint.

   void *alloc(uint size, Arena *context = NULL);
   void arena_free();
};

Arena_Checkpoint Arena::checkpoint() {
   Arena_Checkpoint checkpoint;
   if (last) {
      checkpoint.cap = last->cap;
      checkpoint.mark = last->pos;
   } else {
      checkpoint.cap = cap;
      checkpoint.mark = pos;
   }

   return checkpoint;
}

void Arena::restore(Arena_Checkpoint checkpoint) {
   if (!last) {
      memset((char *)buf+checkpoint.mark, 0, pos-checkpoint.mark);
      pos = checkpoint.mark;
      return;
   }

   if (checkpoint.cap == last->cap) {
      memset((char *)last->buf+checkpoint.mark, 0, last->pos-checkpoint.mark);
      last->pos = checkpoint.mark;
      return;
   }

   auto arena = last;
   while (true) {
      if (arena->cap > checkpoint.cap) arena = arena->prev;
      else if (arena->cap < checkpoint.cap) arena = arena->next;

      // @Cleanup:
      // For now, we only reset the position of the matched arena but this is
      // wrong if the arena allocated a new arena before the checkpoint then the
      // newly created arena and beyond that should get reset to zero. :ArenaMemory
      else if (arena->cap == checkpoint.cap) {
         memset((char *)arena->buf+checkpoint.mark, 0, arena->pos-checkpoint.mark);
         arena->pos = checkpoint.mark;
         return;
      }
   }
}

void Arena::arena_free() {
   unmap(buf, cap);
   auto *arena = next;
   while (arena) { // @Leak: not free'ing the malloc'd 'Arena' strcut  :ArenaRemoveMalloc
      unmap(arena->buf, arena->cap);
      arena = arena->next;
   }
}

void *Arena::alloc(uint size, Arena *context) {
   if (pos+size <= cap) {
      void *mem = (char *)buf+pos;
      pos += size;

      if (!context) return mem;

      context = this;
      return mem;
   }

   if (!next) {
      uint new_size = cap * 4;
      while (new_size < size) new_size *= 4;
      auto mem = allocate(new_size);
      auto new_arena = (Arena *)xcalloc(1, sizeof(Arena)); // @Temporary: We should be using a memory-pool here, instead of calling mallo  :ArenaRemoveMalloc

      new_arena->buf = mem;
      new_arena->cap = new_size;
      new_arena->pos += size;
      new_arena->prev = this;
      next = new_arena;
      last = new_arena;

      if (!context) return mem;

      context = new_arena;
      return mem;
   }

   Arena *next_arena = next;
   while (true) {
      if (next_arena->pos+size <= next_arena->cap) {
         void *mem = (char *)next_arena->buf + next_arena->pos;
         next_arena->pos += size;

         last = next_arena;
         if (!context) return mem;
         context = next_arena;
         return mem;
      }

      if (!next_arena->next) break;
      next_arena = next_arena->next;
   }

   uint new_size = next_arena->cap * 4;
   while (new_size < size)
      new_size *= 4;

   auto mem = allocate(new_size);
   auto new_arena = (Arena *)xcalloc(1, sizeof(Arena)); // :ArenaRemoveMalloc

   new_arena->buf = mem;
   new_arena->pos += size;
   new_arena->cap = new_size;
   new_arena->prev = this;
   last = new_arena;
   next_arena->next = new_arena;

   if (!context) return mem;

   context = new_arena;
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
