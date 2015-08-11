/*
 * $Id: mp3-scanner.h 1423 2006-11-06 03:42:38Z rpedde $
 * Header file for mp3 scanner and monitor
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _MP3_SCANNER_H_
#define _MP3_SCANNER_H_

#include <sys/types.h>
#include "ff-dbstruct.h"  /* for MP3FILE */


#define SCAN_NOT_COMPDIR  0
#define SCAN_IS_COMPDIR   1
#define SCAN_TEST_COMPDIR 2

#define WINAMP_GENRE_UNKNOWN 148

extern void scan_filename(char *path, int compdir, char *extensions);

extern char *scan_winamp_genre[];
extern int scan_init(char **patharray);
extern void make_composite_tags(MP3FILE *song);

#ifndef TRUE
# define TRUE  1
# define FALSE 0
#endif


#endif /* _MP3_SCANNER_H_ */
