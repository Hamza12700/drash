package main

import "core:mem"
import "core:sys/linux"

PAGE_SIZE :: mem.DEFAULT_PAGE_SIZE;

Arena :: struct {
  prev: ^Arena,
  next: ^Arena,
  offset: int,
  cap:    int // Capacity of the 'current' Arena
}

Temp_Arena :: struct {
  arena: ^Arena,
  prev_offset: int,
}

arena_alloc_and_init :: proc(size: int) -> ^Arena {
  aligned_size := align_pow2(size, mem.DEFAULT_PAGE_SIZE);
  mem_ptr, errno := linux.mmap(0, uint(aligned_size), {.READ, .WRITE}, {.PRIVATE, .ANONYMOUS}, -1);
  assert(errno == .NONE);

  arena := cast(^Arena)mem_ptr;
  membuf := mem.byte_slice(mem_ptr, aligned_size);
  arena.cap = aligned_size;
  arena.offset += size_of(mem.Arena);
  return arena;
}

arena_alloc_bytes :: proc(
	arena:       ^Arena,
	size:        int,
	alignment := mem.DEFAULT_ALIGNMENT,
	loc       := #caller_location,
) -> ([]byte, mem.Allocator_Error)
{
  arena := arena;
  membuf := mem.byte_slice(rawptr(arena), arena.cap);
	#no_bounds_check end := &membuf[arena.offset]
	ptr := mem.align_forward(end, uintptr(alignment))
	total_size := size + mem.ptr_sub((^byte)(ptr), (^byte)(end))
	if arena.offset + total_size > len(membuf) {
    tmp := arena;
    arena = arena_alloc_and_init(size+arena.cap*4);
    arena.prev = tmp;
    return arena_alloc_bytes(arena, size, alignment, loc);
	}
	arena.offset += total_size
	result := mem.byte_slice(ptr, size)
	// ensure_poisoned(result)
	// sanitizer.address_unpoison(result)
	return result, nil
}

arena_free_all :: proc(arena: ^Arena) {
  mem.zero(arena, arena.cap);
  arena.offset = 0;
}

arena_allocator :: proc(arena: ^Arena) -> mem.Allocator {
  return {
    procedure = arena_allocator_proc,
    data = arena,
  }
}

arena_temp_begin :: proc(arena: ^Arena) -> Temp_Arena {
  return {
    arena = arena,
    prev_offset = arena.offset
  }
}

arena_temp_end :: proc(temp: Temp_Arena) {
  arena := temp.arena;
  if arena.offset > temp.prev_offset {
    offset := uintptr(arena) + uintptr(temp.prev_offset);
    mem.zero(rawptr(offset), arena.offset-temp.prev_offset);
    arena.offset = temp.prev_offset;
    return;
  }
}

arena_allocator_proc :: proc(
	allocator_data: rawptr,
	mode:           mem.Allocator_Mode,
	size:           int,
	alignment:      int,
	old_memory:     rawptr,
	old_size:       int,
	loc := #caller_location,
) -> ([]byte, mem.Allocator_Error)
{
	arena := cast(^Arena)allocator_data
	switch mode {
	case .Alloc, .Alloc_Non_Zeroed:
    return arena_alloc_bytes(arena, size, alignment, loc)
	case .Free:
		return nil, .Mode_Not_Implemented
	case .Free_All:
		arena_free_all(arena)
	case .Resize:
		return mem.default_resize_bytes_align(mem.byte_slice(old_memory, old_size), size, alignment, arena_allocator(arena), loc)
	case .Resize_Non_Zeroed:
		return mem.default_resize_bytes_align_non_zeroed(mem.byte_slice(old_memory, old_size), size, alignment, arena_allocator(arena), loc)
	case .Query_Features:
		set := (^mem.Allocator_Mode_Set)(old_memory)
		if set != nil {
			set^ = {.Alloc, .Alloc_Non_Zeroed, .Free_All, .Resize, .Resize_Non_Zeroed, .Query_Features}
		}
		return nil, nil
	case .Query_Info:
		return nil, .Mode_Not_Implemented
	}
	return nil, nil
}

align_pow2 :: #force_inline proc(x, b: int) -> int {
  return (x+(b-1)) & (~(b-1))
}
