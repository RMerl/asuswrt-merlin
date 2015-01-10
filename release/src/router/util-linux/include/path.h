#include <stdio.h>

extern FILE *path_fopen(const char *mode, int exit_on_err, const char *path, ...)
			__attribute__ ((__format__ (__printf__, 3, 4)));
extern void path_getstr(char *result, size_t len, const char *path, ...)
			__attribute__ ((__format__ (__printf__, 3, 4)));
extern int path_writestr(const char *str, const char *path, ...)
			 __attribute__ ((__format__ (__printf__, 2, 3)));
extern int path_getnum(const char *path, ...)
		       __attribute__ ((__format__ (__printf__, 1, 2)));
extern int path_exist(const char *path, ...)
		      __attribute__ ((__format__ (__printf__, 1, 2)));
extern cpu_set_t *path_cpuset(int, const char *path, ...)
			      __attribute__ ((__format__ (__printf__, 2, 3)));
extern cpu_set_t *path_cpulist(int, const char *path, ...)
			       __attribute__ ((__format__ (__printf__, 2, 3)));
extern void path_setprefix(const char *);
