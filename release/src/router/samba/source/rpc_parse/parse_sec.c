/* 
 *  Unix SMB/Netbios implementation.
 *  Version 1.9.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Copyright (C) Jeremy R. Allison            1995-1998
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1998,
 *  Copyright (C) Paul Ashton                  1997-1998.
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

#define SD_HEADER_SIZE 0x14

/*******************************************************************
 Sets up a SEC_ACCESS structure.
********************************************************************/

void init_sec_access(SEC_ACCESS *t, uint32 mask)
{
	t->mask = mask;
}

/*******************************************************************
 Reads or writes a SEC_ACCESS structure.
********************************************************************/

BOOL sec_io_access(char *desc, SEC_ACCESS *t, prs_struct *ps, int depth)
{
	if (t == NULL)
		return False;

	prs_debug(ps, depth, desc, "sec_io_access");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32("mask", ps, depth, &(t->mask)))
		return False;

	return True;
}


/*******************************************************************
 Sets up a SEC_ACE structure.
********************************************************************/

void init_sec_ace(SEC_ACE *t, DOM_SID *sid, uint8 type, SEC_ACCESS mask, uint8 flag)
{
	t->type = type;
	t->flags = flag;
	t->size = sid_size(sid) + 8;
	t->info = mask;

	ZERO_STRUCTP(&t->sid);
	sid_copy(&t->sid, sid);
}

/*******************************************************************
 Reads or writes a SEC_ACE structure.
********************************************************************/

BOOL sec_io_ace(char *desc, SEC_ACE *psa, prs_struct *ps, int depth)
{
	uint32 old_offset;
	uint32 offset_ace_size;

	if (psa == NULL)
		return False;

	prs_debug(ps, depth, desc, "sec_io_ace");
	depth++;

	if(!prs_align(ps))
		return False;
	
	old_offset = prs_offset(ps);

	if(!prs_uint8("type ", ps, depth, &psa->type))
		return False;

	if(!prs_uint8("flags", ps, depth, &psa->flags))
		return False;

	if(!prs_uint16_pre("size ", ps, depth, &psa->size, &offset_ace_size))
		return False;

	if(!sec_io_access("info ", &psa->info, ps, depth))
		return False;

	if(!prs_align(ps))
		return False;

	if(!smb_io_dom_sid("sid  ", &psa->sid , ps, depth))
		return False;

	if(!prs_uint16_post("size ", ps, depth, &psa->size, offset_ace_size, old_offset))
		return False;

	return True;
}

/*******************************************************************
 Create a SEC_ACL structure.  
********************************************************************/

SEC_ACL *make_sec_acl(uint16 revision, int num_aces, SEC_ACE *ace_list)
{
	SEC_ACL *dst;
	int i;

	if((dst = (SEC_ACL *)malloc(sizeof(SEC_ACL))) == NULL)
		return NULL;

	ZERO_STRUCTP(dst);

	dst->revision = revision;
	dst->num_aces = num_aces;
	dst->size = 8;

	if((dst->ace_list = (SEC_ACE *)malloc( sizeof(SEC_ACE) * num_aces )) == NULL) {
		free_sec_acl(&dst);
		return NULL;
	}

	for (i = 0; i < num_aces; i++) {
		dst->ace_list[i] = ace_list[i]; /* Structure copy. */
		dst->size += ace_list[i].size;
	}

	return dst;
}

/*******************************************************************
 Duplicate a SEC_ACL structure.  
********************************************************************/

SEC_ACL *dup_sec_acl( SEC_ACL *src)
{
	if(src == NULL)
		return NULL;

	return make_sec_acl( src->revision, src->num_aces, src->ace_list);
}

/*******************************************************************
 Delete a SEC_ACL structure.  
********************************************************************/

void free_sec_acl(SEC_ACL **ppsa)
{
	SEC_ACL *psa;

	if(ppsa == NULL || *ppsa == NULL)
		return;

	psa = *ppsa;
	if (psa->ace_list != NULL)
		free(psa->ace_list);

	free(psa);
	*ppsa = NULL;
}

/*******************************************************************
 Reads or writes a SEC_ACL structure.  

 First of the xx_io_xx functions that allocates its data structures
 for you as it reads them.
********************************************************************/

BOOL sec_io_acl(char *desc, SEC_ACL **ppsa, prs_struct *ps, int depth)
{
	int i;
	uint32 old_offset;
	uint32 offset_acl_size;
	SEC_ACL *psa;

	if (ppsa == NULL)
		return False;

	psa = *ppsa;

	if(UNMARSHALLING(ps) && psa == NULL) {
		/*
		 * This is a read and we must allocate the stuct to read into.
		 */
		if((psa = (SEC_ACL *)malloc(sizeof(SEC_ACL))) == NULL)
			return False;
		ZERO_STRUCTP(psa);
		*ppsa = psa;
	}

	prs_debug(ps, depth, desc, "sec_io_acl");
	depth++;

	if(!prs_align(ps))
		return False;
	
	old_offset = prs_offset(ps);

	if(!prs_uint16("revision", ps, depth, &psa->revision))
		return False;

	if(!prs_uint16_pre("size     ", ps, depth, &psa->size, &offset_acl_size))
		return False;

	if(!prs_uint32("num_aces ", ps, depth, &psa->num_aces))
		return False;

	if (UNMARSHALLING(ps) && psa->num_aces != 0) {
		/* reading */
		if((psa->ace_list = malloc(sizeof(psa->ace_list[0]) * psa->num_aces)) == NULL)
			return False;
		ZERO_STRUCTP(psa->ace_list);
	}

	for (i = 0; i < psa->num_aces; i++) {
		fstring tmp;
		slprintf(tmp, sizeof(tmp)-1, "ace_list[%02d]: ", i);
		if(!sec_io_ace(tmp, &psa->ace_list[i], ps, depth))
			return False;
	}

	if(!prs_align(ps))
		return False;

	if(!prs_uint16_post("size     ", ps, depth, &psa->size, offset_acl_size, old_offset))
		return False;

	return True;
}

/*******************************************************************
 Creates a SEC_DESC structure
********************************************************************/

SEC_DESC *make_sec_desc(uint16 revision, uint16 type,
			DOM_SID *owner_sid, DOM_SID *grp_sid,
			SEC_ACL *sacl, SEC_ACL *dacl, size_t *sec_desc_size)
{
	SEC_DESC *dst;
	uint32 offset;

	*sec_desc_size = 0;

	if(( dst = (SEC_DESC *)malloc(sizeof(SEC_DESC))) == NULL)
		return NULL;

	ZERO_STRUCTP(dst);

	dst->revision = revision;
	dst->type     = type;

	dst->off_owner_sid = 0;
	dst->off_grp_sid   = 0;
	dst->off_sacl      = 0;
	dst->off_dacl      = 0;

	if(owner_sid && ((dst->owner_sid = sid_dup(owner_sid)) == NULL))
		goto error_exit;

	if(grp_sid && ((dst->grp_sid = sid_dup(grp_sid)) == NULL))
		goto error_exit;

	if(sacl && ((dst->sacl = dup_sec_acl(sacl)) == NULL))
		goto error_exit;

	if(dacl && ((dst->dacl = dup_sec_acl(dacl)) == NULL))
		goto error_exit;
		
	offset = 0x0;

	/*
	 * Work out the linearization sizes.
	 */

	if (dst->owner_sid != NULL) {

		if (offset == 0)
			offset = SD_HEADER_SIZE;

		dst->off_owner_sid = offset;
		offset += ((sid_size(dst->owner_sid) + 3) & ~3);
	}

	if (dst->grp_sid != NULL) {

		if (offset == 0)
			offset = SD_HEADER_SIZE;

		dst->off_grp_sid = offset;
		offset += ((sid_size(dst->grp_sid) + 3) & ~3);
	}

	if (dst->sacl != NULL) {

		if (offset == 0)
			offset = SD_HEADER_SIZE;

		dst->off_sacl = offset;
		offset += ((sacl->size + 3) & ~3);
	}

	if (dst->dacl != NULL) {

		if (offset == 0)
			offset = SD_HEADER_SIZE;

		dst->off_dacl = offset;
		offset += ((dacl->size + 3) & ~3);
	}

	*sec_desc_size = (size_t)((offset == 0) ? SD_HEADER_SIZE : offset);
	return dst;

error_exit:

	*sec_desc_size = 0;
	free_sec_desc(&dst);
	return NULL;
}

/*******************************************************************
 Duplicate a SEC_DESC structure.  
********************************************************************/

SEC_DESC *dup_sec_desc( SEC_DESC *src)
{
	size_t dummy;

	if(src == NULL)
		return NULL;

	return make_sec_desc( src->revision, src->type, 
				src->owner_sid, src->grp_sid, src->sacl,
				src->dacl, &dummy);
}

/*******************************************************************
 Deletes a SEC_DESC structure
********************************************************************/

void free_sec_desc(SEC_DESC **ppsd)
{
	SEC_DESC *psd;

	if(ppsd == NULL || *ppsd == NULL)
		return;

	psd = *ppsd;

	free_sec_acl(&psd->dacl);
	free_sec_acl(&psd->dacl);
	free(psd->owner_sid);
	free(psd->grp_sid);
	free(psd);
	*ppsd = NULL;
}

/*******************************************************************
 Creates a SEC_DESC structure with typical defaults.
********************************************************************/

SEC_DESC *make_standard_sec_desc(DOM_SID *owner_sid, DOM_SID *grp_sid,
				 SEC_ACL *dacl, size_t *sec_desc_size)
{
	return make_sec_desc(1, SEC_DESC_SELF_RELATIVE|SEC_DESC_DACL_PRESENT,
			     owner_sid, grp_sid, NULL, dacl, sec_desc_size);
}


/*******************************************************************
 Reads or writes a SEC_DESC structure.
 If reading and the *ppsd = NULL, allocates the structure.
********************************************************************/

BOOL sec_io_desc(char *desc, SEC_DESC **ppsd, prs_struct *ps, int depth)
{
	uint32 old_offset;
	uint32 max_offset = 0; /* after we're done, move offset to end */
	SEC_DESC *psd;

	if (ppsd == NULL)
		return False;

	psd = *ppsd;

	if(UNMARSHALLING(ps) && psd == NULL) {
		if((psd = (SEC_DESC *)malloc(sizeof(SEC_DESC))) == NULL)
			return False;
		ZERO_STRUCTP(psd);
		*ppsd = psd;
	}

	prs_debug(ps, depth, desc, "sec_io_desc");
	depth++;

	if(!prs_align(ps))
		return False;
	
	/* start of security descriptor stored for back-calc offset purposes */
	old_offset = prs_offset(ps);

	if(!prs_uint16("revision ", ps, depth, &psd->revision))
		return False;

	if(!prs_uint16("type     ", ps, depth, &psd->type))
		return False;

	if(!prs_uint32("off_owner_sid", ps, depth, &psd->off_owner_sid))
		return False;

	if(!prs_uint32("off_grp_sid  ", ps, depth, &psd->off_grp_sid))
		return False;

	if(!prs_uint32("off_sacl     ", ps, depth, &psd->off_sacl))
		return False;

	if(!prs_uint32("off_dacl     ", ps, depth, &psd->off_dacl))
		return False;

	max_offset = MAX(max_offset, prs_offset(ps));

	if (psd->off_owner_sid != 0) {

		if (UNMARSHALLING(ps)) {
			if(!prs_set_offset(ps, old_offset + psd->off_owner_sid))
				return False;
			/* reading */
			if((psd->owner_sid = malloc(sizeof(*psd->owner_sid))) == NULL)
				return False;
			ZERO_STRUCTP(psd->owner_sid);
		}

		if(!smb_io_dom_sid("owner_sid ", psd->owner_sid , ps, depth))
			return False;
		if(!prs_align(ps))
			return False;
	}

	max_offset = MAX(max_offset, prs_offset(ps));

	if (psd->off_grp_sid != 0) {

		if (UNMARSHALLING(ps)) {
			/* reading */
			if(!prs_set_offset(ps, old_offset + psd->off_grp_sid))
				return False;
			if((psd->grp_sid = malloc(sizeof(*psd->grp_sid))) == NULL)
				return False;
			ZERO_STRUCTP(psd->grp_sid);
		}

		if(!smb_io_dom_sid("grp_sid", psd->grp_sid, ps, depth))
			return False;
		if(!prs_align(ps))
			return False;
	}

	max_offset = MAX(max_offset, prs_offset(ps));

	if (IS_BITS_SET_ALL(psd->type, SEC_DESC_SACL_PRESENT) && psd->off_sacl) {
		if(!prs_set_offset(ps, old_offset + psd->off_sacl))
			return False;
		if(!sec_io_acl("sacl", &psd->sacl, ps, depth))
			return False;
		if(!prs_align(ps))
			return False;
	}

	max_offset = MAX(max_offset, prs_offset(ps));

	if (IS_BITS_SET_ALL(psd->type, SEC_DESC_DACL_PRESENT) && psd->off_dacl != 0) {
		if(!prs_set_offset(ps, old_offset + psd->off_dacl))
			return False;
		if(!sec_io_acl("dacl", &psd->dacl, ps, depth))
			return False;
		if(!prs_align(ps))
			return False;
	}

	max_offset = MAX(max_offset, prs_offset(ps));

	if(!prs_set_offset(ps, max_offset))
		return False;
	return True;
}

/*******************************************************************
 Creates a SEC_DESC_BUF structure.
********************************************************************/

SEC_DESC_BUF *make_sec_desc_buf(int len, SEC_DESC *sec_desc)
{
	SEC_DESC_BUF *dst;

	if((dst = (SEC_DESC_BUF *)malloc(sizeof(SEC_DESC_BUF))) == NULL)
		return NULL;

	ZERO_STRUCTP(dst);

	/* max buffer size (allocated size) */
	dst->max_len = len;
	dst->len = len;

	if(sec_desc && ((dst->sec = dup_sec_desc(sec_desc)) == NULL)) {
		free_sec_desc_buf(&dst);
		return NULL;
	}

	return dst;
}

/*******************************************************************
 Duplicates a SEC_DESC_BUF structure.
********************************************************************/

SEC_DESC_BUF *dup_sec_desc_buf(SEC_DESC_BUF *src)
{
	if(src == NULL)
		return NULL;

	return make_sec_desc_buf( src->len, src->sec);
}

/*******************************************************************
 Deletes a SEC_DESC_BUF structure.
********************************************************************/

void free_sec_desc_buf(SEC_DESC_BUF **ppsdb)
{
	SEC_DESC_BUF *psdb;

	if(ppsdb == NULL || *ppsdb == NULL)
		return;

	psdb = *ppsdb;
	free_sec_desc(&psdb->sec);
	free(psdb);
	*ppsdb = NULL;
}


/*******************************************************************
 Reads or writes a SEC_DESC_BUF structure.
********************************************************************/

BOOL sec_io_desc_buf(char *desc, SEC_DESC_BUF **ppsdb, prs_struct *ps, int depth)
{
	uint32 off_len;
	uint32 off_max_len;
	uint32 old_offset;
	uint32 size;
	SEC_DESC_BUF *psdb;

	if (ppsdb == NULL)
		return False;

	psdb = *ppsdb;

	if (UNMARSHALLING(ps) && psdb == NULL) {
		if((psdb = (SEC_DESC_BUF *)malloc(sizeof(SEC_DESC_BUF))) == NULL)
			return False;
		ZERO_STRUCTP(psdb);
		*ppsdb = psdb;
	}

	prs_debug(ps, depth, desc, "sec_io_desc_buf");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if(!prs_uint32_pre("max_len", ps, depth, &psdb->max_len, &off_max_len))
		return False;

	if(!prs_uint32    ("undoc  ", ps, depth, &psdb->undoc))
		return False;

	if(!prs_uint32_pre("len    ", ps, depth, &psdb->len, &off_len))
		return False;

	old_offset = prs_offset(ps);

	/* reading, length is non-zero; writing, descriptor is non-NULL */
	if ((psdb->len != 0 || MARSHALLING(ps)) && psdb->sec != NULL) {
		if(!sec_io_desc("sec   ", &psdb->sec, ps, depth))
			return False;
	}

	size = prs_offset(ps) - old_offset;
	if(!prs_uint32_post("max_len", ps, depth, &psdb->max_len, off_max_len, size == 0 ? psdb->max_len : size))
		return False;

	if(!prs_uint32_post("len    ", ps, depth, &psdb->len, off_len, size))
		return False;

	return True;
}
