/* 
   Unix SMB/CIFS implementation.

   fast routines for getting the wire size of security objects

   Copyright (C) Andrew Tridgell 2003
   
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


#include "includes.h"

/*
  return the wire size of a dom_sid
*/
size_t ndr_size_dom_sid(const struct dom_sid *sid, int flags)
{
	if (!sid) return 0;
	return 8 + 4*sid->num_auths;
}

/*
  return the wire size of a security_ace
*/
size_t ndr_size_security_ace(const struct security_ace *ace, int flags)
{
	if (!ace) return 0;
	return 8 + ndr_size_dom_sid(&ace->trustee, flags);
}


/*
  return the wire size of a security_acl
*/
size_t ndr_size_security_acl(const struct security_acl *acl, int flags)
{
	size_t ret;
	int i;
	if (!acl) return 0;
	ret = 8;
	for (i=0;i<acl->num_aces;i++) {
		ret += ndr_size_security_ace(&acl->aces[i], flags);
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

static NTSTATUS ndr_push_security_ace_flags(struct ndr_push *ndr, int ndr_flags, uint8_t r)
{
	NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r));
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_ace_flags(struct ndr_pull *ndr, int ndr_flags, uint8_t *r)
{
	uint8_t v;
	NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &v));
	*r = v;
	return NT_STATUS_OK;
}

void ndr_print_security_ace_flags(struct ndr_print *ndr, const char *name, uint8_t r)
{
	ndr_print_uint8(ndr, name, r);
	ndr->depth++;
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_OBJECT_INHERIT", SEC_ACE_FLAG_OBJECT_INHERIT, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_CONTAINER_INHERIT", SEC_ACE_FLAG_CONTAINER_INHERIT, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_NO_PROPAGATE_INHERIT", SEC_ACE_FLAG_NO_PROPAGATE_INHERIT, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_INHERIT_ONLY", SEC_ACE_FLAG_INHERIT_ONLY, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_INHERITED_ACE", SEC_ACE_FLAG_INHERITED_ACE, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_VALID_INHERIT", SEC_ACE_FLAG_VALID_INHERIT, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_SUCCESSFUL_ACCESS", SEC_ACE_FLAG_SUCCESSFUL_ACCESS, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint8_t), "SEC_ACE_FLAG_FAILED_ACCESS", SEC_ACE_FLAG_FAILED_ACCESS, r);
	ndr->depth--;
}

static NTSTATUS ndr_push_security_ace_type(struct ndr_push *ndr, int ndr_flags, enum security_ace_type r)
{
	NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r));
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_ace_type(struct ndr_pull *ndr, int ndr_flags, enum security_ace_type *r)
{
	uint8_t v;
	NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &v));
	*r = (enum security_ace_type)v;
	return NT_STATUS_OK;
}

void ndr_print_security_ace_type(struct ndr_print *ndr, const char *name, enum security_ace_type r)
{
	const char *val = NULL;

	switch (r) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED: val = "SEC_ACE_TYPE_ACCESS_ALLOWED"; break;
		case SEC_ACE_TYPE_ACCESS_DENIED: val = "SEC_ACE_TYPE_ACCESS_DENIED"; break;
		case SEC_ACE_TYPE_SYSTEM_AUDIT: val = "SEC_ACE_TYPE_SYSTEM_AUDIT"; break;
		case SEC_ACE_TYPE_SYSTEM_ALARM: val = "SEC_ACE_TYPE_SYSTEM_ALARM"; break;
		case SEC_ACE_TYPE_ALLOWED_COMPOUND: val = "SEC_ACE_TYPE_ALLOWED_COMPOUND"; break;
		case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT: val = "SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT"; break;
		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT: val = "SEC_ACE_TYPE_ACCESS_DENIED_OBJECT"; break;
		case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT: val = "SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT"; break;
		case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT: val = "SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT"; break;
	}
	ndr_print_enum(ndr, name, "ENUM", val, r);
}

static NTSTATUS ndr_push_security_ace_object_flags(struct ndr_push *ndr, int ndr_flags, uint32_t r)
{
	NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r));
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_ace_object_flags(struct ndr_pull *ndr, int ndr_flags, uint32_t *r)
{
	uint32_t v;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &v));
	*r = v;
	return NT_STATUS_OK;
}

void ndr_print_security_ace_object_flags(struct ndr_print *ndr, const char *name, uint32_t r)
{
	ndr_print_uint32(ndr, name, r);
	ndr->depth++;
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "SEC_ACE_OBJECT_TYPE_PRESENT", SEC_ACE_OBJECT_TYPE_PRESENT, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT", SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT, r);
	ndr->depth--;
}

static NTSTATUS ndr_push_security_ace_object_type(struct ndr_push *ndr, int ndr_flags, const union security_ace_object_type *r)
{
	int level;
	level = ndr_push_get_switch_value(ndr, r);
	if (ndr_flags & NDR_SCALARS) {
		switch (level) {
			case SEC_ACE_OBJECT_TYPE_PRESENT:
				NDR_CHECK(ndr_push_GUID(ndr, NDR_SCALARS, &r->type));
			break;

			default:
			break;

		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		switch (level) {
			case SEC_ACE_OBJECT_TYPE_PRESENT:
			break;

			default:
			break;

		}
	}
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_ace_object_type(struct ndr_pull *ndr, int ndr_flags, union security_ace_object_type *r)
{
	int level;
	level = ndr_pull_get_switch_value(ndr, r);
	if (ndr_flags & NDR_SCALARS) {
		switch (level) {
			case SEC_ACE_OBJECT_TYPE_PRESENT: {
				NDR_CHECK(ndr_pull_GUID(ndr, NDR_SCALARS, &r->type));
			break; }

			default: {
			break; }

		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		switch (level) {
			case SEC_ACE_OBJECT_TYPE_PRESENT:
			break;

			default:
			break;

		}
	}
	return NT_STATUS_OK;
}

void ndr_print_security_ace_object_type(struct ndr_print *ndr, const char *name, const union security_ace_object_type *r)
{
	int level;
	level = ndr_print_get_switch_value(ndr, r);
	ndr_print_union(ndr, name, level, "security_ace_object_type");
	switch (level) {
		case SEC_ACE_OBJECT_TYPE_PRESENT:
			ndr_print_GUID(ndr, "type", &r->type);
		break;

		default:
		break;

	}
}

static NTSTATUS ndr_push_security_ace_object_inherited_type(struct ndr_push *ndr, int ndr_flags, const union security_ace_object_inherited_type *r)
{
	int level;
	level = ndr_push_get_switch_value(ndr, r);
	if (ndr_flags & NDR_SCALARS) {
		switch (level) {
			case SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT:
				NDR_CHECK(ndr_push_GUID(ndr, NDR_SCALARS, &r->inherited_type));
			break;

			default:
			break;

		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		switch (level) {
			case SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT:
			break;

			default:
			break;

		}
	}
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_ace_object_inherited_type(struct ndr_pull *ndr, int ndr_flags, union security_ace_object_inherited_type *r)
{
	int level;
	level = ndr_pull_get_switch_value(ndr, r);
	if (ndr_flags & NDR_SCALARS) {
		switch (level) {
			case SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT: {
				NDR_CHECK(ndr_pull_GUID(ndr, NDR_SCALARS, &r->inherited_type));
			break; }

			default: {
			break; }

		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		switch (level) {
			case SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT:
			break;

			default:
			break;

		}
	}
	return NT_STATUS_OK;
}

void ndr_print_security_ace_object_inherited_type(struct ndr_print *ndr, const char *name, const union security_ace_object_inherited_type *r)
{
	int level;
	level = ndr_print_get_switch_value(ndr, r);
	ndr_print_union(ndr, name, level, "security_ace_object_inherited_type");
	switch (level) {
		case SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT:
			ndr_print_GUID(ndr, "inherited_type", &r->inherited_type);
		break;

		default:
		break;

	}
}

static NTSTATUS ndr_push_security_ace_object(struct ndr_push *ndr, int ndr_flags, const struct security_ace_object *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_security_ace_object_flags(ndr, NDR_SCALARS, r->flags));
		NDR_CHECK(ndr_push_set_switch_value(ndr, &r->type, r->flags&SEC_ACE_OBJECT_TYPE_PRESENT));
		NDR_CHECK(ndr_push_security_ace_object_type(ndr, NDR_SCALARS, &r->type));
		NDR_CHECK(ndr_push_set_switch_value(ndr, &r->inherited_type, r->flags&SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT));
		NDR_CHECK(ndr_push_security_ace_object_inherited_type(ndr, NDR_SCALARS, &r->inherited_type));
	}
	if (ndr_flags & NDR_BUFFERS) {
		NDR_CHECK(ndr_push_security_ace_object_type(ndr, NDR_BUFFERS, &r->type));
		NDR_CHECK(ndr_push_security_ace_object_inherited_type(ndr, NDR_BUFFERS, &r->inherited_type));
	}
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_ace_object(struct ndr_pull *ndr, int ndr_flags, struct security_ace_object *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_security_ace_object_flags(ndr, NDR_SCALARS, &r->flags));
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->type, r->flags&SEC_ACE_OBJECT_TYPE_PRESENT));
		NDR_CHECK(ndr_pull_security_ace_object_type(ndr, NDR_SCALARS, &r->type));
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->inherited_type, r->flags&SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT));
		NDR_CHECK(ndr_pull_security_ace_object_inherited_type(ndr, NDR_SCALARS, &r->inherited_type));
	}
	if (ndr_flags & NDR_BUFFERS) {
		NDR_CHECK(ndr_pull_security_ace_object_type(ndr, NDR_BUFFERS, &r->type));
		NDR_CHECK(ndr_pull_security_ace_object_inherited_type(ndr, NDR_BUFFERS, &r->inherited_type));
	}
	return NT_STATUS_OK;
}

void ndr_print_security_ace_object(struct ndr_print *ndr, const char *name, const struct security_ace_object *r)
{
	ndr_print_struct(ndr, name, "security_ace_object");
	ndr->depth++;
	ndr_print_security_ace_object_flags(ndr, "flags", r->flags);
	ndr_print_set_switch_value(ndr, &r->type, r->flags&SEC_ACE_OBJECT_TYPE_PRESENT);
	ndr_print_security_ace_object_type(ndr, "type", &r->type);
	ndr_print_set_switch_value(ndr, &r->inherited_type, r->flags&SEC_ACE_INHERITED_OBJECT_TYPE_PRESENT);
	ndr_print_security_ace_object_inherited_type(ndr, "inherited_type", &r->inherited_type);
	ndr->depth--;
}

static NTSTATUS ndr_push_security_ace_object_ctr(struct ndr_push *ndr, int ndr_flags, const union security_ace_object_ctr *r)
{
	int level;
	level = ndr_push_get_switch_value(ndr, r);
	if (ndr_flags & NDR_SCALARS) {
		switch (level) {
			case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break;

			case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break;

			case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break;

			case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break;

			default:
			break;

		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		switch (level) {
			case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
				NDR_CHECK(ndr_push_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			default:
			break;

		}
	}
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_ace_object_ctr(struct ndr_pull *ndr, int ndr_flags, union security_ace_object_ctr *r)
{
	int level;
	level = ndr_pull_get_switch_value(ndr, r);
	if (ndr_flags & NDR_SCALARS) {
		switch (level) {
			case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT: {
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break; }

			case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT: {
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break; }

			case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT: {
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break; }

			case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT: {
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_SCALARS, &r->object));
			break; }

			default: {
			break; }

		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		switch (level) {
			case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
				NDR_CHECK(ndr_pull_security_ace_object(ndr, NDR_BUFFERS, &r->object));
			break;

			default:
			break;

		}
	}
	return NT_STATUS_OK;
}

void ndr_print_security_ace_object_ctr(struct ndr_print *ndr, const char *name, const union security_ace_object_ctr *r)
{
	int level;
	level = ndr_print_get_switch_value(ndr, r);
	ndr_print_union(ndr, name, level, "security_ace_object_ctr");
	switch (level) {
		case SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT:
			ndr_print_security_ace_object(ndr, "object", &r->object);
		break;

		case SEC_ACE_TYPE_ACCESS_DENIED_OBJECT:
			ndr_print_security_ace_object(ndr, "object", &r->object);
		break;

		case SEC_ACE_TYPE_SYSTEM_AUDIT_OBJECT:
			ndr_print_security_ace_object(ndr, "object", &r->object);
		break;

		case SEC_ACE_TYPE_SYSTEM_ALARM_OBJECT:
			ndr_print_security_ace_object(ndr, "object", &r->object);
		break;

		default:
		break;

	}
}

NTSTATUS ndr_push_security_ace(struct ndr_push *ndr, int ndr_flags, const struct security_ace *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_security_ace_type(ndr, NDR_SCALARS, r->type));
		NDR_CHECK(ndr_push_security_ace_flags(ndr, NDR_SCALARS, r->flags));
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, ndr_size_security_ace(r,ndr->flags)));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->access_mask));
		NDR_CHECK(ndr_push_set_switch_value(ndr, &r->object, r->type));
		NDR_CHECK(ndr_push_security_ace_object_ctr(ndr, NDR_SCALARS, &r->object));
		NDR_CHECK(ndr_push_dom_sid(ndr, NDR_SCALARS, &r->trustee));
	}
	if (ndr_flags & NDR_BUFFERS) {
		NDR_CHECK(ndr_push_security_ace_object_ctr(ndr, NDR_BUFFERS, &r->object));
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_security_ace(struct ndr_pull *ndr, int ndr_flags, struct security_ace *r)
{
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_security_ace_type(ndr, NDR_SCALARS, &r->type));
		NDR_CHECK(ndr_pull_security_ace_flags(ndr, NDR_SCALARS, &r->flags));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->size));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->access_mask));
		NDR_CHECK(ndr_pull_set_switch_value(ndr, &r->object, r->type));
		NDR_CHECK(ndr_pull_security_ace_object_ctr(ndr, NDR_SCALARS, &r->object));
		NDR_CHECK(ndr_pull_dom_sid(ndr, NDR_SCALARS, &r->trustee));
	}
	if (ndr_flags & NDR_BUFFERS) {
		NDR_CHECK(ndr_pull_security_ace_object_ctr(ndr, NDR_BUFFERS, &r->object));
	}
	return NT_STATUS_OK;
}

void ndr_print_security_ace(struct ndr_print *ndr, const char *name, const struct security_ace *r)
{
	ndr_print_struct(ndr, name, "security_ace");
	ndr->depth++;
	ndr_print_security_ace_type(ndr, "type", r->type);
	ndr_print_security_ace_flags(ndr, "flags", r->flags);
	ndr_print_uint16(ndr, "size", (ndr->flags & LIBNDR_PRINT_SET_VALUES)?ndr_size_security_ace(r,ndr->flags):r->size);
	ndr_print_uint32(ndr, "access_mask", r->access_mask);
	ndr_print_set_switch_value(ndr, &r->object, r->type);
	ndr_print_security_ace_object_ctr(ndr, "object", &r->object);
	ndr_print_dom_sid(ndr, "trustee", &r->trustee);
	ndr->depth--;
}

static NTSTATUS ndr_push_security_acl_revision(struct ndr_push *ndr, int ndr_flags, enum security_acl_revision r)
{
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r));
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_acl_revision(struct ndr_pull *ndr, int ndr_flags, enum security_acl_revision *r)
{
	uint16_t v;
	NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &v));
	*r = (enum security_acl_revision)v;
	return NT_STATUS_OK;
}

void ndr_print_security_acl_revision(struct ndr_print *ndr, const char *name, enum security_acl_revision r)
{
	const char *val = NULL;

	switch (r) {
		case SECURITY_ACL_REVISION_NT4: val = "SECURITY_ACL_REVISION_NT4"; break;
		case SECURITY_ACL_REVISION_ADS: val = "SECURITY_ACL_REVISION_ADS"; break;
	}
	ndr_print_enum(ndr, name, "ENUM", val, r);
}

NTSTATUS ndr_push_security_acl(struct ndr_push *ndr, int ndr_flags, const struct security_acl *r)
{
	uint32_t cntr_aces_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_push_align(ndr, 4));
		NDR_CHECK(ndr_push_security_acl_revision(ndr, NDR_SCALARS, r->revision));
		NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, ndr_size_security_acl(r,ndr->flags)));
		NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r->num_aces));
		for (cntr_aces_0 = 0; cntr_aces_0 < r->num_aces; cntr_aces_0++) {
			NDR_CHECK(ndr_push_security_ace(ndr, NDR_SCALARS, &r->aces[cntr_aces_0]));
		}
	}
	if (ndr_flags & NDR_BUFFERS) {
		for (cntr_aces_0 = 0; cntr_aces_0 < r->num_aces; cntr_aces_0++) {
			NDR_CHECK(ndr_push_security_ace(ndr, NDR_BUFFERS, &r->aces[cntr_aces_0]));
		}
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_security_acl(struct ndr_pull *ndr, int ndr_flags, struct security_acl *r)
{
	uint32_t cntr_aces_0;
	TALLOC_CTX *_mem_save_aces_0;
	if (ndr_flags & NDR_SCALARS) {
		NDR_CHECK(ndr_pull_align(ndr, 4));
		NDR_CHECK(ndr_pull_security_acl_revision(ndr, NDR_SCALARS, &r->revision));
		NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &r->size));
		NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &r->num_aces));
		if (r->num_aces > 1000) { /* num_aces is unsigned */
			return ndr_pull_error(ndr, NDR_ERR_RANGE, "value out of range");
		}
		NDR_PULL_ALLOC_N(ndr, r->aces, r->num_aces);
		_mem_save_aces_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->aces, 0);
		for (cntr_aces_0 = 0; cntr_aces_0 < r->num_aces; cntr_aces_0++) {
			NDR_CHECK(ndr_pull_security_ace(ndr, NDR_SCALARS, &r->aces[cntr_aces_0]));
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_aces_0, 0);
	}
	if (ndr_flags & NDR_BUFFERS) {
		_mem_save_aces_0 = NDR_PULL_GET_MEM_CTX(ndr);
		NDR_PULL_SET_MEM_CTX(ndr, r->aces, 0);
		for (cntr_aces_0 = 0; cntr_aces_0 < r->num_aces; cntr_aces_0++) {
			NDR_CHECK(ndr_pull_security_ace(ndr, NDR_BUFFERS, &r->aces[cntr_aces_0]));
		}
		NDR_PULL_SET_MEM_CTX(ndr, _mem_save_aces_0, 0);
	}
	return NT_STATUS_OK;
}

void ndr_print_security_acl(struct ndr_print *ndr, const char *name, const struct security_acl *r)
{
	uint32_t cntr_aces_0;
	ndr_print_struct(ndr, name, "security_acl");
	ndr->depth++;
	ndr_print_security_acl_revision(ndr, "revision", r->revision);
	ndr_print_uint16(ndr, "size", (ndr->flags & LIBNDR_PRINT_SET_VALUES)?ndr_size_security_acl(r,ndr->flags):r->size);
	ndr_print_uint32(ndr, "num_aces", r->num_aces);
	ndr->print(ndr, "%s: ARRAY(%d)", "aces", r->num_aces);
	ndr->depth++;
	for (cntr_aces_0=0;cntr_aces_0<r->num_aces;cntr_aces_0++) {
		char *idx_0=NULL;
		asprintf(&idx_0, "[%d]", cntr_aces_0);
		if (idx_0) {
			ndr_print_security_ace(ndr, "aces", &r->aces[cntr_aces_0]);
			free(idx_0);
		}
	}
	ndr->depth--;
	ndr->depth--;
}

static NTSTATUS ndr_push_security_descriptor_revision(struct ndr_push *ndr, int ndr_flags, enum security_descriptor_revision r)
{
	NDR_CHECK(ndr_push_uint8(ndr, NDR_SCALARS, r));
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_descriptor_revision(struct ndr_pull *ndr, int ndr_flags, enum security_descriptor_revision *r)
{
	uint8_t v;
	NDR_CHECK(ndr_pull_uint8(ndr, NDR_SCALARS, &v));
	*r = (enum security_descriptor_revision)v;
	return NT_STATUS_OK;
}

void ndr_print_security_descriptor_revision(struct ndr_print *ndr, const char *name, enum security_descriptor_revision r)
{
	const char *val = NULL;

	switch (r) {
		case SECURITY_DESCRIPTOR_REVISION_1: val = "SECURITY_DESCRIPTOR_REVISION_1"; break;
	}
	ndr_print_enum(ndr, name, "ENUM", val, r);
}

static NTSTATUS ndr_push_security_descriptor_type(struct ndr_push *ndr, int ndr_flags, uint16_t r)
{
	NDR_CHECK(ndr_push_uint16(ndr, NDR_SCALARS, r));
	return NT_STATUS_OK;
}

static NTSTATUS ndr_pull_security_descriptor_type(struct ndr_pull *ndr, int ndr_flags, uint16_t *r)
{
	uint16_t v;
	NDR_CHECK(ndr_pull_uint16(ndr, NDR_SCALARS, &v));
	*r = v;
	return NT_STATUS_OK;
}

void ndr_print_security_descriptor_type(struct ndr_print *ndr, const char *name, uint16_t r)
{
	ndr_print_uint16(ndr, name, r);
	ndr->depth++;
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_OWNER_DEFAULTED", SEC_DESC_OWNER_DEFAULTED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_GROUP_DEFAULTED", SEC_DESC_GROUP_DEFAULTED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_DACL_PRESENT", SEC_DESC_DACL_PRESENT, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_DACL_DEFAULTED", SEC_DESC_DACL_DEFAULTED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_SACL_PRESENT", SEC_DESC_SACL_PRESENT, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_SACL_DEFAULTED", SEC_DESC_SACL_DEFAULTED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_DACL_TRUSTED", SEC_DESC_DACL_TRUSTED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_SERVER_SECURITY", SEC_DESC_SERVER_SECURITY, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_DACL_AUTO_INHERIT_REQ", SEC_DESC_DACL_AUTO_INHERIT_REQ, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_SACL_AUTO_INHERIT_REQ", SEC_DESC_SACL_AUTO_INHERIT_REQ, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_DACL_AUTO_INHERITED", SEC_DESC_DACL_AUTO_INHERITED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_SACL_AUTO_INHERITED", SEC_DESC_SACL_AUTO_INHERITED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_DACL_PROTECTED", SEC_DESC_DACL_PROTECTED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_SACL_PROTECTED", SEC_DESC_SACL_PROTECTED, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_RM_CONTROL_VALID", SEC_DESC_RM_CONTROL_VALID, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint16_t), "SEC_DESC_SELF_RELATIVE", SEC_DESC_SELF_RELATIVE, r);
	ndr->depth--;
}

NTSTATUS ndr_push_security_descriptor(struct ndr_push *ndr, int ndr_flags, const struct security_descriptor *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_LITTLE_ENDIAN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			NDR_CHECK(ndr_push_security_descriptor_revision(ndr, NDR_SCALARS, r->revision));
			NDR_CHECK(ndr_push_security_descriptor_type(ndr, NDR_SCALARS, r->type));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->owner_sid));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->group_sid));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->sacl));
			NDR_CHECK(ndr_push_relative_ptr1(ndr, r->dacl));
		}
		if (ndr_flags & NDR_BUFFERS) {
			if (r->owner_sid) {
				NDR_CHECK(ndr_push_relative_ptr2(ndr, r->owner_sid));
				NDR_CHECK(ndr_push_dom_sid(ndr, NDR_SCALARS, r->owner_sid));
			}
			if (r->group_sid) {
				NDR_CHECK(ndr_push_relative_ptr2(ndr, r->group_sid));
				NDR_CHECK(ndr_push_dom_sid(ndr, NDR_SCALARS, r->group_sid));
			}
			if (r->sacl) {
				NDR_CHECK(ndr_push_relative_ptr2(ndr, r->sacl));
				NDR_CHECK(ndr_push_security_acl(ndr, NDR_SCALARS|NDR_BUFFERS, r->sacl));
			}
			if (r->dacl) {
				NDR_CHECK(ndr_push_relative_ptr2(ndr, r->dacl));
				NDR_CHECK(ndr_push_security_acl(ndr, NDR_SCALARS|NDR_BUFFERS, r->dacl));
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_security_descriptor(struct ndr_pull *ndr, int ndr_flags, struct security_descriptor *r)
{
	uint32_t _ptr_owner_sid;
	TALLOC_CTX *_mem_save_owner_sid_0;
	uint32_t _ptr_group_sid;
	TALLOC_CTX *_mem_save_group_sid_0;
	uint32_t _ptr_sacl;
	TALLOC_CTX *_mem_save_sacl_0;
	uint32_t _ptr_dacl;
	TALLOC_CTX *_mem_save_dacl_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_LITTLE_ENDIAN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			NDR_CHECK(ndr_pull_security_descriptor_revision(ndr, NDR_SCALARS, &r->revision));
			NDR_CHECK(ndr_pull_security_descriptor_type(ndr, NDR_SCALARS, &r->type));
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_owner_sid));
			if (_ptr_owner_sid) {
				NDR_PULL_ALLOC(ndr, r->owner_sid);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->owner_sid, _ptr_owner_sid));
			} else {
				r->owner_sid = NULL;
			}
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_group_sid));
			if (_ptr_group_sid) {
				NDR_PULL_ALLOC(ndr, r->group_sid);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->group_sid, _ptr_group_sid));
			} else {
				r->group_sid = NULL;
			}
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_sacl));
			if (_ptr_sacl) {
				NDR_PULL_ALLOC(ndr, r->sacl);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->sacl, _ptr_sacl));
			} else {
				r->sacl = NULL;
			}
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_dacl));
			if (_ptr_dacl) {
				NDR_PULL_ALLOC(ndr, r->dacl);
				NDR_CHECK(ndr_pull_relative_ptr1(ndr, r->dacl, _ptr_dacl));
			} else {
				r->dacl = NULL;
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
			if (r->owner_sid) {
				struct ndr_pull_save _relative_save;
				ndr_pull_save(ndr, &_relative_save);
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->owner_sid));
				_mem_save_owner_sid_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->owner_sid, 0);
				NDR_CHECK(ndr_pull_dom_sid(ndr, NDR_SCALARS, r->owner_sid));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_owner_sid_0, 0);
				ndr_pull_restore(ndr, &_relative_save);
			}
			if (r->group_sid) {
				struct ndr_pull_save _relative_save;
				ndr_pull_save(ndr, &_relative_save);
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->group_sid));
				_mem_save_group_sid_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->group_sid, 0);
				NDR_CHECK(ndr_pull_dom_sid(ndr, NDR_SCALARS, r->group_sid));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_group_sid_0, 0);
				ndr_pull_restore(ndr, &_relative_save);
			}
			if (r->sacl) {
				struct ndr_pull_save _relative_save;
				ndr_pull_save(ndr, &_relative_save);
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->sacl));
				_mem_save_sacl_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->sacl, 0);
				NDR_CHECK(ndr_pull_security_acl(ndr, NDR_SCALARS|NDR_BUFFERS, r->sacl));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_sacl_0, 0);
				ndr_pull_restore(ndr, &_relative_save);
			}
			if (r->dacl) {
				struct ndr_pull_save _relative_save;
				ndr_pull_save(ndr, &_relative_save);
				NDR_CHECK(ndr_pull_relative_ptr2(ndr, r->dacl));
				_mem_save_dacl_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->dacl, 0);
				NDR_CHECK(ndr_pull_security_acl(ndr, NDR_SCALARS|NDR_BUFFERS, r->dacl));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_dacl_0, 0);
				ndr_pull_restore(ndr, &_relative_save);
			}
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NT_STATUS_OK;
}

void ndr_print_security_descriptor(struct ndr_print *ndr, const char *name, const struct security_descriptor *r)
{
	ndr_print_struct(ndr, name, "security_descriptor");
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_LITTLE_ENDIAN);
		ndr->depth++;
		ndr_print_security_descriptor_revision(ndr, "revision", r->revision);
		ndr_print_security_descriptor_type(ndr, "type", r->type);
		ndr_print_ptr(ndr, "owner_sid", r->owner_sid);
		ndr->depth++;
		if (r->owner_sid) {
			ndr_print_dom_sid(ndr, "owner_sid", r->owner_sid);
		}
		ndr->depth--;
		ndr_print_ptr(ndr, "group_sid", r->group_sid);
		ndr->depth++;
		if (r->group_sid) {
			ndr_print_dom_sid(ndr, "group_sid", r->group_sid);
		}
		ndr->depth--;
		ndr_print_ptr(ndr, "sacl", r->sacl);
		ndr->depth++;
		if (r->sacl) {
			ndr_print_security_acl(ndr, "sacl", r->sacl);
		}
		ndr->depth--;
		ndr_print_ptr(ndr, "dacl", r->dacl);
		ndr->depth++;
		if (r->dacl) {
			ndr_print_security_acl(ndr, "dacl", r->dacl);
		}
		ndr->depth--;
		ndr->depth--;
		ndr->flags = _flags_save_STRUCT;
	}
}

NTSTATUS ndr_push_security_secinfo(struct ndr_push *ndr, int ndr_flags, uint32_t r)
{
	NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, r));
	return NT_STATUS_OK;
}

NTSTATUS ndr_pull_security_secinfo(struct ndr_pull *ndr, int ndr_flags, uint32_t *r)
{
	uint32_t v;
	NDR_CHECK(ndr_pull_uint32(ndr, NDR_SCALARS, &v));
	*r = v;
	return NT_STATUS_OK;
}

void ndr_print_security_secinfo(struct ndr_print *ndr, const char *name, uint32_t r)
{
	ndr_print_uint32(ndr, name, r);
	ndr->depth++;
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "SECINFO_OWNER", SECINFO_OWNER, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "SECINFO_GROUP", SECINFO_GROUP, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "SECINFO_DACL", SECINFO_DACL, r);
	ndr_print_bitmap_flag(ndr, sizeof(uint32_t), "SECINFO_SACL", SECINFO_SACL, r);
	ndr->depth--;
}

