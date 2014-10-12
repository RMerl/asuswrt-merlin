/*
   Unix SMB/CIFS implementation.
   Samba memory buffer functions
   Copyright (C) Andrew Tridgell              1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Jeremy Allison               1999
   Copyright (C) Andrew Bartlett              2003.

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

#include "includes.h"
#include "reg_parse_prs.h"
#include "rpc_dce.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_PARSE

/*******************************************************************
 Debug output for parsing info

 XXXX side-effect of this function is to increase the debug depth XXXX.

********************************************************************/

void prs_debug(prs_struct *ps, int depth, const char *desc, const char *fn_name)
{
	DEBUG(5+depth, ("%s%06x %s %s\n", tab_depth(5+depth,depth), ps->data_offset, fn_name, desc));
}

/**
 * Initialise an expandable parse structure.
 *
 * @param size Initial buffer size.  If >0, a new buffer will be
 * created with talloc().
 *
 * @return False if allocation fails, otherwise True.
 **/

bool prs_init(prs_struct *ps, uint32 size, TALLOC_CTX *ctx, bool io)
{
	ZERO_STRUCTP(ps);
	ps->io = io;
	ps->bigendian_data = RPC_LITTLE_ENDIAN;
	ps->align = RPC_PARSE_ALIGN;
	ps->is_dynamic = False;
	ps->data_offset = 0;
	ps->buffer_size = 0;
	ps->data_p = NULL;
	ps->mem_ctx = ctx;

	if (size != 0) {
		ps->buffer_size = size;
		ps->data_p = (char *)talloc_zero_size(ps->mem_ctx, size);
		if(ps->data_p == NULL) {
			DEBUG(0,("prs_init: talloc fail for %u bytes.\n", (unsigned int)size));
			return False;
		}
		ps->is_dynamic = True; /* We own this memory. */
	} else if (MARSHALLING(ps)) {
		/* If size is zero and we're marshalling we should allocate memory on demand. */
		ps->is_dynamic = True;
	}

	return True;
}

/*******************************************************************
 Delete the memory in a parse structure - if we own it.

 NOTE: Contrary to the somewhat confusing naming, this function is not
       intended for freeing memory allocated by prs_alloc_mem().
       That memory is also attached to the talloc context given by
       ps->mem_ctx, but is only freed when that talloc context is
       freed. prs_mem_free() is used to delete "dynamic" memory
       allocated in marshalling/unmarshalling.
 ********************************************************************/

void prs_mem_free(prs_struct *ps)
{
	if(ps->is_dynamic) {
		TALLOC_FREE(ps->data_p);
	}
	ps->is_dynamic = False;
	ps->buffer_size = 0;
	ps->data_offset = 0;
}

/*******************************************************************
 Allocate memory when unmarshalling... Always zero clears.
 ********************************************************************/

#if defined(PARANOID_MALLOC_CHECKER)
char *prs_alloc_mem_(prs_struct *ps, size_t size, unsigned int count)
#else
char *prs_alloc_mem(prs_struct *ps, size_t size, unsigned int count)
#endif
{
	char *ret = NULL;

	if (size && count) {
		/* We can't call the type-safe version here. */
		ret = (char *)_talloc_zero_array(ps->mem_ctx, size, count,
						 "parse_prs");
	}
	return ret;
}

/*******************************************************************
 Return the current talloc context we're using.
 ********************************************************************/

TALLOC_CTX *prs_get_mem_context(prs_struct *ps)
{
	return ps->mem_ctx;
}

/*******************************************************************
 Attempt, if needed, to grow a data buffer.
 Also depends on the data stream mode (io).
 ********************************************************************/

bool prs_grow(prs_struct *ps, uint32 extra_space)
{
	uint32 new_size;

	ps->grow_size = MAX(ps->grow_size, ps->data_offset + extra_space);

	if(ps->data_offset + extra_space <= ps->buffer_size)
		return True;

	/*
	 * We cannot grow the buffer if we're not reading
	 * into the prs_struct, or if we don't own the memory.
	 */

	if(UNMARSHALLING(ps) || !ps->is_dynamic) {
		DEBUG(0,("prs_grow: Buffer overflow - unable to expand buffer by %u bytes.\n",
				(unsigned int)extra_space));
		return False;
	}

	/*
	 * Decide how much extra space we really need.
	 */

	extra_space -= (ps->buffer_size - ps->data_offset);
	if(ps->buffer_size == 0) {

		/*
		 * Start with 128 bytes (arbitrary value), enough for small rpc
		 * requests
		 */
		new_size = MAX(128, extra_space);

		ps->data_p = (char *)talloc_zero_size(ps->mem_ctx, new_size);
		if(ps->data_p == NULL) {
			DEBUG(0,("prs_grow: talloc failure for size %u.\n", (unsigned int)new_size));
			return False;
		}
	} else {
		/*
		 * If the current buffer size is bigger than the space needed,
		 * just double it, else add extra_space. Always keep 64 bytes
		 * more, so that after we added a large blob we don't have to
		 * realloc immediately again.
		 */
		new_size = MAX(ps->buffer_size*2,
			       ps->buffer_size + extra_space + 64);

		ps->data_p = talloc_realloc(ps->mem_ctx,
						ps->data_p,
						char,
						new_size);
		if (ps->data_p == NULL) {
			DEBUG(0,("prs_grow: Realloc failure for size %u.\n",
				(unsigned int)new_size));
			return False;
		}

		memset(&ps->data_p[ps->buffer_size], '\0', (size_t)(new_size - ps->buffer_size));
	}
	ps->buffer_size = new_size;

	return True;
}

/*******************************************************************
 Get the data pointer (external interface).
********************************************************************/

char *prs_data_p(prs_struct *ps)
{
	return ps->data_p;
}

/*******************************************************************
 Get the current data size (external interface).
 ********************************************************************/

uint32 prs_data_size(prs_struct *ps)
{
	return ps->buffer_size;
}

/*******************************************************************
 Fetch the current offset (external interface).
 ********************************************************************/

uint32 prs_offset(prs_struct *ps)
{
	return ps->data_offset;
}

/*******************************************************************
 Set the current offset (external interface).
 ********************************************************************/

bool prs_set_offset(prs_struct *ps, uint32 offset)
{
	if ((offset > ps->data_offset)
	    && !prs_grow(ps, offset - ps->data_offset)) {
		return False;
	}

	ps->data_offset = offset;
	return True;
}

/*******************************************************************
 Append the data from a buffer into a parse_struct.
 ********************************************************************/

bool prs_copy_data_in(prs_struct *dst, const char *src, uint32 len)
{
	if (len == 0)
		return True;

	if(!prs_grow(dst, len))
		return False;

	memcpy(&dst->data_p[dst->data_offset], src, (size_t)len);
	dst->data_offset += len;

	return True;
}

/*******************************************************************
 Align a the data_len to a multiple of align bytes - filling with
 zeros.
 ********************************************************************/

bool prs_align(prs_struct *ps)
{
	uint32 mod = ps->data_offset & (ps->align-1);

	if (ps->align != 0 && mod != 0) {
		uint32 extra_space = (ps->align - mod);
		if(!prs_grow(ps, extra_space))
			return False;
		memset(&ps->data_p[ps->data_offset], '\0', (size_t)extra_space);
		ps->data_offset += extra_space;
	}

	return True;
}

/******************************************************************
 Align on a 8 byte boundary
 *****************************************************************/

bool prs_align_uint64(prs_struct *ps)
{
	bool ret;
	uint8 old_align = ps->align;

	ps->align = 8;
	ret = prs_align(ps);
	ps->align = old_align;

	return ret;
}

/*******************************************************************
 Ensure we can read/write to a given offset.
 ********************************************************************/

char *prs_mem_get(prs_struct *ps, uint32 extra_size)
{
	if(UNMARSHALLING(ps)) {
		/*
		 * If reading, ensure that we can read the requested size item.
		 */
		if (ps->data_offset + extra_size > ps->buffer_size) {
			DEBUG(0,("prs_mem_get: reading data of size %u would overrun "
				"buffer by %u bytes.\n",
				(unsigned int)extra_size,
				(unsigned int)(ps->data_offset + extra_size - ps->buffer_size) ));
			return NULL;
		}
	} else {
		/*
		 * Writing - grow the buffer if needed.
		 */
		if(!prs_grow(ps, extra_size))
			return NULL;
	}
	return &ps->data_p[ps->data_offset];
}

/*******************************************************************
 Change the struct type.
 ********************************************************************/

void prs_switch_type(prs_struct *ps, bool io)
{
	if ((ps->io ^ io) == True)
		ps->io=io;
}

/*******************************************************************
 Stream a uint8.
 ********************************************************************/

bool prs_uint8(const char *name, prs_struct *ps, int depth, uint8 *data8)
{
	char *q = prs_mem_get(ps, 1);
	if (q == NULL)
		return False;

	if (UNMARSHALLING(ps))
		*data8 = CVAL(q,0);
	else
		SCVAL(q,0,*data8);

	DEBUGADD(5,("%s%04x %s: %02x\n", tab_depth(5,depth), ps->data_offset, name, *data8));

	ps->data_offset += 1;

	return True;
}

/*******************************************************************
 Stream a uint16.
 ********************************************************************/

bool prs_uint16(const char *name, prs_struct *ps, int depth, uint16 *data16)
{
	char *q = prs_mem_get(ps, sizeof(uint16));
	if (q == NULL)
		return False;

	if (UNMARSHALLING(ps)) {
		if (ps->bigendian_data)
			*data16 = RSVAL(q,0);
		else
			*data16 = SVAL(q,0);
	} else {
		if (ps->bigendian_data)
			RSSVAL(q,0,*data16);
		else
			SSVAL(q,0,*data16);
	}

	DEBUGADD(5,("%s%04x %s: %04x\n", tab_depth(5,depth), ps->data_offset, name, *data16));

	ps->data_offset += sizeof(uint16);

	return True;
}

/*******************************************************************
 Stream a uint32.
 ********************************************************************/

bool prs_uint32(const char *name, prs_struct *ps, int depth, uint32 *data32)
{
	char *q = prs_mem_get(ps, sizeof(uint32));
	if (q == NULL)
		return False;

	if (UNMARSHALLING(ps)) {
		if (ps->bigendian_data)
			*data32 = RIVAL(q,0);
		else
			*data32 = IVAL(q,0);
	} else {
		if (ps->bigendian_data)
			RSIVAL(q,0,*data32);
		else
			SIVAL(q,0,*data32);
	}

	DEBUGADD(5,("%s%04x %s: %08x\n", tab_depth(5,depth), ps->data_offset, name, *data32));

	ps->data_offset += sizeof(uint32);

	return True;
}

/*******************************************************************
 Stream a uint64_struct
 ********************************************************************/
bool prs_uint64(const char *name, prs_struct *ps, int depth, uint64 *data64)
{
	if (UNMARSHALLING(ps)) {
		uint32 high, low;

		if (!prs_uint32(name, ps, depth+1, &low))
			return False;

		if (!prs_uint32(name, ps, depth+1, &high))
			return False;

		*data64 = ((uint64_t)high << 32) + low;

		return True;
	} else {
		uint32 high = (*data64) >> 32, low = (*data64) & 0xFFFFFFFF;
		return prs_uint32(name, ps, depth+1, &low) &&
			   prs_uint32(name, ps, depth+1, &high);
	}
}

/******************************************************************
 Stream an array of uint8s. Length is number of uint8s.
 ********************************************************************/

bool prs_uint8s(bool charmode, const char *name, prs_struct *ps, int depth, uint8 *data8s, int len)
{
	int i;
	char *q = prs_mem_get(ps, len);
	if (q == NULL)
		return False;

	if (UNMARSHALLING(ps)) {
		for (i = 0; i < len; i++)
			data8s[i] = CVAL(q,i);
	} else {
		for (i = 0; i < len; i++)
			SCVAL(q, i, data8s[i]);
	}

	DEBUGADD(5,("%s%04x %s: ", tab_depth(5,depth), ps->data_offset ,name));
	if (charmode)
		print_asc(5, (unsigned char*)data8s, len);
	else {
		for (i = 0; i < len; i++)
			DEBUGADD(5,("%02x ", data8s[i]));
	}
	DEBUGADD(5,("\n"));

	ps->data_offset += len;

	return True;
}
