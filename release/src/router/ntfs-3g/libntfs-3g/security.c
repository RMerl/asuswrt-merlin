/**
 * security.c - Handling security/ACLs in NTFS.  Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2004 Anton Altaparmakov
 * Copyright (c) 2005-2006 Szabolcs Szakacsits
 * Copyright (c) 2006 Yura Pakhuchiy
 * Copyright (c) 2007-2009 Jean-Pierre Andre
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "param.h"
#include "types.h"
#include "layout.h"
#include "attrib.h"
#include "index.h"
#include "dir.h"
#include "bitmap.h"
#include "security.h"
#include "acls.h"
#include "cache.h"
#include "misc.h"

/*
 *	JPA NTFS constants or structs
 *	should be moved to layout.h
 */

#define ALIGN_SDS_BLOCK 0x40000 /* Alignment for a $SDS block */
#define ALIGN_SDS_ENTRY 16 /* Alignment for a $SDS entry */
#define STUFFSZ 0x4000 /* unitary stuffing size for $SDS */
#define FIRST_SECURITY_ID 0x100 /* Lowest security id */

	/* Mask for attributes which can be forced */
#define FILE_ATTR_SETTABLE ( FILE_ATTR_READONLY		\
				| FILE_ATTR_HIDDEN	\
				| FILE_ATTR_SYSTEM	\
				| FILE_ATTR_ARCHIVE	\
				| FILE_ATTR_TEMPORARY	\
				| FILE_ATTR_OFFLINE	\
				| FILE_ATTR_NOT_CONTENT_INDEXED )

struct SII {		/* this is an image of an $SII index entry */
	le16 offs;
	le16 size;
	le32 fill1;
	le16 indexsz;
	le16 indexksz;
	le16 flags;
	le16 fill2;
	le32 keysecurid;

	/* did not find official description for the following */
	le32 hash;
	le32 securid;
	le32 dataoffsl;	/* documented as badly aligned */
	le32 dataoffsh;
	le32 datasize;
} ;

struct SDH {		/* this is an image of an $SDH index entry */
	le16 offs;
	le16 size;
	le32 fill1;
	le16 indexsz;
	le16 indexksz;
	le16 flags;
	le16 fill2;
	le32 keyhash;
	le32 keysecurid;

	/* did not find official description for the following */
	le32 hash;
	le32 securid;
	le32 dataoffsl;
	le32 dataoffsh;
	le32 datasize;
	le32 fill3;
	} ;

/*
 *	A few useful constants
 */

static ntfschar sii_stream[] = { const_cpu_to_le16('$'),
				 const_cpu_to_le16('S'),
				 const_cpu_to_le16('I'),   
				 const_cpu_to_le16('I'),   
				 const_cpu_to_le16(0) };
static ntfschar sdh_stream[] = { const_cpu_to_le16('$'),
				 const_cpu_to_le16('S'),
				 const_cpu_to_le16('D'),
				 const_cpu_to_le16('H'),
				 const_cpu_to_le16(0) };

/*
 *		null SID (S-1-0-0)
 */

extern const SID *nullsid;

/*
 * The zero GUID.
 */

static const GUID __zero_guid = { const_cpu_to_le32(0), const_cpu_to_le16(0),
		const_cpu_to_le16(0), { 0, 0, 0, 0, 0, 0, 0, 0 } };
static const GUID *const zero_guid = &__zero_guid;

/**
 * ntfs_guid_is_zero - check if a GUID is zero
 * @guid:	[IN] guid to check
 *
 * Return TRUE if @guid is a valid pointer to a GUID and it is the zero GUID
 * and FALSE otherwise.
 */
BOOL ntfs_guid_is_zero(const GUID *guid)
{
	return (memcmp(guid, zero_guid, sizeof(*zero_guid)));
}

/**
 * ntfs_guid_to_mbs - convert a GUID to a multi byte string
 * @guid:	[IN]  guid to convert
 * @guid_str:	[OUT] string in which to return the GUID (optional)
 *
 * Convert the GUID pointed to by @guid to a multi byte string of the form
 * "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX".  Therefore, @guid_str (if not NULL)
 * needs to be able to store at least 37 bytes.
 *
 * If @guid_str is not NULL it will contain the converted GUID on return.  If
 * it is NULL a string will be allocated and this will be returned.  The caller
 * is responsible for free()ing the string in that case.
 *
 * On success return the converted string and on failure return NULL with errno
 * set to the error code.
 */
char *ntfs_guid_to_mbs(const GUID *guid, char *guid_str)
{
	char *_guid_str;
	int res;

	if (!guid) {
		errno = EINVAL;
		return NULL;
	}
	_guid_str = guid_str;
	if (!_guid_str) {
		_guid_str = (char*)ntfs_malloc(37);
		if (!_guid_str)
			return _guid_str;
	}
	res = snprintf(_guid_str, 37,
			"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			(unsigned int)le32_to_cpu(guid->data1),
			le16_to_cpu(guid->data2), le16_to_cpu(guid->data3),
			guid->data4[0], guid->data4[1],
			guid->data4[2], guid->data4[3], guid->data4[4],
			guid->data4[5], guid->data4[6], guid->data4[7]);
	if (res == 36)
		return _guid_str;
	if (!guid_str)
		free(_guid_str);
	errno = EINVAL;
	return NULL;
}

/**
 * ntfs_sid_to_mbs_size - determine maximum size for the string of a SID
 * @sid:	[IN]  SID for which to determine the maximum string size
 *
 * Determine the maximum multi byte string size in bytes which is needed to
 * store the standard textual representation of the SID pointed to by @sid.
 * See ntfs_sid_to_mbs(), below.
 *
 * On success return the maximum number of bytes needed to store the multi byte
 * string and on failure return -1 with errno set to the error code.
 */
int ntfs_sid_to_mbs_size(const SID *sid)
{
	int size, i;

	if (!ntfs_sid_is_valid(sid)) {
		errno = EINVAL;
		return -1;
	}
	/* Start with "S-". */
	size = 2;
	/*
	 * Add the SID_REVISION.  Hopefully the compiler will optimize this
	 * away as SID_REVISION is a constant.
	 */
	for (i = SID_REVISION; i > 0; i /= 10)
		size++;
	/* Add the "-". */
	size++;
	/*
	 * Add the identifier authority.  If it needs to be in decimal, the
	 * maximum is 2^32-1 = 4294967295 = 10 characters.  If it needs to be
	 * in hexadecimal, then maximum is 0x665544332211 = 14 characters.
	 */
	if (!sid->identifier_authority.high_part)
		size += 10;
	else
		size += 14;
	/*
	 * Finally, add the sub authorities.  For each we have a "-" followed
	 * by a decimal which can be up to 2^32-1 = 4294967295 = 10 characters.
	 */
	size += (1 + 10) * sid->sub_authority_count;
	/* We need the zero byte at the end, too. */
	size++;
	return size * sizeof(char);
}

/**
 * ntfs_sid_to_mbs - convert a SID to a multi byte string
 * @sid:		[IN]  SID to convert
 * @sid_str:		[OUT] string in which to return the SID (optional)
 * @sid_str_size:	[IN]  size in bytes of @sid_str
 *
 * Convert the SID pointed to by @sid to its standard textual representation.
 * @sid_str (if not NULL) needs to be able to store at least
 * ntfs_sid_to_mbs_size() bytes.  @sid_str_size is the size in bytes of
 * @sid_str if @sid_str is not NULL.
 *
 * The standard textual representation of the SID is of the form:
 *	S-R-I-S-S...
 * Where:
 *    - The first "S" is the literal character 'S' identifying the following
 *	digits as a SID.
 *    - R is the revision level of the SID expressed as a sequence of digits
 *	in decimal.
 *    - I is the 48-bit identifier_authority, expressed as digits in decimal,
 *	if I < 2^32, or hexadecimal prefixed by "0x", if I >= 2^32.
 *    - S... is one or more sub_authority values, expressed as digits in
 *	decimal.
 *
 * If @sid_str is not NULL it will contain the converted SUID on return.  If it
 * is NULL a string will be allocated and this will be returned.  The caller is
 * responsible for free()ing the string in that case.
 *
 * On success return the converted string and on failure return NULL with errno
 * set to the error code.
 */
char *ntfs_sid_to_mbs(const SID *sid, char *sid_str, size_t sid_str_size)
{
	u64 u;
	le32 leauth;
	char *s;
	int i, j, cnt;

	/*
	 * No need to check @sid if !@sid_str since ntfs_sid_to_mbs_size() will
	 * check @sid, too.  8 is the minimum SID string size.
	 */
	if (sid_str && (sid_str_size < 8 || !ntfs_sid_is_valid(sid))) {
		errno = EINVAL;
		return NULL;
	}
	/* Allocate string if not provided. */
	if (!sid_str) {
		cnt = ntfs_sid_to_mbs_size(sid);
		if (cnt < 0)
			return NULL;
		s = (char*)ntfs_malloc(cnt);
		if (!s)
			return s;
		sid_str = s;
		/* So we know we allocated it. */
		sid_str_size = 0;
	} else {
		s = sid_str;
		cnt = sid_str_size;
	}
	/* Start with "S-R-". */
	i = snprintf(s, cnt, "S-%hhu-", (unsigned char)sid->revision);
	if (i < 0 || i >= cnt)
		goto err_out;
	s += i;
	cnt -= i;
	/* Add the identifier authority. */
	for (u = i = 0, j = 40; i < 6; i++, j -= 8)
		u += (u64)sid->identifier_authority.value[i] << j;
	if (!sid->identifier_authority.high_part)
		i = snprintf(s, cnt, "%lu", (unsigned long)u);
	else
		i = snprintf(s, cnt, "0x%llx", (unsigned long long)u);
	if (i < 0 || i >= cnt)
		goto err_out;
	s += i;
	cnt -= i;
	/* Finally, add the sub authorities. */
	for (j = 0; j < sid->sub_authority_count; j++) {
		leauth = sid->sub_authority[j];
		i = snprintf(s, cnt, "-%u", (unsigned int)
				le32_to_cpu(leauth));
		if (i < 0 || i >= cnt)
			goto err_out;
		s += i;
		cnt -= i;
	}
	return sid_str;
err_out:
	if (i >= cnt)
		i = EMSGSIZE;
	else
		i = errno;
	if (!sid_str_size)
		free(sid_str);
	errno = i;
	return NULL;
}

/**
 * ntfs_generate_guid - generatates a random current guid.
 * @guid:	[OUT]   pointer to a GUID struct to hold the generated guid.
 *
 * perhaps not a very good random number generator though...
 */
void ntfs_generate_guid(GUID *guid)
{
	unsigned int i;
	u8 *p = (u8 *)guid;

	for (i = 0; i < sizeof(GUID); i++) {
		p[i] = (u8)(random() & 0xFF);
		if (i == 7)
			p[7] = (p[7] & 0x0F) | 0x40;
		if (i == 8)
			p[8] = (p[8] & 0x3F) | 0x80;
	}
}

/**
 * ntfs_security_hash - calculate the hash of a security descriptor
 * @sd:         self-relative security descriptor whose hash to calculate
 * @length:     size in bytes of the security descritor @sd
 *
 * Calculate the hash of the self-relative security descriptor @sd of length
 * @length bytes.
 *
 * This hash is used in the $Secure system file as the primary key for the $SDH
 * index and is also stored in the header of each security descriptor in the
 * $SDS data stream as well as in the index data of both the $SII and $SDH
 * indexes.  In all three cases it forms part of the SDS_ENTRY_HEADER
 * structure.
 *
 * Return the calculated security hash in little endian.
 */
le32 ntfs_security_hash(const SECURITY_DESCRIPTOR_RELATIVE *sd, const u32 len)
{
	const le32 *pos = (const le32*)sd;
	const le32 *end = pos + (len >> 2);
	u32 hash = 0;

	while (pos < end) {
		hash = le32_to_cpup(pos) + ntfs_rol32(hash, 3);
		pos++;
	}
	return cpu_to_le32(hash);
}

/*
 *		Internal read
 *	copied and pasted from ntfs_fuse_read() and made independent
 *	of fuse context
 */

static int ntfs_local_read(ntfs_inode *ni,
		ntfschar *stream_name, int stream_name_len,
		char *buf, size_t size, off_t offset)
{
	ntfs_attr *na = NULL;
	int res, total = 0;

	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na) {
		res = -errno;
		goto exit;
	}
	if ((size_t)offset < (size_t)na->data_size) {
		if (offset + size > (size_t)na->data_size)
			size = na->data_size - offset;
		while (size) {
			res = ntfs_attr_pread(na, offset, size, buf);
			if ((off_t)res < (off_t)size)
				ntfs_log_perror("ntfs_attr_pread partial read "
					"(%lld : %lld <> %d)",
					(long long)offset,
					(long long)size, res);
			if (res <= 0) {
				res = -errno;
				goto exit;
			}
			size -= res;
			offset += res;
			total += res;
		}
	}
	res = total;
exit:
	if (na)
		ntfs_attr_close(na);
	return res;
}


/*
 *		Internal write
 *	copied and pasted from ntfs_fuse_write() and made independent
 *	of fuse context
 */

static int ntfs_local_write(ntfs_inode *ni,
		ntfschar *stream_name, int stream_name_len,
		char *buf, size_t size, off_t offset)
{
	ntfs_attr *na = NULL;
	int res, total = 0;

	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na) {
		res = -errno;
		goto exit;
	}
	while (size) {
		res = ntfs_attr_pwrite(na, offset, size, buf);
		if (res < (s64)size)
			ntfs_log_perror("ntfs_attr_pwrite partial write (%lld: "
				"%lld <> %d)", (long long)offset,
				(long long)size, res);
		if (res <= 0) {
			res = -errno;
			goto exit;
		}
		size -= res;
		offset += res;
		total += res;
	}
	res = total;
exit:
	if (na)
		ntfs_attr_close(na);
	return res;
}


/*
 *	Get the first entry of current index block
 *	cut and pasted form ntfs_ie_get_first() in index.c
 */

static INDEX_ENTRY *ntfs_ie_get_first(INDEX_HEADER *ih)
{
	return (INDEX_ENTRY*)((u8*)ih + le32_to_cpu(ih->entries_offset));
}

/*
 *		Stuff a 256KB block into $SDS before writing descriptors
 *	into the block.
 *
 *	This prevents $SDS from being automatically declared as sparse
 *	when the second copy of the first security descriptor is written
 *	256KB further ahead.
 *
 *	Having $SDS declared as a sparse file is not wrong by itself
 *	and chkdsk leaves it as a sparse file. It does however complain
 *	and add a sparse flag (0x0200) into field file_attributes of
 *	STANDARD_INFORMATION of $Secure. This probably means that a
 *	sparse attribute (ATTR_IS_SPARSE) is only allowed in sparse
 *	files (FILE_ATTR_SPARSE_FILE).
 *
 *	Windows normally does not convert to sparse attribute or sparse
 *	file. Stuffing is just a way to get to the same result.
 */

static int entersecurity_stuff(ntfs_volume *vol, off_t offs)
{
	int res;
	int written;
	unsigned long total;
	char *stuff;

	res = 0;
	total = 0;
	stuff = (char*)ntfs_malloc(STUFFSZ);
	if (stuff) {
		memset(stuff, 0, STUFFSZ);
		do {
			written = ntfs_local_write(vol->secure_ni,
				STREAM_SDS, 4, stuff, STUFFSZ, offs);
			if (written == STUFFSZ) {
				total += STUFFSZ;
				offs += STUFFSZ;
			} else {
				errno = ENOSPC;
				res = -1;
			}
		} while (!res && (total < ALIGN_SDS_BLOCK));
		free(stuff);
	} else {
		errno = ENOMEM;
		res = -1;
	}
	return (res);
}

/*
 *		Enter a new security descriptor into $Secure (data only)
 *      it has to be written twice with an offset of 256KB
 *
 *	Should only be called by entersecurityattr() to ensure consistency
 *
 *	Returns zero if sucessful
 */

static int entersecurity_data(ntfs_volume *vol,
			const SECURITY_DESCRIPTOR_RELATIVE *attr, s64 attrsz,
			le32 hash, le32 keyid, off_t offs, int gap)
{
	int res;
	int written1;
	int written2;
	char *fullattr;
	int fullsz;
	SECURITY_DESCRIPTOR_HEADER *phsds;

	res = -1;
	fullsz = attrsz + gap + sizeof(SECURITY_DESCRIPTOR_HEADER);
	fullattr = (char*)ntfs_malloc(fullsz);
	if (fullattr) {
			/*
			 * Clear the gap from previous descriptor
			 * this could be useful for appending the second
			 * copy to the end of file. When creating a new
			 * 256K block, the gap is cleared while writing
			 * the first copy
			 */
		if (gap)
			memset(fullattr,0,gap);
		memcpy(&fullattr[gap + sizeof(SECURITY_DESCRIPTOR_HEADER)],
				attr,attrsz);
		phsds = (SECURITY_DESCRIPTOR_HEADER*)&fullattr[gap];
		phsds->hash = hash;
		phsds->security_id = keyid;
		phsds->offset = cpu_to_le64(offs);
		phsds->length = cpu_to_le32(fullsz - gap);
		written1 = ntfs_local_write(vol->secure_ni,
			STREAM_SDS, 4, fullattr, fullsz,
			offs - gap);
		written2 = ntfs_local_write(vol->secure_ni,
			STREAM_SDS, 4, fullattr, fullsz,
			offs - gap + ALIGN_SDS_BLOCK);
		if ((written1 == fullsz)
		     && (written2 == written1))
			res = 0;
		else
			errno = ENOSPC;
		free(fullattr);
	} else
		errno = ENOMEM;
	return (res);
}

/*
 *	Enter a new security descriptor in $Secure (indexes only)
 *
 *	Should only be called by entersecurityattr() to ensure consistency
 *
 *	Returns zero if sucessful
 */

static int entersecurity_indexes(ntfs_volume *vol, s64 attrsz,
			le32 hash, le32 keyid, off_t offs)
{
	union {
		struct {
			le32 dataoffsl;
			le32 dataoffsh;
		} parts;
		le64 all;
	} realign;
	int res;
	ntfs_index_context *xsii;
	ntfs_index_context *xsdh;
	struct SII newsii;
	struct SDH newsdh;

	res = -1;
				/* enter a new $SII record */

	xsii = vol->secure_xsii;
	ntfs_index_ctx_reinit(xsii);
	newsii.offs = const_cpu_to_le16(20);
	newsii.size = const_cpu_to_le16(sizeof(struct SII) - 20);
	newsii.fill1 = const_cpu_to_le32(0);
	newsii.indexsz = const_cpu_to_le16(sizeof(struct SII));
	newsii.indexksz = const_cpu_to_le16(sizeof(SII_INDEX_KEY));
	newsii.flags = const_cpu_to_le16(0);
	newsii.fill2 = const_cpu_to_le16(0);
	newsii.keysecurid = keyid;
	newsii.hash = hash;
	newsii.securid = keyid;
	realign.all = cpu_to_le64(offs);
	newsii.dataoffsh = realign.parts.dataoffsh;
	newsii.dataoffsl = realign.parts.dataoffsl;
	newsii.datasize = cpu_to_le32(attrsz
			 + sizeof(SECURITY_DESCRIPTOR_HEADER));
	if (!ntfs_ie_add(xsii,(INDEX_ENTRY*)&newsii)) {

		/* enter a new $SDH record */

		xsdh = vol->secure_xsdh;
		ntfs_index_ctx_reinit(xsdh);
		newsdh.offs = const_cpu_to_le16(24);
		newsdh.size = const_cpu_to_le16(
			sizeof(SECURITY_DESCRIPTOR_HEADER));
		newsdh.fill1 = const_cpu_to_le32(0);
		newsdh.indexsz = const_cpu_to_le16(
				sizeof(struct SDH));
		newsdh.indexksz = const_cpu_to_le16(
				sizeof(SDH_INDEX_KEY));
		newsdh.flags = const_cpu_to_le16(0);
		newsdh.fill2 = const_cpu_to_le16(0);
		newsdh.keyhash = hash;
		newsdh.keysecurid = keyid;
		newsdh.hash = hash;
		newsdh.securid = keyid;
		newsdh.dataoffsh = realign.parts.dataoffsh;
		newsdh.dataoffsl = realign.parts.dataoffsl;
		newsdh.datasize = cpu_to_le32(attrsz
			 + sizeof(SECURITY_DESCRIPTOR_HEADER));
                           /* special filler value, Windows generally */
                           /* fills with 0x00490049, sometimes with zero */
		newsdh.fill3 = const_cpu_to_le32(0x00490049);
		if (!ntfs_ie_add(xsdh,(INDEX_ENTRY*)&newsdh))
			res = 0;
	}
	return (res);
}

/*
 *	Enter a new security descriptor in $Secure (data and indexes)
 *	Returns id of entry, or zero if there is a problem.
 *	(should not be called for NTFS version < 3.0)
 *
 *	important : calls have to be serialized, however no locking is
 *	needed while fuse is not multithreaded
 */

static le32 entersecurityattr(ntfs_volume *vol,
			const SECURITY_DESCRIPTOR_RELATIVE *attr, s64 attrsz,
			le32 hash)
{
	union {
		struct {
			le32 dataoffsl;
			le32 dataoffsh;
		} parts;
		le64 all;
	} realign;
	le32 securid;
	le32 keyid;
	u32 newkey;
	off_t offs;
	int gap;
	int size;
	BOOL found;
	struct SII *psii;
	INDEX_ENTRY *entry;
	INDEX_ENTRY *next;
	ntfs_index_context *xsii;
	int retries;
	ntfs_attr *na;
	int olderrno;

	/* find the first available securid beyond the last key */
	/* in $Secure:$SII. This also determines the first */
	/* available location in $Secure:$SDS, as this stream */
	/* is always appended to and the id's are allocated */
	/* in sequence */

	securid = const_cpu_to_le32(0);
	xsii = vol->secure_xsii;
	ntfs_index_ctx_reinit(xsii);
	offs = size = 0;
	keyid = const_cpu_to_le32(-1);
	olderrno = errno;
	found = !ntfs_index_lookup((char*)&keyid,
			       sizeof(SII_INDEX_KEY), xsii);
	if (!found && (errno != ENOENT)) {
		ntfs_log_perror("Inconsistency in index $SII");
		psii = (struct SII*)NULL;
	} else {
			/* restore errno to avoid misinterpretation */
		errno = olderrno;
		entry = xsii->entry;
		psii = (struct SII*)xsii->entry;
	}
	if (psii) {
		/*
		 * Get last entry in block, but must get first one
		 * one first, as we should already be beyond the
		 * last one. For some reason the search for the last
		 * entry sometimes does not return the last block...
		 * we assume this can only happen in root block
		 */
		if (xsii->is_in_root)
			entry = ntfs_ie_get_first
				((INDEX_HEADER*)&xsii->ir->index);
		else
			entry = ntfs_ie_get_first
				((INDEX_HEADER*)&xsii->ib->index);
		/*
		 * All index blocks should be at least half full
		 * so there always is a last entry but one,
		 * except when creating the first entry in index root.
		 * This was however found not to be true : chkdsk
		 * sometimes deletes all the (unused) keys in the last
		 * index block without rebalancing the tree.
		 * When this happens, a new search is restarted from
		 * the smallest key.
		 */
		keyid = const_cpu_to_le32(0);
		retries = 0;
		while (entry) {
			next = ntfs_index_next(entry,xsii);
			if (next) { 
				psii = (struct SII*)next;
					/* save last key and */
					/* available position */
				keyid = psii->keysecurid;
				realign.parts.dataoffsh
						 = psii->dataoffsh;
				realign.parts.dataoffsl
						 = psii->dataoffsl;
				offs = le64_to_cpu(realign.all);
				size = le32_to_cpu(psii->datasize);
			}
			entry = next;
			if (!entry && !keyid && !retries) {
				/* search failed, retry from smallest key */
				ntfs_index_ctx_reinit(xsii);
				found = !ntfs_index_lookup((char*)&keyid,
					       sizeof(SII_INDEX_KEY), xsii);
				if (!found && (errno != ENOENT)) {
					ntfs_log_perror("Index $SII is broken");
				} else {
						/* restore errno */
					errno = olderrno;
					entry = xsii->entry;
				}
				retries++;
			}
		}
	}
	if (!keyid) {
		/*
		 * could not find any entry, before creating the first
		 * entry, make a double check by making sure size of $SII
		 * is less than needed for one entry
		 */
		securid = const_cpu_to_le32(0);
		na = ntfs_attr_open(vol->secure_ni,AT_INDEX_ROOT,sii_stream,4);
		if (na) {
			if ((size_t)na->data_size < sizeof(struct SII)) {
				ntfs_log_error("Creating the first security_id\n");
				securid = const_cpu_to_le32(FIRST_SECURITY_ID);
			}
			ntfs_attr_close(na);
		}
		if (!securid) {
			ntfs_log_error("Error creating a security_id\n");
			errno = EIO;
		}
	} else {
		newkey = le32_to_cpu(keyid) + 1;
		securid = cpu_to_le32(newkey);
	}
	/*
	 * The security attr has to be written twice 256KB
	 * apart. This implies that offsets like
	 * 0x40000*odd_integer must be left available for
	 * the second copy. So align to next block when
	 * the last byte overflows on a wrong block.
	 */

	if (securid) {
		gap = (-size) & (ALIGN_SDS_ENTRY - 1);
		offs += gap + size;
		if ((offs + attrsz + sizeof(SECURITY_DESCRIPTOR_HEADER) - 1)
	 	   & ALIGN_SDS_BLOCK) {
			offs = ((offs + attrsz
				 + sizeof(SECURITY_DESCRIPTOR_HEADER) - 1)
			 	| (ALIGN_SDS_BLOCK - 1)) + 1;
		}
		if (!(offs & (ALIGN_SDS_BLOCK - 1)))
			entersecurity_stuff(vol, offs);
		/*
		 * now write the security attr to storage :
		 * first data, then SII, then SDH
		 * If failure occurs while writing SDS, data will never
		 *    be accessed through indexes, and will be overwritten
		 *    by the next allocated descriptor
		 * If failure occurs while writing SII, the id has not
		 *    recorded and will be reallocated later
		 * If failure occurs while writing SDH, the space allocated
		 *    in SDS or SII will not be reused, an inconsistency
		 *    will persist with no significant consequence
		 */
		if (entersecurity_data(vol, attr, attrsz, hash, securid, offs, gap)
		    || entersecurity_indexes(vol, attrsz, hash, securid, offs))
			securid = const_cpu_to_le32(0);
	}
		/* inode now is dirty, synchronize it all */
	ntfs_index_entry_mark_dirty(vol->secure_xsii);
	ntfs_index_ctx_reinit(vol->secure_xsii);
	ntfs_index_entry_mark_dirty(vol->secure_xsdh);
	ntfs_index_ctx_reinit(vol->secure_xsdh);
	NInoSetDirty(vol->secure_ni);
	if (ntfs_inode_sync(vol->secure_ni))
		ntfs_log_perror("Could not sync $Secure\n");
	return (securid);
}

/*
 *		Find a matching security descriptor in $Secure,
 *	if none, allocate a new id and write the descriptor to storage
 *	Returns id of entry, or zero if there is a problem.
 *
 *	important : calls have to be serialized, however no locking is
 *	needed while fuse is not multithreaded
 */

static le32 setsecurityattr(ntfs_volume *vol,
			const SECURITY_DESCRIPTOR_RELATIVE *attr, s64 attrsz)
{
	struct SDH *psdh;	/* this is an image of index (le) */
	union {
		struct {
			le32 dataoffsl;
			le32 dataoffsh;
		} parts;
		le64 all;
	} realign;
	BOOL found;
	BOOL collision;
	size_t size;
	size_t rdsize;
	s64 offs;
	int res;
	ntfs_index_context *xsdh;
	char *oldattr;
	SDH_INDEX_KEY key;
	INDEX_ENTRY *entry;
	le32 securid;
	le32 hash;
	int olderrno;

	hash = ntfs_security_hash(attr,attrsz);
	oldattr = (char*)NULL;
	securid = const_cpu_to_le32(0);
	res = 0;
	xsdh = vol->secure_xsdh;
	if (vol->secure_ni && xsdh && !vol->secure_reentry++) {
		ntfs_index_ctx_reinit(xsdh);
		/*
		 * find the nearest key as (hash,0)
		 * (do not search for partial key : in case of collision,
		 * it could return a key which is not the first one which
		 * collides)
		 */
		key.hash = hash;
		key.security_id = const_cpu_to_le32(0);
		olderrno = errno;
		found = !ntfs_index_lookup((char*)&key,
				 sizeof(SDH_INDEX_KEY), xsdh);
		if (!found && (errno != ENOENT))
			ntfs_log_perror("Inconsistency in index $SDH");
		else {
				/* restore errno to avoid misinterpretation */
			errno = olderrno;
			entry = xsdh->entry;
			found = FALSE;
			/*
			 * lookup() may return a node with no data,
			 * if so get next
			 */
			if (entry->ie_flags & INDEX_ENTRY_END)
				entry = ntfs_index_next(entry,xsdh);
			do {
				collision = FALSE;
				psdh = (struct SDH*)entry;
				if (psdh)
					size = (size_t) le32_to_cpu(psdh->datasize)
						 - sizeof(SECURITY_DESCRIPTOR_HEADER);
				else size = 0;
			   /* if hash is not the same, the key is not present */
				if (psdh && (size > 0)
				   && (psdh->keyhash == hash)) {
					   /* if hash is the same */
					   /* check the whole record */
					realign.parts.dataoffsh = psdh->dataoffsh;
					realign.parts.dataoffsl = psdh->dataoffsl;
					offs = le64_to_cpu(realign.all)
						+ sizeof(SECURITY_DESCRIPTOR_HEADER);
					oldattr = (char*)ntfs_malloc(size);
					if (oldattr) {
						rdsize = ntfs_local_read(
							vol->secure_ni,
							STREAM_SDS, 4,
							oldattr, size, offs);
						found = (rdsize == size)
							&& !memcmp(oldattr,attr,size);
						free(oldattr);
					  /* if the records do not compare */
					  /* (hash collision), try next one */
						if (!found) {
							entry = ntfs_index_next(
								entry,xsdh);
							collision = TRUE;
						}
					} else
						res = ENOMEM;
				}
			} while (collision && entry);
			if (found)
				securid = psdh->keysecurid;
			else {
				if (res) {
					errno = res;
					securid = const_cpu_to_le32(0);
				} else {
					/*
					 * no matching key :
					 * have to build a new one
					 */
					securid = entersecurityattr(vol,
						attr, attrsz, hash);
				}
			}
		}
	}
	if (--vol->secure_reentry)
		ntfs_log_perror("Reentry error, check no multithreading\n");
	return (securid);
}


/*
 *		Update the security descriptor of a file
 *	Either as an attribute (complying with pre v3.x NTFS version)
 *	or, when possible, as an entry in $Secure (for NTFS v3.x)
 *
 *	returns 0 if success
 */

static int update_secur_descr(ntfs_volume *vol,
				char *newattr, ntfs_inode *ni)
{
	int newattrsz;
	int written;
	int res;
	ntfs_attr *na;

	newattrsz = ntfs_attr_size(newattr);

#if !FORCE_FORMAT_v1x
	if ((vol->major_ver < 3) || !vol->secure_ni) {
#endif

		/* update for NTFS format v1.x */

		/* update the old security attribute */
		na = ntfs_attr_open(ni, AT_SECURITY_DESCRIPTOR, AT_UNNAMED, 0);
		if (na) {
			/* resize attribute */
			res = ntfs_attr_truncate(na, (s64) newattrsz);
			/* overwrite value */
			if (!res) {
				written = (int)ntfs_attr_pwrite(na, (s64) 0,
					 (s64) newattrsz, newattr);
				if (written != newattrsz) {
					ntfs_log_error("Failed to update "
						"a v1.x security descriptor\n");
					errno = EIO;
					res = -1;
				}
			}

			ntfs_attr_close(na);
			/* if old security attribute was found, also */
			/* truncate standard information attribute to v1.x */
			/* this is needed when security data is wanted */
			/* as v1.x though volume is formatted for v3.x */
			na = ntfs_attr_open(ni, AT_STANDARD_INFORMATION,
				AT_UNNAMED, 0);
			if (na) {
				clear_nino_flag(ni, v3_Extensions);
			/*
			 * Truncating the record does not sweep extensions
			 * from copy in memory. Clear security_id to be safe
			 */
				ni->security_id = const_cpu_to_le32(0);
				res = ntfs_attr_truncate(na, (s64)48);
				ntfs_attr_close(na);
				clear_nino_flag(ni, v3_Extensions);
			}
		} else {
			/*
			 * insert the new security attribute if there
			 * were none
			 */
			res = ntfs_attr_add(ni, AT_SECURITY_DESCRIPTOR,
					    AT_UNNAMED, 0, (u8*)newattr,
					    (s64) newattrsz);
		}
#if !FORCE_FORMAT_v1x
	} else {

		/* update for NTFS format v3.x */

		le32 securid;

		securid = setsecurityattr(vol,
			(const SECURITY_DESCRIPTOR_RELATIVE*)newattr,
			(s64)newattrsz);
		if (securid) {
			na = ntfs_attr_open(ni, AT_STANDARD_INFORMATION,
				AT_UNNAMED, 0);
			if (na) {
				res = 0;
				if (!test_nino_flag(ni, v3_Extensions)) {
			/* expand standard information attribute to v3.x */
					res = ntfs_attr_truncate(na,
					 (s64)sizeof(STANDARD_INFORMATION));
					ni->owner_id = const_cpu_to_le32(0);
					ni->quota_charged = const_cpu_to_le64(0);
					ni->usn = const_cpu_to_le64(0);
					ntfs_attr_remove(ni,
						AT_SECURITY_DESCRIPTOR,
						AT_UNNAMED, 0);
			}
				set_nino_flag(ni, v3_Extensions);
				ni->security_id = securid;
				ntfs_attr_close(na);
			} else {
				ntfs_log_error("Failed to update "
					"standard informations\n");
				errno = EIO;
				res = -1;
			}
		} else
			res = -1;
	}
#endif

	/* mark node as dirty */
	NInoSetDirty(ni);
	return (res);
}

/*
 *		Upgrade the security descriptor of a file
 *	This is intended to allow graceful upgrades for files which
 *	were created in previous versions, with a security attributes
 *	and no security id.
 *	
 *      It will allocate a security id and replace the individual
 *	security attribute by a reference to the global one
 *
 *	Special files are not upgraded (currently / and files in
 *	directories /$*)
 *
 *	Though most code is similar to update_secur_desc() it has
 *	been kept apart to facilitate the further processing of
 *	special cases or even to remove it if found dangerous.
 *
 *	returns 0 if success,
 *		1 if not upgradable. This is not an error.
 *		-1 if there is a problem
 */

static int upgrade_secur_desc(ntfs_volume *vol,
				const char *attr, ntfs_inode *ni)
{
	int attrsz;
	int res;
	le32 securid;
	ntfs_attr *na;

		/*
		 * upgrade requires NTFS format v3.x
		 * also refuse upgrading for special files
		 * whose number is less than FILE_first_user
		 */

	if ((vol->major_ver >= 3)
	    && (ni->mft_no >= FILE_first_user)) {
		attrsz = ntfs_attr_size(attr);
		securid = setsecurityattr(vol,
			(const SECURITY_DESCRIPTOR_RELATIVE*)attr,
			(s64)attrsz);
		if (securid) {
			na = ntfs_attr_open(ni, AT_STANDARD_INFORMATION,
				AT_UNNAMED, 0);
			if (na) {
				res = 0;
			/* expand standard information attribute to v3.x */
				res = ntfs_attr_truncate(na,
					 (s64)sizeof(STANDARD_INFORMATION));
				ni->owner_id = const_cpu_to_le32(0);
				ni->quota_charged = const_cpu_to_le64(0);
				ni->usn = const_cpu_to_le64(0);
				ntfs_attr_remove(ni, AT_SECURITY_DESCRIPTOR,
						AT_UNNAMED, 0);
				set_nino_flag(ni, v3_Extensions);
				ni->security_id = securid;
				ntfs_attr_close(na);
			} else {
				ntfs_log_error("Failed to upgrade "
					"standard informations\n");
				errno = EIO;
				res = -1;
			}
		} else
			res = -1;
			/* mark node as dirty */
		NInoSetDirty(ni);
	} else
		res = 1;

	return (res);
}

/*
 *		Optional simplified checking of group membership
 *
 *	This only takes into account the groups defined in
 *	/etc/group at initialization time.
 *	It does not take into account the groups dynamically set by
 *	setgroups() nor the changes in /etc/group since initialization
 *
 *	This optional method could be useful if standard checking
 *	leads to a performance concern.
 *
 *	Should not be called for user root, however the group may be root
 *
 */

static BOOL staticgroupmember(struct SECURITY_CONTEXT *scx, uid_t uid, gid_t gid)
{
	BOOL ingroup;
	int grcnt;
	gid_t *groups;
	struct MAPPING *user;

	ingroup = FALSE;
	if (uid) {
		user = scx->mapping[MAPUSERS];
		while (user && ((uid_t)user->xid != uid))
			user = user->next;
		if (user) {
			groups = user->groups;
			grcnt = user->grcnt;
			while ((--grcnt >= 0) && (groups[grcnt] != gid)) { }
			ingroup = (grcnt >= 0);
		}
	}
	return (ingroup);
}


/*
 *		Check whether current thread owner is member of file group
 *
 *	Should not be called for user root, however the group may be root
 *
 * As indicated by Miklos Szeredi :
 *
 * The group list is available in
 *
 *   /proc/$PID/task/$TID/status
 *
 * and fuse supplies TID in get_fuse_context()->pid.  The only problem is
 * finding out PID, for which I have no good solution, except to iterate
 * through all processes.  This is rather slow, but may be speeded up
 * with caching and heuristics (for single threaded programs PID = TID).
 *
 * The following implementation gets the group list from
 *   /proc/$TID/task/$TID/status which apparently exists and
 * contains the same data.
 */

static BOOL groupmember(struct SECURITY_CONTEXT *scx, uid_t uid, gid_t gid)
{
	static char key[] = "\nGroups:";
	char buf[BUFSZ+1];
	char filename[64];
	enum { INKEY, INSEP, INNUM, INEND } state;
	int fd;
	char c;
	int matched;
	BOOL ismember;
	int got;
	char *p;
	gid_t grp;
	pid_t tid;

	if (scx->vol->secure_flags & (1 << SECURITY_STATICGRPS))
		ismember = staticgroupmember(scx, uid, gid);
	else {
		ismember = FALSE; /* default return */
		tid = scx->tid;
		sprintf(filename,"/proc/%u/task/%u/status",tid,tid);
		fd = open(filename,O_RDONLY);
		if (fd >= 0) {
			got = read(fd, buf, BUFSZ);
			buf[got] = 0;
			state = INKEY;
			matched = 0;
			p = buf;
			grp = 0;
				/*
				 *  A simple automaton to process lines like
				 *  Groups: 14 500 513
				 */
			do {
				c = *p++;
				if (!c) {
					/* refill buffer */
					got = read(fd, buf, BUFSZ);
					buf[got] = 0;
					p = buf;
					c = *p++; /* 0 at end of file */
				}
				switch (state) {
				case INKEY :
					if (key[matched] == c) {
						if (!key[++matched])
							state = INSEP;
					} else
						if (key[0] == c)
							matched = 1;
						else
							matched = 0;
					break;
				case INSEP :
					if ((c >= '0') && (c <= '9')) {
						grp = c - '0';
						state = INNUM;
					} else
						if ((c != ' ') && (c != '\t'))
							state = INEND;
					break;
				case INNUM :
					if ((c >= '0') && (c <= '9'))
						grp = grp*10 + c - '0';
					else {
						ismember = (grp == gid);
						if ((c != ' ') && (c != '\t'))
							state = INEND;
						else
							state = INSEP;
					}
				default :
					break;
				}
			} while (!ismember && c && (state != INEND));
		close(fd);
		if (!c)
			ntfs_log_error("No group record found in %s\n",filename);
		} else
			ntfs_log_error("Could not open %s\n",filename);
	}
	return (ismember);
}

/*
 *	Cacheing is done two-way :
 *	- from uid, gid and perm to securid (CACHED_SECURID)
 *	- from a securid to uid, gid and perm (CACHED_PERMISSIONS)
 *
 *	CACHED_SECURID data is kept in a most-recent-first list
 *	which should not be too long to be efficient. Its optimal
 *	size is depends on usage and is hard to determine.
 *
 *	CACHED_PERMISSIONS data is kept in a two-level indexed array. It
 *	is optimal at the expense of storage. Use of a most-recent-first
 *	list would save memory and provide similar performances for
 *	standard usage, but not for file servers with too many file
 *	owners
 *
 *	CACHED_PERMISSIONS_LEGACY is a special case for CACHED_PERMISSIONS
 *	for legacy directories which were not allocated a security_id
 *	it is organized in a most-recent-first list.
 *
 *	In main caches, data is never invalidated, as the meaning of
 *	a security_id only changes when user mapping is changed, which
 *	current implies remounting. However returned entries may be
 *	overwritten at next update, so data has to be copied elsewhere
 *	before another cache update is made.
 *	In legacy cache, data has to be invalidated when protection is
 *	changed.
 *
 *	Though the same data may be found in both list, they
 *	must be kept separately : the interpretation of ACL
 *	in both direction are approximations which could be non
 *	reciprocal for some configuration of the user mapping data
 *
 *	During the process of recompiling ntfs-3g from a tgz archive,
 *	security processing added 7.6% to the cpu time used by ntfs-3g
 *	and 30% if the cache is disabled.
 */

static struct PERMISSIONS_CACHE *create_caches(struct SECURITY_CONTEXT *scx,
			u32 securindex)
{
	struct PERMISSIONS_CACHE *cache;
	unsigned int index1;
	unsigned int i;

	cache = (struct PERMISSIONS_CACHE*)NULL;
		/* create the first permissions blocks */
	index1 = securindex >> CACHE_PERMISSIONS_BITS;
	cache = (struct PERMISSIONS_CACHE*)
		ntfs_malloc(sizeof(struct PERMISSIONS_CACHE)
		      + index1*sizeof(struct CACHED_PERMISSIONS*));
	if (cache) {
		cache->head.last = index1;
		cache->head.p_reads = 0;
		cache->head.p_hits = 0;
		cache->head.p_writes = 0;
		*scx->pseccache = cache;
		for (i=0; i<=index1; i++)
			cache->cachetable[i]
			   = (struct CACHED_PERMISSIONS*)NULL;
	}
	return (cache);
}

/*
 *		Free memory used by caches
 *	The only purpose is to facilitate the detection of memory leaks
 */

static void free_caches(struct SECURITY_CONTEXT *scx)
{
	unsigned int index1;
	struct PERMISSIONS_CACHE *pseccache;

	pseccache = *scx->pseccache;
	if (pseccache) {
		for (index1=0; index1<=pseccache->head.last; index1++)
			if (pseccache->cachetable[index1]) {
#if POSIXACLS
				struct CACHED_PERMISSIONS *cacheentry;
				unsigned int index2;

				for (index2=0; index2<(1<< CACHE_PERMISSIONS_BITS); index2++) {
					cacheentry = &pseccache->cachetable[index1][index2];
					if (cacheentry->valid
					    && cacheentry->pxdesc)
						free(cacheentry->pxdesc);
					}
#endif
				free(pseccache->cachetable[index1]);
			}
		free(pseccache);
	}
}

static int compare(const struct CACHED_SECURID *cached,
			const struct CACHED_SECURID *item)
{
#if POSIXACLS
	size_t csize;
	size_t isize;

		/* only compare data and sizes */
	csize = (cached->variable ?
		sizeof(struct POSIX_ACL)
		+ (((struct POSIX_SECURITY*)cached->variable)->acccnt
		   + ((struct POSIX_SECURITY*)cached->variable)->defcnt)
			*sizeof(struct POSIX_ACE) :
		0);
	isize = (item->variable ?
		sizeof(struct POSIX_ACL)
		+ (((struct POSIX_SECURITY*)item->variable)->acccnt
		   + ((struct POSIX_SECURITY*)item->variable)->defcnt)
			*sizeof(struct POSIX_ACE) :
		0);
	return ((cached->uid != item->uid)
		 || (cached->gid != item->gid)
		 || (cached->dmode != item->dmode)
		 || (csize != isize)
		 || (csize
		    && isize
		    && memcmp(&((struct POSIX_SECURITY*)cached->variable)->acl,
		       &((struct POSIX_SECURITY*)item->variable)->acl, csize)));
#else
	return ((cached->uid != item->uid)
		 || (cached->gid != item->gid)
		 || (cached->dmode != item->dmode));
#endif
}

static int leg_compare(const struct CACHED_PERMISSIONS_LEGACY *cached,
			const struct CACHED_PERMISSIONS_LEGACY *item)
{
	return (cached->mft_no != item->mft_no);
}

/*
 *	Resize permission cache table
 *	do not call unless resizing is needed
 *	
 *	If allocation fails, the cache size is not updated
 *	Lack of memory is not considered as an error, the cache is left
 *	consistent and errno is not set.
 */

static void resize_cache(struct SECURITY_CONTEXT *scx,
			u32 securindex)
{
	struct PERMISSIONS_CACHE *oldcache;
	struct PERMISSIONS_CACHE *newcache;
	int newcnt;
	int oldcnt;
	unsigned int index1;
	unsigned int i;

	oldcache = *scx->pseccache;
	index1 = securindex >> CACHE_PERMISSIONS_BITS;
	newcnt = index1 + 1;
	if (newcnt <= ((CACHE_PERMISSIONS_SIZE
			+ (1 << CACHE_PERMISSIONS_BITS)
			- 1) >> CACHE_PERMISSIONS_BITS)) {
		/* expand cache beyond current end, do not use realloc() */
		/* to avoid losing data when there is no more memory */
		oldcnt = oldcache->head.last + 1;
		newcache = (struct PERMISSIONS_CACHE*)
			ntfs_malloc(
			    sizeof(struct PERMISSIONS_CACHE)
			      + (newcnt - 1)*sizeof(struct CACHED_PERMISSIONS*));
		if (newcache) {
			memcpy(newcache,oldcache,
			    sizeof(struct PERMISSIONS_CACHE)
			      + (oldcnt - 1)*sizeof(struct CACHED_PERMISSIONS*));
			free(oldcache);
			     /* mark new entries as not valid */
			for (i=newcache->head.last+1; i<=index1; i++)
				newcache->cachetable[i]
					 = (struct CACHED_PERMISSIONS*)NULL;
			newcache->head.last = index1;
			*scx->pseccache = newcache;
		}
	}
}

/*
 *	Enter uid, gid and mode into cache, if possible
 *
 *	returns the updated or created cache entry,
 *	or NULL if not possible (typically if there is no
 *		security id associated)
 */

#if POSIXACLS
static struct CACHED_PERMISSIONS *enter_cache(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, uid_t uid, gid_t gid,
		struct POSIX_SECURITY *pxdesc)
#else
static struct CACHED_PERMISSIONS *enter_cache(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, uid_t uid, gid_t gid, mode_t mode)
#endif
{
	struct CACHED_PERMISSIONS *cacheentry;
	struct CACHED_PERMISSIONS *cacheblock;
	struct PERMISSIONS_CACHE *pcache;
	u32 securindex;
#if POSIXACLS
	int pxsize;
	struct POSIX_SECURITY *pxcached;
#endif
	unsigned int index1;
	unsigned int index2;
	int i;

	/* cacheing is only possible if a security_id has been defined */
	if (test_nino_flag(ni, v3_Extensions)
	   && ni->security_id) {
		/*
		 *  Immediately test the most frequent situation
		 *  where the entry exists
		 */
		securindex = le32_to_cpu(ni->security_id);
		index1 = securindex >> CACHE_PERMISSIONS_BITS;
		index2 = securindex & ((1 << CACHE_PERMISSIONS_BITS) - 1);
		pcache = *scx->pseccache;
		if (pcache
		     && (pcache->head.last >= index1)
		     && pcache->cachetable[index1]) {
			cacheentry = &pcache->cachetable[index1][index2];
			cacheentry->uid = uid;
			cacheentry->gid = gid;
#if POSIXACLS
			if (cacheentry->valid && cacheentry->pxdesc)
				free(cacheentry->pxdesc);
			if (pxdesc) {
				pxsize = sizeof(struct POSIX_SECURITY)
					+ (pxdesc->acccnt + pxdesc->defcnt)*sizeof(struct POSIX_ACE);
				pxcached = (struct POSIX_SECURITY*)malloc(pxsize);
				if (pxcached) {
					memcpy(pxcached, pxdesc, pxsize);
					cacheentry->pxdesc = pxcached;
				} else {
					cacheentry->valid = 0;
					cacheentry = (struct CACHED_PERMISSIONS*)NULL;
				}
				cacheentry->mode = pxdesc->mode & 07777;
			} else
				cacheentry->pxdesc = (struct POSIX_SECURITY*)NULL;
#else
			cacheentry->mode = mode & 07777;
#endif
			cacheentry->inh_fileid = const_cpu_to_le32(0);
			cacheentry->inh_dirid = const_cpu_to_le32(0);
			cacheentry->valid = 1;
			pcache->head.p_writes++;
		} else {
			if (!pcache) {
				/* create the first cache block */
				pcache = create_caches(scx, securindex);
			} else {
				if (index1 > pcache->head.last) {
					resize_cache(scx, securindex);
					pcache = *scx->pseccache;
				}
			}
			/* allocate block, if cache table was allocated */
			if (pcache && (index1 <= pcache->head.last)) {
				cacheblock = (struct CACHED_PERMISSIONS*)
					malloc(sizeof(struct CACHED_PERMISSIONS)
						<< CACHE_PERMISSIONS_BITS);
				pcache->cachetable[index1] = cacheblock;
				for (i=0; i<(1 << CACHE_PERMISSIONS_BITS); i++)
					cacheblock[i].valid = 0;
				cacheentry = &cacheblock[index2];
				if (cacheentry) {
					cacheentry->uid = uid;
					cacheentry->gid = gid;
#if POSIXACLS
					if (pxdesc) {
						pxsize = sizeof(struct POSIX_SECURITY)
							+ (pxdesc->acccnt + pxdesc->defcnt)*sizeof(struct POSIX_ACE);
						pxcached = (struct POSIX_SECURITY*)malloc(pxsize);
						if (pxcached) {
							memcpy(pxcached, pxdesc, pxsize);
							cacheentry->pxdesc = pxcached;
						} else {
							cacheentry->valid = 0;
							cacheentry = (struct CACHED_PERMISSIONS*)NULL;
						}
						cacheentry->mode = pxdesc->mode & 07777;
					} else
						cacheentry->pxdesc = (struct POSIX_SECURITY*)NULL;
#else
					cacheentry->mode = mode & 07777;
#endif
					cacheentry->inh_fileid = const_cpu_to_le32(0);
					cacheentry->inh_dirid = const_cpu_to_le32(0);
					cacheentry->valid = 1;
					pcache->head.p_writes++;
				}
			} else
				cacheentry = (struct CACHED_PERMISSIONS*)NULL;
		}
	} else {
		cacheentry = (struct CACHED_PERMISSIONS*)NULL;
#if CACHE_LEGACY_SIZE
		if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
			struct CACHED_PERMISSIONS_LEGACY wanted;
			struct CACHED_PERMISSIONS_LEGACY *legacy;

			wanted.perm.uid = uid;
			wanted.perm.gid = gid;
#if POSIXACLS
			wanted.perm.mode = pxdesc->mode & 07777;
			wanted.perm.inh_fileid = const_cpu_to_le32(0);
			wanted.perm.inh_dirid = const_cpu_to_le32(0);
			wanted.mft_no = ni->mft_no;
			wanted.variable = (void*)pxdesc;
			wanted.varsize = sizeof(struct POSIX_SECURITY)
					+ (pxdesc->acccnt + pxdesc->defcnt)*sizeof(struct POSIX_ACE);
#else
			wanted.perm.mode = mode & 07777;
			wanted.perm.inh_fileid = const_cpu_to_le32(0);
			wanted.perm.inh_dirid = const_cpu_to_le32(0);
			wanted.mft_no = ni->mft_no;
			wanted.variable = (void*)NULL;
			wanted.varsize = 0;
#endif
			legacy = (struct CACHED_PERMISSIONS_LEGACY*)ntfs_enter_cache(
				scx->vol->legacy_cache, GENERIC(&wanted),
				(cache_compare)leg_compare);
			if (legacy) {
				cacheentry = &legacy->perm;
#if POSIXACLS
				/*
				 * give direct access to the cached pxdesc
				 * in the permissions structure
				 */
				cacheentry->pxdesc = legacy->variable;
#endif
			}
		}
#endif
	}
	return (cacheentry);
}

/*
 *	Fetch owner, group and permission of a file, if cached
 *
 *	Beware : do not use the returned entry after a cache update :
 *	the cache may be relocated making the returned entry meaningless
 *
 *	returns the cache entry, or NULL if not available
 */

static struct CACHED_PERMISSIONS *fetch_cache(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni)
{
	struct CACHED_PERMISSIONS *cacheentry;
	struct PERMISSIONS_CACHE *pcache;
	u32 securindex;
	unsigned int index1;
	unsigned int index2;

	/* cacheing is only possible if a security_id has been defined */
	cacheentry = (struct CACHED_PERMISSIONS*)NULL;
	if (test_nino_flag(ni, v3_Extensions)
	   && (ni->security_id)) {
		securindex = le32_to_cpu(ni->security_id);
		index1 = securindex >> CACHE_PERMISSIONS_BITS;
		index2 = securindex & ((1 << CACHE_PERMISSIONS_BITS) - 1);
		pcache = *scx->pseccache;
		if (pcache
		     && (pcache->head.last >= index1)
		     && pcache->cachetable[index1]) {
			cacheentry = &pcache->cachetable[index1][index2];
			/* reject if entry is not valid */
			if (!cacheentry->valid)
				cacheentry = (struct CACHED_PERMISSIONS*)NULL;
			else
				pcache->head.p_hits++;
		if (pcache)
			pcache->head.p_reads++;
		}
	}
#if CACHE_LEGACY_SIZE
	else {
		cacheentry = (struct CACHED_PERMISSIONS*)NULL;
		if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
			struct CACHED_PERMISSIONS_LEGACY wanted;
			struct CACHED_PERMISSIONS_LEGACY *legacy;

			wanted.mft_no = ni->mft_no;
			wanted.variable = (void*)NULL;
			wanted.varsize = 0;
			legacy = (struct CACHED_PERMISSIONS_LEGACY*)ntfs_fetch_cache(
				scx->vol->legacy_cache, GENERIC(&wanted),
				(cache_compare)leg_compare);
			if (legacy) cacheentry = &legacy->perm;
		}
	}
#endif
#if POSIXACLS
	if (cacheentry && !cacheentry->pxdesc) {
		ntfs_log_error("No Posix descriptor in cache\n");
		cacheentry = (struct CACHED_PERMISSIONS*)NULL;
	}
#endif
	return (cacheentry);
}

/*
 *	Retrieve a security attribute from $Secure
 */

static char *retrievesecurityattr(ntfs_volume *vol, SII_INDEX_KEY id)
{
	struct SII *psii;
	union {
		struct {
			le32 dataoffsl;
			le32 dataoffsh;
		} parts;
		le64 all;
	} realign;
	int found;
	size_t size;
	size_t rdsize;
	s64 offs;
	ntfs_inode *ni;
	ntfs_index_context *xsii;
	char *securattr;

	securattr = (char*)NULL;
	ni = vol->secure_ni;
	xsii = vol->secure_xsii;
	if (ni && xsii) {
		ntfs_index_ctx_reinit(xsii);
		found =
		    !ntfs_index_lookup((char*)&id,
				       sizeof(SII_INDEX_KEY), xsii);
		if (found) {
			psii = (struct SII*)xsii->entry;
			size =
			    (size_t) le32_to_cpu(psii->datasize)
				 - sizeof(SECURITY_DESCRIPTOR_HEADER);
			/* work around bad alignment problem */
			realign.parts.dataoffsh = psii->dataoffsh;
			realign.parts.dataoffsl = psii->dataoffsl;
			offs = le64_to_cpu(realign.all)
				+ sizeof(SECURITY_DESCRIPTOR_HEADER);

			securattr = (char*)ntfs_malloc(size);
			if (securattr) {
				rdsize = ntfs_local_read(
					ni, STREAM_SDS, 4,
					securattr, size, offs);
				if ((rdsize != size)
					|| !ntfs_valid_descr(securattr,
						rdsize)) {
					/* error to be logged by caller */
					free(securattr);
					securattr = (char*)NULL;
				}
			}
		} else
			if (errno != ENOENT)
				ntfs_log_perror("Inconsistency in index $SII");
	}
	if (!securattr) {
		ntfs_log_error("Failed to retrieve a security descriptor\n");
		errno = EIO;
	}
	return (securattr);
}

/*
 *		Get the security descriptor associated to a file
 *
 *	Either :
 *	   - read the security descriptor attribute (v1.x format)
 *	   - or find the descriptor in $Secure:$SDS (v3.x format)
 *
 *	in both case, sanity checks are done on the attribute and
 *	the descriptor can be assumed safe
 *
 *	The returned descriptor is dynamically allocated and has to be freed
 */

static char *getsecurityattr(ntfs_volume *vol, ntfs_inode *ni)
{
	SII_INDEX_KEY securid;
	char *securattr;
	s64 readallsz;

		/*
		 * Warning : in some situations, after fixing by chkdsk,
		 * v3_Extensions are marked present (long standard informations)
		 * with a default security descriptor inserted in an
		 * attribute
		 */
	if (test_nino_flag(ni, v3_Extensions)
			&& vol->secure_ni && ni->security_id) {
			/* get v3.x descriptor in $Secure */
		securid.security_id = ni->security_id;
		securattr = retrievesecurityattr(vol,securid);
		if (!securattr)
			ntfs_log_error("Bad security descriptor for 0x%lx\n",
					(long)le32_to_cpu(ni->security_id));
	} else {
			/* get v1.x security attribute */
		readallsz = 0;
		securattr = ntfs_attr_readall(ni, AT_SECURITY_DESCRIPTOR,
				AT_UNNAMED, 0, &readallsz);
		if (securattr && !ntfs_valid_descr(securattr, readallsz)) {
			ntfs_log_error("Bad security descriptor for inode %lld\n",
				(long long)ni->mft_no);
			free(securattr);
			securattr = (char*)NULL;
		}
	}
	if (!securattr) {
			/*
			 * in some situations, there is no security
			 * descriptor, and chkdsk does not detect or fix
			 * anything. This could be a normal situation.
			 * When this happens, simulate a descriptor with
			 * minimum rights, so that a real descriptor can
			 * be created by chown or chmod
			 */
		ntfs_log_error("No security descriptor found for inode %lld\n",
				(long long)ni->mft_no);
		securattr = ntfs_build_descr(0, 0, adminsid, adminsid);
	}
	return (securattr);
}

#if POSIXACLS

/*
 *		Determine which access types to a file are allowed
 *	according to the relation of current process to the file
 *
 *	Do not call if default_permissions is set
 */

static int access_check_posix(struct SECURITY_CONTEXT *scx,
			struct POSIX_SECURITY *pxdesc, mode_t request,
			uid_t uid, gid_t gid)
{
	struct POSIX_ACE *pxace;
	int userperms;
	int groupperms;
	int mask;
	BOOL somegroup;
	BOOL needgroups;
	mode_t perms;
	int i;

	perms = pxdesc->mode;
					/* owner and root access */
	if (!scx->uid || (uid == scx->uid)) {
		if (!scx->uid) {
					/* root access if owner or other execution */
			if (perms & 0101)
				perms = 07777;
			else {
					/* root access if some group execution */
				groupperms = 0;
				mask = 7;
				for (i=pxdesc->acccnt-1; i>=0 ; i--) {
					pxace = &pxdesc->acl.ace[i];
					switch (pxace->tag) {
					case POSIX_ACL_USER_OBJ :
					case POSIX_ACL_GROUP_OBJ :
					case POSIX_ACL_GROUP :
						groupperms |= pxace->perms;
						break;
					case POSIX_ACL_MASK :
						mask = pxace->perms & 7;
						break;
					default :
						break;
					}
				}
				perms = (groupperms & mask & 1) | 6;
			}
		} else
			perms &= 07700;
	} else {
				/*
				 * analyze designated users, get mask
				 * and identify whether we need to check
				 * the group memberships. The groups are
				 * not needed when all groups have the
				 * same permissions as other for the
				 * requested modes.
				 */
		userperms = -1;
		groupperms = -1;
		needgroups = FALSE;
		mask = 7;
		for (i=pxdesc->acccnt-1; i>=0 ; i--) {
			pxace = &pxdesc->acl.ace[i];
			switch (pxace->tag) {
			case POSIX_ACL_USER :
				if ((uid_t)pxace->id == scx->uid)
					userperms = pxace->perms;
				break;
			case POSIX_ACL_MASK :
				mask = pxace->perms & 7;
				break;
			case POSIX_ACL_GROUP_OBJ :
			case POSIX_ACL_GROUP :
				if (((pxace->perms & mask) ^ perms)
				    & (request >> 6) & 7)
					needgroups = TRUE;
				break;
			default :
				break;
			}
		}
					/* designated users */
		if (userperms >= 0)
			perms = (perms & 07000) + (userperms & mask);
		else if (!needgroups)
				perms &= 07007;
		else {
					/* owning group */
			if (!(~(perms >> 3) & request & mask)
			    && ((gid == scx->gid)
				|| groupmember(scx, scx->uid, gid)))
				perms &= 07070;
			else {
					/* other groups */
				groupperms = -1;
				somegroup = FALSE;
				for (i=pxdesc->acccnt-1; i>=0 ; i--) {
					pxace = &pxdesc->acl.ace[i];
					if ((pxace->tag == POSIX_ACL_GROUP)
					    && groupmember(scx, uid, pxace->id)) {
						if (!(~pxace->perms & request & mask))
							groupperms = pxace->perms;
						somegroup = TRUE;
					}
				}
				if (groupperms >= 0)
					perms = (perms & 07000) + (groupperms & mask);
				else
					if (somegroup)
						perms = 0;
					else
						perms &= 07007;
			}
		}
	}
	return (perms);
}

/*
 *		Get permissions to access a file
 *	Takes into account the relation of user to file (owner, group, ...)
 *	Do no use as mode of the file
 *	Do no call if default_permissions is set
 *
 *	returns -1 if there is a problem
 */

static int ntfs_get_perm(struct SECURITY_CONTEXT *scx,
		 ntfs_inode * ni, mode_t request)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const struct CACHED_PERMISSIONS *cached;
	char *securattr;
	const SID *usid;	/* owner of file/directory */
	const SID *gsid;	/* group of file/directory */
	uid_t uid;
	gid_t gid;
	int perm;
	BOOL isdir;
	struct POSIX_SECURITY *pxdesc;

	if (!scx->mapping[MAPUSERS])
		perm = 07777;
	else {
		/* check whether available in cache */
		cached = fetch_cache(scx,ni);
		if (cached) {
			uid = cached->uid;
			gid = cached->gid;
			perm = access_check_posix(scx,cached->pxdesc,request,uid,gid);
		} else {
			perm = 0;	/* default to no permission */
			isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
				!= const_cpu_to_le16(0);
			securattr = getsecurityattr(scx->vol, ni);
			if (securattr) {
				phead = (const SECURITY_DESCRIPTOR_RELATIVE*)
				    	securattr;
				gsid = (const SID*)&
					   securattr[le32_to_cpu(phead->group)];
				gid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
#if OWNERFROMACL
				usid = ntfs_acl_owner(securattr);
				pxdesc = ntfs_build_permissions_posix(scx->mapping,securattr,
						 usid, gsid, isdir);
				if (pxdesc)
					perm = pxdesc->mode & 07777;
				else
					perm = -1;
				uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#else
				usid = (const SID*)&
					    securattr[le32_to_cpu(phead->owner)];
				pxdesc = ntfs_build_permissions_posix(scx,securattr,
						 usid, gsid, isdir);
				if (pxdesc)
					perm = pxdesc->mode & 07777;
				else
					perm = -1;
				if (!perm && ntfs_same_sid(usid, adminsid)) {
					uid = find_tenant(scx, securattr);
					if (uid)
						perm = 0700;
				} else
					uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#endif
				/*
				 *  Create a security id if there were none
				 * and upgrade option is selected
				 */
				if (!test_nino_flag(ni, v3_Extensions)
				   && (perm >= 0)
				   && (scx->vol->secure_flags
				     & (1 << SECURITY_ADDSECURIDS))) {
					upgrade_secur_desc(scx->vol,
						securattr, ni);
					/*
					 * fetch owner and group for cacheing
					 * if there is a securid
					 */
				}
				if (test_nino_flag(ni, v3_Extensions)
				    && (perm >= 0)) {
					enter_cache(scx, ni, uid,
							gid, pxdesc);
				}
				if (pxdesc) {
					perm = access_check_posix(scx,pxdesc,request,uid,gid);
					free(pxdesc);
				}
				free(securattr);
			} else {
				perm = -1;
				uid = gid = 0;
			}
		}
	}
	return (perm);
}

/*
 *		Get a Posix ACL
 *
 *	returns size or -errno if there is a problem
 *	if size was too small, no copy is done and errno is not set,
 *	the caller is expected to issue a new call
 */

int ntfs_get_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *name, char *value, size_t size) 
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	struct POSIX_SECURITY *pxdesc;
	const struct CACHED_PERMISSIONS *cached;
	char *securattr;
	const SID *usid;	/* owner of file/directory */
	const SID *gsid;	/* group of file/directory */
	uid_t uid;
	gid_t gid;
	int perm;
	BOOL isdir;
	size_t outsize;

	outsize = 0;	/* default to error */
	if (!scx->mapping[MAPUSERS])
		errno = ENOTSUP;
	else {
			/* check whether available in cache */
		cached = fetch_cache(scx,ni);
		if (cached)
			pxdesc = cached->pxdesc;
		else {
			securattr = getsecurityattr(scx->vol, ni);
			isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
				!= const_cpu_to_le16(0);
			if (securattr) {
				phead =
				    (const SECURITY_DESCRIPTOR_RELATIVE*)
			    			securattr;
				gsid = (const SID*)&
					  securattr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
				usid = ntfs_acl_owner(securattr);
#else
				usid = (const SID*)&
					  securattr[le32_to_cpu(phead->owner)];
#endif
				pxdesc = ntfs_build_permissions_posix(scx->mapping,securattr,
					  usid, gsid, isdir);

					/*
					 * fetch owner and group for cacheing
					 */
				if (pxdesc) {
					perm = pxdesc->mode & 07777;
				/*
				 *  Create a security id if there were none
				 * and upgrade option is selected
				 */
					if (!test_nino_flag(ni, v3_Extensions)
					   && (scx->vol->secure_flags
					     & (1 << SECURITY_ADDSECURIDS))) {
						upgrade_secur_desc(scx->vol,
							 securattr, ni);
					}
#if OWNERFROMACL
					uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#else
					if (!perm && ntfs_same_sid(usid, adminsid)) {
						uid = find_tenant(scx,
								securattr);
						if (uid)
							perm = 0700;
					} else
						uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#endif
					gid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
					if (pxdesc->tagsset & POSIX_ACL_EXTENSIONS)
					enter_cache(scx, ni, uid,
							gid, pxdesc);
				}
				free(securattr);
			} else
				pxdesc = (struct POSIX_SECURITY*)NULL;
		}

		if (pxdesc) {
			if (ntfs_valid_posix(pxdesc)) {
				if (!strcmp(name,"system.posix_acl_default")) {
					if (ni->mrec->flags
						    & MFT_RECORD_IS_DIRECTORY)
						outsize = sizeof(struct POSIX_ACL)
							+ pxdesc->defcnt*sizeof(struct POSIX_ACE);
					else {
					/*
					 * getting default ACL from plain file :
					 * return EACCES if size > 0 as
					 * indicated in the man, but return ok
					 * if size == 0, so that ls does not
					 * display an error
					 */
						if (size > 0) {
							outsize = 0;
							errno = EACCES;
						} else
							outsize = sizeof(struct POSIX_ACL);
					}
					if (outsize && (outsize <= size)) {
						memcpy(value,&pxdesc->acl,sizeof(struct POSIX_ACL));
						memcpy(&value[sizeof(struct POSIX_ACL)],
							&pxdesc->acl.ace[pxdesc->firstdef],
							outsize-sizeof(struct POSIX_ACL));
					}
				} else {
					outsize = sizeof(struct POSIX_ACL)
						+ pxdesc->acccnt*sizeof(struct POSIX_ACE);
					if (outsize <= size)
						memcpy(value,&pxdesc->acl,outsize);
				}
			} else {
				outsize = 0;
				errno = EIO;
				ntfs_log_error("Invalid Posix ACL built\n");
			}
			if (!cached)
				free(pxdesc);
		} else
			outsize = 0;
	}
	return (outsize ? (int)outsize : -errno);
}

#else /* POSIXACLS */


/*
 *		Get permissions to access a file
 *	Takes into account the relation of user to file (owner, group, ...)
 *	Do no use as mode of the file
 *
 *	returns -1 if there is a problem
 */

static int ntfs_get_perm(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni,	mode_t request)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const struct CACHED_PERMISSIONS *cached;
	char *securattr;
	const SID *usid;	/* owner of file/directory */
	const SID *gsid;	/* group of file/directory */
	BOOL isdir;
	uid_t uid;
	gid_t gid;
	int perm;

	if (!scx->mapping[MAPUSERS] || (!scx->uid && !(request & S_IEXEC)))
		perm = 07777;
	else {
		/* check whether available in cache */
		cached = fetch_cache(scx,ni);
		if (cached) {
			perm = cached->mode;
			uid = cached->uid;
			gid = cached->gid;
		} else {
			perm = 0;	/* default to no permission */
			isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
				!= const_cpu_to_le16(0);
			securattr = getsecurityattr(scx->vol, ni);
			if (securattr) {
				phead = (const SECURITY_DESCRIPTOR_RELATIVE*)
				    	securattr;
				gsid = (const SID*)&
					   securattr[le32_to_cpu(phead->group)];
				gid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
#if OWNERFROMACL
				usid = ntfs_acl_owner(securattr);
				perm = ntfs_build_permissions(securattr,
						 usid, gsid, isdir);
				uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#else
				usid = (const SID*)&
					    securattr[le32_to_cpu(phead->owner)];
				perm = ntfs_build_permissions(securattr,
						 usid, gsid, isdir);
				if (!perm && ntfs_same_sid(usid, adminsid)) {
					uid = find_tenant(scx, securattr);
					if (uid)
						perm = 0700;
				} else
					uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#endif
				/*
				 *  Create a security id if there were none
				 * and upgrade option is selected
				 */
				if (!test_nino_flag(ni, v3_Extensions)
				   && (perm >= 0)
				   && (scx->vol->secure_flags
				     & (1 << SECURITY_ADDSECURIDS))) {
					upgrade_secur_desc(scx->vol,
						securattr, ni);
					/*
					 * fetch owner and group for cacheing
					 * if there is a securid
					 */
				}
				if (test_nino_flag(ni, v3_Extensions)
				    && (perm >= 0)) {
					enter_cache(scx, ni, uid,
							gid, perm);
				}
				free(securattr);
			} else {
				perm = -1;
				uid = gid = 0;
			}
		}
		if (perm >= 0) {
			if (!scx->uid) {
				/* root access and execution */
				if (perm & 0111)
					perm = 07777;
				else
					perm = 0;
			} else
				if (uid == scx->uid)
					perm &= 07700;
				else
				/*
				 * avoid checking group membership
				 * when the requested perms for group
				 * are the same as perms for other
				 */
					if ((gid == scx->gid)
					  || ((((perm >> 3) ^ perm)
						& (request >> 6) & 7)
					    && groupmember(scx, scx->uid, gid)))
						perm &= 07070;
					else
						perm &= 07007;
		}
	}
	return (perm);
}

#endif /* POSIXACLS */

/*
 *		Get an NTFS ACL
 *
 *	Returns size or -errno if there is a problem
 *	if size was too small, no copy is done and errno is not set,
 *	the caller is expected to issue a new call
 */

int ntfs_get_ntfs_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			char *value, size_t size)
{
	char *securattr;
	size_t outsize;

	outsize = 0;	/* default to no data and no error */
	securattr = getsecurityattr(scx->vol, ni);
	if (securattr) {
		outsize = ntfs_attr_size(securattr);
		if (outsize <= size) {
			memcpy(value,securattr,outsize);
		}
		free(securattr);
	}
	return (outsize ? (int)outsize : -errno);
}

/*
 *		Get owner, group and permissions in an stat structure
 *	returns permissions, or -1 if there is a problem
 */

int ntfs_get_owner_mode(struct SECURITY_CONTEXT *scx,
		ntfs_inode * ni, struct stat *stbuf)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	char *securattr;
	const SID *usid;	/* owner of file/directory */
	const SID *gsid;	/* group of file/directory */
	const struct CACHED_PERMISSIONS *cached;
	int perm;
	BOOL isdir;
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
#endif

	if (!scx->mapping[MAPUSERS])
		perm = 07777;
	else {
			/* check whether available in cache */
		cached = fetch_cache(scx,ni);
		if (cached) {
			perm = cached->mode;
			stbuf->st_uid = cached->uid;
			stbuf->st_gid = cached->gid;
			stbuf->st_mode = (stbuf->st_mode & ~07777) + perm;
		} else {
			perm = -1;	/* default to error */
			isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
				!= const_cpu_to_le16(0);
			securattr = getsecurityattr(scx->vol, ni);
			if (securattr) {
				phead =
				    (const SECURITY_DESCRIPTOR_RELATIVE*)
					    	securattr;
				gsid = (const SID*)&
					  securattr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
				usid = ntfs_acl_owner(securattr);
#else
				usid = (const SID*)&
					  securattr[le32_to_cpu(phead->owner)];
#endif
#if POSIXACLS
				pxdesc = ntfs_build_permissions_posix(scx->mapping, securattr,
					  usid, gsid, isdir);
				if (pxdesc)
					perm = pxdesc->mode & 07777;
				else
					perm = -1;
#else
				perm = ntfs_build_permissions(securattr,
					  usid, gsid, isdir);
#endif
					/*
					 * fetch owner and group for cacheing
					 */
				if (perm >= 0) {
				/*
				 *  Create a security id if there were none
				 * and upgrade option is selected
				 */
					if (!test_nino_flag(ni, v3_Extensions)
					   && (scx->vol->secure_flags
					     & (1 << SECURITY_ADDSECURIDS))) {
						upgrade_secur_desc(scx->vol,
							 securattr, ni);
					}
#if OWNERFROMACL
					stbuf->st_uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#else
					if (!perm && ntfs_same_sid(usid, adminsid)) {
						stbuf->st_uid = 
							find_tenant(scx,
								securattr);
						if (stbuf->st_uid)
							perm = 0700;
					} else
						stbuf->st_uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#endif
					stbuf->st_gid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
					stbuf->st_mode =
					    (stbuf->st_mode & ~07777) + perm;
#if POSIXACLS
					enter_cache(scx, ni, stbuf->st_uid,
						stbuf->st_gid, pxdesc);
					free(pxdesc);
#else
					enter_cache(scx, ni, stbuf->st_uid,
						stbuf->st_gid, perm);
#endif
				}
				free(securattr);
			}
		}
	}
	return (perm);
}

#if POSIXACLS

/*
 *		Get the base for a Posix inheritance and
 *	build an inherited Posix descriptor
 */

static struct POSIX_SECURITY *inherit_posix(struct SECURITY_CONTEXT *scx,
			ntfs_inode *dir_ni, mode_t mode, BOOL isdir)
{
	const struct CACHED_PERMISSIONS *cached;
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	struct POSIX_SECURITY *pxdesc;
	struct POSIX_SECURITY *pydesc;
	char *securattr;
	const SID *usid;
	const SID *gsid;
	uid_t uid;
	gid_t gid;

	pydesc = (struct POSIX_SECURITY*)NULL;
		/* check whether parent directory is available in cache */
	cached = fetch_cache(scx,dir_ni);
	if (cached) {
		uid = cached->uid;
		gid = cached->gid;
		pxdesc = cached->pxdesc;
		if (pxdesc) {
			pydesc = ntfs_build_inherited_posix(pxdesc,mode,
					scx->umask,isdir);
		}
	} else {
		securattr = getsecurityattr(scx->vol, dir_ni);
		if (securattr) {
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)
			    	securattr;
			gsid = (const SID*)&
				   securattr[le32_to_cpu(phead->group)];
			gid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
#if OWNERFROMACL
			usid = ntfs_acl_owner(securattr);
			pxdesc = ntfs_build_permissions_posix(scx->mapping,securattr,
						 usid, gsid, TRUE);
			uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#else
			usid = (const SID*)&
				    securattr[le32_to_cpu(phead->owner)];
			pxdesc = ntfs_build_permissions_posix(scx->mapping,securattr,
						 usid, gsid, TRUE);
			if (pxdesc && ntfs_same_sid(usid, adminsid)) {
				uid = find_tenant(scx, securattr);
			} else
				uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
#endif
			if (pxdesc) {
				/*
				 *  Create a security id if there were none
				 * and upgrade option is selected
				 */
				if (!test_nino_flag(dir_ni, v3_Extensions)
				   && (scx->vol->secure_flags
				     & (1 << SECURITY_ADDSECURIDS))) {
					upgrade_secur_desc(scx->vol,
						securattr, dir_ni);
					/*
					 * fetch owner and group for cacheing
					 * if there is a securid
					 */
				}
				if (test_nino_flag(dir_ni, v3_Extensions)) {
					enter_cache(scx, dir_ni, uid,
							gid, pxdesc);
				}
				pydesc = ntfs_build_inherited_posix(pxdesc,
					mode, scx->umask, isdir);
				free(pxdesc);
			}
			free(securattr);
		}
	}
	return (pydesc);
}

/*
 *		Allocate a security_id for a file being created
 *	
 *	Returns zero if not possible (NTFS v3.x required)
 */

le32 ntfs_alloc_securid(struct SECURITY_CONTEXT *scx,
		uid_t uid, gid_t gid, ntfs_inode *dir_ni,
		mode_t mode, BOOL isdir)
{
#if !FORCE_FORMAT_v1x
	const struct CACHED_SECURID *cached;
	struct CACHED_SECURID wanted;
	struct POSIX_SECURITY *pxdesc;
	char *newattr;
	int newattrsz;
	const SID *usid;
	const SID *gsid;
	BIGSID defusid;
	BIGSID defgsid;
	le32 securid;
#endif

	securid = const_cpu_to_le32(0);

#if !FORCE_FORMAT_v1x

	pxdesc = inherit_posix(scx, dir_ni, mode, isdir);
	if (pxdesc) {
		/* check whether target securid is known in cache */

		wanted.uid = uid;
		wanted.gid = gid;
		wanted.dmode = pxdesc->mode & mode & 07777;
		if (isdir) wanted.dmode |= 0x10000;
		wanted.variable = (void*)pxdesc;
		wanted.varsize = sizeof(struct POSIX_SECURITY)
				+ (pxdesc->acccnt + pxdesc->defcnt)*sizeof(struct POSIX_ACE);
		cached = (const struct CACHED_SECURID*)ntfs_fetch_cache(
				scx->vol->securid_cache, GENERIC(&wanted),
				(cache_compare)compare);
			/* quite simple, if we are lucky */
		if (cached)
			securid = cached->securid;

			/* not in cache : make sure we can create ids */

		if (!cached && (scx->vol->major_ver >= 3)) {
			usid = ntfs_find_usid(scx->mapping[MAPUSERS],uid,(SID*)&defusid);
			gsid = ntfs_find_gsid(scx->mapping[MAPGROUPS],gid,(SID*)&defgsid);
			if (!usid || !gsid) {
				ntfs_log_error("File created by an unmapped user/group %d/%d\n",
						(int)uid, (int)gid);
				usid = gsid = adminsid;
			}
			newattr = ntfs_build_descr_posix(scx->mapping, pxdesc,
					isdir, usid, gsid);
			if (newattr) {
				newattrsz = ntfs_attr_size(newattr);
				securid = setsecurityattr(scx->vol,
					(const SECURITY_DESCRIPTOR_RELATIVE*)newattr,
					newattrsz);
				if (securid) {
					/* update cache, for subsequent use */
					wanted.securid = securid;
					ntfs_enter_cache(scx->vol->securid_cache,
							GENERIC(&wanted),
							(cache_compare)compare);
				}
				free(newattr);
			} else {
				/*
				 * could not build new security attribute
				 * errno set by ntfs_build_descr()
				 */
			}
		}
	free(pxdesc);
	}
#endif
	return (securid);
}

/*
 *		Apply Posix inheritance to a newly created file
 *	(for NTFS 1.x only : no securid)
 */

int ntfs_set_inherited_posix(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni, uid_t uid, gid_t gid,
		ntfs_inode *dir_ni, mode_t mode)
{
	struct POSIX_SECURITY *pxdesc;
	char *newattr;
	const SID *usid;
	const SID *gsid;
	BIGSID defusid;
	BIGSID defgsid;
	BOOL isdir;
	int res;

	res = -1;
	isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) != const_cpu_to_le16(0);
	pxdesc = inherit_posix(scx, dir_ni, mode, isdir);
	if (pxdesc) {
		usid = ntfs_find_usid(scx->mapping[MAPUSERS],uid,(SID*)&defusid);
		gsid = ntfs_find_gsid(scx->mapping[MAPGROUPS],gid,(SID*)&defgsid);
		if (!usid || !gsid) {
			ntfs_log_error("File created by an unmapped user/group %d/%d\n",
					(int)uid, (int)gid);
			usid = gsid = adminsid;
		}
		newattr = ntfs_build_descr_posix(scx->mapping, pxdesc,
					isdir, usid, gsid);
		if (newattr) {
				/* Adjust Windows read-only flag */
			res = update_secur_descr(scx->vol, newattr, ni);
			if (!res && !isdir) {
				if (mode & S_IWUSR)
					ni->flags &= ~FILE_ATTR_READONLY;
				else
					ni->flags |= FILE_ATTR_READONLY;
			}
#if CACHE_LEGACY_SIZE
			/* also invalidate legacy cache */
			if (isdir && !ni->security_id) {
				struct CACHED_PERMISSIONS_LEGACY legacy;

				legacy.mft_no = ni->mft_no;
				legacy.variable = pxdesc;
				legacy.varsize = sizeof(struct POSIX_SECURITY)
					+ (pxdesc->acccnt + pxdesc->defcnt)*sizeof(struct POSIX_ACE);
				ntfs_invalidate_cache(scx->vol->legacy_cache,
						GENERIC(&legacy),
						(cache_compare)leg_compare,0);
			}
#endif
			free(newattr);

		} else {
			/*
			 * could not build new security attribute
			 * errno set by ntfs_build_descr()
			 */
		}
	}
	return (res);
}

#else

le32 ntfs_alloc_securid(struct SECURITY_CONTEXT *scx,
		uid_t uid, gid_t gid, mode_t mode, BOOL isdir)
{
#if !FORCE_FORMAT_v1x
	const struct CACHED_SECURID *cached;
	struct CACHED_SECURID wanted;
	char *newattr;
	int newattrsz;
	const SID *usid;
	const SID *gsid;
	BIGSID defusid;
	BIGSID defgsid;
	le32 securid;
#endif

	securid = const_cpu_to_le32(0);

#if !FORCE_FORMAT_v1x
		/* check whether target securid is known in cache */

	wanted.uid = uid;
	wanted.gid = gid;
	wanted.dmode = mode & 07777;
	if (isdir) wanted.dmode |= 0x10000;
	wanted.variable = (void*)NULL;
	wanted.varsize = 0;
	cached = (const struct CACHED_SECURID*)ntfs_fetch_cache(
			scx->vol->securid_cache, GENERIC(&wanted),
			(cache_compare)compare);
		/* quite simple, if we are lucky */
	if (cached)
		securid = cached->securid;

		/* not in cache : make sure we can create ids */

	if (!cached && (scx->vol->major_ver >= 3)) {
		usid = ntfs_find_usid(scx->mapping[MAPUSERS],uid,(SID*)&defusid);
		gsid = ntfs_find_gsid(scx->mapping[MAPGROUPS],gid,(SID*)&defgsid);
		if (!usid || !gsid) {
			ntfs_log_error("File created by an unmapped user/group %d/%d\n",
					(int)uid, (int)gid);
			usid = gsid = adminsid;
		}
		newattr = ntfs_build_descr(mode, isdir, usid, gsid);
		if (newattr) {
			newattrsz = ntfs_attr_size(newattr);
			securid = setsecurityattr(scx->vol,
				(const SECURITY_DESCRIPTOR_RELATIVE*)newattr,
				newattrsz);
			if (securid) {
				/* update cache, for subsequent use */
				wanted.securid = securid;
				ntfs_enter_cache(scx->vol->securid_cache,
						GENERIC(&wanted),
						(cache_compare)compare);
			}
			free(newattr);
		} else {
			/*
			 * could not build new security attribute
			 * errno set by ntfs_build_descr()
			 */
		}
	}
#endif
	return (securid);
}

#endif

/*
 *		Update ownership and mode of a file, reusing an existing
 *	security descriptor when possible
 *	
 *	Returns zero if successful
 */

#if POSIXACLS
int ntfs_set_owner_mode(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
		uid_t uid, gid_t gid, mode_t mode,
		struct POSIX_SECURITY *pxdesc)
#else
int ntfs_set_owner_mode(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
		uid_t uid, gid_t gid, mode_t mode)
#endif
{
	int res;
	const struct CACHED_SECURID *cached;
	struct CACHED_SECURID wanted;
	char *newattr;
	const SID *usid;
	const SID *gsid;
	BIGSID defusid;
	BIGSID defgsid;
	BOOL isdir;

	res = 0;

		/* check whether target securid is known in cache */

	isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) != const_cpu_to_le16(0);
	wanted.uid = uid;
	wanted.gid = gid;
	wanted.dmode = mode & 07777;
	if (isdir) wanted.dmode |= 0x10000;
#if POSIXACLS
	wanted.variable = (void*)pxdesc;
	if (pxdesc)
		wanted.varsize = sizeof(struct POSIX_SECURITY)
			+ (pxdesc->acccnt + pxdesc->defcnt)*sizeof(struct POSIX_ACE);
	else
		wanted.varsize = 0;
#else
	wanted.variable = (void*)NULL;
	wanted.varsize = 0;
#endif
	if (test_nino_flag(ni, v3_Extensions)) {
		cached = (const struct CACHED_SECURID*)ntfs_fetch_cache(
				scx->vol->securid_cache, GENERIC(&wanted),
				(cache_compare)compare);
			/* quite simple, if we are lucky */
		if (cached) {
			ni->security_id = cached->securid;
			NInoSetDirty(ni);
		}
	} else cached = (struct CACHED_SECURID*)NULL;

	if (!cached) {
			/*
			 * Do not use usid and gsid from former attributes,
			 * but recompute them to get repeatable results
			 * which can be kept in cache.
			 */
		usid = ntfs_find_usid(scx->mapping[MAPUSERS],uid,(SID*)&defusid);
		gsid = ntfs_find_gsid(scx->mapping[MAPGROUPS],gid,(SID*)&defgsid);
		if (!usid || !gsid) {
			ntfs_log_error("File made owned by an unmapped user/group %d/%d\n",
				uid, gid);
			usid = gsid = adminsid;
		}
#if POSIXACLS
		if (pxdesc)
			newattr = ntfs_build_descr_posix(scx->mapping, pxdesc,
					 isdir, usid, gsid);
		else
			newattr = ntfs_build_descr(mode,
					 isdir, usid, gsid);
#else
		newattr = ntfs_build_descr(mode,
					 isdir, usid, gsid);
#endif
		if (newattr) {
			res = update_secur_descr(scx->vol, newattr, ni);
			if (!res) {
				/* adjust Windows read-only flag */
				if (!isdir) {
					if (mode & S_IWUSR)
						ni->flags &= ~FILE_ATTR_READONLY;
					else
						ni->flags |= FILE_ATTR_READONLY;
					NInoFileNameSetDirty(ni);
				}
				/* update cache, for subsequent use */
				if (test_nino_flag(ni, v3_Extensions)) {
					wanted.securid = ni->security_id;
					ntfs_enter_cache(scx->vol->securid_cache,
							GENERIC(&wanted),
							(cache_compare)compare);
				}
#if CACHE_LEGACY_SIZE
				/* also invalidate legacy cache */
				if (isdir && !ni->security_id) {
					struct CACHED_PERMISSIONS_LEGACY legacy;

					legacy.mft_no = ni->mft_no;
#if POSIXACLS
					legacy.variable = wanted.variable;
					legacy.varsize = wanted.varsize;
#else
					legacy.variable = (void*)NULL;
					legacy.varsize = 0;
#endif
					ntfs_invalidate_cache(scx->vol->legacy_cache,
						GENERIC(&legacy),
						(cache_compare)leg_compare,0);
				}
#endif
			}
			free(newattr);
		} else {
			/*
			 * could not build new security attribute
			 * errno set by ntfs_build_descr()
			 */
			res = -1;
		}
	}
	return (res);
}

/*
 *		Check whether user has ownership rights on a file
 *
 *	Returns TRUE if allowed
 *		if not, errno tells why
 */

BOOL ntfs_allowed_as_owner(struct SECURITY_CONTEXT *scx, ntfs_inode *ni)
{
	const struct CACHED_PERMISSIONS *cached;
	char *oldattr;
	const SID *usid;
	uid_t processuid;
	uid_t uid;
	BOOL gotowner;
	int allowed;

	processuid = scx->uid;
/* TODO : use CAP_FOWNER process capability */
	/*
	 * Always allow for root
	 * Also always allow if no mapping has been defined
	 */
	if (!scx->mapping[MAPUSERS] || !processuid)
		allowed = TRUE;
	else {
		gotowner = FALSE; /* default */
		/* get the owner, either from cache or from old attribute  */
		cached = fetch_cache(scx, ni);
		if (cached) {
			uid = cached->uid;
			gotowner = TRUE;
		} else {
			oldattr = getsecurityattr(scx->vol, ni);
			if (oldattr) {
#if OWNERFROMACL
				usid = ntfs_acl_owner(oldattr);
#else
				const SECURITY_DESCRIPTOR_RELATIVE *phead;

				phead = (const SECURITY_DESCRIPTOR_RELATIVE*)
								oldattr;
				usid = (const SID*)&oldattr
						[le32_to_cpu(phead->owner)];
#endif
				uid = ntfs_find_user(scx->mapping[MAPUSERS],
						usid);
				gotowner = TRUE;
				free(oldattr);
			}
		}
		allowed = FALSE;
		if (gotowner) {
/* TODO : use CAP_FOWNER process capability */
			if (!processuid || (processuid == uid))
				allowed = TRUE;
			else
				errno = EPERM;
		}
	}
	return (allowed);
}

#ifdef HAVE_SETXATTR    /* extended attributes interface required */

#if POSIXACLS

/*
 *		Set a new access or default Posix ACL to a file
 *		(or remove ACL if no input data)
 *	Validity of input data is checked after merging
 *
 *	Returns 0, or -1 if there is a problem which errno describes
 */

int ntfs_set_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *name, const char *value, size_t size,
			int flags)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const struct CACHED_PERMISSIONS *cached;
	char *oldattr;
	uid_t processuid;
	const SID *usid;
	const SID *gsid;
	uid_t uid;
	uid_t gid;
	int res;
	mode_t mode;
	BOOL isdir;
	BOOL deflt;
	BOOL exist;
	int count;
	struct POSIX_SECURITY *oldpxdesc;
	struct POSIX_SECURITY *newpxdesc;

	/* get the current pxsec, either from cache or from old attribute  */
	res = -1;
	deflt = !strcmp(name,"system.posix_acl_default");
	if (size)
		count = (size - sizeof(struct POSIX_ACL)) / sizeof(struct POSIX_ACE);
	else
		count = 0;
	isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) != const_cpu_to_le16(0);
	newpxdesc = (struct POSIX_SECURITY*)NULL;
	if ((!value
		|| (((const struct POSIX_ACL*)value)->version == POSIX_VERSION))
	    && (!deflt || isdir || (!size && !value))) {
		cached = fetch_cache(scx, ni);
		if (cached) {
			uid = cached->uid;
			gid = cached->gid;
			oldpxdesc = cached->pxdesc;
			if (oldpxdesc) {
				mode = oldpxdesc->mode;
				newpxdesc = ntfs_replace_acl(oldpxdesc,
						(const struct POSIX_ACL*)value,count,deflt);
				}
		} else {
			oldattr = getsecurityattr(scx->vol, ni);
			if (oldattr) {
				phead = (const SECURITY_DESCRIPTOR_RELATIVE*)oldattr;
#if OWNERFROMACL
				usid = ntfs_acl_owner(oldattr);
#else
				usid = (const SID*)&oldattr[le32_to_cpu(phead->owner)];
#endif
				gsid = (const SID*)&oldattr[le32_to_cpu(phead->group)];
				uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
				gid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
				oldpxdesc = ntfs_build_permissions_posix(scx->mapping,
					oldattr, usid, gsid, isdir);
				if (oldpxdesc) {
					if (deflt)
						exist = oldpxdesc->defcnt > 0;
					else
						exist = oldpxdesc->acccnt > 3;
					if ((exist && (flags & XATTR_CREATE))
					  || (!exist && (flags & XATTR_REPLACE))) {
						errno = (exist ? EEXIST : ENODATA);
					} else {
						mode = oldpxdesc->mode;
						newpxdesc = ntfs_replace_acl(oldpxdesc,
							(const struct POSIX_ACL*)value,count,deflt);
					}
					free(oldpxdesc);
				}
				free(oldattr);
			}
		}
	} else
		errno = EINVAL;

	if (newpxdesc) {
		processuid = scx->uid;
/* TODO : use CAP_FOWNER process capability */
		if (!processuid || (uid == processuid)) {
				/*
				 * clear setgid if file group does
				 * not match process group
				 */
			if (processuid && (gid != scx->gid)
			    && !groupmember(scx, scx->uid, gid)) {
				newpxdesc->mode &= ~S_ISGID;
			}
			res = ntfs_set_owner_mode(scx, ni, uid, gid,
				newpxdesc->mode, newpxdesc);
		} else
			errno = EPERM;
		free(newpxdesc);
	}
	return (res ? -1 : 0);
}

/*
 *		Remove a default Posix ACL from a file
 *
 *	Returns 0, or -1 if there is a problem which errno describes
 */

int ntfs_remove_posix_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *name)
{
	return (ntfs_set_posix_acl(scx, ni, name,
			(const char*)NULL, 0, 0));
}

#endif

/*
 *		Set a new NTFS ACL to a file
 *
 *	Returns 0, or -1 if there is a problem
 */

int ntfs_set_ntfs_acl(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			const char *value, size_t size,	int flags)
{
	char *attr;
	int res;

	res = -1;
	if ((size > 0)
	   && !(flags & XATTR_CREATE)
	   && ntfs_valid_descr(value,size)
	   && (ntfs_attr_size(value) == size)) {
			/* need copying in order to write */
		attr = (char*)ntfs_malloc(size);
		if (attr) {
			memcpy(attr,value,size);
			res = update_secur_descr(scx->vol, attr, ni);
			/*
			 * No need to invalidate standard caches :
			 * the relation between a securid and
			 * the associated protection is unchanged,
			 * only the relation between a file and
			 * its securid and protection is changed.
			 */
#if CACHE_LEGACY_SIZE
			/*
			 * we must however invalidate the legacy
			 * cache, which is based on inode numbers.
			 * For safety, invalidate even if updating
			 * failed.
			 */
			if ((ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
			   && !ni->security_id) {
				struct CACHED_PERMISSIONS_LEGACY legacy;

				legacy.mft_no = ni->mft_no;
				legacy.variable = (char*)NULL;
				legacy.varsize = 0;
				ntfs_invalidate_cache(scx->vol->legacy_cache,
					GENERIC(&legacy),
					(cache_compare)leg_compare,0);
			}
#endif
			free(attr);
		} else
			errno = ENOMEM;
	} else
		errno = EINVAL;
	return (res ? -1 : 0);
}

#endif /* HAVE_SETXATTR */

/*
 *		Set new permissions to a file
 *	Checks user mapping has been defined before request for setting
 *
 *	rejected if request is not originated by owner or root
 *
 *	returns 0 on success
 *		-1 on failure, with errno = EIO
 */

int ntfs_set_mode(struct SECURITY_CONTEXT *scx, ntfs_inode *ni, mode_t mode)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const struct CACHED_PERMISSIONS *cached;
	char *oldattr;
	const SID *usid;
	const SID *gsid;
	uid_t processuid;
	uid_t uid;
	uid_t gid;
	int res;
#if POSIXACLS
	BOOL isdir;
	int pxsize;
	const struct POSIX_SECURITY *oldpxdesc;
	struct POSIX_SECURITY *newpxdesc = (struct POSIX_SECURITY*)NULL;
#endif

	/* get the current owner, either from cache or from old attribute  */
	res = 0;
	cached = fetch_cache(scx, ni);
	if (cached) {
		uid = cached->uid;
		gid = cached->gid;
#if POSIXACLS
		oldpxdesc = cached->pxdesc;
		if (oldpxdesc) {
				/* must copy before merging */
			pxsize = sizeof(struct POSIX_SECURITY)
				+ (oldpxdesc->acccnt + oldpxdesc->defcnt)*sizeof(struct POSIX_ACE);
			newpxdesc = (struct POSIX_SECURITY*)malloc(pxsize);
			if (newpxdesc) {
				memcpy(newpxdesc, oldpxdesc, pxsize);
				if (ntfs_merge_mode_posix(newpxdesc, mode))
					res = -1;
			} else
				res = -1;
		} else
			newpxdesc = (struct POSIX_SECURITY*)NULL;
#endif
	} else {
		oldattr = getsecurityattr(scx->vol, ni);
		if (oldattr) {
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)oldattr;
#if OWNERFROMACL
			usid = ntfs_acl_owner(oldattr);
#else
			usid = (const SID*)&oldattr[le32_to_cpu(phead->owner)];
#endif
			gsid = (const SID*)&oldattr[le32_to_cpu(phead->group)];
			uid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
			gid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
#if POSIXACLS
			isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) != const_cpu_to_le16(0);
			newpxdesc = ntfs_build_permissions_posix(scx->mapping,
				oldattr, usid, gsid, isdir);
			if (!newpxdesc || ntfs_merge_mode_posix(newpxdesc, mode))
				res = -1;
#endif
			free(oldattr);
		} else
			res = -1;
	}

	if (!res) {
		processuid = scx->uid;
/* TODO : use CAP_FOWNER process capability */
		if (!processuid || (uid == processuid)) {
				/*
				 * clear setgid if file group does
				 * not match process group
				 */
			if (processuid && (gid != scx->gid)
			    && !groupmember(scx, scx->uid, gid))
				mode &= ~S_ISGID;
#if POSIXACLS
			if (newpxdesc) {
				newpxdesc->mode = mode;
				res = ntfs_set_owner_mode(scx, ni, uid, gid,
					mode, newpxdesc);
			} else
				res = ntfs_set_owner_mode(scx, ni, uid, gid,
					mode, newpxdesc);
#else
			res = ntfs_set_owner_mode(scx, ni, uid, gid, mode);
#endif
		} else {
			errno = EPERM;
			res = -1;	/* neither owner nor root */
		}
	} else {
		/*
		 * Should not happen : a default descriptor is generated
		 * by getsecurityattr() when there are none
		 */
		ntfs_log_error("File has no security descriptor\n");
		res = -1;
		errno = EIO;
	}
#if POSIXACLS
	if (newpxdesc) free(newpxdesc);
#endif
	return (res ? -1 : 0);
}

/*
 *	Create a default security descriptor for files whose descriptor
 *	cannot be inherited
 */

int ntfs_sd_add_everyone(ntfs_inode *ni)
{
	/* JPA SECURITY_DESCRIPTOR_ATTR *sd; */
	SECURITY_DESCRIPTOR_RELATIVE *sd;
	ACL *acl;
	ACCESS_ALLOWED_ACE *ace;
	SID *sid;
	int ret, sd_len;
	
	/* Create SECURITY_DESCRIPTOR attribute (everyone has full access). */
	/*
	 * Calculate security descriptor length. We have 2 sub-authorities in
	 * owner and group SIDs, but structure SID contain only one, so add
	 * 4 bytes to every SID.
	 */
	sd_len = sizeof(SECURITY_DESCRIPTOR_ATTR) + 2 * (sizeof(SID) + 4) +
		sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE); 
	sd = (SECURITY_DESCRIPTOR_RELATIVE*)ntfs_calloc(sd_len);
	if (!sd)
		return -1;
	
	sd->revision = SECURITY_DESCRIPTOR_REVISION;
	sd->control = SE_DACL_PRESENT | SE_SELF_RELATIVE;
	
	sid = (SID*)((u8*)sd + sizeof(SECURITY_DESCRIPTOR_ATTR));
	sid->revision = SID_REVISION;
	sid->sub_authority_count = 2;
	sid->sub_authority[0] = const_cpu_to_le32(SECURITY_BUILTIN_DOMAIN_RID);
	sid->sub_authority[1] = const_cpu_to_le32(DOMAIN_ALIAS_RID_ADMINS);
	sid->identifier_authority.value[5] = 5;
	sd->owner = cpu_to_le32((u8*)sid - (u8*)sd);
	
	sid = (SID*)((u8*)sid + sizeof(SID) + 4); 
	sid->revision = SID_REVISION;
	sid->sub_authority_count = 2;
	sid->sub_authority[0] = const_cpu_to_le32(SECURITY_BUILTIN_DOMAIN_RID);
	sid->sub_authority[1] = const_cpu_to_le32(DOMAIN_ALIAS_RID_ADMINS);
	sid->identifier_authority.value[5] = 5;
	sd->group = cpu_to_le32((u8*)sid - (u8*)sd);
	
	acl = (ACL*)((u8*)sid + sizeof(SID) + 4);
	acl->revision = ACL_REVISION;
	acl->size = const_cpu_to_le16(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE));
	acl->ace_count = const_cpu_to_le16(1);
	sd->dacl = cpu_to_le32((u8*)acl - (u8*)sd);
	
	ace = (ACCESS_ALLOWED_ACE*)((u8*)acl + sizeof(ACL));
	ace->type = ACCESS_ALLOWED_ACE_TYPE;
	ace->flags = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
	ace->size = const_cpu_to_le16(sizeof(ACCESS_ALLOWED_ACE));
	ace->mask = const_cpu_to_le32(0x1f01ff); /* FIXME */
	ace->sid.revision = SID_REVISION;
	ace->sid.sub_authority_count = 1;
	ace->sid.sub_authority[0] = const_cpu_to_le32(0);
	ace->sid.identifier_authority.value[5] = 1;

	ret = ntfs_attr_add(ni, AT_SECURITY_DESCRIPTOR, AT_UNNAMED, 0, (u8*)sd,
			    sd_len);
	if (ret)
		ntfs_log_perror("Failed to add initial SECURITY_DESCRIPTOR");
	
	free(sd);
	return ret;
}

/*
 *		Check whether user can access a file in a specific way
 *
 *	Returns 1 if access is allowed, including user is root or no
 *		  user mapping defined
 *		2 if sticky and accesstype is S_IWRITE + S_IEXEC + S_ISVTX
 *		0 and sets errno if there is a problem or if access
 *		  is not allowed
 *
 *	This is used for Posix ACL and checking creation of DOS file names
 */

int ntfs_allowed_access(struct SECURITY_CONTEXT *scx,
		ntfs_inode *ni,
		int accesstype) /* access type required (S_Ixxx values) */
{
	int perm;
	int res;
	int allow;
	struct stat stbuf;

	/*
	 * Always allow for root unless execution is requested.
	 * (was checked by fuse until kernel 2.6.29)
	 * Also always allow if no mapping has been defined
	 */
	if (!scx->mapping[MAPUSERS]
	    || (!scx->uid
		&& (!(accesstype & S_IEXEC)
		    || (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY))))
		allow = 1;
	else {
		perm = ntfs_get_perm(scx, ni, accesstype);
		if (perm >= 0) {
			res = EACCES;
			switch (accesstype) {
			case S_IEXEC:
				allow = (perm & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
				break;
			case S_IWRITE:
				allow = (perm & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0;
				break;
			case S_IWRITE + S_IEXEC:
				allow = ((perm & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0)
				    && ((perm & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0);
				break;
			case S_IREAD:
				allow = (perm & (S_IRUSR | S_IRGRP | S_IROTH)) != 0;
				break;
			case S_IREAD + S_IEXEC:
				allow = ((perm & (S_IRUSR | S_IRGRP | S_IROTH)) != 0)
				    && ((perm & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0);
				break;
			case S_IREAD + S_IWRITE:
				allow = ((perm & (S_IRUSR | S_IRGRP | S_IROTH)) != 0)
				    && ((perm & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0);
				break;
			case S_IWRITE + S_IEXEC + S_ISVTX:
				if (perm & S_ISVTX) {
					if ((ntfs_get_owner_mode(scx,ni,&stbuf) >= 0)
					    && (stbuf.st_uid == scx->uid))
						allow = 1;
					else
						allow = 2;
				} else
					allow = ((perm & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0)
					    && ((perm & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0);
				break;
			case S_IREAD + S_IWRITE + S_IEXEC:
				allow = ((perm & (S_IRUSR | S_IRGRP | S_IROTH)) != 0)
				    && ((perm & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0)
				    && ((perm & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0);
				break;
			default :
				res = EINVAL;
				allow = 0;
				break;
			}
			if (!allow)
				errno = res;
		} else
			allow = 0;
	}
	return (allow);
}

#if 0 /* not needed any more */

/*
 *		Check whether user can access the parent directory
 *	of a file in a specific way
 *
 *	Returns true if access is allowed, including user is root and
 *		no user mapping defined
 *	
 *	Sets errno if there is a problem or if not allowed
 *
 *	This is used for Posix ACL and checking creation of DOS file names
 */

BOOL old_ntfs_allowed_dir_access(struct SECURITY_CONTEXT *scx,
		const char *path, int accesstype)
{
	int allow;
	char *dirpath;
	char *name;
	ntfs_inode *ni;
	ntfs_inode *dir_ni;
	struct stat stbuf;

	allow = 0;
	dirpath = strdup(path);
	if (dirpath) {
		/* the root of file system is seen as a parent of itself */
		/* is that correct ? */
		name = strrchr(dirpath, '/');
		*name = 0;
		dir_ni = ntfs_pathname_to_inode(scx->vol, NULL, dirpath);
		if (dir_ni) {
			allow = ntfs_allowed_access(scx,
				 dir_ni, accesstype);
			ntfs_inode_close(dir_ni);
				/*
				 * for an not-owned sticky directory, have to
				 * check whether file itself is owned
				 */
			if ((accesstype == (S_IWRITE + S_IEXEC + S_ISVTX))
			   && (allow == 2)) {
				ni = ntfs_pathname_to_inode(scx->vol, NULL,
					 path);
				allow = FALSE;
				if (ni) {
					allow = (ntfs_get_owner_mode(scx,ni,&stbuf) >= 0)
						&& (stbuf.st_uid == scx->uid);
				ntfs_inode_close(ni);
				}
			}
		}
		free(dirpath);
	}
	return (allow);		/* errno is set if not allowed */
}

#endif

/*
 *		Define a new owner/group to a file
 *
 *	returns zero if successful
 */

int ntfs_set_owner(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			uid_t uid, gid_t gid)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const struct CACHED_PERMISSIONS *cached;
	char *oldattr;
	const SID *usid;
	const SID *gsid;
	uid_t fileuid;
	uid_t filegid;
	mode_t mode;
	int perm;
	BOOL isdir;
	int res;
#if POSIXACLS
	struct POSIX_SECURITY *pxdesc;
	BOOL pxdescbuilt = FALSE;
#endif

	res = 0;
	/* get the current owner and mode from cache or security attributes */
	oldattr = (char*)NULL;
	cached = fetch_cache(scx,ni);
	if (cached) {
		fileuid = cached->uid;
		filegid = cached->gid;
		mode = cached->mode;
#if POSIXACLS
		pxdesc = cached->pxdesc;
		if (!pxdesc)
			res = -1;
#endif
	} else {
		fileuid = 0;
		filegid = 0;
		mode = 0;
		oldattr = getsecurityattr(scx->vol, ni);
		if (oldattr) {
			isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
				!= const_cpu_to_le16(0);
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)
				oldattr;
			gsid = (const SID*)
				&oldattr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
			usid = ntfs_acl_owner(oldattr);
#else
			usid = (const SID*)
				&oldattr[le32_to_cpu(phead->owner)];
#endif
#if POSIXACLS
			pxdesc = ntfs_build_permissions_posix(scx->mapping, oldattr,
					usid, gsid, isdir);
			if (pxdesc) {
				pxdescbuilt = TRUE;
				fileuid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
				filegid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
				mode = perm = pxdesc->mode;
			} else
				res = -1;
#else
			mode = perm = ntfs_build_permissions(oldattr,
					 usid, gsid, isdir);
			if (perm >= 0) {
				fileuid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
				filegid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
			} else
				res = -1;
#endif
			free(oldattr);
		} else
			res = -1;
	}
	if (!res) {
		/* check requested by root */
		/* or chgrp requested by owner to an owned group */
		if (!scx->uid
		   || ((((int)uid < 0) || (uid == fileuid))
		      && ((gid == scx->gid) || groupmember(scx, scx->uid, gid))
		      && (fileuid == scx->uid))) {
			/* replace by the new usid and gsid */
			/* or reuse old gid and sid for cacheing */
			if ((int)uid < 0)
				uid = fileuid;
			if ((int)gid < 0)
				gid = filegid;
			/* clear setuid and setgid if owner has changed */
                        /* unless request originated by root */
			if (uid && (fileuid != uid))
				mode &= 01777;
#if POSIXACLS
			res = ntfs_set_owner_mode(scx, ni, uid, gid, 
				mode, pxdesc);
#else
			res = ntfs_set_owner_mode(scx, ni, uid, gid, mode);
#endif
		} else {
			res = -1;	/* neither owner nor root */
			errno = EPERM;
		}
#if POSIXACLS
		if (pxdescbuilt)
			free(pxdesc);
#endif
	} else {
		/*
		 * Should not happen : a default descriptor is generated
		 * by getsecurityattr() when there are none
		 */
		ntfs_log_error("File has no security descriptor\n");
		res = -1;
		errno = EIO;
	}
	return (res ? -1 : 0);
}

/*
 *		Define new owner/group and mode to a file
 *
 *	returns zero if successful
 */

int ntfs_set_ownmod(struct SECURITY_CONTEXT *scx, ntfs_inode *ni,
			uid_t uid, gid_t gid, const mode_t mode)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	const struct CACHED_PERMISSIONS *cached;
	char *oldattr;
	const SID *usid;
	const SID *gsid;
	uid_t fileuid;
	uid_t filegid;
	BOOL isdir;
	int res;
#if POSIXACLS
	const struct POSIX_SECURITY *oldpxdesc;
	struct POSIX_SECURITY *newpxdesc = (struct POSIX_SECURITY*)NULL;
	int pxsize;
#endif

	res = 0;
	/* get the current owner and mode from cache or security attributes */
	oldattr = (char*)NULL;
	cached = fetch_cache(scx,ni);
	if (cached) {
		fileuid = cached->uid;
		filegid = cached->gid;
#if POSIXACLS
		oldpxdesc = cached->pxdesc;
		if (oldpxdesc) {
				/* must copy before merging */
			pxsize = sizeof(struct POSIX_SECURITY)
				+ (oldpxdesc->acccnt + oldpxdesc->defcnt)*sizeof(struct POSIX_ACE);
			newpxdesc = (struct POSIX_SECURITY*)malloc(pxsize);
			if (newpxdesc) {
				memcpy(newpxdesc, oldpxdesc, pxsize);
				if (ntfs_merge_mode_posix(newpxdesc, mode))
					res = -1;
			} else
				res = -1;
		}
#endif
	} else {
		fileuid = 0;
		filegid = 0;
		oldattr = getsecurityattr(scx->vol, ni);
		if (oldattr) {
			isdir = (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
				!= const_cpu_to_le16(0);
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)
				oldattr;
			gsid = (const SID*)
				&oldattr[le32_to_cpu(phead->group)];
#if OWNERFROMACL
			usid = ntfs_acl_owner(oldattr);
#else
			usid = (const SID*)
				&oldattr[le32_to_cpu(phead->owner)];
#endif
#if POSIXACLS
			newpxdesc = ntfs_build_permissions_posix(scx->mapping, oldattr,
					usid, gsid, isdir);
			if (!newpxdesc || ntfs_merge_mode_posix(newpxdesc, mode))
				res = -1;
			else {
				fileuid = ntfs_find_user(scx->mapping[MAPUSERS],usid);
				filegid = ntfs_find_group(scx->mapping[MAPGROUPS],gsid);
			}
#endif
			free(oldattr);
		} else
			res = -1;
	}
	if (!res) {
		/* check requested by root */
		/* or chgrp requested by owner to an owned group */
		if (!scx->uid
		   || ((((int)uid < 0) || (uid == fileuid))
		      && ((gid == scx->gid) || groupmember(scx, scx->uid, gid))
		      && (fileuid == scx->uid))) {
			/* replace by the new usid and gsid */
			/* or reuse old gid and sid for cacheing */
			if ((int)uid < 0)
				uid = fileuid;
			if ((int)gid < 0)
				gid = filegid;
#if POSIXACLS
			res = ntfs_set_owner_mode(scx, ni, uid, gid, 
				mode, newpxdesc);
#else
			res = ntfs_set_owner_mode(scx, ni, uid, gid, mode);
#endif
		} else {
			res = -1;	/* neither owner nor root */
			errno = EPERM;
		}
	} else {
		/*
		 * Should not happen : a default descriptor is generated
		 * by getsecurityattr() when there are none
		 */
		ntfs_log_error("File has no security descriptor\n");
		res = -1;
		errno = EIO;
	}
#if POSIXACLS
	free(newpxdesc);
#endif
	return (res ? -1 : 0);
}

/*
 *		Build a security id for a descriptor inherited from
 *	parent directory the Windows way
 */

static le32 build_inherited_id(struct SECURITY_CONTEXT *scx,
			const char *parentattr, BOOL fordir)
{
	const SECURITY_DESCRIPTOR_RELATIVE *pphead;
	const ACL *ppacl;
	const SID *usid;
	const SID *gsid;
	BIGSID defusid;
	BIGSID defgsid;
	int offpacl;
	int offowner;
	int offgroup;
	SECURITY_DESCRIPTOR_RELATIVE *pnhead;
	ACL *pnacl;
	int parentattrsz;
	char *newattr;
	int newattrsz;
	int aclsz;
	int usidsz;
	int gsidsz;
	int pos;
	le32 securid;

	parentattrsz = ntfs_attr_size(parentattr);
	pphead = (const SECURITY_DESCRIPTOR_RELATIVE*)parentattr;
	if (scx->mapping[MAPUSERS]) {
		usid = ntfs_find_usid(scx->mapping[MAPUSERS], scx->uid, (SID*)&defusid);
		gsid = ntfs_find_gsid(scx->mapping[MAPGROUPS], scx->gid, (SID*)&defgsid);
		if (!usid)
			usid = adminsid;
		if (!gsid)
			gsid = adminsid;
	} else {
		/*
		 * If there is no user mapping, we have to copy owner
		 * and group from parent directory.
		 * Windows never has to do that, because it can always
		 * rely on a user mapping
		 */
		offowner = le32_to_cpu(pphead->owner);
		usid = (const SID*)&parentattr[offowner];
		offgroup = le32_to_cpu(pphead->group);
		gsid = (const SID*)&parentattr[offgroup];
	}
		/*
		 * new attribute is smaller than parent's
		 * except for differences in SIDs which appear in
		 * owner, group and possible grants and denials in
		 * generic creator-owner and creator-group ACEs.
		 * For directories, an ACE may be duplicated for
		 * access and inheritance, so we double the count.
		 */
	usidsz = ntfs_sid_size(usid);
	gsidsz = ntfs_sid_size(gsid);
	newattrsz = parentattrsz + 3*usidsz + 3*gsidsz;
	if (fordir)
		newattrsz *= 2;
	newattr = (char*)ntfs_malloc(newattrsz);
	if (newattr) {
		pnhead = (SECURITY_DESCRIPTOR_RELATIVE*)newattr;
		pnhead->revision = SECURITY_DESCRIPTOR_REVISION;
		pnhead->alignment = 0;
		pnhead->control = SE_SELF_RELATIVE;
		pos = sizeof(SECURITY_DESCRIPTOR_RELATIVE);
			/*
			 * locate and inherit DACL
			 * do not test SE_DACL_PRESENT (wrong for "DR Watson")
			 */
		pnhead->dacl = const_cpu_to_le32(0);
		if (pphead->dacl) {
			offpacl = le32_to_cpu(pphead->dacl);
			ppacl = (const ACL*)&parentattr[offpacl];
			pnacl = (ACL*)&newattr[pos];
			aclsz = ntfs_inherit_acl(ppacl, pnacl, usid, gsid, fordir);
			if (aclsz) {
				pnhead->dacl = cpu_to_le32(pos);
				pos += aclsz;
				pnhead->control |= SE_DACL_PRESENT;
			}
		}
			/*
			 * locate and inherit SACL
			 */
		pnhead->sacl = const_cpu_to_le32(0);
		if (pphead->sacl) {
			offpacl = le32_to_cpu(pphead->sacl);
			ppacl = (const ACL*)&parentattr[offpacl];
			pnacl = (ACL*)&newattr[pos];
			aclsz = ntfs_inherit_acl(ppacl, pnacl, usid, gsid, fordir);
			if (aclsz) {
				pnhead->sacl = cpu_to_le32(pos);
				pos += aclsz;
				pnhead->control |= SE_SACL_PRESENT;
			}
		}
			/*
			 * inherit or redefine owner
			 */
		memcpy(&newattr[pos],usid,usidsz);
		pnhead->owner = cpu_to_le32(pos);
		pos += usidsz;
			/*
			 * inherit or redefine group
			 */
		memcpy(&newattr[pos],gsid,gsidsz);
		pnhead->group = cpu_to_le32(pos);
		pos += usidsz;
		securid = setsecurityattr(scx->vol,
			(SECURITY_DESCRIPTOR_RELATIVE*)newattr, pos);
		free(newattr);
	} else
		securid = const_cpu_to_le32(0);
	return (securid);
}

/*
 *		Get an inherited security id
 *
 *	For Windows compatibility, the normal initial permission setting
 *	may be inherited from the parent directory instead of being
 *	defined by the creation arguments.
 *
 *	The following creates an inherited id for that purpose.
 *
 *	Note : the owner and group of parent directory are also
 *	inherited (which is not the case on Windows) if no user mapping
 *	is defined.
 *
 *	Returns the inherited id, or zero if not possible (eg on NTFS 1.x)
 */

le32 ntfs_inherited_id(struct SECURITY_CONTEXT *scx,
			ntfs_inode *dir_ni, BOOL fordir)
{
	struct CACHED_PERMISSIONS *cached;
	char *parentattr;
	le32 securid;

	securid = const_cpu_to_le32(0);
	cached = (struct CACHED_PERMISSIONS*)NULL;
		/*
		 * Try to get inherited id from cache
		 */
	if (test_nino_flag(dir_ni, v3_Extensions)
			&& dir_ni->security_id) {
		cached = fetch_cache(scx, dir_ni);
		if (cached)
			securid = (fordir ? cached->inh_dirid
					: cached->inh_fileid);
	}
		/*
		 * Not cached or not available in cache, compute it all
		 * Note : if parent directory has no id, it is not cacheable
		 */
	if (!securid) {
		parentattr = getsecurityattr(scx->vol, dir_ni);
		if (parentattr) {
			securid = build_inherited_id(scx,
						parentattr, fordir);
			free(parentattr);
			/*
			 * Store the result into cache for further use
			 */
			if (securid) {
				cached = fetch_cache(scx, dir_ni);
				if (cached) {
					if (fordir)
						cached->inh_dirid = securid;
					else
						cached->inh_fileid = securid;
				}
			}
		}
	}
	return (securid);
}

/*
 *		Link a group to a member of group
 *
 *	Returns 0 if OK, -1 (and errno set) if error
 */

static int link_single_group(struct MAPPING *usermapping, struct passwd *user,
			gid_t gid)
{
	struct group *group;
	char **grmem;
	int grcnt;
	gid_t *groups;
	int res;

	res = 0;
	group = getgrgid(gid);
	if (group && group->gr_mem) {
		grcnt = usermapping->grcnt;
		groups = usermapping->groups;
		grmem = group->gr_mem;
		while (*grmem && strcmp(user->pw_name, *grmem))
			grmem++;
		if (*grmem) {
			if (!grcnt)
				groups = (gid_t*)malloc(sizeof(gid_t));
			else
				groups = (gid_t*)realloc(groups,
					(grcnt+1)*sizeof(gid_t));
			if (groups)
				groups[grcnt++]	= gid;
			else {
				res = -1;
				errno = ENOMEM;
			}
		}
		usermapping->grcnt = grcnt;
		usermapping->groups = groups;
	}
	return (res);
}


/*
 *		Statically link group to users
 *	This is based on groups defined in /etc/group and does not take
 *	the groups dynamically set by setgroups() nor any changes in
 *	/etc/group into account
 *
 *	Only mapped groups and root group are linked to mapped users
 *
 *	Returns 0 if OK, -1 (and errno set) if error
 *
 */

static int link_group_members(struct SECURITY_CONTEXT *scx)
{
	struct MAPPING *usermapping;
	struct MAPPING *groupmapping;
	struct passwd *user;
	int res;

	res = 0;
	for (usermapping=scx->mapping[MAPUSERS]; usermapping && !res;
			usermapping=usermapping->next) {
		usermapping->grcnt = 0;
		usermapping->groups = (gid_t*)NULL;
		user = getpwuid(usermapping->xid);
		if (user && user->pw_name) {
			for (groupmapping=scx->mapping[MAPGROUPS];
					groupmapping && !res;
					groupmapping=groupmapping->next) {
				if (link_single_group(usermapping, user,
				    groupmapping->xid))
					res = -1;
				}
			if (!res && link_single_group(usermapping,
					 user, (gid_t)0))
				res = -1;
		}
	}
	return (res);
}

/*
 *		Apply default single user mapping
 *	returns zero if successful
 */

static int ntfs_do_default_mapping(struct SECURITY_CONTEXT *scx,
			 uid_t uid, gid_t gid, const SID *usid)
{
	struct MAPPING *usermapping;
	struct MAPPING *groupmapping;
	SID *sid;
	int sidsz;
	int res;

	res = -1;
	sidsz = ntfs_sid_size(usid);
	sid = (SID*)ntfs_malloc(sidsz);
	if (sid) {
		memcpy(sid,usid,sidsz);
		usermapping = (struct MAPPING*)ntfs_malloc(sizeof(struct MAPPING));
		if (usermapping) {
			groupmapping = (struct MAPPING*)ntfs_malloc(sizeof(struct MAPPING));
			if (groupmapping) {
				usermapping->sid = sid;
				usermapping->xid = uid;
				usermapping->next = (struct MAPPING*)NULL;
				groupmapping->sid = sid;
				groupmapping->xid = gid;
				groupmapping->next = (struct MAPPING*)NULL;
				scx->mapping[MAPUSERS] = usermapping;
				scx->mapping[MAPGROUPS] = groupmapping;
				res = 0;
			}
		}
	}
	return (res);
}

/*
 *		Make sure there are no ambiguous mapping
 *	Ambiguous mapping may lead to undesired configurations and
 *	we had rather be safe until the consequences are understood
 */

#if 0 /* not activated for now */

static BOOL check_mapping(const struct MAPPING *usermapping,
		const struct MAPPING *groupmapping)
{
	const struct MAPPING *mapping1;
	const struct MAPPING *mapping2;
	BOOL ambiguous;

	ambiguous = FALSE;
	for (mapping1=usermapping; mapping1; mapping1=mapping1->next)
		for (mapping2=mapping1->next; mapping2; mapping1=mapping2->next)
			if (ntfs_same_sid(mapping1->sid,mapping2->sid)) {
				if (mapping1->xid != mapping2->xid)
					ambiguous = TRUE;
			} else {
				if (mapping1->xid == mapping2->xid)
					ambiguous = TRUE;
			}
	for (mapping1=groupmapping; mapping1; mapping1=mapping1->next)
		for (mapping2=mapping1->next; mapping2; mapping1=mapping2->next)
			if (ntfs_same_sid(mapping1->sid,mapping2->sid)) {
				if (mapping1->xid != mapping2->xid)
					ambiguous = TRUE;
			} else {
				if (mapping1->xid == mapping2->xid)
					ambiguous = TRUE;
			}
	return (ambiguous);
}

#endif

#if 0 /* not used any more */

/*
 *		Try and apply default single user mapping
 *	returns zero if successful
 */

static int ntfs_default_mapping(struct SECURITY_CONTEXT *scx)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	ntfs_inode *ni;
	char *securattr;
	const SID *usid;
	int res;

	res = -1;
	ni = ntfs_pathname_to_inode(scx->vol, NULL, "/.");
	if (ni) {
		securattr = getsecurityattr(scx->vol, ni);
		if (securattr) {
			phead = (const SECURITY_DESCRIPTOR_RELATIVE*)securattr;
			usid = (SID*)&securattr[le32_to_cpu(phead->owner)];
			if (ntfs_is_user_sid(usid))
				res = ntfs_do_default_mapping(scx,
						scx->uid, scx->gid, usid);
			free(securattr);
		}
		ntfs_inode_close(ni);
	}
	return (res);
}

#endif

/*
 *		Basic read from a user mapping file on another volume
 */

static int basicread(void *fileid, char *buf, size_t size, off_t offs __attribute__((unused)))
{
	return (read(*(int*)fileid, buf, size));
}


/*
 *		Read from a user mapping file on current NTFS partition
 */

static int localread(void *fileid, char *buf, size_t size, off_t offs)
{
	return (ntfs_local_read((ntfs_inode*)fileid,
			AT_UNNAMED, 0, buf, size, offs));
}

/*
 *		Build the user mapping
 *	- according to a mapping file if defined (or default present),
 *	- or try default single user mapping if possible
 *
 *	The mapping is specific to a mounted device
 *	No locking done, mounting assumed non multithreaded
 *
 *	returns zero if mapping is successful
 *	(failure should not be interpreted as an error)
 */

int ntfs_build_mapping(struct SECURITY_CONTEXT *scx, const char *usermap_path,
			BOOL allowdef)
{
	struct MAPLIST *item;
	struct MAPLIST *firstitem;
	struct MAPPING *usermapping;
	struct MAPPING *groupmapping;
	ntfs_inode *ni;
	int fd;
	static struct {
		u8 revision;
		u8 levels;
		be16 highbase;
		be32 lowbase;
		le32 level1;
		le32 level2;
		le32 level3;
		le32 level4;
		le32 level5;
	} defmap = {
		1, 5, const_cpu_to_be16(0), const_cpu_to_be32(5),
		const_cpu_to_le32(21),
		const_cpu_to_le32(DEFSECAUTH1), const_cpu_to_le32(DEFSECAUTH2),
		const_cpu_to_le32(DEFSECAUTH3), const_cpu_to_le32(DEFSECBASE)
	} ;

	/* be sure not to map anything until done */
	scx->mapping[MAPUSERS] = (struct MAPPING*)NULL;
	scx->mapping[MAPGROUPS] = (struct MAPPING*)NULL;

	if (!usermap_path) usermap_path = MAPPINGFILE;
	if (usermap_path[0] == '/') {
		fd = open(usermap_path,O_RDONLY);
		if (fd > 0) {
			firstitem = ntfs_read_mapping(basicread, (void*)&fd);
			close(fd);
		} else
			firstitem = (struct MAPLIST*)NULL;
	} else {
		ni = ntfs_pathname_to_inode(scx->vol, NULL, usermap_path);
		if (ni) {
			firstitem = ntfs_read_mapping(localread, ni);
			ntfs_inode_close(ni);
		} else
			firstitem = (struct MAPLIST*)NULL;
	}


	if (firstitem) {
		usermapping = ntfs_do_user_mapping(firstitem);
		groupmapping = ntfs_do_group_mapping(firstitem);
		if (usermapping && groupmapping) {
			scx->mapping[MAPUSERS] = usermapping;
			scx->mapping[MAPGROUPS] = groupmapping;
		} else
			ntfs_log_error("There were no valid user or no valid group\n");
		/* now we can free the memory copy of input text */
		/* and rely on internal representation */
		while (firstitem) {
			item = firstitem->next;
			free(firstitem);
			firstitem = item;
		}
	} else {
			/* no mapping file, try a default mapping */
		if (allowdef) {
			if (!ntfs_do_default_mapping(scx,
					0, 0, (const SID*)&defmap))
				ntfs_log_info("Using default user mapping\n");
		}
	}
	return (!scx->mapping[MAPUSERS] || link_group_members(scx));
}

#ifdef HAVE_SETXATTR    /* extended attributes interface required */

/*
 *		Get the ntfs attribute into an extended attribute
 *	The attribute is returned according to cpu endianness
 */

int ntfs_get_ntfs_attrib(ntfs_inode *ni, char *value, size_t size)
{
	u32 attrib;
	size_t outsize;

	outsize = 0;	/* default to no data and no error */
	if (ni) {
		attrib = le32_to_cpu(ni->flags);
		if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
			attrib |= const_le32_to_cpu(FILE_ATTR_DIRECTORY);
		else
			attrib &= ~const_le32_to_cpu(FILE_ATTR_DIRECTORY);
		if (!attrib)
			attrib |= const_le32_to_cpu(FILE_ATTR_NORMAL);
		outsize = sizeof(FILE_ATTR_FLAGS);
		if (size >= outsize) {
			if (value)
				memcpy(value,&attrib,outsize);
			else
				errno = EINVAL;
		}
	}
	return (outsize ? (int)outsize : -errno);
}

/*
 *		Return the ntfs attribute into an extended attribute
 *	The attribute is expected according to cpu endianness
 *
 *	Returns 0, or -1 if there is a problem
 */

int ntfs_set_ntfs_attrib(ntfs_inode *ni,
			const char *value, size_t size,	int flags)
{
	u32 attrib;
	le32 settable;
	ATTR_FLAGS dirflags;
	int res;

	res = -1;
	if (ni && value && (size >= sizeof(FILE_ATTR_FLAGS))) {
		if (!(flags & XATTR_CREATE)) {
			/* copy to avoid alignment problems */
			memcpy(&attrib,value,sizeof(FILE_ATTR_FLAGS));
			settable = FILE_ATTR_SETTABLE;
			res = 0;
			if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
				/*
				 * Accept changing compression for a directory
				 * and set index root accordingly
				 */
				settable |= FILE_ATTR_COMPRESSED;
				if ((ni->flags ^ cpu_to_le32(attrib))
				             & FILE_ATTR_COMPRESSED) {
					if (ni->flags & FILE_ATTR_COMPRESSED)
						dirflags = const_cpu_to_le16(0);
					else
						dirflags = ATTR_IS_COMPRESSED;
					res = ntfs_attr_set_flags(ni,
						AT_INDEX_ROOT,
					        NTFS_INDEX_I30, 4,
						dirflags,
						ATTR_COMPRESSION_MASK);
				}
			}
			if (!res) {
				ni->flags = (ni->flags & ~settable)
					 | (cpu_to_le32(attrib) & settable);
				NInoFileNameSetDirty(ni);
				NInoSetDirty(ni);
			}
		} else
			errno = EEXIST;
	} else
		errno = EINVAL;
	return (res ? -1 : 0);
}

#endif /* HAVE_SETXATTR */

/*
 *	Open $Secure once for all
 *	returns zero if it succeeds
 *		non-zero if it fails. This is not an error (on NTFS v1.x)
 */


int ntfs_open_secure(ntfs_volume *vol)
{
	ntfs_inode *ni;
	int res;

	res = -1;
	vol->secure_ni = (ntfs_inode*)NULL;
	vol->secure_xsii = (ntfs_index_context*)NULL;
	vol->secure_xsdh = (ntfs_index_context*)NULL;
	if (vol->major_ver >= 3) {
			/* make sure this is a genuine $Secure inode 9 */
		ni = ntfs_pathname_to_inode(vol, NULL, "$Secure");
		if (ni && (ni->mft_no == 9)) {
			vol->secure_reentry = 0;
			vol->secure_xsii = ntfs_index_ctx_get(ni,
						sii_stream, 4);
			vol->secure_xsdh = ntfs_index_ctx_get(ni,
						sdh_stream, 4);
			if (ni && vol->secure_xsii && vol->secure_xsdh) {
				vol->secure_ni = ni;
				res = 0;
			}
		}
	}
	return (res);
}

/*
 *		Final cleaning
 *	Allocated memory is freed to facilitate the detection of memory leaks
 */

void ntfs_close_secure(struct SECURITY_CONTEXT *scx)
{
	ntfs_volume *vol;

	vol = scx->vol;
	if (vol->secure_ni) {
		ntfs_index_ctx_put(vol->secure_xsii);
		ntfs_index_ctx_put(vol->secure_xsdh);
		ntfs_inode_close(vol->secure_ni);
		
	}
	ntfs_free_mapping(scx->mapping);
	free_caches(scx);
}

/*
 *		API for direct access to security descriptors
 *	based on Win32 API
 */


/*
 *		Selective feeding of a security descriptor into user buffer
 *
 *	Returns TRUE if successful
 */

static BOOL feedsecurityattr(const char *attr, u32 selection,
		char *buf, u32 buflen, u32 *psize)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	SECURITY_DESCRIPTOR_RELATIVE *pnhead;
	const ACL *pdacl;
	const ACL *psacl;
	const SID *pusid;
	const SID *pgsid;
	unsigned int offdacl;
	unsigned int offsacl;
	unsigned int offowner;
	unsigned int offgroup;
	unsigned int daclsz;
	unsigned int saclsz;
	unsigned int usidsz;
	unsigned int gsidsz;
	unsigned int size; /* size of requested attributes */
	BOOL ok;
	unsigned int pos;
	unsigned int avail;
	le16 control;

	avail = 0;
	control = SE_SELF_RELATIVE;
	phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
	size = sizeof(SECURITY_DESCRIPTOR_RELATIVE);

		/* locate DACL if requested and available */
	if (phead->dacl && (selection & DACL_SECURITY_INFORMATION)) {
		offdacl = le32_to_cpu(phead->dacl);
		pdacl = (const ACL*)&attr[offdacl];
		daclsz = le16_to_cpu(pdacl->size);
		size += daclsz;
		avail |= DACL_SECURITY_INFORMATION;
	} else
		offdacl = daclsz = 0;

		/* locate owner if requested and available */
	offowner = le32_to_cpu(phead->owner);
	if (offowner && (selection & OWNER_SECURITY_INFORMATION)) {
			/* find end of USID */
		pusid = (const SID*)&attr[offowner];
		usidsz = ntfs_sid_size(pusid);
		size += usidsz;
		avail |= OWNER_SECURITY_INFORMATION;
	} else
		offowner = usidsz = 0;

		/* locate group if requested and available */
	offgroup = le32_to_cpu(phead->group);
	if (offgroup && (selection & GROUP_SECURITY_INFORMATION)) {
			/* find end of GSID */
		pgsid = (const SID*)&attr[offgroup];
		gsidsz = ntfs_sid_size(pgsid);
		size += gsidsz;
		avail |= GROUP_SECURITY_INFORMATION;
	} else
		offgroup = gsidsz = 0;

		/* locate SACL if requested and available */
	if (phead->sacl && (selection & SACL_SECURITY_INFORMATION)) {
			/* find end of SACL */
		offsacl = le32_to_cpu(phead->sacl);
		psacl = (const ACL*)&attr[offsacl];
		saclsz = le16_to_cpu(psacl->size);
		size += saclsz;
		avail |= SACL_SECURITY_INFORMATION;
	} else
		offsacl = saclsz = 0;

		/*
		 * Check having enough size in destination buffer
		 * (required size is returned nevertheless so that
		 * the request can be reissued with adequate size)
		 */
	if (size > buflen) {
		*psize = size;
		errno = EINVAL;
		ok = FALSE;
	} else {
		if (selection & OWNER_SECURITY_INFORMATION)
			control |= phead->control & SE_OWNER_DEFAULTED;
		if (selection & GROUP_SECURITY_INFORMATION)
			control |= phead->control & SE_GROUP_DEFAULTED;
		if (selection & DACL_SECURITY_INFORMATION)
			control |= phead->control
					& (SE_DACL_PRESENT
					   | SE_DACL_DEFAULTED
					   | SE_DACL_AUTO_INHERITED
					   | SE_DACL_PROTECTED);
		if (selection & SACL_SECURITY_INFORMATION)
			control |= phead->control
					& (SE_SACL_PRESENT
					   | SE_SACL_DEFAULTED
					   | SE_SACL_AUTO_INHERITED
					   | SE_SACL_PROTECTED);
		/*
		 * copy header and feed new flags, even if no detailed data
		 */
		memcpy(buf,attr,sizeof(SECURITY_DESCRIPTOR_RELATIVE));
		pnhead = (SECURITY_DESCRIPTOR_RELATIVE*)buf;
		pnhead->control = control;
		pos = sizeof(SECURITY_DESCRIPTOR_RELATIVE);

		/* copy DACL if requested and available */
		if (selection & avail & DACL_SECURITY_INFORMATION) {
			pnhead->dacl = cpu_to_le32(pos);
			memcpy(&buf[pos],&attr[offdacl],daclsz);
			pos += daclsz;
		} else
			pnhead->dacl = const_cpu_to_le32(0);

		/* copy SACL if requested and available */
		if (selection & avail & SACL_SECURITY_INFORMATION) {
			pnhead->sacl = cpu_to_le32(pos);
			memcpy(&buf[pos],&attr[offsacl],saclsz);
			pos += saclsz;
		} else
			pnhead->sacl = const_cpu_to_le32(0);

		/* copy owner if requested and available */
		if (selection & avail & OWNER_SECURITY_INFORMATION) {
			pnhead->owner = cpu_to_le32(pos);
			memcpy(&buf[pos],&attr[offowner],usidsz);
			pos += usidsz;
		} else
			pnhead->owner = const_cpu_to_le32(0);

		/* copy group if requested and available */
		if (selection & avail & GROUP_SECURITY_INFORMATION) {
			pnhead->group = cpu_to_le32(pos);
			memcpy(&buf[pos],&attr[offgroup],gsidsz);
			pos += gsidsz;
		} else
			pnhead->group = const_cpu_to_le32(0);
		if (pos != size)
			ntfs_log_error("Error in security descriptor size\n");
		*psize = size;
		ok = TRUE;
	}

	return (ok);
}

/*
 *		Merge a new security descriptor into the old one
 *	and assign to designated file
 *
 *	Returns TRUE if successful
 */

static BOOL mergesecurityattr(ntfs_volume *vol, const char *oldattr,
		const char *newattr, u32 selection, ntfs_inode *ni)
{
	const SECURITY_DESCRIPTOR_RELATIVE *oldhead;
	const SECURITY_DESCRIPTOR_RELATIVE *newhead;
	SECURITY_DESCRIPTOR_RELATIVE *targhead;
	const ACL *pdacl;
	const ACL *psacl;
	const SID *powner;
	const SID *pgroup;
	int offdacl;
	int offsacl;
	int offowner;
	int offgroup;
	unsigned int size;
	le16 control;
	char *target;
	int pos;
	int oldattrsz;
	int newattrsz;
	BOOL ok;

	ok = FALSE; /* default return */
	oldhead = (const SECURITY_DESCRIPTOR_RELATIVE*)oldattr;
	newhead = (const SECURITY_DESCRIPTOR_RELATIVE*)newattr;
	oldattrsz = ntfs_attr_size(oldattr);
	newattrsz = ntfs_attr_size(newattr);
	target = (char*)ntfs_malloc(oldattrsz + newattrsz);
	if (target) {
		targhead = (SECURITY_DESCRIPTOR_RELATIVE*)target;
		pos = sizeof(SECURITY_DESCRIPTOR_RELATIVE);
		control = SE_SELF_RELATIVE;
			/*
			 * copy new DACL if selected
			 * or keep old DACL if any
			 */
		if ((selection & DACL_SECURITY_INFORMATION) ?
				newhead->dacl : oldhead->dacl) {
			if (selection & DACL_SECURITY_INFORMATION) {
				offdacl = le32_to_cpu(newhead->dacl);
				pdacl = (const ACL*)&newattr[offdacl];
			} else {
				offdacl = le32_to_cpu(oldhead->dacl);
				pdacl = (const ACL*)&oldattr[offdacl];
			}
			size = le16_to_cpu(pdacl->size);
			memcpy(&target[pos], pdacl, size);
			targhead->dacl = cpu_to_le32(pos);
			pos += size;
		} else
			targhead->dacl = const_cpu_to_le32(0);
		if (selection & DACL_SECURITY_INFORMATION) {
			control |= newhead->control
					& (SE_DACL_PRESENT
					   | SE_DACL_DEFAULTED
					   | SE_DACL_PROTECTED);
			if (newhead->control & SE_DACL_AUTO_INHERIT_REQ)
				control |= SE_DACL_AUTO_INHERITED;
		} else
			control |= oldhead->control
					& (SE_DACL_PRESENT
					   | SE_DACL_DEFAULTED
					   | SE_DACL_AUTO_INHERITED
					   | SE_DACL_PROTECTED);
			/*
			 * copy new SACL if selected
			 * or keep old SACL if any
			 */
		if ((selection & SACL_SECURITY_INFORMATION) ?
				newhead->sacl : oldhead->sacl) {
			if (selection & SACL_SECURITY_INFORMATION) {
				offsacl = le32_to_cpu(newhead->sacl);
				psacl = (const ACL*)&newattr[offsacl];
			} else {
				offsacl = le32_to_cpu(oldhead->sacl);
				psacl = (const ACL*)&oldattr[offsacl];
			}
			size = le16_to_cpu(psacl->size);
			memcpy(&target[pos], psacl, size);
			targhead->sacl = cpu_to_le32(pos);
			pos += size;
		} else
			targhead->sacl = const_cpu_to_le32(0);
		if (selection & SACL_SECURITY_INFORMATION) {
			control |= newhead->control
					& (SE_SACL_PRESENT
					   | SE_SACL_DEFAULTED
					   | SE_SACL_PROTECTED);
			if (newhead->control & SE_SACL_AUTO_INHERIT_REQ)
				control |= SE_SACL_AUTO_INHERITED;
		} else
			control |= oldhead->control
					& (SE_SACL_PRESENT
					   | SE_SACL_DEFAULTED
					   | SE_SACL_AUTO_INHERITED
					   | SE_SACL_PROTECTED);
			/*
			 * copy new OWNER if selected
			 * or keep old OWNER if any
			 */
		if ((selection & OWNER_SECURITY_INFORMATION) ?
				newhead->owner : oldhead->owner) {
			if (selection & OWNER_SECURITY_INFORMATION) {
				offowner = le32_to_cpu(newhead->owner);
				powner = (const SID*)&newattr[offowner];
			} else {
				offowner = le32_to_cpu(oldhead->owner);
				powner = (const SID*)&oldattr[offowner];
			}
			size = ntfs_sid_size(powner);
			memcpy(&target[pos], powner, size);
			targhead->owner = cpu_to_le32(pos);
			pos += size;
		} else
			targhead->owner = const_cpu_to_le32(0);
		if (selection & OWNER_SECURITY_INFORMATION)
			control |= newhead->control & SE_OWNER_DEFAULTED;
		else
			control |= oldhead->control & SE_OWNER_DEFAULTED;
			/*
			 * copy new GROUP if selected
			 * or keep old GROUP if any
			 */
		if ((selection & GROUP_SECURITY_INFORMATION) ?
				newhead->group : oldhead->group) {
			if (selection & GROUP_SECURITY_INFORMATION) {
				offgroup = le32_to_cpu(newhead->group);
				pgroup = (const SID*)&newattr[offgroup];
				control |= newhead->control
						 & SE_GROUP_DEFAULTED;
			} else {
				offgroup = le32_to_cpu(oldhead->group);
				pgroup = (const SID*)&oldattr[offgroup];
				control |= oldhead->control
						 & SE_GROUP_DEFAULTED;
			}
			size = ntfs_sid_size(pgroup);
			memcpy(&target[pos], pgroup, size);
			targhead->group = cpu_to_le32(pos);
			pos += size;
		} else
			targhead->group = const_cpu_to_le32(0);
		if (selection & GROUP_SECURITY_INFORMATION)
			control |= newhead->control & SE_GROUP_DEFAULTED;
		else
			control |= oldhead->control & SE_GROUP_DEFAULTED;
		targhead->revision = SECURITY_DESCRIPTOR_REVISION;
		targhead->alignment = 0;
		targhead->control = control;
		ok = !update_secur_descr(vol, target, ni);
		free(target);
	}
	return (ok);
}

/*
 *		Return the security descriptor of a file
 *	This is intended to be similar to GetFileSecurity() from Win32
 *	in order to facilitate the development of portable tools
 *
 *	returns zero if unsuccessful (following Win32 conventions)
 *		-1 if no securid
 *		the securid if any
 *
 *  The Win32 API is :
 *
 *  BOOL WINAPI GetFileSecurity(
 *    __in          LPCTSTR lpFileName,
 *    __in          SECURITY_INFORMATION RequestedInformation,
 *    __out_opt     PSECURITY_DESCRIPTOR pSecurityDescriptor,
 *    __in          DWORD nLength,
 *    __out         LPDWORD lpnLengthNeeded
 *  );
 *
 */

int ntfs_get_file_security(struct SECURITY_API *scapi,
		const char *path, u32 selection,
		char *buf, u32 buflen, u32 *psize)
{
	ntfs_inode *ni;
	char *attr;
	int res;

	res = 0; /* default return */
	if (scapi && (scapi->magic == MAGIC_API)) {
		ni = ntfs_pathname_to_inode(scapi->security.vol, NULL, path);
		if (ni) {
			attr = getsecurityattr(scapi->security.vol, ni);
			if (attr) {
				if (feedsecurityattr(attr,selection,
						buf,buflen,psize)) {
					if (test_nino_flag(ni, v3_Extensions)
					    && ni->security_id)
						res = le32_to_cpu(
							ni->security_id);
					else
						res = -1;
				}
				free(attr);
			}
			ntfs_inode_close(ni);
		} else
			errno = ENOENT;
		if (!res) *psize = 0;
	} else
		errno = EINVAL; /* do not clear *psize */
	return (res);
}


/*
 *		Set the security descriptor of a file or directory
 *	This is intended to be similar to SetFileSecurity() from Win32
 *	in order to facilitate the development of portable tools
 *
 *	returns zero if unsuccessful (following Win32 conventions)
 *		-1 if no securid
 *		the securid if any
 *
 *  The Win32 API is :
 *
 *  BOOL WINAPI SetFileSecurity(
 *    __in          LPCTSTR lpFileName,
 *    __in          SECURITY_INFORMATION SecurityInformation,
 *    __in          PSECURITY_DESCRIPTOR pSecurityDescriptor
 *  );
 */

int ntfs_set_file_security(struct SECURITY_API *scapi,
		const char *path, u32 selection, const char *attr)
{
	const SECURITY_DESCRIPTOR_RELATIVE *phead;
	ntfs_inode *ni;
	int attrsz;
	BOOL missing;
	char *oldattr;
	int res;

	res = 0; /* default return */
	if (scapi && (scapi->magic == MAGIC_API) && attr) {
		phead = (const SECURITY_DESCRIPTOR_RELATIVE*)attr;
		attrsz = ntfs_attr_size(attr);
		/* if selected, owner and group must be present or defaulted */
		missing = ((selection & OWNER_SECURITY_INFORMATION)
				&& !phead->owner
				&& !(phead->control & SE_OWNER_DEFAULTED))
			|| ((selection & GROUP_SECURITY_INFORMATION)
				&& !phead->group
				&& !(phead->control & SE_GROUP_DEFAULTED));
		if (!missing
		    && (phead->control & SE_SELF_RELATIVE)
		    && ntfs_valid_descr(attr, attrsz)) {
			ni = ntfs_pathname_to_inode(scapi->security.vol,
				NULL, path);
			if (ni) {
				oldattr = getsecurityattr(scapi->security.vol,
						ni);
				if (oldattr) {
					if (mergesecurityattr(
						scapi->security.vol,
						oldattr, attr,
						selection, ni)) {
						if (test_nino_flag(ni,
							    v3_Extensions))
							res = le32_to_cpu(
							    ni->security_id);
						else
							res = -1;
					}
					free(oldattr);
				}
				ntfs_inode_close(ni);
			}
		} else
			errno = EINVAL;
	} else
		errno = EINVAL;
	return (res);
}


/*
 *		Return the attributes of a file
 *	This is intended to be similar to GetFileAttributes() from Win32
 *	in order to facilitate the development of portable tools
 *
 *	returns -1 if unsuccessful (Win32 : INVALID_FILE_ATTRIBUTES)
 *
 *  The Win32 API is :
 *
 *  DWORD WINAPI GetFileAttributes(
 *   __in  LPCTSTR lpFileName
 *  );
 */

int ntfs_get_file_attributes(struct SECURITY_API *scapi, const char *path)
{
	ntfs_inode *ni;
	s32 attrib;

	attrib = -1; /* default return */
	if (scapi && (scapi->magic == MAGIC_API) && path) {
		ni = ntfs_pathname_to_inode(scapi->security.vol, NULL, path);
		if (ni) {
			attrib = le32_to_cpu(ni->flags);
			if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
				attrib |= const_le32_to_cpu(FILE_ATTR_DIRECTORY);
			else
				attrib &= ~const_le32_to_cpu(FILE_ATTR_DIRECTORY);
			if (!attrib)
				attrib |= const_le32_to_cpu(FILE_ATTR_NORMAL);

			ntfs_inode_close(ni);
		} else
			errno = ENOENT;
	} else
		errno = EINVAL; /* do not clear *psize */
	return (attrib);
}


/*
 *		Set attributes to a file or directory
 *	This is intended to be similar to SetFileAttributes() from Win32
 *	in order to facilitate the development of portable tools
 *
 *	Only a few flags can be set (same list as Win32)
 *
 *	returns zero if unsuccessful (following Win32 conventions)
 *		nonzero if successful
 *
 *  The Win32 API is :
 *
 *  BOOL WINAPI SetFileAttributes(
 *    __in  LPCTSTR lpFileName,
 *    __in  DWORD dwFileAttributes
 *  );
 */

BOOL ntfs_set_file_attributes(struct SECURITY_API *scapi,
		const char *path, s32 attrib)
{
	ntfs_inode *ni;
	le32 settable;
	ATTR_FLAGS dirflags;
	int res;

	res = 0; /* default return */
	if (scapi && (scapi->magic == MAGIC_API) && path) {
		ni = ntfs_pathname_to_inode(scapi->security.vol, NULL, path);
		if (ni) {
			settable = FILE_ATTR_SETTABLE;
			if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
				/*
				 * Accept changing compression for a directory
				 * and set index root accordingly
				 */
				settable |= FILE_ATTR_COMPRESSED;
				if ((ni->flags ^ cpu_to_le32(attrib))
				             & FILE_ATTR_COMPRESSED) {
					if (ni->flags & FILE_ATTR_COMPRESSED)
						dirflags = const_cpu_to_le16(0);
					else
						dirflags = ATTR_IS_COMPRESSED;
					res = ntfs_attr_set_flags(ni,
						AT_INDEX_ROOT,
					        NTFS_INDEX_I30, 4,
						dirflags,
						ATTR_COMPRESSION_MASK);
				}
			}
			if (!res) {
				ni->flags = (ni->flags & ~settable)
					 | (cpu_to_le32(attrib) & settable);
				NInoSetDirty(ni);
			}
			if (!ntfs_inode_close(ni))
				res = -1;
		} else
			errno = ENOENT;
	}
	return (res);
}


BOOL ntfs_read_directory(struct SECURITY_API *scapi,
		const char *path, ntfs_filldir_t callback, void *context)
{
	ntfs_inode *ni;
	BOOL ok;
	s64 pos;

	ok = FALSE; /* default return */
	if (scapi && (scapi->magic == MAGIC_API) && callback) {
		ni = ntfs_pathname_to_inode(scapi->security.vol, NULL, path);
		if (ni) {
			if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
				pos = 0;
				ntfs_readdir(ni,&pos,context,callback);
				ok = !ntfs_inode_close(ni);
			} else {
				ntfs_inode_close(ni);
				errno = ENOTDIR;
			}
		} else
			errno = ENOENT;
	} else
		errno = EINVAL; /* do not clear *psize */
	return (ok);
}

/*
 *		read $SDS (for auditing security data)
 *
 *	Returns the number or read bytes, or -1 if there is an error
 */

int ntfs_read_sds(struct SECURITY_API *scapi,
		char *buf, u32 size, u32 offset)
{
	int got;

	got = -1; /* default return */
	if (scapi && (scapi->magic == MAGIC_API)) {
		if (scapi->security.vol->secure_ni)
			got = ntfs_local_read(scapi->security.vol->secure_ni,
				STREAM_SDS, 4, buf, size, offset);
		else
			errno = EOPNOTSUPP;
	} else
		errno = EINVAL;
	return (got);
}

/*
 *		read $SII (for auditing security data)
 *
 *	Returns next entry, or NULL if there is an error
 */

INDEX_ENTRY *ntfs_read_sii(struct SECURITY_API *scapi,
		INDEX_ENTRY *entry)
{
	SII_INDEX_KEY key;
	INDEX_ENTRY *ret;
	BOOL found;
	ntfs_index_context *xsii;

	ret = (INDEX_ENTRY*)NULL; /* default return */
	if (scapi && (scapi->magic == MAGIC_API)) {
		xsii = scapi->security.vol->secure_xsii;
		if (xsii) {
			if (!entry) {
				key.security_id = const_cpu_to_le32(0);
				found = !ntfs_index_lookup((char*)&key,
						sizeof(SII_INDEX_KEY), xsii);
				/* not supposed to find */
				if (!found && (errno == ENOENT))
					ret = xsii->entry;
			} else
				ret = ntfs_index_next(entry,xsii);
			if (!ret)
				errno = ENODATA;
		} else
			errno = EOPNOTSUPP;
	} else
		errno = EINVAL;
	return (ret);
}

/*
 *		read $SDH (for auditing security data)
 *
 *	Returns next entry, or NULL if there is an error
 */

INDEX_ENTRY *ntfs_read_sdh(struct SECURITY_API *scapi,
		INDEX_ENTRY *entry)
{
	SDH_INDEX_KEY key;
	INDEX_ENTRY *ret;
	BOOL found;
	ntfs_index_context *xsdh;

	ret = (INDEX_ENTRY*)NULL; /* default return */
	if (scapi && (scapi->magic == MAGIC_API)) {
		xsdh = scapi->security.vol->secure_xsdh;
		if (xsdh) {
			if (!entry) {
				key.hash = const_cpu_to_le32(0);
				key.security_id = const_cpu_to_le32(0);
				found = !ntfs_index_lookup((char*)&key,
						sizeof(SDH_INDEX_KEY), xsdh);
				/* not supposed to find */
				if (!found && (errno == ENOENT))
					ret = xsdh->entry;
			} else
				ret = ntfs_index_next(entry,xsdh);
			if (!ret)
				errno = ENODATA;
		} else errno = ENOTSUP;
	} else
		errno = EINVAL;
	return (ret);
}

/*
 *		Get the mapped user SID
 *	A buffer of 40 bytes has to be supplied
 *
 *	returns the size of the SID, or zero and errno set if not found
 */

int ntfs_get_usid(struct SECURITY_API *scapi, uid_t uid, char *buf)
{
	const SID *usid;
	BIGSID defusid;
	int size;

	size = 0;
	if (scapi && (scapi->magic == MAGIC_API)) {
		usid = ntfs_find_usid(scapi->security.mapping[MAPUSERS], uid, (SID*)&defusid);
		if (usid) {
			size = ntfs_sid_size(usid);
			memcpy(buf,usid,size);
		} else
			errno = ENODATA;
	} else
		errno = EINVAL;
	return (size);
}

/*
 *		Get the mapped group SID
 *	A buffer of 40 bytes has to be supplied
 *
 *	returns the size of the SID, or zero and errno set if not found
 */

int ntfs_get_gsid(struct SECURITY_API *scapi, gid_t gid, char *buf)
{
	const SID *gsid;
	BIGSID defgsid;
	int size;

	size = 0;
	if (scapi && (scapi->magic == MAGIC_API)) {
		gsid = ntfs_find_gsid(scapi->security.mapping[MAPGROUPS], gid, (SID*)&defgsid);
		if (gsid) {
			size = ntfs_sid_size(gsid);
			memcpy(buf,gsid,size);
		} else
			errno = ENODATA;
	} else
		errno = EINVAL;
	return (size);
}

/*
 *		Get the user mapped to a SID
 *
 *	returns the uid, or -1 if not found
 */

int ntfs_get_user(struct SECURITY_API *scapi, const SID *usid)
{
	int uid;

	uid = -1;
	if (scapi && (scapi->magic == MAGIC_API) && ntfs_valid_sid(usid)) {
		if (ntfs_same_sid(usid,adminsid))
			uid = 0;
		else {
			uid = ntfs_find_user(scapi->security.mapping[MAPUSERS], usid);
			if (!uid) {
				uid = -1;
				errno = ENODATA;
			}
		}
	} else
		errno = EINVAL;
	return (uid);
}

/*
 *		Get the group mapped to a SID
 *
 *	returns the uid, or -1 if not found
 */

int ntfs_get_group(struct SECURITY_API *scapi, const SID *gsid)
{
	int gid;

	gid = -1;
	if (scapi && (scapi->magic == MAGIC_API) && ntfs_valid_sid(gsid)) {
		if (ntfs_same_sid(gsid,adminsid))
			gid = 0;
		else {
			gid = ntfs_find_group(scapi->security.mapping[MAPGROUPS], gsid);
			if (!gid) {
				gid = -1;
				errno = ENODATA;
			}
		}
	} else
		errno = EINVAL;
	return (gid);
}

/*
 *		Initializations before calling ntfs_get_file_security()
 *	ntfs_set_file_security() and ntfs_read_directory()
 *
 *	Only allowed for root
 *
 *	Returns an (obscured) struct SECURITY_API* needed for further calls
 *		NULL if not root (EPERM) or device is mounted (EBUSY)
 */

struct SECURITY_API *ntfs_initialize_file_security(const char *device,
				int flags)
{
	ntfs_volume *vol;
	unsigned long mntflag;
	int mnt;
	struct SECURITY_API *scapi;
	struct SECURITY_CONTEXT *scx;

	scapi = (struct SECURITY_API*)NULL;
	mnt = ntfs_check_if_mounted(device, &mntflag);
	if (!mnt && !(mntflag & NTFS_MF_MOUNTED) && !getuid()) {
		vol = ntfs_mount(device, flags);
		if (vol) {
			scapi = (struct SECURITY_API*)
				ntfs_malloc(sizeof(struct SECURITY_API));
			if (!ntfs_volume_get_free_space(vol)
			    && scapi) {
				scapi->magic = MAGIC_API;
				scapi->seccache = (struct PERMISSIONS_CACHE*)NULL;
				scx = &scapi->security;
				scx->vol = vol;
				scx->uid = getuid();
				scx->gid = getgid();
				scx->pseccache = &scapi->seccache;
				scx->vol->secure_flags = 0;
					/* accept no mapping and no $Secure */
				ntfs_build_mapping(scx,(const char*)NULL,TRUE);
				ntfs_open_secure(vol);
			} else {
				if (scapi)
					free(scapi);
				else
					errno = ENOMEM;
				mnt = ntfs_umount(vol,FALSE);
				scapi = (struct SECURITY_API*)NULL;
			}
		}
	} else
		if (getuid())
			errno = EPERM;
		else
			errno = EBUSY;
	return (scapi);
}

/*
 *		Leaving after ntfs_initialize_file_security()
 *
 *	Returns FALSE if FAILED
 */

BOOL ntfs_leave_file_security(struct SECURITY_API *scapi)
{
	int ok;
	ntfs_volume *vol;

	ok = FALSE;
	if (scapi && (scapi->magic == MAGIC_API) && scapi->security.vol) {
		vol = scapi->security.vol;
		ntfs_close_secure(&scapi->security);
		free(scapi);
 		if (!ntfs_umount(vol, 0))
			ok = TRUE;
	}
	return (ok);
}

