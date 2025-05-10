#ifndef STRINGS_H
#define STRINGS_H

#include <signal.h>
#include <iterator> // Can't remove this, because we are using variadic arguments in templates that gets turned into a list or whatever

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
#define rtap(type, buf, index) ((type *)buf+index)

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
      if (with_allocator) {
         fprintf(stderr, "string - can't concat strings because not enought space\n");
         fprintf(stderr, "capacity is: '%u' but got: '%lu'.\n", capacity, strlen(buf) + slen+1);
         STOP;
      }

      free(buf);
      int new_size = capacity;
      while (new_size <= slen+capacity) new_size *= 2;
      buf = (char *)xcalloc(new_size, 1);
      capacity = new_size;
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
   if (idx >= capacity) {
      if (with_allocator) {
         fprintf(stderr, "string - attempted to index into position '%d' which is out of bounds.\n", idx);
         fprintf(stderr, "max size is '%u'.\n", capacity);
         STOP;
      }

      free(buf);
      buf = (char *)xcalloc(capacity*2, 1);
      capacity += capacity*2;
   }

   return buf[idx];
}

void String::operator= (const char *other) {
   memset(buf, 0, strlen(buf));
   for (int i = 0; other[i] != '\0'; i++) buf[i] = other[i];
}

String string_with_size(Fixed_Allocator *allocator, const int size) {
   String ret;
   ret.buf = static_cast <char *>(allocator->alloc(sizeof(char) * size+1));
   ret.capacity = size+1;
   ret.with_allocator = true;
   return ret;
}

String string_with_size(const int size) {
   String ret;
   ret.buf = (char *)xcalloc(size+1, 1);
   ret.capacity = size+1;

   return ret;
}

String string_with_size(const char *s) {
   const int size = strlen(s);

   String ret;
   ret.buf = static_cast <char *>(xcalloc(size+1, 1));
   ret.capacity = size+1;

   ret = s;
   return ret;
}

String string_with_size(Fixed_Allocator *allocator, const char *s) {
   const int size = strlen(s);

   String ret;
   ret.buf = static_cast <char *>(allocator->alloc(sizeof(char) * size+1));
   ret.capacity = size+1;
   ret.with_allocator = true;

   ret = s;
   return ret;
}

enum String_Flags {
   Default = 0,
   SubString = 0x1,
   Referenced = 0x2,
   Malloced = 0x4,
   Custom_Allocator = 0x8,
};

// Dynamically growable string that works with relative pointers
struct New_String {
   char *buf = NULL;
   int flags = Default;

   i32 capacity = 0;
   i32 index = 0;

   ~New_String();

   void empty();
   int concat(const char *str);
   void skip(int idx);
   void take_ref(New_String *ref);
   New_String sub_string(int idx); // Splits the 'buf' into half (capacity / 2), point the sub-string after that (buf = (capacity / 2)+1) so they don't overlap.

   char& operator[] (const int idx);
};

New_String::~New_String() {
   if (flags & SubString || flags & Referenced || flags & Custom_Allocator) return;

   if (flags & Malloced) {
      free(buf);
      buf = NULL;
      return;
   }

   unmap(buf, capacity);
}

void New_String::skip(int idx) {
   if (idx >= capacity) {
      fprintf(stderr, "(new_string) - can't index into '%d' because it's out of bounds\n", idx);
      fprintf(stderr, "max size is: %d\n", capacity);
      STOP;
   }

   index += idx;
}

void New_String::take_ref(New_String *ref) {
   buf = ref->buf;
   capacity = ref->capacity;
   index = ref->index;
   flags = ref->flags;
   ref->flags |= Referenced;
}

New_String New_String::sub_string(int idx) {
   assert(buf == NULL, "(new_string) - buffer is null");
   assert(idx == 0, "(new_string) - create sub-string starting at index 0");

   New_String ret;
   ret.capacity = capacity;
   ret.buf = rtap(char, buf, idx);
   ret.flags  |= SubString;
   return ret;
}

void New_String::empty() {
   memset(buf, 0, strlen(buf)+1);
}

char &New_String::operator[] (const int idx) {
   if (idx >= capacity) {
      if (flags & SubString) { // Sub-String can not grow the memory buffer because it doesn't have reference to the mapped memory, only the offset.
         fprintf(stderr, "(new_string) - can't grow the string because not enough space (sub-string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", capacity, idx);
         STOP;
      }

      if (flags & Malloced) { // We could use 'realloc' to grow the buffer, but it has wired error reporting that I don't understand
         fprintf(stderr, "(new_string) - can't grow the string because not enough space (malloc'd string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", capacity, idx);
         STOP;
      }

      if (flags & Custom_Allocator) {
         fprintf(stderr, "(new_string) - can't grow the string because not enough space (custom-allocator)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", capacity, idx);
         STOP;
      }

      int page_align_size = page_size;
      while (page_align_size < idx) page_align_size *= 2;
      buf = (char *)reallocate(buf, capacity, page_align_size);
      capacity = page_align_size;
   }

   return buf[index+idx]; // Move relative to 'index'
}

int New_String::concat(const char *str) {
   int slen = strlen(str);
   int buflen = strlen(buf);
   int total_size = buflen+slen+1;

   if (total_size >= capacity) {
      if (flags & SubString) { // Sub-String can not grow the memory buffer because it doesn't have reference to the mapped memory, only the offset.
         fprintf(stderr, "(new_string) - can't concat because not enough space (sub-string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", capacity, slen);
         STOP;
      }

      if (flags & Malloced) { // We could use 'realloc' to grow the buffer, but it has wired error reporting that I don't understand
         fprintf(stderr, "(new_string) - can't concat because not enough space (malloc'd string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", capacity, slen);
         STOP;
      }

      if (flags & Custom_Allocator) {
         fprintf(stderr, "(new_string) - can't concat because not enough space (custom-allocator)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", capacity, slen);
         STOP;
      }

      int page_align_size = page_size;
      while (page_align_size < total_size) page_align_size *= 2;
      buf = (char *)reallocate(buf, capacity, page_align_size);
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

New_String malloc_new_string(int size) {
   New_String ret;

   ret.buf = (char *)xcalloc(size+1, 1); // Effectively, the same thing, it's just we want the memory to be zero-initialized because we use zero-terminated strings.
   ret.capacity = size;
   ret.flags |= Malloced;
   return ret;
}

New_String alloc_new_string(Fixed_Allocator *allocator, int size) {
   New_String ret;
   ret.buf = (char *)allocator->alloc(size);
   ret.capacity = size;
   ret.flags |= Custom_Allocator;
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
