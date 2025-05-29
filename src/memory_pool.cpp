#ifndef MEM_POOL
#define MEM_POOL

#include "types.cpp"

template<typename T>
struct Pool { // @Temporary: For now, it only creates the objects and doesn't free them
   T  *ptr = NULL;
   u16 pos = 0;
   u16 cap = 0;

   enum Alloc_Type {
      MALLOC,
      ARENA,
   };

   Alloc_Type type = MALLOC;

   T create() {
      if (pos >= cap) {
         assert(type == ARENA, "can't grow the memory pool with arena (TODO)");

         if (type == MALLOC) {
            u16 new_size = cap*2;
            auto mem = (T *)xcalloc(cap*2, sizeof(T));
            memcpy(mem, ptr, cap*sizeof(T));
            free(ptr);
            ptr = mem;
            cap = new_size;
         }
      }

      auto ret = ptr[pos*sizeof(T)];
      pos += 1;
      return ret;
   } 
};

template<typename T>
void pool_free(Pool<T> *pool) {
   free(pool->ptr);
   pool->ptr = NULL;
   pool->cap = 0;
   pool->pos = 0;
}

template<typename T>
Pool<T> make_pool(u16 size) {
   auto buf = (T *)xcalloc(size, sizeof(T));
   Pool<T> ret;
   ret.ptr = buf;
   ret.cap = size;
   return ret;
}

#endif // MEM_POOL
