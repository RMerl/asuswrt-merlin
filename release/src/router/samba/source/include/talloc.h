/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   Samba temporary memory allocation functions
   Copyright (C) Andrew Tridgell 2000
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

struct talloc_chunk {
	struct talloc_chunk *next;
	void *ptr;
	size_t alloc_size;
	size_t total_size;
};

typedef struct {
	struct talloc_chunk *list;
} TALLOC_CTX;

