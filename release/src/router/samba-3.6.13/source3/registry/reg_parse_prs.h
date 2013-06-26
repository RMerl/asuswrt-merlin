/*
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
   Copyright (C) Jeremy Allison 2000-2004

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _REG_PARSE_PRS_H_
#define _REG_PARSE_PRS_H_


#define prs_init_empty( _ps_, _ctx_, _io_ ) (void) prs_init((_ps_), 0, (_ctx_), (_io_))

typedef struct _prs_struct {
	bool io; /* parsing in or out of data stream */
	/*
	 * If the (incoming) data is big-endian. On output we are
	 * always little-endian.
	 */
	bool bigendian_data;
	uint8 align; /* data alignment */
	bool is_dynamic; /* Do we own this memory or not ? */
	uint32 data_offset; /* Current working offset into data. */
	uint32 buffer_size; /* Current allocated size of the buffer. */
	uint32 grow_size; /* size requested via prs_grow() calls */
	/* The buffer itself. If "is_dynamic" is true this
	 * MUST BE TALLOC'ed off mem_ctx. */
	char *data_p;
	TALLOC_CTX *mem_ctx; /* When unmarshalling, use this.... */
} prs_struct;

/*
 * Defines for io member of prs_struct.
 */

#define MARSHALL 0
#define UNMARSHALL 1

#define MARSHALLING(ps) (!(ps)->io)
#define UNMARSHALLING(ps) ((ps)->io)

#define RPC_PARSE_ALIGN 4

void prs_debug(prs_struct *ps, int depth, const char *desc, const char *fn_name);
bool prs_init(prs_struct *ps, uint32 size, TALLOC_CTX *ctx, bool io);
void prs_mem_free(prs_struct *ps);
char *prs_alloc_mem_(prs_struct *ps, size_t size, unsigned int count);
char *prs_alloc_mem(prs_struct *ps, size_t size, unsigned int count);
TALLOC_CTX *prs_get_mem_context(prs_struct *ps);
bool prs_grow(prs_struct *ps, uint32 extra_space);
char *prs_data_p(prs_struct *ps);
uint32 prs_data_size(prs_struct *ps);
uint32 prs_offset(prs_struct *ps);
bool prs_set_offset(prs_struct *ps, uint32 offset);
bool prs_copy_data_in(prs_struct *dst, const char *src, uint32 len);
bool prs_align(prs_struct *ps);
bool prs_align_uint64(prs_struct *ps);
char *prs_mem_get(prs_struct *ps, uint32 extra_size);
void prs_switch_type(prs_struct *ps, bool io);
bool prs_uint8(const char *name, prs_struct *ps, int depth, uint8 *data8);
bool prs_uint16(const char *name, prs_struct *ps, int depth, uint16 *data16);
bool prs_uint32(const char *name, prs_struct *ps, int depth, uint32 *data32);
bool prs_uint64(const char *name, prs_struct *ps, int depth, uint64 *data64);
bool prs_uint8s(bool charmode, const char *name, prs_struct *ps, int depth, uint8 *data8s, int len);

#endif
