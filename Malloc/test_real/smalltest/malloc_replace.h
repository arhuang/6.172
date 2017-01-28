#ifdef USE_LIBC_MALLOC
#define _malloc libc_impl.malloc
#define _realloc libc_impl.realloc
#define _free libc_impl.free
#define _mem_init() libc_impl.reset_brk(); \
    if(libc_impl.init()<0) return 0;
#endif

#ifdef USE_MY_MALLOC
#define _malloc my_impl.malloc
#define _realloc my_impl.realloc
#define _free my_impl.free
#define _mem_init() my_impl.reset_brk(); \
    if(my_impl.init()<0) return 0;
#endif

#ifdef USE_BAD_MALLOC
#define _malloc bad_impl.malloc
#define _realloc bad_impl.realloc
#define _free bad_impl.free
#define _mem_init() bad_impl.reset_brk(); \
    if(bad_impl.init()<0) return 0;
#endif
