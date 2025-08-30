package main

import "core:sys/linux"
import "core:slice"
import "core:os"
import "core:mem"
import "core:fmt"
import "base:intrinsics"
import rt "base:runtime"

PAGE_SIZE :uint: 4096 // On pretty-much every Linux-System

Arena :: struct {
  backing_buffer: rawptr,
  reserve_size: u32, // Size of the allocated backing buffer
  commit_size: u32,  // Used memory

  prev: ^Arena,    // previous arena in chain
  current: ^Arena, // current arena in chain
}

// - arena globals
ARENA_DEFAULT_RESERVE_SIZE :u32: mem.Megabyte * 2;

// - arena creation/destruction
arena_allocate :: proc(reserve_size: u32 = ARENA_DEFAULT_RESERVE_SIZE) -> ^Arena {
  reserve_size := reserve_size;
  reserve_size = u32(align_pow2(uint(reserve_size), PAGE_SIZE));
  backing_buffer := allocate_page(uint(reserve_size));
  assert(backing_buffer != nil);

  arena := cast(^Arena) backing_buffer;
  arena.backing_buffer = backing_buffer;
  arena.current = arena;
  arena.commit_size = size_of(Arena);
  return arena;
}

arena_destroy :: proc(arena: ^Arena) {
  for x := arena.current; x != nil; x = arena.prev {
    linux.munmap(x.backing_buffer, uint(x.reserve_size));
  }
}

// - arena standard library interface and procedure
arena_allocator_interface :: proc(arena: ^Arena) -> mem.Allocator {
  return mem.Allocator{arena_allocator_proc, arena};
}

arena_allocator_proc :: proc(allocator_data: rawptr, mode: mem.Allocator_Mode,
                             size, alignment: int, old_memory: rawptr, old_size: int,
                             location := #caller_location) -> ([]byte, mem.Allocator_Error)
{
  arena := cast(^Arena)allocator_data;
  size, align := uint(size), uint(alignment);
  switch mode {
  case .Alloc,  .Alloc_Non_Zeroed:   return arena_push(arena, size, align);
  case .Resize, .Resize_Non_Zeroed:  return arena_resize(arena, old_memory, uint(old_size), size, align);
  case .Free:                        return arena_pop(arena, u32(old_size));
  case .Free_All: {
    arena_destroy(arena);
    return nil, .None;
  };

  case .Query_Features, .Query_Info: return nil, .Mode_Not_Implemented;
  }
  return nil, .Mode_Not_Implemented;
}

// - arena push/pop core functions
arena_push :: proc(arena: ^Arena, size, align: uint) -> ([]byte, mem.Allocator_Error) {
  current := arena.current;
  align_size := align_pow2(size, align);

  if (align_size + uint(current.commit_size)) > auto_cast current.reserve_size {
    res_size := align_pow2(align_size + size_of(Arena), align);
    new_arena := arena_allocate(u32(res_size) + ARENA_DEFAULT_RESERVE_SIZE);
    new_arena.prev = arena;
    arena.current = new_arena;
    new_arena.current = new_arena;
    current = new_arena;
  }

  mem_offset := uintptr(current.backing_buffer) + uintptr(current.commit_size);
  mem := slice.bytes_from_ptr(rawptr(mem_offset), int(align_size));
  current.commit_size += u32(align_size);
  return mem, .None;
}

arena_pop :: proc(arena: ^Arena, size: u32) -> ([]byte, mem.Allocator_Error) {
  current := arena.current;
  assert((current.commit_size + size) < current.reserve_size); // Sanity check
  ptr_offset := uintptr(current.commit_size) + uintptr(size);
  intrinsics.mem_zero(rawptr(ptr_offset), size - current.commit_size);
  return nil, .None;
}

// resize in-place if the previous allocation is the last
arena_resize :: proc(arena: ^Arena, old_mem: rawptr, old_size, size, align: uint) -> ([]byte, mem.Allocator_Error) {
  current := arena.current;
  prev_pos := uintptr(old_mem);
  cur_pos := uintptr(current.backing_buffer) + uintptr(current.commit_size - u32(old_size));
  align_size := align_pow2(size, align);

  if prev_pos == cur_pos {
    assert(align_size > old_size); // Can't shrink it (for now)
    current.commit_size += u32(align_size);
    return slice.bytes_from_ptr(rawptr(cur_pos), int(align_size)), .None;
  }
  return arena_push(arena, align_size, align);
}

// - temporary arena scopes
Temp_Arena :: struct {
  arena: ^Arena,
  pos: u32,
}

temp_begin :: proc(arena: ^Arena) -> Temp_Arena {
  return {
    arena = arena,
    pos = arena.current.commit_size,
  };
}

temp_end :: proc(temp: Temp_Arena) {
  arena_pop(temp.arena, temp.pos);
}

// - helper functions
@(require_results)
allocate_page :: proc(size: uint) -> rawptr {
  assert((size % PAGE_SIZE) == 0); // Sanity check
  mem_ptr, _ := linux.mmap(0, size, {.READ, .WRITE}, {.PRIVATE, .ANONYMOUS});
  return mem_ptr;
}

align_pow2 :: #force_inline proc(x, align: uint) -> uint {
  return (((x) + (align) - 1)&(~((align) - 1)));
}
