/*
 * $Id: playlist.h,v 1.1 2009-06-30 02:31:09 steven Exp $
 * iTunes-style smart playlists
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

#ifndef _PL_H_
#define _PL_H_

#include "mp3-scanner.h"

#define T_INT  0
#define T_STR  1
#define T_DATE 2

typedef struct tag_pl_node {
    int op;
    int type;
    union { 
	int ival;
	struct tag_pl_node *plval;
    } arg1;
    union {
	char *cval;
	int ival;
	struct tag_pl_node *plval;
    } arg2;
} PL_NODE;

typedef struct tag_smart_playlist {
    char *name;
    unsigned int id;
    PL_NODE *root;
    struct tag_smart_playlist *next;
} SMART_PLAYLIST;

extern SMART_PLAYLIST pl_smart;
extern int pl_error;

extern void pl_dump(void);
extern int pl_load(char *file);
extern void pl_eval(MP3FILE *pmp3);
extern void pl_register(void);

#endif /* _PL_H_ */


