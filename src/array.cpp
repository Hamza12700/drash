#ifndef ARRAY_H
#define ARRAY_H

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "types.cpp"

template<typename T>
struct Array {
   T *ptr = NULL;
   u16 size = 0;
   bool with_allocator = false;

   Array() {
      void *mem = xmalloc(sizeof(T));
      ptr = mem;
      size = 1;
   }

   ~Array() {
      if (!with_allocator) {
         free(ptr); 
         size = 0;
      }
   }

   void push(T value) {
      ptr[size+1] = value;
   }

   T& operator[] (const uint idx) {
      if (idx > size) {
         fprintf(stderr, "array - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", size);
         STOP;
      }

      if (idx == 0) return ptr[idx];
      return ptr[idx+sizeof(T)];
   }
};

#endif
