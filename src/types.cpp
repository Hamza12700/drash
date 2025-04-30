#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include "assert.cpp"

#define VERSION "1.0.0"

typedef unsigned int uint;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define MAX_U8  1 << 8
#define MAX_U16 1 << 16
#define MAX_U32 1 << 32
#define MAX_U64 1 << 64

const int page_size = getpagesize();

void *xmalloc(uint size) {
   void *mem = malloc(size);
   assert_err(mem == NULL, "malloc - failed");
   return mem;
}

void *xcalloc(uint number, uint size) {
   void *mem = calloc(number, size);
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

void *realloc_align(void *old_addr, int old_size, int new_size) {
   int page_align_size = page_size;
   while (page_align_size < new_size) page_align_size *= 2;

   auto mem = mremap(old_addr, old_size, page_align_size, MREMAP_MAYMOVE); // This unmap's the old memory
   assert_err(mem == MAP_FAILED, "mremap failed");
   return mem;
}

void *realloc(void *old_addr, int old_size, int new_size) {
   auto mem = mremap(old_addr, old_size, new_size, MREMAP_MAYMOVE); // This unmap's the old memory
   assert_err(mem == MAP_FAILED, "mremap failed");
   return mem;
}

void unmap(void *addr, int length) {
   assert_err(munmap(addr, length) != 0, "munmap failed");
}

#ifdef DEBUG
#define STOP raise(SIGTRAP)

#else
#define STOP exit(-1)

#endif // DEBUG

#endif // TYPES_H
