/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "includes.h"

extern int DEBUGLEVEL;

/*******************************************************************
 Reads or writes a UTIME type.
********************************************************************/

static BOOL smb_io_utime(char *desc, UTIME *t, prs_struct *ps, int depth)
{
	if (t == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_utime");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32 ("time", ps, depth, &t->time))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes an NTTIME structure.
********************************************************************/

BOOL smb_io_time(char *desc, NTTIME *nttime, prs_struct *ps, int depth)
{
	if (nttime == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_time");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("low ", ps, depth, &nttime->low)) /* low part */
		return False;
	if(!prs_uint32("high", ps, depth, &nttime->high)) /* high part */
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a LOOKUP_LEVEL structure.
********************************************************************/

BOOL smb_io_lookup_level(char *desc, LOOKUP_LEVEL *level, prs_struct *ps, int depth)
{
	if (level == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_lookup_level");
	depth++;

	if(!prs_align(ps))
		return False;
	if(!prs_uint16("value", ps, depth, &level->value))
		return False;
	if(!prs_align(ps))
		return False;

	return True;
}

/*******************************************************************
 Gets an enumeration handle from an ENUM_HND structure.
********************************************************************/

uint32 get_enum_hnd(ENUM_HND *enh)
{
	return (enh && enh->ptr_hnd != 0) ? enh->handle : 0;
}

/*******************************************************************
 Inits an ENUM_HND structure.
********************************************************************/

void init_enum_hnd(ENUM_HND *enh, uint32 hnd)
{
	DEBUG(5,("smb_io_enum_hnd\n"));

	enh->ptr_hnd = (hnd != 0) ? 1 : 0;
	enh->handle = hnd;
}

/*******************************************************************
 Reads or writes an ENUM_HND structure.
********************************************************************/

BOOL smb_io_enum_hnd(char *desc, ENUM_HND *hnd, prs_struct *ps, int depth)
{
	if (hnd == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_enum_hnd");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("ptr_hnd", ps, depth, &hnd->ptr_hnd)) /* pointer */
		return False;

	if (hnd->ptr_hnd != 0) {
		if(!prs_uint32("handle ", ps, depth, &hnd->handle )) /* enum handle */
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes a DOM_SID structure.
********************************************************************/

BOOL smb_io_dom_sid(char *desc, DOM_SID *sid, prs_struct *ps, int depth)
{
	int i;

	if (sid == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_dom_sid");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint8 ("sid_rev_num", ps, depth, &sid->sid_rev_num))
		return False;
	if(!prs_uint8 ("num_auths  ", ps, depth, &sid->num_auths))
		return False;

	for (i = 0; i < 6; i++)
	{
		fstring tmp;
		slprintf(tmp, sizeof(tmp) - 1, "id_auth[%d] ", i);
		if(!prs_uint8 (tmp, ps, depth, &sid->id_auth[i]))
			return False;
	}

	/* oops! XXXX should really issue a warning here... */
	if (sid->num_auths > MAXSUBAUTHS)
		sid->num_auths = MAXSUBAUTHS;

	if(!prs_uint32s(False, "sub_auths ", ps, depth, sid->sub_auths, sid->num_auths))
		return False;

	return True;
}

/*******************************************************************
 Inits a DOM_SID structure.

 BIG NOTE: this function only does SIDS where the identauth is not >= 2^32 
 identauth >= 2^32 can be detected because it will be specified in hex
********************************************************************/

void init_dom_sid(DOM_SID *sid, char *str_sid)
{
	pstring domsid;
	int identauth;
	char *p;

	if (str_sid == NULL)
	{
		DEBUG(4,("netlogon domain SID: none\n"));
		sid->sid_rev_num = 0;
		sid->num_auths = 0;
		return;
	}
		
	pstrcpy(domsid, str_sid);

	DEBUG(4,("init_dom_sid %d SID:  %s\n", __LINE__, domsid));

	/* assume, but should check, that domsid starts "S-" */
	p = strtok(domsid+2,"-");
	sid->sid_rev_num = atoi(p);

	/* identauth in decimal should be <  2^32 */
	/* identauth in hex     should be >= 2^32 */
	identauth = atoi(strtok(0,"-"));

	DEBUG(4,("netlogon rev %d\n", sid->sid_rev_num));
	DEBUG(4,("netlogon %s ia %d\n", p, identauth));

	sid->id_auth[0] = 0;
	sid->id_auth[1] = 0;
	sid->id_auth[2] = (identauth & 0xff000000) >> 24;
	sid->id_auth[3] = (identauth & 0x00ff0000) >> 16;
	sid->id_auth[4] = (identauth & 0x0000ff00) >> 8;
	sid->id_auth[5] = (identauth & 0x000000ff);

	sid->num_auths = 0;

	while ((p = strtok(0, "-")) != NULL && sid->num_auths < MAXSUBAUTHS)
		sid->sub_auths[sid->num_auths++] = atoi(p);

	DEBUG(4,("init_dom_sid: %d SID:  %s\n", __LINE__, domsid));
}

/*******************************************************************
 Inits a DOM_SID2 structure.
********************************************************************/

void init_dom_sid2(DOM_SID2 *sid2, DOM_SID *sid)
{
	sid2->sid = *sid;
	sid2->num_auths = sid2->sid.num_auths;
}

/*******************************************************************
 Reads or writes a DOM_SID2 structure.
********************************************************************/

BOOL smb_io_dom_sid2(char *desc, DOM_SID2 *sid, prs_struct *ps, int depth)
{
	if (sid == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_dom_sid2");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("num_auths", ps, depth, &sid->num_auths))
		return False;

	if(!smb_io_dom_sid("sid", &sid->sid, ps, depth))
		return False;

	return True;
}

/*******************************************************************
creates a STRHDR structure.
********************************************************************/

void init_str_hdr(STRHDR *hdr, int max_len, int len, uint32 buffer)
{
	hdr->str_max_len = max_len;
	hdr->str_str_len = len;
	hdr->buffer      = buffer;
}

/*******************************************************************
 Reads or writes a STRHDR structure.
********************************************************************/

BOOL smb_io_strhdr(char *desc,  STRHDR *hdr, prs_struct *ps, int depth)
{
	if (hdr == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_strhdr");
	depth++;

	prs_align(ps);
	
	if(!prs_uint16("str_str_len", ps, depth, &hdr->str_str_len))
		return False;
	if(!prs_uint16("str_max_len", ps, depth, &hdr->str_max_len))
		return False;
	if(!prs_uint32("buffer     ", ps, depth, &hdr->buffer))
		return False;

	/* oops! XXXX maybe issue a warning that this is happening... */
	if (hdr->str_max_len > MAX_STRINGLEN)
		hdr->str_max_len = MAX_STRINGLEN;
	if (hdr->str_str_len > MAX_STRINGLEN)
		hdr->str_str_len = MAX_STRINGLEN;

	return True;
}

/*******************************************************************
 Inits a UNIHDR structure.
********************************************************************/

void init_uni_hdr(UNIHDR *hdr, int len)
{
	hdr->uni_str_len = 2 * len;
	hdr->uni_max_len = 2 * len;
	hdr->buffer      = len != 0 ? 1 : 0;
}

/*******************************************************************
 Reads or writes a UNIHDR structure.
********************************************************************/

BOOL smb_io_unihdr(char *desc, UNIHDR *hdr, prs_struct *ps, int depth)
{
	if (hdr == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_unihdr");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint16("uni_str_len", ps, depth, &hdr->uni_str_len))
		return False;
	if(!prs_uint16("uni_max_len", ps, depth, &hdr->uni_max_len))
		return False;
	if(!prs_uint32("buffer     ", ps, depth, &hdr->buffer))
		return False;

	/* oops! XXXX maybe issue a warning that this is happening... */
	if (hdr->uni_max_len > MAX_UNISTRLEN)
		hdr->uni_max_len = MAX_UNISTRLEN;
	if (hdr->uni_str_len > MAX_UNISTRLEN)
		hdr->uni_str_len = MAX_UNISTRLEN;

	return True;
}

/*******************************************************************
 Inits a BUFHDR structure.
********************************************************************/

void init_buf_hdr(BUFHDR *hdr, int max_len, int len)
{
	hdr->buf_max_len = max_len;
	hdr->buf_len     = len;
}

/*******************************************************************
 prs_uint16 wrapper. Call this and it sets up a pointer to where the
 uint16 should be stored, or gets the size if reading.
 ********************************************************************/

BOOL smb_io_hdrbuf_pre(char *desc, BUFHDR *hdr, prs_struct *ps, int depth, uint32 *offset)
{
	(*offset) = prs_offset(ps);
	if (ps->io) {

		/* reading. */

		if(!smb_io_hdrbuf(desc, hdr, ps, depth))
			return False;

	} else {

		/* writing. */

		if(!prs_set_offset(ps, prs_offset(ps) + (sizeof(uint32) * 2)))
			return False;
	}

	return True;
}

/*******************************************************************
 smb_io_hdrbuf wrapper. Call this and it retrospectively stores the size.
 Does nothing on reading, as that is already handled by ...._pre()
 ********************************************************************/

BOOL smb_io_hdrbuf_post(char *desc, BUFHDR *hdr, prs_struct *ps, int depth, 
				uint32 ptr_hdrbuf, uint32 max_len, uint32 len)
{
	if (!ps->io) {
		/* writing: go back and do a retrospective job.  i hate this */

		uint32 old_offset = prs_offset(ps);

		init_buf_hdr(hdr, max_len, len);
		if(!prs_set_offset(ps, ptr_hdrbuf))
			return False;
		if(!smb_io_hdrbuf(desc, hdr, ps, depth))
			return False;

		if(!prs_set_offset(ps, old_offset))
			return False;
	}

	return True;
}

/*******************************************************************
 Reads or writes a BUFHDR structure.
********************************************************************/

BOOL smb_io_hdrbuf(char *desc, BUFHDR *hdr, prs_struct *ps, int depth)
{
	if (hdr == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_hdrbuf");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("buf_max_len", ps, depth, &hdr->buf_max_len))
		return False;
	if(!prs_uint32("buf_len    ", ps, depth, &hdr->buf_len))
		return False;

	/* oops! XXXX maybe issue a warning that this is happening... */
	if (hdr->buf_max_len > MAX_BUFFERLEN)
		hdr->buf_max_len = MAX_BUFFERLEN;
	if (hdr->buf_len > MAX_BUFFERLEN)
		hdr->buf_len = MAX_BUFFERLEN;

	return True;
}

/*******************************************************************
creates a UNIHDR2 structure.
********************************************************************/

void init_uni_hdr2(UNIHDR2 *hdr, int len)
{
	init_uni_hdr(&hdr->unihdr, len);
	hdr->buffer = (len > 0) ? 1 : 0;
}

/*******************************************************************
 Reads or writes a UNIHDR2 structure.
********************************************************************/

BOOL smb_io_unihdr2(char *desc, UNIHDR2 *hdr2, prs_struct *ps, int depth)
{
	if (hdr2 == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_unihdr2");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_unihdr("hdr", &hdr2->unihdr, ps, depth))
		return False;
	if(!prs_uint32("buffer", ps, depth, &hdr2->buffer))
		return False;

	return True;
}

/*******************************************************************
 Inits a UNISTR structure.
********************************************************************/

void init_unistr(UNISTR *str, char *buf)
{
	/* store the string (null-terminated copy) */
	dos_struni2((char *)str->buffer, buf, sizeof(str->buffer));
}

/*******************************************************************
reads or writes a UNISTR structure.
XXXX NOTE: UNISTR structures NEED to be null-terminated.
********************************************************************/

BOOL smb_io_unistr(char *desc, UNISTR *uni, prs_struct *ps, int depth)
{
	if (uni == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_unistr");
	depth++;

	if(!prs_align(ps))
		return False;
	if(!prs_unistr("unistr", ps, depth, uni))
		return False;

	return True;
}

/*******************************************************************
 Inits a BUFFER3 structure from a uint32
********************************************************************/

void init_buffer3_uint32(BUFFER3 *str, uint32 val)
{
	ZERO_STRUCTP(str);

	/* set up string lengths. */
	str->buf_max_len = sizeof(uint32);
	str->buf_len     = sizeof(uint32);

	SIVAL(str->buffer, 0, val);
}

/*******************************************************************
 Inits a BUFFER3 structure.
********************************************************************/

void init_buffer3_str(BUFFER3 *str, char *buf, int len)
{
	ZERO_STRUCTP(str);

	/* set up string lengths. */
	str->buf_max_len = len * 2;
	str->buf_len     = len * 2;

	/* store the string (null-terminated 8 bit chars into 16 bit chars) */
	dos_struni2((char *)str->buffer, buf, sizeof(str->buffer));
}

/*******************************************************************
 Inits a BUFFER3 structure from a hex string.
********************************************************************/

void init_buffer3_hex(BUFFER3 *str, char *buf)
{
	ZERO_STRUCTP(str);
	str->buf_max_len = str->buf_len = strhex_to_str((char *)str->buffer, sizeof(str->buffer), buf);
}

/*******************************************************************
 Inits a BUFFER3 structure.
********************************************************************/

void init_buffer3_bytes(BUFFER3 *str, uint8 *buf, int len)
{
	ZERO_STRUCTP(str);

	/* max buffer size (allocated size) */
	str->buf_max_len = len;
	if (buf != NULL)
		memcpy(str->buffer, buf, MIN(str->buf_len, sizeof(str->buffer)));
	str->buf_len = buf != NULL ? len : 0;
}

/*******************************************************************
 Reads or writes a BUFFER3 structure.
   the uni_max_len member tells you how large the buffer is.
   the uni_str_len member tells you how much of the buffer is really used.
********************************************************************/

BOOL smb_io_buffer3(char *desc, BUFFER3 *buf3, prs_struct *ps, int depth)
{
	if (buf3 == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_buffer3");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("uni_max_len", ps, depth, &buf3->buf_max_len))
		return False;

	if (buf3->buf_max_len > MAX_UNISTRLEN)
		buf3->buf_max_len = MAX_UNISTRLEN;

	if(!prs_uint8s(True, "buffer     ", ps, depth, buf3->buffer, buf3->buf_max_len))
		return False;

	if(!prs_uint32("buf_len    ", ps, depth, &buf3->buf_len))
		return False;
	if (buf3->buf_len > MAX_UNISTRLEN)
		buf3->buf_len = MAX_UNISTRLEN;

	return True;
}

/*******************************************************************
 Inits a BUFFER2 structure.
********************************************************************/

void init_buffer2(BUFFER2 *str, uint8 *buf, int len)
{
	ZERO_STRUCTP(str);

	/* max buffer size (allocated size) */
	str->buf_max_len = len;
	str->undoc       = 0;
	str->buf_len = buf != NULL ? len : 0;

	if (buf != NULL)
		memcpy(str->buffer, buf, MIN(str->buf_len, sizeof(str->buffer)));
}

/*******************************************************************
 Reads or writes a BUFFER2 structure.
   the uni_max_len member tells you how large the buffer is.
   the uni_str_len member tells you how much of the buffer is really used.
********************************************************************/

BOOL smb_io_buffer2(char *desc, BUFFER2 *buf2, uint32 buffer, prs_struct *ps, int depth)
{
	if (buf2 == NULL)
		return False;

	if (buffer) {

		prs_debug(ps, depth, desc, "smb_io_buffer2");
		depth++;

		if(!prs_align(ps))
			return False;
		
		if(!prs_uint32("uni_max_len", ps, depth, &buf2->buf_max_len))
			return False;
		if(!prs_uint32("undoc      ", ps, depth, &buf2->undoc))
			return False;
		if(!prs_uint32("buf_len    ", ps, depth, &buf2->buf_len))
			return False;

		/* oops! XXXX maybe issue a warning that this is happening... */
		if (buf2->buf_max_len > MAX_UNISTRLEN)
			buf2->buf_max_len = MAX_UNISTRLEN;
		if (buf2->buf_len > MAX_UNISTRLEN)
			buf2->buf_len = MAX_UNISTRLEN;

		/* buffer advanced by indicated length of string
		   NOT by searching for null-termination */

		if(!prs_buffer2(True, "buffer     ", ps, depth, buf2))
			return False;

	} else {

		prs_debug(ps, depth, desc, "smb_io_buffer2 - NULL");
		depth++;
		memset((char *)buf2, '\0', sizeof(*buf2));

	}
	return True;
}

/*******************************************************************
creates a UNISTR2 structure: sets up the buffer, too
********************************************************************/

void init_buf_unistr2(UNISTR2 *str, uint32 *ptr, char *buf)
{
	if (buf != NULL) {

		*ptr = 1;
		init_unistr2(str, buf, strlen(buf)+1);

	} else {

		*ptr = 0;
		init_unistr2(str, "", 0);

	}
}

/*******************************************************************
 Copies a UNISTR2 structure.
********************************************************************/

void copy_unistr2(UNISTR2 *str, UNISTR2 *from)
{
	/* set up string lengths. add one if string is not null-terminated */
	str->uni_max_len = from->uni_max_len;
	str->undoc       = from->undoc;
	str->uni_str_len = from->uni_str_len;

	/* copy the string */
	memcpy(str->buffer, from->buffer, sizeof(from->buffer));
}

/*******************************************************************
 Creates a STRING2 structure.
********************************************************************/

void init_string2(STRING2 *str, char *buf, int len)
{
  /* set up string lengths. */
  str->str_max_len = len;
  str->undoc       = 0;
  str->str_str_len = len;

  /* store the string */
  if(len != 0)
    memcpy(str->buffer, buf, len);
}

/*******************************************************************
 Reads or writes a STRING2 structure.
 XXXX NOTE: STRING2 structures need NOT be null-terminated.
   the str_str_len member tells you how long the string is;
   the str_max_len member tells you how large the buffer is.
********************************************************************/

BOOL smb_io_string2(char *desc, STRING2 *str2, uint32 buffer, prs_struct *ps, int depth)
{
	if (str2 == NULL)
		return False;

	if (buffer) {

		prs_debug(ps, depth, desc, "smb_io_string2");
		depth++;

		if(!prs_align(ps))
			return False;
		
		if(!prs_uint32("str_max_len", ps, depth, &str2->str_max_len))
			return False;
		if(!prs_uint32("undoc      ", ps, depth, &str2->undoc))
			return False;
		if(!prs_uint32("str_str_len", ps, depth, &str2->str_str_len))
			return False;

		/* oops! XXXX maybe issue a warning that this is happening... */
		if (str2->str_max_len > MAX_STRINGLEN)
			str2->str_max_len = MAX_STRINGLEN;
		if (str2->str_str_len > MAX_STRINGLEN)
			str2->str_str_len = MAX_STRINGLEN;

		/* buffer advanced by indicated length of string
		   NOT by searching for null-termination */
		if(!prs_string2(True, "buffer     ", ps, depth, str2))
			return False;

	} else {

		prs_debug(ps, depth, desc, "smb_io_string2 - NULL");
		depth++;
		memset((char *)str2, '\0', sizeof(*str2));

	}

	return True;
}

/*******************************************************************
 Inits a UNISTR2 structure.
********************************************************************/

void init_unistr2(UNISTR2 *str, char *buf, int len)
{
	ZERO_STRUCTP(str);

	/* set up string lengths. */
	str->uni_max_len = len;
	str->undoc       = 0;
	str->uni_str_len = len;

	/* store the string (null-terminated 8 bit chars into 16 bit chars) */
	dos_struni2((char *)str->buffer, buf, sizeof(str->buffer));
}

/*******************************************************************
 Reads or writes a UNISTR2 structure.
 XXXX NOTE: UNISTR2 structures need NOT be null-terminated.
   the uni_str_len member tells you how long the string is;
   the uni_max_len member tells you how large the buffer is.
********************************************************************/

BOOL smb_io_unistr2(char *desc, UNISTR2 *uni2, uint32 buffer, prs_struct *ps, int depth)
{
	if (uni2 == NULL)
		return False;

	if (buffer) {

		prs_debug(ps, depth, desc, "smb_io_unistr2");
		depth++;

		if(!prs_align(ps))
			return False;
		
		if(!prs_uint32("uni_max_len", ps, depth, &uni2->uni_max_len))
			return False;
		if(!prs_uint32("undoc      ", ps, depth, &uni2->undoc))
			return False;
		if(!prs_uint32("uni_str_len", ps, depth, &uni2->uni_str_len))
			return False;

		/* oops! XXXX maybe issue a warning that this is happening... */
		if (uni2->uni_max_len > MAX_UNISTRLEN)
			uni2->uni_max_len = MAX_UNISTRLEN;
		if (uni2->uni_str_len > MAX_UNISTRLEN)
			uni2->uni_str_len = MAX_UNISTRLEN;

		/* buffer advanced by indicated length of string
		   NOT by searching for null-termination */
		if(!prs_unistr2(True, "buffer     ", ps, depth, uni2))
			return False;

	} else {

		prs_debug(ps, depth, desc, "smb_io_unistr2 - NULL");
		depth++;
		memset((char *)uni2, '\0', sizeof(*uni2));

	}

	return True;
}

/*******************************************************************
 Inits a DOM_RID2 structure.
********************************************************************/

void init_dom_rid2(DOM_RID2 *rid2, uint32 rid, uint8 type, uint32 idx)
{
	rid2->type    = type;
	rid2->rid     = rid;
	rid2->rid_idx = idx;
}

/*******************************************************************
 Reads or writes a DOM_RID2 structure.
********************************************************************/

BOOL smb_io_dom_rid2(char *desc, DOM_RID2 *rid2, prs_struct *ps, int depth)
{
	if (rid2 == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_dom_rid2");
	depth++;

	if(!prs_align(ps))
		return False;
   
	if(!prs_uint8("type   ", ps, depth, &rid2->type))
		return False;
	if(!prs_align(ps))
		return False;
	if(!prs_uint32("rid    ", ps, depth, &rid2->rid))
		return False;
	if(!prs_uint32("rid_idx", ps, depth, &rid2->rid_idx))
		return False;

	return True;
}

/*******************************************************************
creates a DOM_RID3 structure.
********************************************************************/

void init_dom_rid3(DOM_RID3 *rid3, uint32 rid, uint8 type)
{
    rid3->rid      = rid;
    rid3->type1    = type;
    rid3->ptr_type = 0x1; /* non-zero, basically. */
    rid3->type2    = 0x1;
    rid3->unk      = type;
}

/*******************************************************************
reads or writes a DOM_RID3 structure.
********************************************************************/

BOOL smb_io_dom_rid3(char *desc, DOM_RID3 *rid3, prs_struct *ps, int depth)
{
	if (rid3 == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_dom_rid3");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("rid     ", ps, depth, &rid3->rid))
		return False;
	if(!prs_uint32("type1   ", ps, depth, &rid3->type1))
		return False;
	if(!prs_uint32("ptr_type", ps, depth, &rid3->ptr_type))
		return False;
	if(!prs_uint32("type2   ", ps, depth, &rid3->type2))
		return False;
	if(!prs_uint32("unk     ", ps, depth, &rid3->unk))
		return False;

	return True;
}

/*******************************************************************
 Inits a DOM_RID4 structure.
********************************************************************/

void init_dom_rid4(DOM_RID4 *rid4, uint16 unknown, uint16 attr, uint32 rid)
{
    rid4->unknown = unknown;
    rid4->attr    = attr;
    rid4->rid     = rid;
}

/*******************************************************************
 Inits a DOM_CLNT_SRV structure.
********************************************************************/

static void init_clnt_srv(DOM_CLNT_SRV *log, char *logon_srv, char *comp_name)
{
	DEBUG(5,("init_clnt_srv: %d\n", __LINE__));

	if (logon_srv != NULL) {
		log->undoc_buffer = 1;
		init_unistr2(&(log->uni_logon_srv), logon_srv, strlen(logon_srv)+1);
	} else {
		log->undoc_buffer = 0;
	}

	if (comp_name != NULL) {
		log->undoc_buffer2 = 1;
		init_unistr2(&(log->uni_comp_name), comp_name, strlen(comp_name)+1);
	} else {
		log->undoc_buffer2 = 0;
	}
}

/*******************************************************************
 Inits or writes a DOM_CLNT_SRV structure.
********************************************************************/

static BOOL smb_io_clnt_srv(char *desc, DOM_CLNT_SRV *log, prs_struct *ps, int depth)
{
	if (log == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_clnt_srv");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("undoc_buffer ", ps, depth, &log->undoc_buffer))
		return False;

	if (log->undoc_buffer != 0) {
		if(!smb_io_unistr2("unistr2", &log->uni_logon_srv, log->undoc_buffer, ps, depth))
			return False;
	}

	if(!prs_align(ps))
		return False;

	if(!prs_uint32("undoc_buffer2", ps, depth, &log->undoc_buffer2))
		return False;

	if (log->undoc_buffer2 != 0) {
		if(!smb_io_unistr2("unistr2", &log->uni_comp_name, log->undoc_buffer2, ps, depth))
			return False;
	}

	return True;
}

/*******************************************************************
 Inits a DOM_LOG_INFO structure.
********************************************************************/

void init_log_info(DOM_LOG_INFO *log, char *logon_srv, char *acct_name,
		uint16 sec_chan, char *comp_name)
{
	DEBUG(5,("make_log_info %d\n", __LINE__));

	log->undoc_buffer = 1;

	init_unistr2(&log->uni_logon_srv, logon_srv, strlen(logon_srv)+1);
	init_unistr2(&log->uni_acct_name, acct_name, strlen(acct_name)+1);

	log->sec_chan = sec_chan;

	init_unistr2(&log->uni_comp_name, comp_name, strlen(comp_name)+1);
}

/*******************************************************************
 Reads or writes a DOM_LOG_INFO structure.
********************************************************************/

BOOL smb_io_log_info(char *desc, DOM_LOG_INFO *log, prs_struct *ps, int depth)
{
	if (log == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_log_info");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("undoc_buffer", ps, depth, &log->undoc_buffer))
		return False;

	if(!smb_io_unistr2("unistr2", &log->uni_logon_srv, True, ps, depth))
		return False;
	if(!smb_io_unistr2("unistr2", &log->uni_acct_name, True, ps, depth))
		return False;

	if(!prs_uint16("sec_chan", ps, depth, &log->sec_chan))
		return False;

	if(!smb_io_unistr2("unistr2", &log->uni_comp_name, True, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a DOM_CHAL structure.
********************************************************************/

BOOL smb_io_chal(char *desc, DOM_CHAL *chal, prs_struct *ps, int depth)
{
	if (chal == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_chal");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint8s (False, "data", ps, depth, chal->data, 8))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a DOM_CRED structure.
********************************************************************/

BOOL smb_io_cred(char *desc,  DOM_CRED *cred, prs_struct *ps, int depth)
{
	if (cred == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_cred");
	depth++;

	if(!prs_align(ps))
		return False;

	if(!smb_io_chal ("", &cred->challenge, ps, depth))
		return False;
	if(!smb_io_utime("", &cred->timestamp, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a DOM_CLNT_INFO2 structure.
********************************************************************/

void init_clnt_info2(DOM_CLNT_INFO2 *clnt,
				char *logon_srv, char *comp_name,
				DOM_CRED *clnt_cred)
{
	DEBUG(5,("make_clnt_info: %d\n", __LINE__));

	init_clnt_srv(&(clnt->login), logon_srv, comp_name);

	if (clnt_cred != NULL) {
		clnt->ptr_cred = 1;
		memcpy(&(clnt->cred), clnt_cred, sizeof(clnt->cred));
	} else {
		clnt->ptr_cred = 0;
	}
}

/*******************************************************************
 Reads or writes a DOM_CLNT_INFO2 structure.
********************************************************************/

BOOL smb_io_clnt_info2(char *desc, DOM_CLNT_INFO2 *clnt, prs_struct *ps, int depth)
{
	if (clnt == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_clnt_info2");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!smb_io_clnt_srv("", &clnt->login, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("ptr_cred", ps, depth, &clnt->ptr_cred))
		return False;
	if(!smb_io_cred("", &clnt->cred, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a DOM_CLNT_INFO structure.
********************************************************************/

void init_clnt_info(DOM_CLNT_INFO *clnt,
		char *logon_srv, char *acct_name,
		uint16 sec_chan, char *comp_name,
				DOM_CRED *cred)
{
	DEBUG(5,("make_clnt_info\n"));

	init_log_info(&clnt->login, logon_srv, acct_name, sec_chan, comp_name);
	memcpy(&clnt->cred, cred, sizeof(clnt->cred));
}

/*******************************************************************
 Reads or writes a DOM_CLNT_INFO structure.
********************************************************************/

BOOL smb_io_clnt_info(char *desc,  DOM_CLNT_INFO *clnt, prs_struct *ps, int depth)
{
	if (clnt == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_clnt_info");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!smb_io_log_info("", &clnt->login, ps, depth))
		return False;
	if(!smb_io_cred("", &clnt->cred, ps, depth))
		return False;

	return True;
}

/*******************************************************************
 Inits a DOM_LOGON_ID structure.
********************************************************************/

void init_logon_id(DOM_LOGON_ID *log, uint32 log_id_low, uint32 log_id_high)
{
	DEBUG(5,("make_logon_id: %d\n", __LINE__));

	log->low  = log_id_low;
	log->high = log_id_high;
}

/*******************************************************************
 Reads or writes a DOM_LOGON_ID structure.
********************************************************************/

BOOL smb_io_logon_id(char *desc, DOM_LOGON_ID *log, prs_struct *ps, int depth)
{
	if (log == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_logon_id");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("low ", ps, depth, &log->low ))
		return False;
	if(!prs_uint32("high", ps, depth, &log->high))
		return False;

	return True;
}

/*******************************************************************
 Inits an OWF_INFO structure.
********************************************************************/

void init_owf_info(OWF_INFO *hash, uint8 data[16])
{
	DEBUG(5,("init_owf_info: %d\n", __LINE__));
	
	if (data != NULL)
		memcpy(hash->data, data, sizeof(hash->data));
	else
		memset((char *)hash->data, '\0', sizeof(hash->data));
}

/*******************************************************************
 Reads or writes an OWF_INFO structure.
********************************************************************/

BOOL smb_io_owf_info(char *desc, OWF_INFO *hash, prs_struct *ps, int depth)
{
	if (hash == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_owf_info");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint8s (False, "data", ps, depth, hash->data, 16))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a DOM_GID structure.
********************************************************************/

BOOL smb_io_gid(char *desc,  DOM_GID *gid, prs_struct *ps, int depth)
{
	if (gid == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_gid");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("g_rid", ps, depth, &gid->g_rid))
		return False;
	if(!prs_uint32("attr ", ps, depth, &gid->attr))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes an POLICY_HND structure.
********************************************************************/

BOOL smb_io_pol_hnd(char *desc, POLICY_HND *pol, prs_struct *ps, int depth)
{
	if (pol == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_pol_hnd");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint8s (False, "data", ps, depth, pol->data, POL_HND_SIZE))
		return False;

	return True;
}

/*******************************************************************
 Reads or writes a dom query structure.
********************************************************************/

static BOOL smb_io_dom_query(char *desc, DOM_QUERY *d_q, prs_struct *ps, int depth)
{
	if (d_q == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_dom_query");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint16("uni_dom_max_len", ps, depth, &d_q->uni_dom_max_len)) /* domain name string length * 2 */
		return False;
	if(!prs_uint16("uni_dom_str_len", ps, depth, &d_q->uni_dom_str_len)) /* domain name string length * 2 */
		return False;

	if(!prs_uint32("buffer_dom_name", ps, depth, &d_q->buffer_dom_name)) /* undocumented domain name string buffer pointer */
		return False;
	if(!prs_uint32("buffer_dom_sid ", ps, depth, &d_q->buffer_dom_sid)) /* undocumented domain SID string buffer pointer */
		return False;

	if(!smb_io_unistr2("unistr2", &d_q->uni_domain_name, d_q->buffer_dom_name, ps, depth)) /* domain name (unicode string) */
		return False;

	if(!prs_align(ps))
		return False;
	
	if (d_q->buffer_dom_sid != 0) {
		if(!smb_io_dom_sid2("", &d_q->dom_sid, ps, depth)) /* domain SID */
			return False;
	} else {
		memset((char *)&d_q->dom_sid, '\0', sizeof(d_q->dom_sid));
	}

	return True;
}

/*******************************************************************
 Reads or writes a dom query structure.
********************************************************************/

BOOL smb_io_dom_query_3(char *desc, DOM_QUERY_3 *d_q, prs_struct *ps, int depth)
{
	return smb_io_dom_query("", d_q, ps, depth);
}

/*******************************************************************
 Reads or writes a dom query structure.
********************************************************************/

BOOL smb_io_dom_query_5(char *desc, DOM_QUERY_3 *d_q, prs_struct *ps, int depth)
{
	return smb_io_dom_query("", d_q, ps, depth);
}


/*******************************************************************
 Reads or writes a UNISTR3 structure.
********************************************************************/

BOOL smb_io_unistr3(char *desc, UNISTR3 *name, prs_struct *ps, int depth)
{
	if (name == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_unistr3");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("uni_str_len", ps, depth, &name->uni_str_len))
		return False;

	/* don't know if len is specified by uni_str_len member... */
	/* assume unicode string is unicode-null-terminated, instead */

	if(!prs_unistr3(True, "unistr", name, ps, depth))
		return False;

	return True;
}
