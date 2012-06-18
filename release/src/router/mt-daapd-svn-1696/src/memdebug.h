/*
 * $Id: $
 */

#ifdef DEBUG_MEM
# define free(ptr) debug_free(__FILE__,__LINE__,(ptr))
# define malloc(size) debug_malloc(__FILE__,__LINE__,(size))
# define realloc(ptr, size) debug_realloc(__FILE__,__LINE__,(ptr),(size))
# define calloc(count,size) debug_calloc(__FILE__,__LINE__,(count),(size))
# define strdup(str) debug_strdup(__FILE__,__LINE__,(str))
# define mem_dump() debug_dump()
# define mem_register(ptr, size) debug_register(__FILE__,__LINE__,(ptr),(size))

extern void debug_free(char *file, int line, void *ptr);
extern void *debug_malloc(char *file, int line, size_t size);
extern void *debug_realloc(char *file, int line, void *ptr, size_t size);
extern void *debug_calloc(char *file, int line, size_t count, size_t size);
extern void *debug_strdup(char *file, int line, const char *str);
extern void debug_dump(void);
extern void debug_register(char *file, int line, void *ptr, size_t size);

#else
# define mem_dump();
# define mem_register(ptr, size);
#endif


