#ifndef ARRAY_H
#define ARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "types.cpp"
#include "fixed_allocator.cpp"

template<typename T>
struct Array {
   T *ptr = NULL;
   uint size = 0;
   bool with_allocator = false;

   ~Array() {
      if (!with_allocator) {
         free(ptr); 
         size = 0;
      }
   }

   void skip(const uint idx) {
      if (idx > size) {
         fprintf(stderr, "array:skip - index is out of bounds: '%u'\n", idx);
         fprintf(stderr, "size of the array is: %u", size);
         STOP;
      }

      ptr += idx;
      size -= idx;
   }

   char& operator[] (const uint idx) {
      if (idx > size) {
         fprintf(stderr, "array - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", size);
         STOP;
      }

      return ptr[idx];
   }
};

template<typename T>
Array<T> make_array(const uint size) {
   void *mem = xmalloc(sizeof(T) * size);

   return Array<T> {
      .ptr = mem,
   };
}

template<typename T>
Array<T> make_array(Fixed_Allocator *allocator, const uint size) {
   void *mem = allocator->alloc(sizeof(T) * size);

   return Array<T> {
      .ptr = mem,
      .with_allocator = true,
   };
}

#endif
