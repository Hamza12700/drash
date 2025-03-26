#ifndef STRINGS_H
#define STRINGS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <iterator>

#include "fixed_allocator.cpp"

struct String {
   char *buf = NULL; // Null-terminated buffer
   uint capacity = 0;
   bool with_allocator = false;

   ~String() {
      if (!with_allocator) free(buf);
   }

   static String with_size(Fixed_Allocator *allocator, const uint size) {
      return String {
         .buf = static_cast <char *>(allocator->alloc(sizeof(char) * size+1)),
         .capacity = size+1,
         .with_allocator = true,
      };
   }

   static String with_size(const uint size) {
      return String {

         //
         // @Hack | @NOTE:
         //
         // This is stupid because malloc gets replaced by ASAN and because we mutate the string buffer and overwrite the null-byte
         // it thinks that we are access memory out of bounds even thought we are not! Because the memory returned by
         // ASAN 'malloc' isn't zero initialize. That's why we have to use 'calloc'.
         //
         // Because we are doing the bounds checking ourself ASAN is useless here.
         //
         // - Hamza, 15 March 2025
         //

         .buf = static_cast <char *>(xcalloc(size+1, sizeof(char))),
         .capacity = size+1,
      };
   }

   void by_reference(const String string) {
      if (!with_allocator) free(buf);
      buf = string.buf;
   }

   void by_reference(const String *string) {
      if (!with_allocator) free(buf);
      buf = string->buf;
   }

   bool is_empty() {
      if (buf[0] == '\0') return true;
      return false;
   }

   void remove(const uint idx) {
      if (idx >= capacity) {
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP;
      }

      buf[idx] = '\0'; // @Robustness: Check if the character in the string buffer is null or not for safety reasons?
   }

   // Return the length of the string and excluding the null-byte
   uint len() const {
      uint idx = 0;
      while (buf[idx] != '\0')  idx += 1;

      return idx;
   }

   // Return the length of the string including the null-byte
   uint nlen() const {
      uint idx = 0;
      while (buf[idx] != '\0')  idx += 1;

      return idx + 1;
   }

   char& operator[] (const uint idx) {
      if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP;
      }

      return buf[idx];
   }

   const char& operator[] (const uint idx) const {
      if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP;
      }

      return buf[idx];
   }

   void operator= (const char *string) {
      memset(buf, 0, len());

      for (uint i = 0; string[i] != '\0'; i++) buf[i] = string[i];
   }

   void concat(const char *s) {
      const uint s_len = strlen(s);

      if (nlen() + s_len >= capacity) {
         fprintf(stderr, "string - can't concat strings because not enought space\n");
         fprintf(stderr, "capacity is: '%u' but got: '%u'.\n", capacity, nlen() + s_len);
         STOP;
      }

      strcat(buf, s);
   }

   void concat(const String *s) {
      if (s->nlen() + len() > capacity) {
         fprintf(stderr, "string - can't concat strings because not enought space\n");
         fprintf(stderr, "capacity is: '%u' but got: '%u'.\n", capacity, s->nlen() + len());
         STOP;
      }

      strcat(buf, s->buf);
   }

   void concat(const char s) {
      if (nlen() >= capacity) {
         fprintf(stderr, "string - can't concat strings because not enought space\n");
         fprintf(stderr, "capacity is: '%u' but got: '%u'.\n", capacity, 1 + nlen());
         STOP;
      }

      buf[len()] = s;
   }

   void skip(const uint idx) {
      if (idx >= capacity) {
         fprintf(stderr, "string - can't index into '%u' because it's out of bounds\n", idx);
         fprintf(stderr, "max size is: %u\n", len());
         STOP;
      }

      buf += idx;
      capacity -= idx;
   }
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
// - Hamza, 12 March 2025
//

template<typename ...Args>
String format_string(Fixed_Allocator *allocator, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... }; 
   const uint format_len = strlen(fmt_string);
   uint total_size = 0;

   for (const auto arg : arg_list) {
      total_size += strlen(arg);
   }

   auto dyn_string = String::with_size(allocator, format_len + total_size);
   uint arg_idx = 0;

   for (uint i = 0; i < format_len; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) dyn_string.concat(*(std::next(arg_list.begin(), arg_idx++)));
         else fprintf(stderr, "format-string - not enough arguments provided for format string");
      } else dyn_string.concat(fmt_string[i]);
   }

   return dyn_string;
}

template<typename ...Args>
String format_string(const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... }; 
   const uint format_len = strlen(fmt_string);
   uint total_size = 0;

   for (const auto arg : arg_list) total_size += strlen(arg);

   auto dyn_string = String::with_size(format_len + total_size);
   uint arg_idx = 0;

   for (uint i = 0; i < format_len; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) dyn_string.concat(*(std::next(arg_list.begin(), arg_idx++)));
         else fprintf(stderr, "format-string - not enough arguments provided for format string");

      } else dyn_string.concat(fmt_string[i]);
   }

   return dyn_string;
}

template<typename ...Args>
void fatal_error(const char *fmt, Args ...args) {
   auto err = format_string(fmt, args...);

   fprintf(stderr, "[ERROR]: %s\n", err.buf);
   fprintf(stderr, "- %s\n", strerror(errno));
   exit(errno);
}

template<typename ...Args>
void report_error(const char *fmt, Args ...args) {
   auto err = format_string(fmt, args...);
   fprintf(stderr, "[ERROR]: %s\n", err.buf);
   fprintf(stderr, "- %s\n", strerror(errno));
}

#endif // STRINGS_H
