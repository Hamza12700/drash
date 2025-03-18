#ifndef STRINGS_H
#define STRINGS_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <iterator>

#include "fixed_allocator.cpp"
#include "types.cpp"

//
// NOTE: Add a de-contractor which will only deallocates memory which is malloc'd
//

struct String {
   char *buf = NULL;
   uint capacity = 0;

   static String with_size(Fixed_Allocator *allocator, const uint size) {
      return String {
         .buf = static_cast <char *>(allocator->alloc(sizeof(char) + size)),
         .capacity = (uint)sizeof(char) + size,
      };
   }

   // @Incomplete: Deallocate the malloc'd memory
   static String with_size(const uint size) {
      return String {

         //
         // @Hack | @NOTE:
         //
         // This is stupid because malloc gets replaced by ASAN and because we mutate the string buffer
         // it thinks that we are access memory out of bounds even thought we are not! Because the memory returned by
         // ASAN 'malloc' isn't zero initialize. That's why we have to use 'calloc'.
         //
         // Because we are doing the bounds checking ourself ASAN is useless here.
         //
         // - Hamza, 15 March 2025
         //

         .buf = static_cast <char *>(calloc(size + sizeof(char), sizeof(char))),
         .capacity = (uint)sizeof(char) + size,
      };
   }

   void remove(const uint idx) {
      if (idx >= capacity) {
         fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP;
      }

      buf[idx] = '\0';
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
      memset(buf, 0, nlen());

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
      if (nlen() + 1 >= capacity) {
         fprintf(stderr, "string - can't concat strings because not enought space\n");
         fprintf(stderr, "capacity is: '%u' but got: '%u'.\n", capacity, 1 + nlen());
         STOP;
      }

      buf[len()] = s;
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

   total_size += 1;
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

   for (const auto arg : arg_list) {
      total_size += strlen(arg);
   }

   auto dyn_string = String::with_size(format_len + total_size + 1);

   uint arg_idx = 0;
   for (uint i = 0; i < format_len; i++) {

      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) dyn_string.concat(*(std::next(arg_list.begin(), arg_idx++)));
         else fprintf(stderr, "format-string - not enough arguments provided for format string");

      } else dyn_string.concat(fmt_string[i]);
   }

   return dyn_string;
}

#endif // STRINGS_H
