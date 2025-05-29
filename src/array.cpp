#ifndef ARRAY_H
#define ARRAY_H

#include <csignal>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "types.cpp"

template<typename T>
struct Array {
   T *buf = NULL;
   u16 pos = 0; // Points at the last element of the array
   u16 cap = 0;

   enum Alloc_Type {
      MALLOC,
      ARENA,
   };

   Alloc_Type type = MALLOC;

   ~Array() {
      if (type == MALLOC) {
         free(buf);
         buf = NULL;
         pos = 0;
         cap = 0;
      }
   }

   void push(T value) {
      if (pos >= cap) {
         fprintf(stderr, "array - out of space\n");
         fprintf(stderr, "capacity is '%u'\n", cap);
         raise(SIGABRT);
      }

      buf[pos*sizeof(T)] = value;
      pos += 1;
   }

   T& get_last() { return buf[pos*sizeof(T)]; }

   T& operator[] (const uint idx) {
      if (idx >= cap) {
         fprintf(stderr, "array - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "capacity is '%u'.\n", cap);
         raise(SIGABRT);
      }

      return buf[idx*sizeof(T)];
   }
};

template<typename T>
Array<T> array(u16 size) {
   auto mem = xcalloc(size, sizeof(T));
   Array<T> ret;
   ret.buf = mem;
   ret.cap = size;
   return ret;
}

#endif
