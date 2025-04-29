#ifndef FIXED_ALLOC_H
#define FIXED_ALLOC_H

#include <sys/mman.h>

#include "types.cpp"

struct Fixed_Allocator {
   int capacity = 0;  // Memory capacity limit
   int size = 0;      // Used memory
   void *buffer;      // Mapped zero-initialized memory

   int  ralloc(const int bytes); // Returns memory address relative to the mapped memory
   void *alloc(const int bytes);
   void reset();
   void free();

private:
   void realloc(const int bytes); // Private because it changes the (*buffer) pointer to point to newly allocated memory address and unmap's the old memory address
};

int Fixed_Allocator::ralloc(const int bytes) {
   if ((size + bytes) > capacity) {
      int new_size = page_size;
      while (new_size < size + bytes) new_size *= 2;
      realloc(new_size);
   }

   int ret = size;
   size += bytes;
   return ret;
}

void Fixed_Allocator::realloc(const int bytes) {
   int page_align_size = page_size;
   while (page_align_size < bytes) page_align_size *= 2;

   buffer = mremap(buffer, capacity, page_align_size, MREMAP_MAYMOVE); // This unmap's the old memory
   assert_err(buffer == MAP_FAILED, "mremap failed");
   capacity = page_align_size;
}

// @NOTE:
//
// We can't call 'realloc' in the routine because then the old mapped memory gets unmapped
// so, every thing that holds a pointer to that pointer mapped memory would no longer be vaild.
void *Fixed_Allocator::alloc(const int bytes) {
   assert((size + bytes) > capacity, "(fixed-allocator) allocation failed because capacity is full");
   void *memory = {0};
   memory = (char *)buffer + size;

   size += bytes;
   return memory;
}

// @NOTE: We have to 'memset' to zero because Null-Terminated Strings won't make your life easier!
void Fixed_Allocator::reset() {
   memset(buffer, 0, size);
   size = 0;
}

void Fixed_Allocator::free() {
   assert_err(munmap(buffer, capacity) != 0, "munmap failed");
   size = 0;
   capacity = 0;
   buffer = NULL;
}

Fixed_Allocator fixed_allocator(const int size) {
   int page_align_size = page_size;
   while (page_align_size < size) page_align_size *= 2;

   void *memory = mmap(NULL, page_align_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
   assert_err(memory == MAP_FAILED, "mmap failed");

   return Fixed_Allocator {
      .capacity = page_align_size,
      .buffer = memory
   };
}

#endif // FIXED_ALLOC_H
