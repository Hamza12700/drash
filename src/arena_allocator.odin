package main

import "core:sys/linux"
import "core:slice"
import "core:os"
import "core:mem"

Arena_Checkpoint :: struct {
  cap: uint,
  pos: uint,
}

Arena :: struct {
  buf:  []byte,
  next: ^Arena,

  offset:  uint,
}

Page_Size :uint: 4096 // On Linux

arena_allocator :: proc(arena: ^Arena, init_size: uint = mem.Megabyte) -> mem.Allocator {
  aligned_size := Page_Size;
  if (init_size % aligned_size) != 0 {
    for aligned_size < init_size {
      aligned_size += Page_Size;
    }
  } else {
    aligned_size = init_size; 
  }

  arena.buf = allocate_page(aligned_size);
  return mem.Allocator{arena_allocator_proc, arena};
}

arena_allocator_proc :: proc(allocator_data: rawptr, mode: mem.Allocator_Mode,
                            size, alignment: int, old_memory: rawptr, old_size: int,
                            location := #caller_location) -> ([]byte, mem.Allocator_Error)
{
  arena := cast(^Arena)allocator_data;
  size, alignment := uint(size), uint(alignment);

  switch mode {
  case .Alloc, .Alloc_Non_Zeroed: return arena_alloc(arena, size, alignment, location);
  case .Resize, .Resize_Non_Zeroed:  return arena_resize(arena, uint(old_size), old_memory, size, alignment, location);
	case .Free_All: return arena_free_all(arena, location);

  case .Query_Features, .Query_Info: return nil, .Mode_Not_Implemented;
  case .Free: return nil, .Mode_Not_Implemented;
  }

  return nil, .Mode_Not_Implemented;
}

arena_alloc :: proc(arena: ^Arena, size, alignment: uint, location := #caller_location) -> ([]byte, mem.Allocator_Error) {
  assert(alignment == 1);

  arena_cap: uint = len(arena.buf);
  if (size+arena.offset) <= arena_cap {
    mem := arena.buf[arena.offset:][:size];
    arena.offset += size;
    return mem, .None;
  }

  if arena.next == nil {
    new_size: uint = arena_cap * 4;
    for new_size < size {
      new_size *= 4;
    }

    mem := allocate_page(new_size);
    new_arena, err := new(Arena); // @Temporary: Should be using a memory-pool here
    assert(err == .None);

    new_arena.buf = mem;
    new_arena.offset = size;
    arena.next = new_arena;
    return mem, .None;
  }

  arena_next := arena.next;
  for arena_next != nil {
    if (arena_next.offset+size) <= arena_cap {
      mem := arena_next.buf[arena_next.offset:size];
      arena_next.offset += size;
      return mem, .None;
    }

    if arena_next.next == nil { break; };
    arena_next = arena_next.next;
  }

  new_size: uint = len(arena_next.buf) * 4;
  for new_size < size {
    new_size *= 4;
  }

  mem := allocate_page(new_size);
  new_arena, err := new(Arena); // @Temporary: Should be using a memory-pool here
  assert(err == .None);

  new_arena.buf = mem;
  new_arena.offset = size;
  arena_next.next = new_arena;
  return mem, .None;
}

arena_resize :: proc(arena: ^Arena, old_size: uint, old_mem: rawptr, new_size, alignment: uint, loc := #caller_location) -> ([]byte, mem.Allocator_Error) {
  assert(alignment == 1);
  assert(new_size >= old_size); // Can't shrink it

  current_pos := uintptr(arena.offset) + uintptr(slice.as_ptr(arena.buf));
  old_pos := uintptr(old_mem) + uintptr(old_size);
  if current_pos == old_pos {
    mem := arena.buf[arena.offset-old_size:][:new_size];
    arena.offset += new_size-old_size;
    return mem, .None;
  }

  mem, err := arena_alloc(arena, new_size, alignment, loc);
  if err != .None { return nil, err; }

  copy(mem, slice.bytes_from_ptr(old_mem, int(old_size)));
  return mem, .None;
}

arena_free_all :: proc(arena: ^Arena, loc := #caller_location) -> ([]byte, mem.Allocator_Error) {
  return nil, .None;
}

allocate_page :: proc(size: uint) -> []byte {
  using linux;
  assert((size % Page_Size) == 0);

  mem_ptr, err := mmap(0, size, {.READ, .WRITE}, {.PRIVATE, .ANONYMOUS});
  assert(err == .NONE);

  return slice.bytes_from_ptr(mem_ptr, int(size));
}

deallocate_page :: proc(mem: []byte) {
  err := linux.munmap(slice.as_ptr(mem), len(mem));
  assert(err == .NONE);
}

deallocate_raw_page :: proc(mem_ptr: rawptr, mem_len: uint) {
  assert((mem_len % Page_Size) == 0);
  err := linux.munmap(mem_ptr, mem_len);
  assert(err == .NONE);
}
