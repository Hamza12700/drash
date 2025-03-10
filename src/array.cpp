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

struct Temp_Array {
  char *ptr = NULL;
  uint size = 0;
  bool with_allocator = false;

  void array_free() {
    if (with_allocator) return;

    free(ptr);
    size = 0;
  }

  String copy_string(Fixed_Allocator allocator) {
    char *buf = (char *)allocator.alloc(size);

    String ret;
    ret.len = size;
    ret.buf = buf;

    return ret;
  }

  String to_string() {
    return String {
      .buf = ptr,
      .len = size
    };
  }

  void skip(const uint idx = 1) {
    ptr += idx;
  }

  char& operator[] (const uint idx) {
    if (idx > size) {
      fprintf(stderr, "array - attempted to index into position '%u' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%u'.\n", size);
      raise(SIGTRAP);
    }

    return ptr[idx];
  }

  void operator= (String &string) {
    if (string.len > size) {
      fprintf(stderr, "array: operator '=' - 'const char *' length exceeds the Array->size: %u \n", string.len);
      fprintf(stderr, "max-array size is '%u'.\n", size);
      raise(SIGTRAP);
    }

    memcpy(ptr, string.buf, string.len);
  }

  void operator= (const String &string) {
    if (string.len > size) {
      fprintf(stderr, "array: operator '=' - 'const char *' length exceeds the Array->size: %u \n", string.len);
      fprintf(stderr, "max-array size is '%u'.\n", size);
      raise(SIGTRAP);
    }

    memcpy(ptr, string.buf, string.len);
  }

  void operator= (const char *s) {
    const uint str_len = strlen(s);

    if (str_len > size) {
      fprintf(stderr, "array: operator '=' - 'const char *' length exceeds the Array->size: %u \n", str_len);
      fprintf(stderr, "max-array size is '%u'.\n", size);
      raise(SIGTRAP);
    }

    strcpy(ptr, s);
  }
};

Temp_Array temp_array(const uint size) {
  Temp_Array ret;
  void *mem = malloc(size);

  ret.ptr = (char *)mem;
  ret.size = size;
  return ret;
}

Temp_Array temp_array(Fixed_Allocator *allocator, const uint size) {
  Temp_Array ret;
  void *mem = allocator->alloc(size);

  ret.size = size;
  ret.ptr = (char *)mem;
  return ret;
}

#endif
