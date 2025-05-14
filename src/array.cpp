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
   i16 size = 0; // Points at the last element of the array
   i16 capacity = 0;

   bool with_allocator = false;

   ~Array() {
      if (!with_allocator) {
         free(ptr);
         size = 0;
         capacity = 0;
      }
   }

   void push(T value) {
      if (size+1 > capacity) {
         fprintf(stderr, "array - out of space\n");
         fprintf(stderr, "capacity is '%u'\n", capacity);
         STOP;
      }

      ptr[size*sizeof(T)] = value;
      size += 1;
   }

   T& operator[] (int idx) {
      if (idx >= capacity) {
         fprintf(stderr, "array - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "capacity is '%u'.\n", capacity);
         STOP;
      }

      return ptr[idx*sizeof(T)];
   }
};

template <typename T>
Array<T> make_array(Fixed_Allocator *allocator, uint num) {
   void *mem = allocator->alloc(num * sizeof(T));

   Array<T> ret;
   ret.ptr = static_cast<T *> (mem);
   ret.capacity = num;
   ret.with_allocator = true;
   return ret;
}

#endif
