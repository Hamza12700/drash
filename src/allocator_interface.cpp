#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "types.cpp"

struct Allocator {

   struct Vtable {
      void *(*fn_alloc)(void *, uint);
      void *(*fn_alloc_with_context)(void *, uint, Allocator *);

      // Set the current allocator position to 'pos'
      void (*fn_reset)(void *, uint);
   };

   void *context;
   Vtable *vtable;

   void *alloc(uint size) {
      return vtable->fn_alloc(context, size);
   };

   void reset(uint pos) {
      return vtable->fn_reset(context, pos); 
   };

   void *alloc(uint size, Allocator *with_context = NULL) {
      return vtable->fn_alloc_with_context(context, size, with_context);
   };
};

#endif
