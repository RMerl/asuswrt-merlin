/* compat.c - Dummy file to avoid an empty library.
 * Copyright (C) 2010  Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "../src/g10lib.h"


const char *
_gcry_compat_identification (void)
{
  static const char blurb[] =
    "\n\n"
    "This is Libgcrypt " PACKAGE_VERSION " - The GNU Crypto Library\n"
    "Copyright 2000, 2002, 2003, 2004, 2007, 2008, 2009,\n"
    "          2010, 2011, 2012 Free Software Foundation, Inc.\n"
    "Copyright 2012, 2013 g10 Code GmbH\n"
    "\n"
    "(" BUILD_REVISION " " BUILD_TIMESTAMP ")\n"
    "\n\n";
  return blurb;
}
