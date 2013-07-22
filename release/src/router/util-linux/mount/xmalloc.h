#ifndef MOUNT_XMALLOC_H
#define MOUNT_XMALLOC_H

extern void *xmalloc(size_t size);
extern void *xrealloc(void *p, size_t size);
extern char *xstrdup(const char *s);

/*
 * free(p); when 'p' is 'const char *' makes gcc unhappy:
 *    warning: passing argument 1 of ‘free’ discards qualifiers from pointer target type
 */
#define my_free(_p)	free((void *) _p)

#endif  /* MOUNT_XMALLOC_H */
