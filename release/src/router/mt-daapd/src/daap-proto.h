/*
 * $Id: daap-proto.h,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Helper functions for formatting a daap message
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

#ifndef _DAAP_PROTO_H_
#define _DAAP_PROTO_H_


#include <sys/types.h>
#include "zlib.h"

#define GZIP_CHUNK 16384
#define GZIP_COMPRESSION_LEVEL Z_DEFAULT_COMPRESSION

typedef struct gzip_stream_tag {
  int bytes_in;
  int bytes_out;
  int in_size;
  char *in;
  char *out;
} GZIP_STREAM;

GZIP_STREAM *gzip_alloc(void);
ssize_t gzip_write(GZIP_STREAM *gz, void *buf, size_t size);
int gzip_compress(GZIP_STREAM *gz);
int gzip_close(GZIP_STREAM *gz, int fd);

typedef struct daap_block_tag {
    char tag[4];
    int reported_size;
    int size;
    int free;
    char *value;
    char svalue[4]; /* for statics up to 4 bytes */
    struct daap_block_tag *parent;
    struct daap_block_tag *children;
    struct daap_block_tag *last_child;
    struct daap_block_tag *next;
} DAAP_BLOCK;

DAAP_BLOCK *daap_add_int(DAAP_BLOCK *parent, char *tag, int value);
DAAP_BLOCK *daap_add_data(DAAP_BLOCK *parent, char *tag, int len, void *value);
DAAP_BLOCK *daap_add_string(DAAP_BLOCK *parent, char *tag, char *value);
DAAP_BLOCK *daap_add_empty(DAAP_BLOCK *parent, char *tag);
DAAP_BLOCK *daap_add_char(DAAP_BLOCK *parent, char *tag, char value);
DAAP_BLOCK *daap_add_short(DAAP_BLOCK *parent, char *tag, short int value);
DAAP_BLOCK *daap_add_long(DAAP_BLOCK *parent, char *tag, int v1, int v2);
int daap_serialize(DAAP_BLOCK *root, int fd, GZIP_STREAM *gz);
void daap_free(DAAP_BLOCK *root);

// remove a block from it's parent (and free it)
void daap_remove(DAAP_BLOCK* root);

// search a block's direct children for a block with a given tag
DAAP_BLOCK *daap_find(DAAP_BLOCK *parent, char* tag);

// search a block's children and change an integer value
int daap_set_int(DAAP_BLOCK* parent, char* tag, int value);

#endif

