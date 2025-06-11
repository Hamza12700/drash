#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "types.cpp"

struct Allocator {

   struct Vtable {
      void *(*fn_alloc)(void *, uint);
      void *(*fn_alloc_with_context)(void *, uint, Allocator *);
   };

   void *context;
   Vtable *vtable;

   void *alloc(uint size) {
      return vtable->fn_alloc(context, size);
   };

   void *alloc(uint size, Allocator *with_context = NULL) {
      return vtable->fn_alloc_with_context(context, size, with_context);
   };
};

#endif
