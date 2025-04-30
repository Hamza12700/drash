#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

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

void *xmalloc(const uint size) {
   void *mem = malloc(size);
   assert_err(mem == NULL, "malloc - failed");
   return mem;
}

void *xcalloc(const uint number, const uint size) {
   void *mem = calloc(number, size);
   assert_err(mem == NULL, "calloc - failed");
   return mem;
}

#ifdef DEBUG
#define STOP raise(SIGTRAP)

#else
#define STOP exit(-1)

#endif // DEBUG

#endif // TYPES_H
