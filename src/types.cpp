#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>

#include "assert.cpp"

#define VERSION "1.0.4"

typedef unsigned int uint;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

const int page_size = getpagesize();

void *xmalloc(uint size) {
  void *mem = malloc(size);
  assert_err(mem == NULL, "malloc - failed");
  return mem;
}

void *xcalloc(int size, int type_size) {
  void *mem = calloc(size, type_size);
  assert_err(mem == NULL, "calloc - failed");
  return mem;
}

void *allocate_align(int size) {
  int page_align_size = page_size;
  while (page_align_size < size) page_align_size *= 2;

  void *memory = mmap(NULL, page_align_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  assert_err(memory == MAP_FAILED, "mmap failed");
  return memory;
}

void *allocate(const int size) {
  void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  assert_err(memory == MAP_FAILED, "mmap failed");
  return memory;
}

void *reallocate_align(void *old_addr, int old_size, int new_size) {
  int page_align_size = page_size;
  while (page_align_size < new_size) page_align_size *= 2;

  auto mem = mremap(old_addr, old_size, page_align_size, MREMAP_MAYMOVE); // This unmap's the old memory
  assert_err(mem == MAP_FAILED, "mremap failed");
  return mem;
}

void *reallocate(void *old_addr, int old_size, int new_size) {
  auto mem = mremap(old_addr, old_size, new_size, MREMAP_MAYMOVE); // This unmap's the old memory
  assert_err(mem == MAP_FAILED, "mremap failed");
  return mem;
}

void unmap(void *addr, int length) {
  assert_err(munmap(addr, length) != 0, "munmap failed");
  addr = NULL; // Because the mapped memory is no longer available the address is invalid, so it is better to set it to null.
}

uint page_align(uint size) {
  uint page_align = page_size;
  while (page_align < size) page_align *= 2;
  return page_align;
}

#endif // TYPES_H
