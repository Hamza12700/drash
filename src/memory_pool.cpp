#ifndef MEM_POOL
#define MEM_POOL

#include "allocator_interface.cpp"

template<typename T>
struct Pool { // @Temporary: For now, it only creates the objects and doesn't free them
   T  *ptr = NULL;
   Allocator allocator;

   u16 pos = 0;
   u16 cap = 0;

   T create() {
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
