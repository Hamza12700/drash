#ifndef ARRAY_H
#define ARRAY_H

#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "types.cpp"
#include "strings.cpp"

//
// @Factor:
//
// Make the Temp-Array allocates 500 size_of (T) on the stack and when it grows more than
// that then, allocate memory on the Heap or use the custom allocator if provided.
//
// -Hamza, 10 March, 2025
//

struct Array {
   char *ptr = NULL;
   uint size = 0;
   bool custom_allocator = false;

   ~Array() {
      if (!custom_allocator) {
         free(ptr); 
         size = 0;
      }
   }

   static Array make(const uint size) {
      Array ret;
      void *mem = xmalloc(size);

      ret.ptr = static_cast <char *>(mem);
      ret.size = size;
      return ret;
   }

   static Array make(Fixed_Allocator *allocator, const uint size) {
      Array ret;
      void *mem = allocator->alloc(size);

      ret.size = size;
      ret.ptr = static_cast <char *>(mem);
      ret.custom_allocator = true;
      return ret;
   }

   // Shallow copy
   String to_string() {
      // NOTE: Do not shallow copy the string if it's malloc'd because it will be free'd at the end of the scope!
      if (!custom_allocator) {
         fprintf(stderr, "temp-array - attempted to shallow copy the string which will be free'd at the end of the scope!");
         STOP;
      }

      return String {
         .buf = ptr,
         .capacity = size
      };
   }

   void skip(const uint idx = 1) {
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

   void operator= (String *string) {
      if (string->len() > size) {
         fprintf(stderr, "array: operator '=' - 'const char *' length exceeds the Array->size: %u \n", string->len());
         fprintf(stderr, "max-array size is '%u'.\n", size);
         STOP;
      }

      memcpy(ptr, string->buf, string->nlen());
   }

   void operator= (const String *string) {
      if (string->len() > size) {
         fprintf(stderr, "array: operator '=' - 'const char *' length exceeds the Array->size: %u \n", string->len());
         fprintf(stderr, "max-array size is '%u'.\n", size);
         STOP;
      }

      memcpy(ptr, string->buf, string->len());
   }

   void operator= (const char *s) {
      const uint str_len = strlen(s);

      if (str_len > size) {
         fprintf(stderr, "array: operator '=' - 'const char *' length exceeds the Array->size: %u \n", str_len);
         fprintf(stderr, "max-array size is '%u'.\n", size);
         STOP;
      }

      strcpy(ptr, s);
   }
};

#endif
