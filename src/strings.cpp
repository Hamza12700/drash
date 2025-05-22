#ifndef STRINGS_H
#define STRINGS_H

#include <iterator> // Can't remove this, because we are using variadic arguments in templates that gets turned into a list or whatever

#include "arena_allocator.cpp"

bool match_string(const char *one, const char *two) {
   int onelen = strlen(one);
   int twolen = strlen(two);
   if (onelen != twolen) return false;

   for (int i = 0; i < onelen; i++) {
      if (one[i] != two[i]) return false;
   }
   return true;
}

enum String_Flags {
   Default = 0,
   SubString = 0x1,
   Referenced = 0x2,
   Malloced = 0x4,

   Custom_Allocator = 0x8, // Need this for the de-constructor
   ARENA = 0x10,
};

// Dynamically growable String
struct New_String {
   char *buf = NULL;
   Arena *arena = NULL;

   i32 flags = Default;
   i32 cap = 0;

   ~New_String();


   int len() const;
   int concat(const char *str);

   void empty();
   void remove(int idx);
   void skip(int idx);
   void take_ref(New_String *ref);
   void assign_string(const char *str);
   void assign_string(New_String *str);
   New_String sub_string(int idx);

   char& operator[] (const int idx);

private:
   void malloc_grow(int size);
   void arena_grow();
   void arena_grow(int size);
};

New_String::~New_String() {
   if (flags & SubString || flags & Referenced || flags & Custom_Allocator) return;

   if (flags & Malloced) {
      free(buf);
      buf = NULL;
      return;
   }
}

void New_String::arena_grow() {
   int new_size = cap*2;
   auto new_buffer = arena->alloc(new_size);
   memcpy(new_buffer, buf, cap);
   buf = (char *)new_buffer;
   cap = new_size;
}


void New_String::arena_grow(int size) {
   assert(size < cap, "(new_string) - new-size is less than to string capacity");
   auto new_buffer = arena->alloc(size);
   memcpy(new_buffer, buf, cap);
   buf = (char *)new_buffer;
   cap = size;
}

void New_String::malloc_grow(int size) {
   assert(size < cap, "(new_string) - new-size is less than to string capacity");
   auto new_buffer = (char *)xcalloc(size, 1);
   memcpy(new_buffer, buf, cap);
   auto old_buffer = buf;
   buf = new_buffer;
   free(old_buffer);
   cap = size;
}

int New_String::len() const {
   int count = 0;
   while (buf[count] != '\0') count += 1;
   return count;
}

void New_String::remove(int idx) {
   if (idx >= cap) {
      fprintf(stderr, "(new_string) - attempt to remove a character which is outside of bounds");
      fprintf(stderr, "idx: %d, capacity: %d\n", idx, cap);
      abort();
   }

   buf[idx] = '\0';
}

void New_String::assign_string(const char *str) {
   int slen = strlen(str);
   if (slen >= cap) {
      if (flags & SubString) {
         fprintf(stderr, "(new_string) - can't concat because not enough space (sub-string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, slen);
         abort();
      }

      if (flags & ARENA) arena_grow(slen); // Grow the buffer
      else if (flags & Custom_Allocator) { // @Temporary: Find a way to grow the string with any custom-allocator
         fprintf(stderr, "(new_string) - can't concat because not enough space (custom-allocator)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, slen);
         abort();
      } else if (flags & Malloced) {
         int new_size = cap*2;
         while (new_size <= slen) new_size *= 2;
         malloc_grow(new_size);
      }
   }

   strcat(buf, str);
}

void New_String::assign_string(New_String *str) {
   int slen = str->len();
   if (slen >= cap) {
      if (flags & SubString) {
         fprintf(stderr, "(new_string) - can't concat because not enough space (sub-string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, slen);
         abort();
      }

      if (flags & ARENA) arena_grow(slen); // Grow the buffer
      else if (flags & Custom_Allocator) { // @Temporary: Find a way to grow the string with any custom-allocator
         fprintf(stderr, "(new_string) - can't concat because not enough space (custom-allocator)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, slen);
         abort();
      } else if (flags & Malloced) {
         int new_size = cap*2;
         while (new_size <= slen) new_size *= 2;
         malloc_grow(new_size);
      }
   }

   strcat(buf, str->buf);
}

void New_String::skip(int idx) {
   if (idx >= cap) {
      fprintf(stderr, "(new_string) - can't index into '%d' because it's out of bounds\n", idx);
      fprintf(stderr, "max size is: %d\n", cap);
      abort();
   }

   buf = (char *)buf+idx;
}

void New_String::take_ref(New_String *ref) {
   buf = ref->buf;
   cap = ref->cap;
   flags = ref->flags;
   ref->flags |= Referenced;
}

New_String New_String::sub_string(int idx) {
   assert(buf == NULL, "(new_string) - buffer is null");
   assert(idx == 0, "(new_string) - create sub-string starting at index 0");

   New_String ret;
   ret.cap = cap;
   ret.buf = (char *)buf+idx;
   ret.flags  |= SubString;
   return ret;
}

void New_String::empty() {
   memset(buf, 0, strlen(buf)+1);
}

char &New_String::operator[] (const int idx) {
   if (idx >= cap) {
      if (flags & SubString) {
         fprintf(stderr, "(new_string) - can't grow the string because not enough space (sub-string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, idx);
         abort();
      }

      if (flags & ARENA) this->arena_grow(); // Double the buffer size
      else if (flags & Custom_Allocator) {
         fprintf(stderr, "(new_string) - can't grow the string because not enough space (custom-allocator)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, idx);
         abort();
      } else if (flags & Malloced) {
         int new_size = cap*2;
         malloc_grow(new_size);
      }
   }

   return buf[idx];
}

int New_String::concat(const char *str) {
   int slen = strlen(str);
   int buflen = this->len();
   int total_size = buflen+slen+1;

   if (total_size >= cap) {
      if (flags & SubString) {
         fprintf(stderr, "(new_string) - can't concat because not enough space (sub-string)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, slen);
         abort();
      }

      if (flags & ARENA) this->arena_grow(total_size); // Increase the buffer size
      else if (flags & Custom_Allocator) {
         fprintf(stderr, "(new_string) - can't concat because not enough space (custom-allocator)\n");
         fprintf(stderr, "capacity size is '%u' but got %d.\n", cap, slen);
         abort();
      } else if (flags & Malloced) {
         int new_size = cap*2;
         while (new_size <= total_size) new_size *= 2;
         malloc_grow(new_size);
      }
   }

   int count = 0;
   while (count < slen) {
      buf[buflen+count] = str[count];
      count += 1;
   }

   return count;
};

New_String malloc_string(int size) {
   New_String ret;
   ret.buf = (char *)xcalloc(size+1, 1); // Effectively, the same thing, it's just we want the memory to be zero-initialized because we use zero-terminated strings.
   ret.cap = size;
   ret.flags |= Malloced;
   return ret;
}

New_String alloc_string(void *buffer, int size) {
   New_String ret;
   ret.buf = (char *)buffer;
   ret.cap = size;
   ret.flags |= Custom_Allocator;
   return ret;
}

New_String alloc_string(Arena *arena, int size) {
   New_String ret;
   ret.buf = (char *)arena->alloc(size+1);
   ret.cap = size+1;
   ret.flags |= Custom_Allocator;
   ret.flags |= ARENA;
   ret.arena = arena;
   return ret;
}

New_String alloc_string(Arena *arena, const char *str) {
   New_String ret;
   int size = strlen(str)+1;

   ret.buf = (char *)arena->alloc(size);
   ret.cap = size;
   ret.flags |= Custom_Allocator;
   ret.flags |= ARENA;
   ret.arena = arena;
   ret.assign_string(str);
   return ret;
}

template<typename ...Args>
New_String format_string(Arena *arena, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... };
   const int format_len = strlen(fmt_string);
   int total_size = 0;

   for (const auto arg : arg_list)
      total_size += strlen(arg);

   auto dyn_string = alloc_string(arena, format_len + total_size+1);
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
New_String format_string(const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... };
   const int format_len = strlen(fmt_string);
   int total_size = 0;

   for (const auto arg : arg_list)
      total_size += strlen(arg);

   auto dyn_string = malloc_string(format_len + total_size+1);
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
void fatal_error(Arena *arena, const char *fmt, Args ...args) {
   auto err = format_string(arena, fmt, args...);

   fprintf(stderr, "[ERROR]: %s\n", err.buf);
   fprintf(stderr, "- %s\n", strerror(errno));
   exit(errno);
}

template<typename ...Args>
void report_error(Arena *arena, const char *fmt, Args ...args) {
   auto err = format_string(arena, fmt, args...);

   fprintf(stderr, "[ERROR]: %s\n", err.buf);
   fprintf(stderr, "- %s\n", strerror(errno));
}

#endif // STRINGS_H
