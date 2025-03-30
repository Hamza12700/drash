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

   // Take a reference to another string
   void take_reference(String *string);

   // Compare a null-terminated string
   bool cmp(const char *s);

   // Remove a character from the buffer
   void remove(const uint idx);

   // Concat a null-terminated string
   void concat(const char *s);

   // Concat a single character
   void concat(const char s);

   // Skip the string 'idx' amount of times
   void skip(const uint num);

   // Return the length of the string and excluding the null-byte
   uint len() const;

   char& operator[] (const uint idx);
   const char& operator[] (const uint idx) const;

   void operator= (const char *string);
};

void String::take_reference(String *other) {
   if (!with_allocator) free(buf); // Free the already allocated buffer if malloc'd

   capacity = other->capacity;
   buf = other->buf;

   //
   // @Hack | @Temporary:
   //
   // We want the string buffer to live outside the scope because we take a reference to it.
   // The string buffer only gets deallocated when it's malloc'd so, here we are abusing the variable 'with_allocator'
   // so it doesn't get deallocated and instead the variable taking the reference should deallocate the memory.
   //
   // We should think about doing this in a better way, but for now it's fine.
   //

   other->with_allocator = true;
}

bool String::cmp(const char *s) {
   for (uint i = 0; i < strlen(s); i++) {
      if (s[i] != buf[i]) return false;
   }

   return true;
}

void String::remove(const uint idx) {
   if (idx >= capacity) {
      fprintf(stderr, "string - attempted to remove a character in index '%u' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%u'.\n", capacity);
      STOP;
   }

   buf[idx] = '\0'; // @Robustness: Check if the character in the string buffer is null or not for safety reasons?
}

uint String::len() const {
   uint idx = 0;
   while (buf[idx] != '\0')  idx += 1;

   return idx;
}

void String::concat(const char *other) {
   const uint s_len = strlen(other);

   if (len() + s_len+1 >= capacity) {
      fprintf(stderr, "string - can't concat strings because not enought space\n");
      fprintf(stderr, "capacity is: '%u' but got: '%u'.\n", capacity, len() + s_len+1);
      STOP;
   }

   strcat(buf, other);
}

void String::concat(const char s) {
   if (len()+1 >= capacity) {
      fprintf(stderr, "string - can't concat strings because not enought space\n");
      fprintf(stderr, "capacity is: '%u' but got: '%u'.\n", capacity, 1 + len()+1);
      STOP;
   }

   buf[len()] = s;
}

void String::skip(const uint num) {
   if (num >= capacity) {
      fprintf(stderr, "string - can't index into '%u' because it's out of bounds\n", num);
      fprintf(stderr, "max size is: %u\n", len());
      STOP;
   }

   buf += num;
   capacity -= num;
}

char& String::operator[] (const uint idx) {
   if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
      fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%u'.\n", capacity);
      STOP;
   }

   return buf[idx];
}

const char& String::operator[] (const uint idx) const {
   if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
      fprintf(stderr, "string - attempted to index into position '%u' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%u'.\n", capacity);
      STOP;
   }

   return buf[idx];
}

void String::operator= (const char *other) {
   memset(buf, 0, len());
   for (uint i = 0; other[i] != '\0'; i++) buf[i] = other[i];
}

String string_with_size(Fixed_Allocator *allocator, const uint size) {
   return String {
      .buf = static_cast <char *>(allocator->alloc(sizeof(char) * size+1)),
      .capacity = size+1,
      .with_allocator = true,
   };
}

String string_with_size(const uint size) {
   return String {
      .buf = static_cast <char *>(xmalloc(size+1)),
      .capacity = size+1,
   };
}

String string_with_size(const char *s) {
   const uint size = strlen(s);

   auto ret = String {
      .buf = static_cast <char *>(xmalloc(size+1)),
      .capacity = size+1,
   };

   ret = s;
   return ret;
}

String string_with_size(Fixed_Allocator *allocator, const char *s) {
   const uint size = strlen(s);

   auto ret = String {
      .buf = static_cast <char *>(allocator->alloc(sizeof(char) * size+1)),
      .capacity = size+1,
      .with_allocator = true,
   };

   ret = s;
   return ret;
}

//
// NOTE:
//
// This only works if the 'Args' are the same type include constant values.
// I would like to fix this but for now this gets the job done.
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

   auto dyn_string = string_with_size(allocator, format_len + total_size);
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

   auto dyn_string = string_with_size(format_len + total_size);
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
