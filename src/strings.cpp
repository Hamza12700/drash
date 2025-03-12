#ifndef STRINGS_H
#define STRINGS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <typeinfo>
#include <iterator>

#include "fixed_allocator.cpp"
#include "types.cpp"

struct String {
   char *buf = NULL;
   uint len = 0;

   char& operator[] (const uint idx) const {
      if (idx > len) {
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", len);
         raise(SIGTRAP);
      }

      return buf[idx];
   }

   char& operator[] (const uint idx) {
      if (idx > len) {
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", len);
         raise(SIGTRAP);
      }

      return buf[idx];
   }

   bool operator== (const String &other) const {
      if (len != other.len) return false;
      else if (strcmp(other.buf, buf) != 0) return false;

      return true;
   }

   bool operator== (const char *other) const {
      if (len != strlen(other)) return false;
      else if (strcmp(buf, other) != 0) return false;

      return true;
   }
};

struct Dynamic_String {
   char *buf = NULL;
   uint capacity = 0;
   uint current_idx = 0;

   static Dynamic_String make(Fixed_Allocator *allocator, const uint size) {
      return Dynamic_String {
         .buf = static_cast <char *>(allocator->alloc(size)),
         .capacity = size,
      }; 
   }

   String to_string() const {
      return String {
         .buf = buf,
         .len = nlen()
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
         fprintf(stderr, "dynamic-string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         raise(SIGTRAP);
      }

      current_idx++;
      return buf[idx];
   }

   void operator= (const char *string) {
      memset(buf, 0, len()+1);
      current_idx = 0;

      for (uint i = 0; string[i] != '\0'; i++) {
         buf[i] = string[i];
         current_idx++;
      }
   }

   void operator= (String &string) {
      memset(buf, 0, len()+1);
      current_idx = 0;

      for (uint i = 0; i < string.len; i++) {
         buf[i] = string.buf[i];
         current_idx++;
      }
   }

   void operator+= (const char *string) {
      uint idx = 0;
      while (string[idx] != '\0') {
         if (current_idx > capacity) {
            fprintf(stderr, "dynamic-string - attempted to index into position '%u' which is out of bounds.\n", current_idx);
            fprintf(stderr, "max size is '%u'.\n", capacity);
            raise(SIGTRAP);
         }

         buf[current_idx] = string[idx];
         current_idx++;
         idx++;
      }
   }

   void operator+= (const char str) {
      if (current_idx > capacity) {
         fprintf(stderr, "dynamic-string - attempted to index into position '%u' which is out of bounds.\n", current_idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         raise(SIGTRAP);
      }

      buf[current_idx] = str;
      current_idx++;
   }

   void operator+= (String &string) {
      for (size_t i = 0; i < string.len; i++) {
         if (current_idx > capacity) {
            fprintf(stderr, "dynamic-string - attempted to index into position '%u' which is out of bounds.\n", current_idx);
            fprintf(stderr, "max size is '%u'.\n", capacity);
            raise(SIGTRAP);
         }

         buf[current_idx] = string[i];
         current_idx++;
      }
   }

   Dynamic_String& operator=(const Dynamic_String &other) {
      if (this == &other) return *this;

      for (size_t i = 0; i < other.len(); i++) {
         if (current_idx > capacity) {
            fprintf(stderr, "dynamic-string - attempted to index into position '%u' which is out of bounds.\n", current_idx);
            fprintf(stderr, "max size is '%u'.\n", capacity);
            raise(SIGTRAP);
         }

         buf[current_idx] = other.buf[i];
         current_idx++;
      }

      return *this;
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
   Char_Iter(const String &s) : str(s.buf), length(s.len) {}

   Iterator begin() const { return Iterator(str, str + length); }
   Iterator end() const { return Iterator(str + length, str + length); }
};

template<typename ...Args>
Dynamic_String format_string(Fixed_Allocator *allocator, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... }; 
   const uint format_len = strlen(fmt_string) + 1;
   uint total_size = 0;

   for (const auto arg : arg_list) {
      total_size += strlen(arg);
   }

   total_size += 1;
   auto dyn_string = Dynamic_String::make(allocator, format_len + total_size);
   uint arg_idx = 0;

   for (size_t i = 0; i < format_len - 1; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) dyn_string += *(std::next(arg_list.begin(), arg_idx++));
         else fprintf(stderr, "format-string - not enough arguments provided for format string");
      } else dyn_string += fmt_string[i];
   }

   return dyn_string;
}

#endif
