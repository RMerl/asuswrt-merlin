/*
  Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifndef TEST_H
#define TEST_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/logger.h>
#include <atalk/volume.h>
#include <atalk/directory.h>
#include <atalk/queue.h>
#include <atalk/bstrlib.h>
#include <atalk/globals.h>

#include "directory.h"
#include "dircache.h"
#include "hash.h"
#include "afp_config.h"
#include "volume.h"
#include "subtests.h"

static inline void alignok(int len)
{
    int i = 1;
    if (len < 80)
        i = 80 - len;
    while (i--)
        printf(" ");
}

#define TEST(a) \
    printf("Testing: %s ... ", (#a) ); \
    alignok(strlen(#a));               \
    a;                                 \
    printf("[ok]\n");

#define TEST_int(a, b) \
    printf("Testing: %s ... ", (#a) );            \
    alignok(strlen(#a));                          \
    if ((reti = (a)) != b) {                      \
        printf("[error]\n");                      \
        exit(1);                                  \
    } else { printf("[ok]\n"); }

#define TEST_expr(a, b)                              \
    printf("Testing: %s ... ", (#a) );               \
    alignok(strlen(#a));                             \
    a;                                               \
    if (b) {                                         \
        printf("[ok]\n");                            \
    } else {                                         \
        printf("[error]\n");                         \
        exit(1);                                     \
    }
#endif  /* TEST_H */
