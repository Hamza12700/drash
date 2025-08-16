package main

import "core:sys/linux"
import "core:slice"
import "core:os"
import "core:mem"
import "base:runtime"

Arena_Checkpoint :: struct {
  ptr: rawptr,
  prev_offset: uint,
}

Arena :: struct {
  buf:  []byte,
  next: ^Arena,

  offset:  uint,
}

PAGE_SIZE :uint: 4096 // On pretty-much every Linux-System

arena_allocator :: proc(arena: ^Arena, init_size: uint = mem.Megabyte) -> mem.Allocator {
  aligned_size := PAGE_SIZE;
  if (init_size % aligned_size) != 0 {
    for aligned_size < init_size {
      aligned_size += PAGE_SIZE;
    }
  } else {
    aligned_size = init_size; 
  }

  arena.buf = allocate_page(aligned_size);
  return mem.Allocator{arena_allocator_proc, arena};
}

arena_checkpoint :: proc(arena: ^Arena) -> Arena_Checkpoint {
  return {
    ptr = mem.ptr_offset(raw_data(arena.buf), arena.offset),
    prev_offset = arena.offset,
  };
}

arena_allocator_proc :: proc(allocator_data: rawptr, mode: mem.Allocator_Mode,
                            size, alignment: int, old_memory: rawptr, old_size: int,
                            location := #caller_location) -> ([]byte, mem.Allocator_Error)
{
  arena := cast(^Arena)allocator_data;
  size, align := uint(size), uint(alignment);

  switch mode {
  case .Alloc, .Alloc_Non_Zeroed: return arena_alloc(arena, size, align);
  case .Resize, .Resize_Non_Zeroed:  return arena_resize(arena, uint(old_size), old_memory, size, align);
	case .Free_All: return arena_clear_all(arena);
  case .Free: return arena_clear_at(arena, old_memory, old_size);

  case .Query_Features, .Query_Info: return nil, .Mode_Not_Implemented;
  }

  return nil, .Mode_Not_Implemented;
}

arena_restore :: proc(arena: ^Arena, checkpoint: Arena_Checkpoint) {
  // It is kind-of stupid to call 'mem.free_with_size' because under the hood it would
  // just call 'arena_clear_at' but because the core library Tracking-Allocator operators
  // on the standard 'Allocator' interface to trace memory usage that's why it's like this.
  mem.free_with_size(checkpoint.ptr, int(arena.offset));
  arena.offset = checkpoint.prev_offset;
}

arena_clear_at :: proc(arena: ^Arena, start_addr: rawptr, len: int) -> ([]byte, mem.Allocator_Error) {
  if len <= 0 do return nil, .None;
  runtime.mem_zero(start_addr, len);
  return nil, .None;
}

arena_alloc :: proc(arena: ^Arena, size, alignment: uint) -> ([]byte, mem.Allocator_Error) {
  align_size := alignment;

  if (size % align_size) != 0 {
    for align_size < size { align_size += alignment; }
  } else {
    align_size = size; 
  }

  arena_cap: uint = len(arena.buf);
  if (align_size+arena.offset) <= arena_cap {
    mem := arena.buf[arena.offset:][:align_size];
    arena.offset += align_size;
    return mem, .None;
  }

  if arena.next == nil {
    new_size: uint = arena_cap * 4;
    for new_size < align_size {
      new_size *= 4;
    }

    mem := allocate_page(new_size);
    new_arena, err := new(Arena); // @Temporary: Should be using a memory-pool here
    assert(err == .None);

    new_arena.buf = mem;
    new_arena.offset = align_size;
    arena.next = new_arena;
    return mem, .None;
  }

  arena_next := arena.next;
  for arena_next != nil {
    if (arena_next.offset+size) <= arena_cap {
      mem := arena_next.buf[arena_next.offset:][:align_size];
      arena_next.offset += align_size;
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
  new_arena.offset = align_size;
  arena_next.next = new_arena;
  return mem, .None;
}

arena_resize :: proc(arena: ^Arena, old_size: uint, old_mem: rawptr, new_size, alignment: uint) -> ([]byte, mem.Allocator_Error) {
  assert(alignment == 1);
  assert(new_size >= old_size); // Can't shrink it

  current_pos := uintptr(arena.offset) + uintptr(slice.as_ptr(arena.buf));
  old_pos := uintptr(old_mem) + uintptr(old_size);

  if current_pos == old_pos && arena.offset + (new_size-old_size) <= len(arena.buf) {
    mem := arena.buf[arena.offset-old_size:][:new_size];
    arena.offset += new_size-old_size;
    return mem, .None;
  }

  mem, err := arena_alloc(arena, new_size, alignment);
  if err != .None { return nil, err; }

  copy(mem, slice.bytes_from_ptr(old_mem, int(old_size)));
  return mem, .None;
}

// Resets the arena and other arena's to zero
arena_clear_all :: proc(arena: ^Arena) -> ([]byte, mem.Allocator_Error) {
  runtime.mem_zero(raw_data(arena.buf), int(arena.offset));
  arena.offset = 0;

  next := arena.next;
  for next != nil {
    runtime.mem_zero(raw_data(next.buf), int(next.offset));
    next.offset = 0;
    next = next.next;
  }

  return nil, .None;
}

@(require_results)
allocate_page :: proc(size: uint) -> []byte {
  using linux;
  assert((size % PAGE_SIZE) == 0);

  mem_ptr, err := mmap(0, size, {.READ, .WRITE}, {.PRIVATE, .ANONYMOUS});
  assert(err == .NONE);

  return slice.bytes_from_ptr(mem_ptr, int(size));
}

deallocate_page :: proc(mem: []byte) {
  err := linux.munmap(slice.as_ptr(mem), len(mem));
  assert(err == .NONE);
}

deallocate_raw_page :: proc(mem_ptr: rawptr, mem_len: uint) {
  assert((mem_len % PAGE_SIZE) == 0);
  err := linux.munmap(mem_ptr, mem_len);
  assert(err == .NONE);
}
