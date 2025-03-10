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

template<typename T>
struct Temp_Array {
  T *ptr = NULL;
  uint size = 0;
  bool with_allocator = false;

  Temp_Array(const uint n) {
    ptr = (char *)malloc(sizeof(T) + n);
    size = n;
  }

  Temp_Array(Fixed_Allocator &allocator, const uint n) {
    ptr = (char *)allocator.alloc(n);
    size = n;
    with_allocator = true;
  }

  ~Temp_Array() {
    if (!with_allocator) free(ptr);
    size = 0;
  }

  uint str_len() const {
    if (typeid(ptr) != typeid(char *)) {
      fprintf(stderr, "temp-array - array is not 'char *' type\n");
      raise(SIGTRAP);
    }

    uint count = 0;
    while (ptr[count] != '\0') count++;

    return count;
  }

  String to_string() {
    if (typeid(ptr) != typeid(char *)) {
      fprintf(stderr, "temp-array - failed to convert type 'T' to String because it's not a 'char *'\n");
      raise(SIGTRAP);
    }

    return String {
      .buf = ptr,
      .len = str_len()
    };
  }

  String copy_string() {
    if (typeid(ptr) != typeid(char *)) {
      fprintf(stderr, "temp-array - failed to convert type 'T' to String because it's not a 'char *'\n");
      raise(SIGTRAP);
    }

    String ret;
    ret.len = str_len();
    memcpy(ret.buf, ptr, ret.len);
    return ret;
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

    memcpy(ptr, string.buf, string.len+1);
  }

  void operator= (const char *s) {
    const uint str_len = strlen(s);

    if (str_len > size) {
      fprintf(stderr, "array: operator '=' - 'const char *' length exceeds the Array->size: %u \n", str_len);
      fprintf(stderr, "max-array size is '%u'.\n", size);
      raise(SIGTRAP);
    }

    for (uint i = 0; i < str_len; i++) {
      ptr[i] = s[i];
    }
  }
};

#endif
