#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "types.cpp"

struct Allocator {
   void *context;

   void *alloc(uint size);
   void *(*fn_alloc)(void *, uint);

   void *alloc(uint size, Allocator *context = NULL);
   void *(*fn_alloc_with_context)(void *, uint, Allocator *);

   // Set the current allocator position to 'pos'
   void reset(uint pos);
   void (*fn_reset)(void *, uint);
};

void *Allocator::alloc(uint size) {
   return fn_alloc(context, size);
}

void *Allocator::alloc(uint size, Allocator *with_context) {
   return fn_alloc_with_context(context, size, with_context);
}

void Allocator::reset(uint pos) { return fn_reset(context, pos); }

#endif
