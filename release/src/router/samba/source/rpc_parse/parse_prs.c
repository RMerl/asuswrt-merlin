/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba memory buffer functions
   Copyright (C) Andrew Tridgell              1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Jeremy Allison 1999.
   
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

extern int DEBUGLEVEL;

#include "includes.h"


/*******************************************************************
 debug output for parsing info.

 XXXX side-effect of this function is to increase the debug depth XXXX

 ********************************************************************/
void prs_debug(prs_struct *ps, int depth, char *desc, char *fn_name)
{
	DEBUG(5+depth, ("%s%06x %s %s\n", tab_depth(depth), ps->data_offset, fn_name, desc));
}

/*******************************************************************
 Initialise a parse structure - malloc the data if requested.
 ********************************************************************/

BOOL prs_init(prs_struct *ps, uint32 size, uint8 align, BOOL io)
{
	ZERO_STRUCTP(ps);
	ps->io = io;
	ps->bigendian_data = False;
	ps->align = align;
	ps->is_dynamic = False;
	ps->data_offset = 0;
	ps->buffer_size = 0;
	ps->data_p = NULL;

	if (size != 0) {
		ps->buffer_size = size;
		if((ps->data_p = (char *)malloc((size_t)size)) == NULL) {
			DEBUG(0,("prs_init: malloc fail for %u bytes.\n", (unsigned int)size));
			return False;
		}
		ps->is_dynamic = True; /* We own this memory. */
	}

	return True;
}

/*******************************************************************
 Delete the memory in a parse structure - if we own it.
 ********************************************************************/

void prs_mem_free(prs_struct *ps)
{
	if(ps->is_dynamic && (ps->data_p != NULL))
		free(ps->data_p);
	ps->is_dynamic = False;
	ps->data_p = NULL;
	ps->buffer_size = 0;
	ps->data_offset = 0;
}

/*******************************************************************
 Hand some already allocated memory to a prs_struct.
 ********************************************************************/

void prs_give_memory(prs_struct *ps, char *buf, uint32 size, BOOL is_dynamic)
{
	ps->is_dynamic = is_dynamic;
	ps->data_p = buf;
	ps->buffer_size = size;
}

/*******************************************************************
 Take some memory back from a prs_struct.
 ********************************************************************/

char *prs_take_memory(prs_struct *ps, uint32 *psize)
{
	char *ret = ps->data_p;
	if(psize)
		*psize = ps->buffer_size;
	ps->is_dynamic = False;
	prs_mem_free(ps);
	return ret;
}

/*******************************************************************
 Attempt, if needed, to grow a data buffer.
 Also depends on the data stream mode (io).
 ********************************************************************/

BOOL prs_grow(prs_struct *ps, uint32 extra_space)
{
	uint32 new_size;
	char *new_data;

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
		 * Ensure we have at least a PDU's length, or extra_space, whichever
		 * is greater.
		 */

		new_size = MAX(MAX_PDU_FRAG_LEN,extra_space);

		if((new_data = malloc(new_size)) == NULL) {
			DEBUG(0,("prs_grow: Malloc failure for size %u.\n", (unsigned int)new_size));
			return False;
		}
		memset(new_data, '\0', new_size );
	} else {

		/*
		 * If the current buffer size is bigger than the space needed, just 
		 * double it, else add extra_space.
		 */

		new_size = MAX(ps->buffer_size*2, ps->buffer_size + extra_space);

		if((new_data = Realloc(ps->data_p, new_size)) == NULL) {
			DEBUG(0,("prs_grow: Realloc failure for size %u.\n",
				(unsigned int)new_size));
			return False;
		}
	}

	ps->buffer_size = new_size;
	ps->data_p = new_data;

	return True;
}

/*******************************************************************
 Attempt to force a data buffer to grow by len bytes.
 This is only used when appending more data onto a prs_struct
 when reading an rpc reply, before unmarshalling it.
 ********************************************************************/

BOOL prs_force_grow(prs_struct *ps, uint32 extra_space)
{
	uint32 new_size = ps->buffer_size + extra_space;
	char *new_data;

	if(!UNMARSHALLING(ps) || !ps->is_dynamic) {
		DEBUG(0,("prs_force_grow: Buffer overflow - unable to expand buffer by %u bytes.\n",
				(unsigned int)extra_space));
		return False;
	}

	if((new_data = Realloc(ps->data_p, new_size)) == NULL) {
		DEBUG(0,("prs_force_grow: Realloc failure for size %u.\n",
			(unsigned int)new_size));
		return False;
	}

	ps->buffer_size = new_size;
	ps->data_p = new_data;

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

BOOL prs_set_offset(prs_struct *ps, uint32 offset)
{
	if(offset <= ps->data_offset) {
		ps->data_offset = offset;
		return True;
	}

	if(!prs_grow(ps, offset - ps->data_offset))
		return False;

	ps->data_offset = offset;
	return True;
}

/*******************************************************************
 Append the data from one parse_struct into another.
 ********************************************************************/

BOOL prs_append_prs_data(prs_struct *dst, prs_struct *src)
{
	if(!prs_grow(dst, prs_offset(src)))
		return False;

	memcpy(&dst->data_p[dst->data_offset], prs_data_p(src), (size_t)prs_offset(src));
	dst->data_offset += prs_offset(src);

	return True;
}

/*******************************************************************
 Append the data from a buffer into a parse_struct.
 ********************************************************************/

BOOL prs_append_data(prs_struct *dst, char *src, uint32 len)
{
	if(!prs_grow(dst, len))
		return False;

	memcpy(&dst->data_p[dst->data_offset], src, (size_t)len);
	dst->data_offset += len;

	return True;
}

/*******************************************************************
 Set the data as big-endian (external interface).
 ********************************************************************/

void prs_set_bigendian_data(prs_struct *ps)
{
	ps->bigendian_data = True;
}

/*******************************************************************
 Align a the data_len to a multiple of align bytes - filling with
 zeros.
 ********************************************************************/

BOOL prs_align(prs_struct *ps)
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
			DEBUG(0,("prs_mem_get: reading data of size %u would overrun buffer.\n",
					(unsigned int)extra_size ));
			return NULL;
		}
	} else {
		/*
		 * Writing - grow the buffer if needed.
		 */
		if(!prs_grow(ps, extra_size))
			return False;
	}
	return &ps->data_p[ps->data_offset];
}

/*******************************************************************
 Change the struct type.
 ********************************************************************/

void prs_switch_type(prs_struct *ps, BOOL io)
{
	if ((ps->io ^ io) == True)
		ps->io=io;
}

/*******************************************************************
 Force a prs_struct to be dynamic even when it's size is 0.
 ********************************************************************/

void prs_force_dynamic(prs_struct *ps)
{
	ps->is_dynamic=True;
}

/*******************************************************************
 Stream a uint8.
 ********************************************************************/

BOOL prs_uint8(char *name, prs_struct *ps, int depth, uint8 *data8)
{
	char *q = prs_mem_get(ps, sizeof(uint8));
	if (q == NULL)
		return False;

	DBG_RW_CVAL(name, depth, ps->data_offset, ps->io, q, *data8)
	ps->data_offset += sizeof(uint8);

	return True;
}

/*******************************************************************
 Stream a uint16.
 ********************************************************************/

BOOL prs_uint16(char *name, prs_struct *ps, int depth, uint16 *data16)
{
	char *q = prs_mem_get(ps, sizeof(uint16));
	if (q == NULL)
		return False;

	DBG_RW_SVAL(name, depth, ps->data_offset, ps->io, ps->bigendian_data, q, *data16)
	ps->data_offset += sizeof(uint16);

	return True;
}

/*******************************************************************
 Stream a uint32.
 ********************************************************************/

BOOL prs_uint32(char *name, prs_struct *ps, int depth, uint32 *data32)
{
	char *q = prs_mem_get(ps, sizeof(uint32));
	if (q == NULL)
		return False;

	DBG_RW_IVAL(name, depth, ps->data_offset, ps->io, ps->bigendian_data, q, *data32)
	ps->data_offset += sizeof(uint32);

	return True;
}


/******************************************************************
 Stream an array of uint8s. Length is number of uint8s.
 ********************************************************************/

BOOL prs_uint8s(BOOL charmode, char *name, prs_struct *ps, int depth, uint8 *data8s, int len)
{
	char *q = prs_mem_get(ps, len * sizeof(uint8));
	if (q == NULL)
		return False;

	DBG_RW_PCVAL(charmode, name, depth, ps->data_offset, ps->io, q, data8s, len)
	ps->data_offset += (len * sizeof(uint8));

	return True;
}

/******************************************************************
 Stream an array of uint32s. Length is number of uint32s.
 ********************************************************************/

BOOL prs_uint32s(BOOL charmode, char *name, prs_struct *ps, int depth, uint32 *data32s, int len)
{
	char *q = prs_mem_get(ps, len * sizeof(uint32));
	if (q == NULL)
		return False;

	DBG_RW_PIVAL(charmode, name, depth, ps->data_offset, ps->io, ps->bigendian_data, q, data32s, len)
	ps->data_offset += (len * sizeof(uint32));

	return True;
}

/******************************************************************
 Stream a "not" unicode string, length/buffer specified separately,
 in byte chars. String is in little-endian format.
 ********************************************************************/

BOOL prs_buffer2(BOOL charmode, char *name, prs_struct *ps, int depth, BUFFER2 *str)
{
	char *p = (char *)str->buffer;
	char *q = prs_mem_get(ps, str->buf_len);
	if (q == NULL)
		return False;

	/* If we're using big-endian, reverse to get little-endian. */
	if(ps->bigendian_data)
		DBG_RW_PSVAL(charmode, name, depth, ps->data_offset, ps->io, ps->bigendian_data, q, p, str->buf_len/2)
	else
		DBG_RW_PCVAL(charmode, name, depth, ps->data_offset, ps->io, q, p, str->buf_len)
	ps->data_offset += str->buf_len;

	return True;
}

/******************************************************************
 Stream a string, length/buffer specified separately,
 in uint8 chars.
 ********************************************************************/

BOOL prs_string2(BOOL charmode, char *name, prs_struct *ps, int depth, STRING2 *str)
{
	char *q = prs_mem_get(ps, str->str_str_len * sizeof(uint8));
	if (q == NULL)
		return False;

	DBG_RW_PCVAL(charmode, name, depth, ps->data_offset, ps->io, q, str->buffer, str->str_max_len)
	ps->data_offset += (str->str_str_len * sizeof(uint8));

	return True;
}

/******************************************************************
 Stream a unicode string, length/buffer specified separately,
 in uint16 chars. We use DBG_RW_PCVAL, not DBG_RW_PSVAL here
 as the unicode string is already in little-endian format.
 ********************************************************************/

BOOL prs_unistr2(BOOL charmode, char *name, prs_struct *ps, int depth, UNISTR2 *str)
{
	char *p = (char *)str->buffer;
	char *q = prs_mem_get(ps, str->uni_str_len * sizeof(uint16));
	if (q == NULL)
		return False;

	/* If we're using big-endian, reverse to get little-endian. */
	if(ps->bigendian_data)
		DBG_RW_PSVAL(charmode, name, depth, ps->data_offset, ps->io, ps->bigendian_data, q, p, str->uni_str_len)
	else
		DBG_RW_PCVAL(charmode, name, depth, ps->data_offset, ps->io, q, p, str->uni_str_len * 2)
	ps->data_offset += (str->uni_str_len * sizeof(uint16));

	return True;
}

/******************************************************************
 Stream a unicode string, length/buffer specified separately,
 in uint16 chars. We use DBG_RW_PCVAL, not DBG_RW_PSVAL here
 as the unicode string is already in little-endian format.
 ********************************************************************/

BOOL prs_unistr3(BOOL charmode, char *name, UNISTR3 *str, prs_struct *ps, int depth)
{
	char *p = (char *)str->str.buffer;
	char *q = prs_mem_get(ps, str->uni_str_len * sizeof(uint16));
	if (q == NULL)
		return False;

	/* If we're using big-endian, reverse to get little-endian. */
	if(ps->bigendian_data)
		DBG_RW_PSVAL(charmode, name, depth, ps->data_offset, ps->io, ps->bigendian_data, q, p, str->uni_str_len)
	else
		DBG_RW_PCVAL(charmode, name, depth, ps->data_offset, ps->io, q, p, str->uni_str_len * 2)
	ps->data_offset += (str->uni_str_len * sizeof(uint16));

	return True;
}

/*******************************************************************
 Stream a unicode  null-terminated string. As the string is already
 in little-endian format then do it as a stream of bytes.
 ********************************************************************/

BOOL prs_unistr(char *name, prs_struct *ps, int depth, UNISTR *str)
{
	int len = 0;
	unsigned char *p = (unsigned char *)str->buffer;
	uint8 *start;
	char *q;

	for(len = 0; len < (sizeof(str->buffer) / sizeof(str->buffer[0])) &&
			   str->buffer[len] != 0; len++)
		;

	q = prs_mem_get(ps, len*2);
	if (q == NULL)
		return False;

	start = (uint8*)q;

	len = 0;
	do 
	{
		if(ps->bigendian_data) {
			RW_SVAL(ps->io, ps->bigendian_data, q, *p, 0)
			p += 2;
			q += 2;
		} else {
			RW_CVAL(ps->io, q, *p, 0);
			p++;
			q++;
			RW_CVAL(ps->io, q, *p, 0);
			p++;
			q++;
		}
		len++;
	} while ((len < (sizeof(str->buffer) / sizeof(str->buffer[0]))) &&
		     (str->buffer[len] != 0));

	ps->data_offset += len*2;

	dump_data(5+depth, (char *)start, len * 2);

	return True;
}

/*******************************************************************
 Stream a null-terminated string.  len is strlen, and therefore does
 not include the null-termination character.
 ********************************************************************/

BOOL prs_string(char *name, prs_struct *ps, int depth, char *str, int len, int max_buf_size)
{
	char *q;
	uint8 *start;
	int i;

	len = MIN(len, (max_buf_size-1));

	q = prs_mem_get(ps, len+1);
	if (q == NULL)
		return False;

	start = (uint8*)q;

	for(i = 0; i < len; i++) {
		RW_CVAL(ps->io, q, str[i],0);
		q++;
	}

	/* The terminating null. */
	str[i] = '\0';

	if (MARSHALLING(ps)) {
		RW_CVAL(ps->io, q, str[i], 0);
	}

	ps->data_offset += len+1;

	dump_data(5+depth, (char *)start, len);

	return True;
}

/*******************************************************************
 prs_uint16 wrapper. Call this and it sets up a pointer to where the
 uint16 should be stored, or gets the size if reading.
 ********************************************************************/

BOOL prs_uint16_pre(char *name, prs_struct *ps, int depth, uint16 *data16, uint32 *offset)
{
	(*offset) = ps->data_offset;
	if (UNMARSHALLING(ps)) {
		/* reading. */
		return prs_uint16(name, ps, depth, data16);
	} else {
		char *q = prs_mem_get(ps, sizeof(uint16));
		if(q ==NULL)
			return False;
		ps->data_offset += sizeof(uint16);
	}
	return True;
}

/*******************************************************************
 prs_uint16 wrapper.  call this and it retrospectively stores the size.
 does nothing on reading, as that is already handled by ...._pre()
 ********************************************************************/

BOOL prs_uint16_post(char *name, prs_struct *ps, int depth, uint16 *data16,
				uint32 ptr_uint16, uint32 start_offset)
{
	if (MARSHALLING(ps)) {
		/* 
		 * Writing - temporarily move the offset pointer.
		 */
		uint16 data_size = ps->data_offset - start_offset;
		uint32 old_offset = ps->data_offset;

		ps->data_offset = ptr_uint16;
		if(!prs_uint16(name, ps, depth, &data_size)) {
			ps->data_offset = old_offset;
			return False;
		}
		ps->data_offset = old_offset;
	} else {
		ps->data_offset = start_offset + (uint32)(*data16);
	}
	return True;
}

/*******************************************************************
 prs_uint32 wrapper. Call this and it sets up a pointer to where the
 uint32 should be stored, or gets the size if reading.
 ********************************************************************/

BOOL prs_uint32_pre(char *name, prs_struct *ps, int depth, uint32 *data32, uint32 *offset)
{
	(*offset) = ps->data_offset;
	if (UNMARSHALLING(ps)) {
		/* reading. */
		return prs_uint32(name, ps, depth, data32);
	} else {
		ps->data_offset += sizeof(uint32);
	}
	return True;
}

/*******************************************************************
 prs_uint32 wrapper.  call this and it retrospectively stores the size.
 does nothing on reading, as that is already handled by ...._pre()
 ********************************************************************/

BOOL prs_uint32_post(char *name, prs_struct *ps, int depth, uint32 *data32,
				uint32 ptr_uint32, uint32 data_size)
{
	if (MARSHALLING(ps)) {
		/* 
		 * Writing - temporarily move the offset pointer.
		 */
		uint32 old_offset = ps->data_offset;
		ps->data_offset = ptr_uint32;
		if(!prs_uint32(name, ps, depth, &data_size)) {
			ps->data_offset = old_offset;
			return False;
		}
		ps->data_offset = old_offset;
	}
	return True;
}
