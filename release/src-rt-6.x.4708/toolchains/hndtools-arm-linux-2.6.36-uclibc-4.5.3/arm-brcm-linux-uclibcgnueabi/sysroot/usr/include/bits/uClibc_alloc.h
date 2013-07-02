/*
 * Macros to transparently switch between the stack and heap for large
 * allocations.  The former is useful on MMU systems as it results in
 * smaller code, but the latter is required on NoMMU systems.  This is
 * due to small stacks that cannot grow and so doing large allocs will
 * cause a stack overflow.
 *
 * Copyright (C) 2010 Mike Frysinger <vapier@gentoo.org>
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#ifndef _UCLIBC_ALLOC_H
#define _UCLIBC_ALLOC_H

#include <alloca.h>
#include <stdlib.h>

#ifdef __ARCH_USE_MMU__
# define stack_heap_alloc(x) alloca(x)
# define stack_heap_free(x)  do { if (0) free(x); } while (0)
#else
# define stack_heap_alloc(x) malloc(x)
# define stack_heap_free(x)  free(x)
#endif

#endif
