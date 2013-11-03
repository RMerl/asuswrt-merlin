/*
  Copyright (c) 2011, Frank Lahm

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifndef ATALK_STANDARDS_H
#define ATALK_STANDARDS_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__)
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 600
# endif
# ifndef __EXTENSIONS__
#  define __EXTENSIONS__
# endif
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

#endif /* ATALK_STANDARDS_H */
