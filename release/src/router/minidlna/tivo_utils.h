/* TiVo helper functions
 *
 * Project : minidlna
 * Website : http://sourceforge.net/projects/minidlna/
 * Author  : Justin Maggard
 *
 * MiniDLNA media server
 * Copyright (C) 2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"

#ifdef TIVO_SUPPORT
#include <sqlite3.h>

struct sqlite3PrngType {
  unsigned char isInit;          /* True if initialized */
  unsigned char i, j;            /* State variables */
  unsigned char s[256];          /* State variables */
} sqlite3Prng;

char *
decodeString(char *string, int inplace);

void
TiVoRandomSeedFunc(sqlite3_context *context, int argc, sqlite3_value **argv);

int
is_tivo_file(const char *path);

#else
#define decodeString(X, Y) ({})
#endif
