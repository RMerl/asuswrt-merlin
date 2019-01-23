/*
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Stefan (metze) Metzmacher 	2002-2004
   Copyright (C) Andrew Tridgell 		1992-2004
   Copyright (C) Jeremy Allison  		1999

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
#include "librpc/gen_ndr/security.h"
#include "dom_sid.h"

/*****************************************************************
 Compare the auth portion of two sids.
*****************************************************************/

int dom_sid_compare_auth(const struct dom_sid *sid1,
			 const struct dom_sid *sid2)
{
	int i;

	if (sid1 == sid2)
		return 0;
	if (!sid1)
		return -1;
	if (!sid2)
		return 1;

	if (sid1->sid_rev_num != sid2->sid_rev_num)
		return sid1->sid_rev_num - sid2->sid_rev_num;

	for (i = 0; i < 6; i++)
		if (sid1->id_auth[i] != sid2->id_auth[i])
			return sid1->id_auth[i] - sid2->id_auth[i];

	return 0;
}

/*****************************************************************
 Compare two sids.
*****************************************************************/

int dom_sid_compare(const struct dom_sid *sid1, const struct dom_sid *sid2)
{
	int i;

	if (sid1 == sid2)
		return 0;
	if (!sid1)
		return -1;
	if (!sid2)
		return 1;

	/* Compare most likely different rids, first: i.e start at end */
	if (sid1->num_auths != sid2->num_auths)
		return sid1->num_auths - sid2->num_auths;

	for (i = sid1->num_auths-1; i >= 0; --i)
		if (sid1->sub_auths[i] != sid2->sub_auths[i])
			return sid1->sub_auths[i] - sid2->sub_auths[i];

	return dom_sid_compare_auth(sid1, sid2);
}

/*****************************************************************
 Compare two sids.
*****************************************************************/

bool dom_sid_equal(const struct dom_sid *sid1, const struct dom_sid *sid2)
{
	return dom_sid_compare(sid1, sid2) == 0;
}

/*****************************************************************
 Add a rid to the end of a sid
*****************************************************************/

bool sid_append_rid(struct dom_sid *sid, uint32_t rid)
{
	if (sid->num_auths < ARRAY_SIZE(sid->sub_auths)) {
		sid->sub_auths[sid->num_auths++] = rid;
		return true;
	}
	return false;
}

/*
  See if 2 SIDs are in the same domain
  this just compares the leading sub-auths
*/
int dom_sid_compare_domain(const struct dom_sid *sid1,
			   const struct dom_sid *sid2)
{
	int n, i;

	n = MIN(sid1->num_auths, sid2->num_auths);

	for (i = n-1; i >= 0; --i)
		if (sid1->sub_auths[i] != sid2->sub_auths[i])
			return sid1->sub_auths[i] - sid2->sub_auths[i];

	return dom_sid_compare_auth(sid1, sid2);
}

/*****************************************************************
 Convert a string to a SID. Returns True on success, False on fail.
 Return the first character not parsed in endp.
*****************************************************************/

bool dom_sid_parse_endp(const char *sidstr,struct dom_sid *sidout,
			const char **endp)
{
	const char *p;
	char *q;
	/* BIG NOTE: this function only does SIDS where the identauth is not >= 2^32 */
	uint32_t conv;

	ZERO_STRUCTP(sidout);

	if ((sidstr[0] != 'S' && sidstr[0] != 's') || sidstr[1] != '-') {
		goto format_error;
	}

	/* Get the revision number. */
	p = sidstr + 2;

	if (!isdigit(*p)) {
		goto format_error;
	}

	conv = (uint32_t) strtoul(p, &q, 10);
	if (!q || (*q != '-')) {
		goto format_error;
	}
	sidout->sid_rev_num = (uint8_t) conv;
	q++;

	if (!isdigit(*q)) {
		goto format_error;
	}

	/* get identauth */
	conv = (uint32_t) strtoul(q, &q, 10);
	if (!q) {
		goto format_error;
	}

	/* identauth in decimal should be <  2^32 */
	/* NOTE - the conv value is in big-endian format. */
	sidout->id_auth[0] = 0;
	sidout->id_auth[1] = 0;
	sidout->id_auth[2] = (conv & 0xff000000) >> 24;
	sidout->id_auth[3] = (conv & 0x00ff0000) >> 16;
	sidout->id_auth[4] = (conv & 0x0000ff00) >> 8;
	sidout->id_auth[5] = (conv & 0x000000ff);

	sidout->num_auths = 0;
	if (*q != '-') {
		/* Just id_auth, no subauths */
		return true;
	}

	q++;

	while (true) {
		char *end;

		if (!isdigit(*q)) {
			goto format_error;
		}

		conv = strtoul(q, &end, 10);
		if (end == q) {
			goto format_error;
		}

		if (!sid_append_rid(sidout, conv)) {
			DEBUG(3, ("Too many sid auths in %s\n", sidstr));
			return false;
		}

		q = end;
		if (*q != '-') {
			break;
		}
		q += 1;
	}
	if (endp != NULL) {
		*endp = q;
	}
	return true;

format_error:
	DEBUG(3, ("string_to_sid: SID %s is not in a valid format\n", sidstr));
	return false;
}

bool string_to_sid(struct dom_sid *sidout, const char *sidstr)
{
	return dom_sid_parse(sidstr, sidout);
}

bool dom_sid_parse(const char *sidstr, struct dom_sid *ret)
{
	return dom_sid_parse_endp(sidstr, ret, NULL);
}

/*
  convert a string to a dom_sid, returning a talloc'd dom_sid
*/
struct dom_sid *dom_sid_parse_talloc(TALLOC_CTX *mem_ctx, const char *sidstr)
{
	struct dom_sid *ret;
	ret = talloc(mem_ctx, struct dom_sid);
	if (!ret) {
		return NULL;
	}
	if (!dom_sid_parse(sidstr, ret)) {
		talloc_free(ret);
		return NULL;
	}

	return ret;
}

/*
  convert a string to a dom_sid, returning a talloc'd dom_sid
*/
struct dom_sid *dom_sid_parse_length(TALLOC_CTX *mem_ctx, const DATA_BLOB *sid)
{
	struct dom_sid *ret;
	char *p = talloc_strndup(mem_ctx, (char *)sid->data, sid->length);
	if (!p) {
		return NULL;
	}
	ret = dom_sid_parse_talloc(mem_ctx, p);
	talloc_free(p);
	return ret;
}

/*
  copy a dom_sid structure
*/
struct dom_sid *dom_sid_dup(TALLOC_CTX *mem_ctx, const struct dom_sid *dom_sid)
{
	struct dom_sid *ret;
	int i;

	if (!dom_sid) {
		return NULL;
	}

	ret = talloc(mem_ctx, struct dom_sid);
	if (!ret) {
		return NULL;
	}

	ret->sid_rev_num = dom_sid->sid_rev_num;
	ret->id_auth[0] = dom_sid->id_auth[0];
	ret->id_auth[1] = dom_sid->id_auth[1];
	ret->id_auth[2] = dom_sid->id_auth[2];
	ret->id_auth[3] = dom_sid->id_auth[3];
	ret->id_auth[4] = dom_sid->id_auth[4];
	ret->id_auth[5] = dom_sid->id_auth[5];
	ret->num_auths = dom_sid->num_auths;

	for (i=0;i<dom_sid->num_auths;i++) {
		ret->sub_auths[i] = dom_sid->sub_auths[i];
	}

	return ret;
}

/*
  add a rid to a domain dom_sid to make a full dom_sid. This function
  returns a new sid in the supplied memory context
*/
struct dom_sid *dom_sid_add_rid(TALLOC_CTX *mem_ctx,
				const struct dom_sid *domain_sid,
				uint32_t rid)
{
	struct dom_sid *sid;

	sid = dom_sid_dup(mem_ctx, domain_sid);
	if (!sid) return NULL;

	if (!sid_append_rid(sid, rid)) {
		talloc_free(sid);
		return NULL;
	}

	return sid;
}

/*
  Split up a SID into its domain and RID part
*/
NTSTATUS dom_sid_split_rid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			   struct dom_sid **domain, uint32_t *rid)
{
	if (sid->num_auths == 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (domain) {
		if (!(*domain = dom_sid_dup(mem_ctx, sid))) {
			return NT_STATUS_NO_MEMORY;
		}

		(*domain)->num_auths -= 1;
	}

	if (rid) {
		*rid = sid->sub_auths[sid->num_auths - 1];
	}

	return NT_STATUS_OK;
}

/*
  return true if the 2nd sid is in the domain given by the first sid
*/
bool dom_sid_in_domain(const struct dom_sid *domain_sid,
		       const struct dom_sid *sid)
{
	int i;

	if (!domain_sid || !sid) {
		return false;
	}

	if (domain_sid->num_auths > sid->num_auths) {
		return false;
	}

	for (i = domain_sid->num_auths-1; i >= 0; --i) {
		if (domain_sid->sub_auths[i] != sid->sub_auths[i]) {
			return false;
		}
	}

	return dom_sid_compare_auth(domain_sid, sid) == 0;
}

/*
  Convert a dom_sid to a string, printing into a buffer. Return the
  string length. If it overflows, return the string length that would
  result (buflen needs to be +1 for the terminating 0).
*/
int dom_sid_string_buf(const struct dom_sid *sid, char *buf, int buflen)
{
	int i, ofs;
	uint32_t ia;

	if (!sid) {
		strlcpy(buf, "(NULL SID)", buflen);
		return 10;	/* strlen("(NULL SID)") */
	}

	ia = (sid->id_auth[5]) +
		(sid->id_auth[4] << 8 ) +
		(sid->id_auth[3] << 16) +
		(sid->id_auth[2] << 24);

	ofs = snprintf(buf, buflen, "S-%u-%lu",
		       (unsigned int)sid->sid_rev_num, (unsigned long)ia);

	for (i = 0; i < sid->num_auths; i++) {
		ofs += snprintf(buf + ofs, MAX(buflen - ofs, 0), "-%lu",
				(unsigned long)sid->sub_auths[i]);
	}
	return ofs;
}

/*
  convert a dom_sid to a string
*/
char *dom_sid_string(TALLOC_CTX *mem_ctx, const struct dom_sid *sid)
{
	char buf[DOM_SID_STR_BUFLEN];
	char *result;
	int len;

	len = dom_sid_string_buf(sid, buf, sizeof(buf));

	if (len+1 > sizeof(buf)) {
		return talloc_strdup(mem_ctx, "(SID ERR)");
	}

	/*
	 * Avoid calling strlen (via talloc_strdup), we already have
	 * the length
	 */
	result = (char *)talloc_memdup(mem_ctx, buf, len+1);

	/*
	 * beautify the talloc_report output
	 */
	talloc_set_name_const(result, result);
	return result;
}
