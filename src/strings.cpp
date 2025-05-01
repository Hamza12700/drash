#ifndef STRINGS_H
#define STRINGS_H

#include <signal.h>
#include <iterator> // Get rid-of this

#include "fixed_allocator.cpp"

bool match_string(const char *one, const char *two) {
   int onelen = strlen(one);
   int twolen = strlen(two);
   if (onelen != twolen) return false;

   for (int i = 0; i < onelen; i++) {
      if (one[i] != two[i]) return false;
   }
   return true;
}

// Convert relative pointer to an absolute pointer
#define rtap(type, buf, index) (type *)buf+index

struct String {
   char *buf = NULL; // Null-terminated buffer
   int capacity = 0;
   bool with_allocator = false;

   ~String() {
      if (!with_allocator) {
         free(buf);
         capacity = 0;
         buf = NULL;
      }
   }

   void take_reference(String *string);
   void remove(const int idx);

   int concat(const char *s); // Return the amount of characters written in the buffer
   void skip(const int num);

   char& operator[] (const int idx);
   const char& operator[] (const int idx) const;

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
   // We should think about doing this in a better way.
   //

   other->with_allocator = true;
}

void String::remove(const int idx) {
   if (idx >= capacity) {
      fprintf(stderr, "string - attempted to remove a character in index '%u' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%u'.\n", capacity);
      STOP;
   }

   buf[idx] = '\0'; // @Robustness: Check if the character in the string buffer is null or not for safety reasons?
}

int String::concat(const char *other) {
   const int slen = strlen(other);

   if (strlen(buf) + slen+1 >= (uint)capacity) {
      fprintf(stderr, "string - can't concat strings because not enought space\n");
      fprintf(stderr, "capacity is: '%u' but got: '%lu'.\n", capacity, strlen(buf) + slen+1);
      STOP;
   }

   int count = 0;
   int buflen = strlen(buf);
   while (count < slen) {
      buf[buflen+count] = other[count];
      count += 1;
   }
   return count;
}

void String::skip(const int num) {
   if (num >= capacity) {
      fprintf(stderr, "string - can't index into '%u' because it's out of bounds\n", num);
      fprintf(stderr, "max size is: %lu\n", strlen(buf));
      STOP;
   }

   buf += num;
   capacity -= num;
}

char& String::operator[] (const int idx) {
   if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
      fprintf(stderr, "string - attempted to index into position '%d' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%u'.\n", capacity);
      STOP;
   }

   return buf[idx];
}

const char& String::operator[] (const int idx) const {
   if (idx >= capacity) { // NOTE: The reason we've to check for '>=' is because we don't wana overwrite the null-byte
      fprintf(stderr, "string - attempted to index into position '%d' which is out of bounds.\n", idx);
      fprintf(stderr, "max size is '%u'.\n", capacity);
      STOP;
   }

   return buf[idx];
}

void String::operator= (const char *other) {
   memset(buf, 0, strlen(buf));
   for (int i = 0; other[i] != '\0'; i++) buf[i] = other[i];
}

String string_with_size(Fixed_Allocator *allocator, const int size) {
   return String {
      .buf = static_cast <char *>(allocator->alloc(sizeof(char) * size+1)),
      .capacity = size+1,
      .with_allocator = true,
   };
}

String string_with_size(const int size) {

   // @Speed:
   //
   // 'mmap' returns memory that is already zero-initialized
   // We need to have memory be zero-initialized because we are dealing with null-terminated strings here!
   // Because I don't wanna go throught the hassle of creating my own version of 'printf' that takes a non-null terminated strings
   // we have to zero-initialized the memory to avoid nasty bugs! :NullString
   //
   // - Hamza 26 April 2025

   int page_align_size = page_size;
   while (page_align_size < size) page_align_size *= 2;

   return String {
      .buf = static_cast <char *>(xcalloc(size+1, 1)),
      .capacity = size+1,
   };
}

String string_with_size(const char *s) {
   const int size = strlen(s);

   auto ret = String {
      .buf = static_cast <char *>(xcalloc(size+1, 1)), // Avoid using 'calloc/malloc' instead use 'mmap'. See :NullString
      .capacity = size+1,
   };

   ret = s;
   return ret;
}

String string_with_size(Fixed_Allocator *allocator, const char *s) {
   const int size = strlen(s);

   auto ret = String {
      .buf = static_cast <char *>(allocator->alloc(sizeof(char) * size+1)),
      .capacity = size+1,
      .with_allocator = true,
   };

   ret = s;
   return ret;
}

enum String_Flags {
   Default,
   SubString,
   Referenced,
};

// An implemention of dynamically growable string that works with relative pointers
struct New_String {
   char *buf = NULL; // We have to hold a pointer to the buffer beacuse we want to compute the address relative to the buffer pointer.
   int flags = Default;

   i32 capacity = 0;
   i32 index = 0;

   ~New_String();

   void empty();
   int concat(const char *str);
   New_String sub_string(); // Splits the 'buf' into half (capacity / 2), point the sub-string after that (buf = (capacity / 2)+1) so they don't overlap.
   void take_ref(New_String *ref);

   char& operator[] (const int idx);
};

New_String::~New_String() {
   if (flags & SubString || flags & Referenced) return;
   unmap(buf, capacity);
   buf = NULL;
}

char &New_String::operator[] (const int idx) {
   if (idx >= capacity) {
      if (flags & SubString) {
         fprintf(stderr, "(sub_string) - attempted to index into position '%d' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP;
      }

      int page_align_size = page_size;
      while (page_align_size < idx) page_align_size *= 2;
      buf = (char *)realloc(buf, capacity, page_align_size);
      capacity = page_align_size;
   }

   return buf[idx];
}

void New_String::take_ref(New_String *ref) {
   buf = ref->buf;
   capacity = ref->capacity;
   index = ref->index;
   flags = ref->flags;
   ref->flags |= Referenced;
}

New_String New_String::sub_string() {
   assert(buf == NULL, "(new_string) - buffer is null");
   New_String ret;

   ret.capacity = (capacity / 2);
   capacity = ret.capacity; // @Temporary: Don't overwrite the null-byte at (capacity / 2)
   ret.index = 0;

   ret.buf = rtap(char, buf, ret.capacity+1);
   ret.flags  |= SubString;
   return ret;
}

void New_String::empty() {
   memset(buf, 0, strlen(buf)+1);
}

int New_String::concat(const char *str) {
   int slen = strlen(str);
   int buflen = strlen(buf);
   int total_size = buflen+slen+1;

   if (total_size >= capacity) {
      if (flags & SubString) { // Sub-String can not grow the memory buffer because it doesn't have reference to the mapped memory, only the offset.
         fprintf(stderr, "(sub_string) - can't concat because not enough space\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", capacity, slen);
         STOP;
      }

      int page_align_size = page_size;
      while (page_align_size < total_size) page_align_size *= 2;
      buf = (char *)realloc(buf, capacity, page_align_size);
      capacity = page_align_size;
   }

   int count = 0;
   while (count < slen) {
      buf[buflen+count] = str[count];
      count += 1;
   }
   return count;
};

New_String new_string(const int size) {
   int page_align_size = page_size;
   while (page_align_size < size) page_align_size *= 2;
   auto mem = allocate(page_align_size);

   New_String ret;
   ret.buf = (char *)mem;
   ret.capacity = page_align_size;
   ret.index = 0;
   return ret;
}

template<typename ...Args>
String format_string(Fixed_Allocator *allocator, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... };
   const int format_len = strlen(fmt_string);
   int total_size = 0;

   for (const auto arg : arg_list)
      total_size += strlen(arg);

   auto dyn_string = string_with_size(allocator, format_len + total_size+1);
   uint arg_idx = 0;
   int filled_buffer = 0;

   for (int i = 0; i < format_len; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) {
            auto arg = *(std::next(arg_list.begin(), arg_idx++));
            filled_buffer += dyn_string.concat(arg);
            continue;
         }

         fprintf(stderr, "format-string - not enough arguments provided for format string");
         continue;
      }

      dyn_string[filled_buffer] = fmt_string[i];
      filled_buffer += 1;
   }

   return dyn_string;
}

template<typename ...Args>
String format_string(const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... };
   const int format_len = strlen(fmt_string);
   int total_size = 0;

   for (const auto arg : arg_list)
      total_size += strlen(arg);

   auto dyn_string = string_with_size(format_len + total_size+1);
   uint arg_idx = 0;
   int filled_buffer = 0;

   for (int i = 0; i < format_len; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) {
            auto arg = *(std::next(arg_list.begin(), arg_idx++));
            filled_buffer += dyn_string.concat(arg);
            continue;
         }

         fprintf(stderr, "format-string - not enough arguments provided for format string");
         continue;
      }

      dyn_string[filled_buffer] = fmt_string[i];
      filled_buffer += 1;
   }

   return dyn_string;
}

template<typename ...Args>
void format_string(char *buffer, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... };
   const int format_len = strlen(fmt_string);
   int total_size = 0;

   for (const auto arg : arg_list)
      total_size += strlen(arg);

   uint arg_idx = 0;
   int filled_buffer = 0;
   for (int i = 0; i < format_len; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) {
            const char *arg = *(std::next(arg_list.begin(), arg_idx++));
            for (uint x = 0; x < strlen(arg); x++, filled_buffer++) { 
               buffer[filled_buffer] = arg[x];
            }
            continue;
         }

         fprintf(stderr, "format-string - not enough arguments provided for format string");
         continue;
      }

      buffer[filled_buffer] = fmt_string[i];
      filled_buffer += 1;
   }
}

template<typename ...Args>
void format_string(New_String *buffer, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... };
   const int format_len = strlen(fmt_string);
   int total_size = 0;

   for (const auto arg : arg_list)
      total_size += strlen(arg);

   uint arg_idx = 0;
   int filled_buffer = 0;
   for (int i = 0; i < format_len; i++) {
      if (fmt_string[i] == '%') {
         if (arg_idx < arg_list.size()) {
            const char *arg = *(std::next(arg_list.begin(), arg_idx++));
            filled_buffer += buffer->concat(arg);
            continue;
         }

         fprintf(stderr, "format-string - not enough arguments provided for format string");
         continue;
      }

      (*buffer)[filled_buffer] = fmt_string[i];
      filled_buffer += 1;
   }
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
