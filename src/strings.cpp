#ifndef STRINGS_H
#define STRINGS_H

#include <iterator> // Can't remove this, because we are using variadic arguments in templates that gets turned into a list or whatever

#include "allocator_interface.cpp"

// Dynamically growable String
struct New_String {
   char *buf = NULL;
   Allocator allocator;
   int cap = 0;

   int len();
   int concat(const char *str);

   void empty();
   void remove(int idx);
   void skip(int idx);
   void assign_string(const char *str);
   void assign_string(New_String *str);

   char& operator[] (const int idx);

private:
   void alloc_grow();
   void alloc_grow(int size);
};

bool match_string(New_String *a, New_String *b) {
   if (a->len() != b->len()) return false;

   for (int i = 0; i < a->cap; i++)
      if ((*a)[i] != (*b)[i]) return false;
   return true;
}

bool match_string(const char *one, const char *two) {
   int onelen = strlen(one);
   int twolen = strlen(two);
   if (onelen != twolen) return false;

   for (int i = 0; i < onelen; i++)
      if (one[i] != two[i]) return false;
   return true;
}


void New_String::alloc_grow() {
   int new_size = cap*2;
   auto new_buffer = allocator.alloc(new_size, NULL);
   memcpy(new_buffer, buf, cap);
   buf = (char *)new_buffer;
   cap = new_size;
}


void New_String::alloc_grow(int size) {
   assert(size < cap, "(new_string) - new-size is less than to string capacity");
   auto new_buffer = allocator.alloc(size, NULL);
   memcpy(new_buffer, buf, cap);
   buf = (char *)new_buffer;
   cap = size;
}

int New_String::len() {
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
   if (slen >= cap) alloc_grow(slen); // Grow the buffer

   strcat(buf, str);
}

void New_String::assign_string(New_String *str) {
   int slen = str->len();
   if (slen >= cap) alloc_grow(slen); // Grow the buffer

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

void New_String::empty() {
   memset(buf, 0, strlen(buf)+1);
}

char &New_String::operator[] (const int idx) {
   if (idx >= cap) this->alloc_grow(); // Double the buffer size

   return buf[idx];
}

int New_String::concat(const char *str) {
   int slen = strlen(str);
   int buflen = this->len();
   int total_size = buflen+slen+1;

   if (total_size >= cap)
      this->alloc_grow(total_size); // Increase the buffer size

   int count = 0;
   while (count < slen) {
      buf[buflen+count] = str[count];
      count += 1;
   }

   return count;
};

New_String alloc_string(void *buffer, int size) {
   New_String ret;
   ret.buf = (char *)buffer;
   ret.cap = size;
   return ret;
}

New_String alloc_string(Allocator allocator, uint size) {
   New_String ret;
   Allocator context = {};

   ret.buf = (char *)allocator.alloc(size+1, &context);
   ret.cap = size+1;
   ret.allocator = context;
   return ret;
}

New_String alloc_string(Allocator allocator, const char *str) {
   New_String ret;
   uint size = strlen(str)+1;
   Allocator context = {};

   ret.buf = (char *)allocator.alloc(size, &context);
   ret.cap = size;
   ret.allocator = context;
   ret.assign_string(str);
   return ret;
}

template<typename ...Args>
New_String format_string(Allocator allocator, const char *fmt_string, const Args ...args) {
   const auto arg_list = { args... };
   const int format_len = strlen(fmt_string);
   int total_size = 0;

   for (const auto arg : arg_list)
      total_size += strlen(arg);

   auto dyn_string = alloc_string(allocator, format_len + total_size);
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
         abort();
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
         abort();
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
         abort();
      }

      (*buffer)[filled_buffer] = fmt_string[i];
      filled_buffer += 1;
   }
}

template<typename ...Args>
void fatal_error(Allocator allocator, const char *fmt, Args ...args) {
   auto err = format_string(allocator, fmt, args...);

   fprintf(stderr, "[ERROR]: %s\n", err.buf);
   fprintf(stderr, "- %s\n", strerror(errno));
   exit(errno);
}

template<typename ...Args>
void report_error(Allocator allocator, const char *fmt, Args ...args) {
   auto err = format_string(allocator, fmt, args...);

   fprintf(stderr, "[ERROR]: %s\n", err.buf);
   fprintf(stderr, "- %s\n", strerror(errno));
}

#endif // STRINGS_H
