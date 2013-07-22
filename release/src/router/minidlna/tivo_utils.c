/* MiniDLNA media server
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sqlite3.h>
#include "tivo_utils.h"

/* This function based on byRequest */
char *
decodeString(char *string, int inplace)
{
	if( !string )
		return NULL;
	int alloc = (int)strlen(string)+1;
	char *ns = NULL;
	unsigned char in;
	int strindex=0;
	long hex;

	if( !inplace )
	{
		if( !(ns = malloc(alloc)) )
			return NULL;
	}

	while(--alloc > 0)
	{
		in = *string;
		if((in == '%') && isxdigit(string[1]) && isxdigit(string[2]))
		{
			/* this is two hexadecimal digits following a '%' */
			char hexstr[3];
			char *ptr;
			hexstr[0] = string[1];
			hexstr[1] = string[2];
			hexstr[2] = 0;

			hex = strtol(hexstr, &ptr, 16);

			in = (unsigned char)hex; /* this long is never bigger than 255 anyway */
			if( inplace )
			{
				*string = in;
				memmove(string+1, string+3, alloc-2);
			}
			else
			{
				string+=2;
			}
			alloc-=2;
		}
		if( !inplace )
			ns[strindex++] = in;
		string++;
	}
	if( inplace )
	{
		free(ns);
		return string;
	}
	else
	{
		ns[strindex] = '\0'; /* terminate it */
		return ns;
	}
}

/* These next functions implement a repeatable random function with a user-provided seed */
static int
seedRandomByte(uint32_t seed)
{
	unsigned char t;

	if( !sqlite3Prng.isInit )
	{
		int i;
		char k[256];
		sqlite3Prng.j = 0;
		sqlite3Prng.i = 0;
		memset(&k, '\0', sizeof(k));
		memcpy(&k, &seed, 4);
		for(i=0; i<256; i++)
			sqlite3Prng.s[i] = i;
		for(i=0; i<256; i++)
		{
			sqlite3Prng.j += sqlite3Prng.s[i] + k[i];
			t = sqlite3Prng.s[sqlite3Prng.j];
			sqlite3Prng.s[sqlite3Prng.j] = sqlite3Prng.s[i];
			sqlite3Prng.s[i] = t;
		}
		sqlite3Prng.isInit = 1;
	}
	/* Generate and return single random byte */
	sqlite3Prng.i++;
	t = sqlite3Prng.s[sqlite3Prng.i];
	sqlite3Prng.j += t;
	sqlite3Prng.s[sqlite3Prng.i] = sqlite3Prng.s[sqlite3Prng.j];
	sqlite3Prng.s[sqlite3Prng.j] = t;
	t += sqlite3Prng.s[sqlite3Prng.i];

	return sqlite3Prng.s[t];
}

void
seedRandomness(int n, void *pbuf, uint32_t seed)
{
	unsigned char *zbuf = pbuf;

	while( n-- )
		*(zbuf++) = seedRandomByte(seed);
}

void
TiVoRandomSeedFunc(sqlite3_context *context, int argc, sqlite3_value **argv)
{
	int64_t r, seed;

	if( argc != 1 || sqlite3_value_type(argv[0]) != SQLITE_INTEGER )
		return;
	seed = sqlite3_value_int64(argv[0]);
	seedRandomness(sizeof(r), &r, seed);
	sqlite3_result_int64(context, r);
}

int
is_tivo_file(const char *path)
{
	unsigned char buf[5];
	unsigned char hdr[5] = { 'T','i','V','o','\0' };
	int fd;

	/* read file header */
	fd = open(path, O_RDONLY);
	if( !fd )
		return 0;
	if( read(fd, buf, 5) < 0 )
		buf[0] = 'X';
	close(fd);

	return !memcmp(buf, hdr, 5);
}

#endif
