#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

#include "assert.c"

typedef unsigned int uint;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

void *xmalloc(const uint size) {
   void *mem = malloc(size);
   assert_err(mem == NULL, "malloc - failed");
   return mem;
}

#ifdef DEBUG
#define STOP raise(SIGTRAP);

#else
#define STOP exit(-1);

#endif // DEBUG

#endif // TYPES_H
