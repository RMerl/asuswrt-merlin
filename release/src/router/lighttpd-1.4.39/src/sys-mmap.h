#ifndef LI_SYS_MMAP_H
#define LI_SYS_MMAP_H

#if defined(HAVE_SYS_MMAN_H)
# include <sys/mman.h>
#else /* HAVE_SYS_MMAN_H */

# define PROT_SHARED 0
# define MAP_SHARED 0
# define PROT_READ 0

# define mmap(a, b, c, d, e, f) (-1)
# define munmap(a, b) (-1)

#endif /* HAVE_SYS_MMAN_H */

/* NetBSD 1.3.x needs it; also make it available if mmap() is not present */
#if !defined(MAP_FAILED)
# define MAP_FAILED ((char*)-1)
#endif

#endif
