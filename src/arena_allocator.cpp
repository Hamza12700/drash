#ifndef ARENA_ALLOC
#define ARENA_ALLOC

#include <sys/mman.h>
#include "types.cpp"

struct Arena_Checkpoint {
  // Because, every arena is bigger in size of its parent arena,
  // we can simply check if the capacity of the arena matches and because the
  // linked-list of arena's is sorted we can simply traverse the list to find the match.

  int cap;
  int mark;
};

struct Arena {
  void  *buf;
  Arena *next;

  int cap;
  int pos;

  Arena_Checkpoint checkpoint();
  void restore(Arena_Checkpoint checkpoint);

  void *alloc(long size);
  void arena_free();
};

Arena_Checkpoint Arena::checkpoint() {
  Arena_Checkpoint checkpoint;
  checkpoint.cap = cap;
  checkpoint.mark = pos;
  return checkpoint;
}

void Arena::restore(Arena_Checkpoint checkpoint) {
  if (checkpoint.cap == this->cap) {
    memset((char *)this->buf+checkpoint.mark, 0, this->pos-checkpoint.mark);
    this->pos = checkpoint.mark;
    return;
  }

  auto arena = next;
  while (arena) {
    if (arena->cap == checkpoint.cap) { // @Incomplete: Reset other arena's that are allocated after the checkpoint
      memset((char *)arena->buf+checkpoint.mark, 0,arena->pos-checkpoint.mark);
      arena->pos = checkpoint.mark;
      return;
    }

    arena = arena->next;
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

void *Arena::alloc(long size) {
  if (pos+size <= cap) {
    void *mem = (char *)buf+pos;
    pos += size;

    return mem;
  }

  if (!next) {
    int new_size = cap * 4;
    while (new_size < size) new_size *= 4;
    auto mem = allocate(new_size);
    auto new_arena = (Arena *)xcalloc(1, sizeof(Arena)); // @Temporary: We should be using a memory-pool here, instead of calling mallo  :ArenaRemoveMalloc

    new_arena->buf = mem;
    new_arena->cap = new_size;
    new_arena->pos += size;
    next = new_arena;

    return mem;
  }

  Arena *next_arena = next;
  while (true) {
    if (next_arena->pos+size <= next_arena->cap) {
      void *mem = (char *)next_arena->buf + next_arena->pos;
      next_arena->pos += size;
      return mem;
    }

    if (!next_arena->next) break;
    next_arena = next_arena->next;
  }

  int new_size = next_arena->cap * 4;
  while (new_size < size)
    new_size *= 4;

  auto mem = allocate(new_size);
  auto new_arena = (Arena *)xcalloc(1, sizeof(Arena)); // :ArenaRemoveMalloc

  new_arena->buf = mem;
  new_arena->pos += size;
  new_arena->cap = new_size;
  next_arena->next = new_arena;

  return mem;
}

Arena make_arena(int size) {
  Arena ret = {};
  auto new_size = page_align(size);
  ret.buf = allocate(new_size);
  ret.cap = new_size;
  return ret;
}

#endif // ARENA_ALLOC
