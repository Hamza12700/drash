#ifndef STRINGS_H
#define STRINGS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <iterator>

#include "fixed_allocator.cpp"
#include "types.cpp"

struct String {
   char *buf = NULL;
   uint capacity = 0;
   uint current_idx = 0;

   static String with_size(Fixed_Allocator *allocator, const uint size) {
      return String {
         .buf = static_cast <char *>(allocator->alloc(sizeof(char) + size)),
         .capacity = (uint)sizeof(char) + size,
      };
   }

   // Return the length of the string and excluding the null-byte
   uint len() const {
      uint count = 0;

      while (buf[count] != '\0') count++;
      return count;
   }

   // Return the length of the string including the null-byte
   uint nlen() const {
      uint count = 0;
      while (buf[count] != '\0') count++;
      count += 1;

      return count;
   }

   char& operator[] (const uint idx) {
      if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP
      }

      current_idx++;
      return buf[idx];
   }

   const char& operator[] (const uint idx) const {
      if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP
      }

      return buf[idx];
   }

   void operator= (const char *string) {
      memset(buf, 0, nlen());
      current_idx = 0;

      for (uint i = 0; string[i] != '\0'; i++) {
         buf[i] = string[i];
         current_idx++;
      }
   }

   void operator+= (const char *string) {
      uint idx = 0;
      while (string[idx] != '\0') {
         if (current_idx > capacity) {
            fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", current_idx);
            fprintf(stderr, "max size is '%u'.\n", capacity);
            STOP
         }

         buf[current_idx] = string[idx];
         current_idx++;
         idx++;
      }
   }

   void operator+= (const char str) {
      if (current_idx > capacity) {
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", current_idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP
      }

      buf[current_idx] = str;
      current_idx++;
   }
};

struct Char_Iter {
   const char* str;
   uint length;

   struct Iterator {
      const char* ptr;
      const char* end;

      Iterator(const char* p, const char* e) : ptr(p), end(e) {}

      char operator*() const { return *ptr; }
      Iterator& operator++() { ++ptr; return *this; }
      bool operator!=(const Iterator& other) const { return ptr != other.ptr; }
   };

   Char_Iter(const char* s) : str(s), length(strlen(s)) {}
   Char_Iter(const String &s) : str(s.buf), length(s.nlen()) {}

   Iterator begin() const { return Iterator(str, str + length); }
   Iterator end() const { return Iterator(str + length, str + length); }
};

//
// NOTE:
//
// This only works if the 'Args' are the same type include constant values.
// I would like to fix this but for now this gets the job done.
//
// Also add a function overload for this that formats the string without taking a custom allocator that
// way if the string is only needed at a specific scope then it would be easy return a custom 'string'
// that has a destructor which will get ran if the string isn't allocated by a custom allocator.
//
// - Hamza, March 12 2025
//

template<typename ...Args>
String format_string(Fixed_Allocator *allocator, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... }; 
   const uint format_len = strlen(fmt_string);
   uint total_size = 0;

   for (const auto arg : arg_list) {
      total_size += strlen(arg);
   }

   total_size += 1;
   auto dyn_string = String::with_size(allocator, format_len + total_size);
   uint arg_idx = 0;

   for (uint i = 0; i < format_len; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) dyn_string += *(std::next(arg_list.begin(), arg_idx++));
         else fprintf(stderr, "format-string - not enough arguments provided for format string");
      } else dyn_string += fmt_string[i];
   }

   return dyn_string;
}

#endif
