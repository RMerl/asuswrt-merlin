/* 
   Unix SMB/CIFS implementation.

   fast routines for getting the wire size of security objects

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan Metzmacher 2006-2008
   
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
#include "librpc/gen_ndr/ndr_security.h"
#include "../libcli/security/security.h"

/*
  return the wire size of a security_ace
*/
size_t ndr_size_security_ace(const struct security_ace *ace, int flags)
{
	size_t ret;

	if (!ace) return 0;

	ret = 8 + ndr_size_dom_sid(&ace->trustee, flags);

	switch (ace->type) {
	case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
	case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
	case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
	case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
		ret += 4; /* uint32 bitmap ace->object.object.flags */
		if (ace->object.object.flags & SEC_ACE_OBJECT_TYPE_PRESENT) {
			ret += 16; /* GUID ace->object.object.type.type */
		}
		if (ace->object.object.flags & SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT) {
			ret += 16; /* GUID ace->object.object.inherited_typeinherited_type */
		}
		break;
	default:
		break;
	}

	return ret;
}

enum ndr_err_code ndr_pull_security_ace(struct ndr_pull *ndr, int ndr_flags, struct security_ace *r)
{
	if (ndr_flags & NDR_SCALARS) {
		uint32_t start_ofs = ndr->offset;
		uint32_t size = 0;
		uint32_t pad = 0;
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_security_ace_type(ndr, NDR_SCALARS, &r->type));
		NDR_CHECK(ndr_pull_security_ace_flags(ndr, NDR_SCALARS, &r->flags));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->size));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->access_mask));
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->object, r->type));
		NDR_CHECK(ndr_pull_security_ace_object_ctr(ndr, NDR_SCALARS, &r->object));
		NDR_CHECK(ndr_pull_dom_sid(ndr, NDR_SCALARS, &r->trustee));
		size = ndr->offset - start_ofs;
		if (r->size < size) {
			return ndr_pull_error(ndr, NDR_ERR_BUFSIZE,
					      "ndr_pull_security_ace: r->size %u < size %u",
					      (unsigned)r->size, size);
		}
		pad = r->size - size;
		NDR_PULL_NEED_BYTES(ndr, pad);
		ndr->offset += pad;
	}
	if (ndr_flags & NDR_BUFFERS) {
		NDR_CHECK(ndr_pull_security_ace_object_ctr(ndr, NDR_BUFFERS, &r->object));
	}
	return NDR_ERR_SUCCESS;
}

/*
  return the wire size of a security_acl
*/
size_t ndr_size_security_acl(const struct security_acl *theacl, int flags)
{
	size_t ret;
	int i;
	if (!theacl) return 0;
	ret = 8;
	for (i=0;i<theacl->num_aces;i++) {
		ret += ndr_size_security_ace(&theacl->aces[i], flags);
	}
	return ret;
}

/*
  return the wire size of a security descriptor
*/
size_t ndr_size_security_descriptor(const struct security_descriptor *sd, int flags)
{
	size_t ret;
	if (!sd) return 0;
	
	ret = 20;
	ret += ndr_size_dom_sid(sd->owner_sid, flags);
	ret += ndr_size_dom_sid(sd->group_sid, flags);
	ret += ndr_size_security_acl(sd->dacl, flags);
	ret += ndr_size_security_acl(sd->sacl, flags);
	return ret;
}

/*
  return the wire size of a dom_sid
*/
size_t ndr_size_dom_sid(const struct dom_sid *sid, int flags)
{
	if (!sid) return 0;
	return 8 + 4*sid->num_auths;
}

size_t ndr_size_dom_sid28(const struct dom_sid *sid, int flags)
{
	struct dom_sid zero_sid;

	if (!sid) return 0;

	ZERO_STRUCT(zero_sid);

	if (memcmp(&zero_sid, sid, sizeof(zero_sid)) == 0) {
		return 0;
	}

	return 8 + 4*sid->num_auths;
}

size_t ndr_size_dom_sid0(const struct dom_sid *sid, int flags)
{
	return ndr_size_dom_sid28(sid, flags);
}

/*
  print a dom_sid
*/
void ndr_print_dom_sid(struct ndr_print *ndr, const char *name, const struct dom_sid *sid)
{
	ndr->print(ndr, "%-25s: %s", name, dom_sid_string(ndr, sid));
}

void ndr_print_dom_sid2(struct ndr_print *ndr, const char *name, const struct dom_sid *sid)
{
	ndr_print_dom_sid(ndr, name, sid);
}

void ndr_print_dom_sid28(struct ndr_print *ndr, const char *name, const struct dom_sid *sid)
{
	ndr_print_dom_sid(ndr, name, sid);
}

void ndr_print_dom_sid0(struct ndr_print *ndr, const char *name, const struct dom_sid *sid)
{
	ndr_print_dom_sid(ndr, name, sid);
}


/*
  parse a dom_sid2 - this is a dom_sid but with an extra copy of the num_auths field
*/
enum ndr_err_code ndr_pull_dom_sid2(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid)
{
	uint32_t num_auths;
	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}
	NDR_CHECK(ndr_pull_uint3264(ndr, NDR_SCALARS, &num_auths));
	NDR_CHECK(ndr_pull_dom_sid(ndr, ndr_flags, sid));
	if (sid->num_auths != num_auths) {
		return ndr_pull_error(ndr, NDR_ERR_ARRAY_SIZE, 
				      "Bad array size %u should exceed %u", 
				      num_auths, sid->num_auths);
	}
	return NDR_ERR_SUCCESS;
}

/*
  parse a dom_sid2 - this is a dom_sid but with an extra copy of the num_auths field
*/
enum ndr_err_code ndr_push_dom_sid2(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}
	NDR_CHECK(ndr_push_uint3264(ndr, NDR_SCALARS, sid->num_auths));
	return ndr_push_dom_sid(ndr, ndr_flags, sid);
}

/*
  parse a dom_sid28 - this is a dom_sid in a fixed 28 byte buffer, so we need to ensure there are only upto 5 sub_auth
*/
enum ndr_err_code ndr_pull_dom_sid28(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid)
{
	enum ndr_err_code status;
	struct ndr_pull *subndr;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	subndr = talloc_zero(ndr, struct ndr_pull);
	NDR_ERR_HAVE_NO_MEMORY(subndr);
	subndr->flags		= ndr->flags;
	subndr->current_mem_ctx	= ndr->current_mem_ctx;

	subndr->data		= ndr->data + ndr->offset;
	subndr->data_size	= 28;
	subndr->offset		= 0;

	NDR_CHECK(ndr_pull_advance(ndr, 28));

	status = ndr_pull_dom_sid(subndr, ndr_flags, sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(status)) {
		/* handle a w2k bug which send random data in the buffer */
		ZERO_STRUCTP(sid);
	} else if (sid->num_auths == 0 && sid->sub_auths) {
		ZERO_STRUCT(sid->sub_auths);
	}

	return NDR_ERR_SUCCESS;
}

/*
  push a dom_sid28 - this is a dom_sid in a 28 byte fixed buffer
*/
enum ndr_err_code ndr_push_dom_sid28(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid)
{
	uint32_t old_offset;
	uint32_t padding;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	if (sid->num_auths > 5) {
		return ndr_push_error(ndr, NDR_ERR_RANGE, 
				      "dom_sid28 allows only upto 5 sub auth [%u]", 
				      sid->num_auths);
	}

	old_offset = ndr->offset;
	NDR_CHECK(ndr_push_dom_sid(ndr, ndr_flags, sid));

	padding = 28 - (ndr->offset - old_offset);

	if (padding > 0) {
		NDR_CHECK(ndr_push_zero(ndr, padding));
	}

	return NDR_ERR_SUCCESS;
}

/*
  parse a dom_sid0 - this is a dom_sid in a variable byte buffer, which is maybe empty
*/
enum ndr_err_code ndr_pull_dom_sid0(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *sid)
{
	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	if (ndr->data_size == ndr->offset) {
		ZERO_STRUCTP(sid);
		return NDR_ERR_SUCCESS;
	}

	return ndr_pull_dom_sid(ndr, ndr_flags, sid);
}

/*
  push a dom_sid0 - this is a dom_sid in a variable byte buffer, which is maybe empty
*/
enum ndr_err_code ndr_push_dom_sid0(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *sid)
{
	struct dom_sid zero_sid;

	if (!(ndr_flags & NDR_SCALARS)) {
		return NDR_ERR_SUCCESS;
	}

	if (!sid) {
		return NDR_ERR_SUCCESS;
	}

	ZERO_STRUCT(zero_sid);

	if (memcmp(&zero_sid, sid, sizeof(zero_sid)) == 0) {
		return NDR_ERR_SUCCESS;
	}

	return ndr_push_dom_sid(ndr, ndr_flags, sid);
}

_PUBLIC_ enum ndr_err_code ndr_push_dom_sid(struct ndr_push *ndr, int ndr_flags, const struct dom_sid *r)
{
	uint32_t cntr_sub_auths_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r->sid_rev_num));
		NDR_CHECK(ndr_push_int8(ndr, NDR_SCALARS, r->num_auths));
		NDR_CHECK(ndr_push_array_uint8(ndr, NDR_SCALARS, r->id_auth, 6));
		for (cntr_sub_auths_0 = 0; cntr_sub_auths_0 < r->num_auths; cntr_sub_auths_0++) {
			NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->sub_auths[cntr_sub_auths_0]));
		}
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_dom_sid(struct ndr_pull *ndr, int ndr_flags, struct dom_sid *r)
{
	uint32_t cntr_sub_auths_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &r->sid_rev_num));
		NDR_CHECK(ndr_pull_int8(ndr, NDR_SCALARS, &r->num_auths));
		if (r->num_auths < 0 || r->num_auths > 15) {
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_CHECK(ndr_pull_array_uint8(ndr, NDR_SCALARS, r->id_auth, 6));
		for (cntr_sub_auths_0 = 0; cntr_sub_auths_0 < r->num_auths; cntr_sub_auths_0++) {
			NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->sub_auths[cntr_sub_auths_0]));
		}
	}
	return NDR_ERR_SUCCESS;
}
