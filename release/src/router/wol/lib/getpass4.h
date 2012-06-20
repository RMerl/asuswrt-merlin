#ifndef _GETPASS4_H
#define _GETPASS4_H

#include <stdio.h>
#include <sys/types.h>

ssize_t getpass4 (const char *prompt, char **lineptr, size_t *n, FILE *stream);

#endif /* _GETPASS4_H */
