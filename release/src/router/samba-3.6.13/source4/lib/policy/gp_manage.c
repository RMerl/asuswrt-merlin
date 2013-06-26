/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Wilco Baan Hofman 2010
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "includes.h"
#include "../libcli/security/dom_sid.h"
#include "../libcli/security/security_descriptor.h"
#include "../librpc/ndr/libndr.h"
#include "../lib/util/charset/charset.h"
#include "param/param.h"
#include "lib/policy/policy.h"

static uint32_t gp_ads_to_dir_access_mask(uint32_t access_mask)
{
	uint32_t fs_mask;

	/* Copy the standard access mask */
	fs_mask = access_mask & 0x001F0000;

	/* When READ_PROP and LIST_CONTENTS are set, read access is granted on the GPT */
	if (access_mask & SEC_ADS_READ_PROP && access_mask & SEC_ADS_LIST) {
		fs_mask |= SEC_STD_SYNCHRONIZE | SEC_DIR_LIST | SEC_DIR_READ_ATTRIBUTE |
				SEC_DIR_READ_EA | SEC_DIR_TRAVERSE;
	}

	/* When WRITE_PROP is set, full write access is granted on the GPT */
	if (access_mask & SEC_ADS_WRITE_PROP) {
		fs_mask |= SEC_STD_SYNCHRONIZE | SEC_DIR_WRITE_ATTRIBUTE |
				SEC_DIR_WRITE_EA | SEC_DIR_ADD_FILE |
				SEC_DIR_ADD_SUBDIR;
	}

	/* Map CREATE_CHILD to add file and add subdir */
	if (access_mask & SEC_ADS_CREATE_CHILD)
		fs_mask |= SEC_DIR_ADD_FILE | SEC_DIR_ADD_SUBDIR;

	/* Map ADS delete child to dir delete child */
	if (access_mask & SEC_ADS_DELETE_CHILD)
		fs_mask |= SEC_DIR_DELETE_CHILD;

	return fs_mask;
}

NTSTATUS gp_create_gpt_security_descriptor (TALLOC_CTX *mem_ctx, struct security_descriptor *ds_sd, struct security_descriptor **ret)
{
	struct security_descriptor *fs_sd;
	NTSTATUS status;
	uint32_t i;

	/* Allocate the file system security descriptor */
	fs_sd = talloc(mem_ctx, struct security_descriptor);
	NT_STATUS_HAVE_NO_MEMORY(fs_sd);

	/* Copy the basic information from the directory server security descriptor */
	fs_sd->owner_sid = talloc_memdup(fs_sd, ds_sd->owner_sid, sizeof(struct dom_sid));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(fs_sd->owner_sid, fs_sd);

	fs_sd->group_sid = talloc_memdup(fs_sd, ds_sd->group_sid, sizeof(struct dom_sid));
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(fs_sd->group_sid, fs_sd);

	fs_sd->type = ds_sd->type;
	fs_sd->revision = ds_sd->revision;

	/* Copy the sacl */
	fs_sd->sacl = security_acl_dup(fs_sd, ds_sd->sacl);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(fs_sd->sacl, fs_sd);

	/* Copy the dacl */
	fs_sd->dacl = talloc_zero(fs_sd, struct security_acl);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(fs_sd->dacl, fs_sd);

	for (i = 0; i < ds_sd->dacl->num_aces; i++) {
		char *trustee = dom_sid_string(fs_sd, &ds_sd->dacl->aces[i].trustee);
		struct security_ace *ace;

		/* Don't add the allow for SID_BUILTIN_PREW2K */
		if (!(ds_sd->dacl->aces[i].type & SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT) &&
				strcmp(trustee, SID_BUILTIN_PREW2K) == 0) {
			talloc_free(trustee);
			continue;
		}

		/* Copy the ace from the directory server security descriptor */
		ace = talloc_memdup(fs_sd, &ds_sd->dacl->aces[i], sizeof(struct security_ace));
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(ace, fs_sd);

		/* Set specific inheritance flags for within the GPO */
		ace->flags |= SEC_ACE_FLAG_OBJECT_INHERIT | SEC_ACE_FLAG_CONTAINER_INHERIT;
		if (strcmp(trustee, SID_CREATOR_OWNER) == 0) {
			ace->flags |= SEC_ACE_FLAG_INHERIT_ONLY;
		}

		/* Get a directory access mask from the assigned access mask on the LDAP object */
		ace->access_mask = gp_ads_to_dir_access_mask(ace->access_mask);

		/* Add the ace to the security descriptor DACL */
		status = security_descriptor_dacl_add(fs_sd, ace);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("Failed to add a dacl to file system security descriptor\n"));
			return status;
		}

		/* Clean up the allocated data in this iteration */
		talloc_free(trustee);
	}

	*ret = fs_sd;
	return NT_STATUS_OK;
}


NTSTATUS gp_create_gpo (struct gp_context *gp_ctx, const char *display_name, struct gp_object **ret)
{
	struct GUID guid_struct;
	char *guid_str;
	char *name;
	struct security_descriptor *sd;
	TALLOC_CTX *mem_ctx;
	struct gp_object *gpo;
	NTSTATUS status;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	/* Create the gpo struct to return later */
	gpo = talloc(gp_ctx, struct gp_object);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo, mem_ctx);

	/* Generate a GUID */
	guid_struct = GUID_random();
	guid_str = GUID_string2(mem_ctx, &guid_struct);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(guid_str, mem_ctx);
	name = strupper_talloc(mem_ctx, guid_str);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(name, mem_ctx);

	/* Prepare the GPO struct */
	gpo->dn = NULL;
	gpo->name = name;
	gpo->flags = 0;
	gpo->version = 0;
	gpo->display_name = talloc_strdup(gpo, display_name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->display_name, mem_ctx);

	gpo->file_sys_path = talloc_asprintf(gpo, "\\\\%s\\sysvol\\%s\\Policies\\%s", lpcfg_dnsdomain(gp_ctx->lp_ctx), lpcfg_dnsdomain(gp_ctx->lp_ctx), name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(gpo->file_sys_path, mem_ctx);

	/* Create the GPT */
	status = gp_create_gpt(gp_ctx, name, gpo->file_sys_path);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to create GPT\n"));
		talloc_free(mem_ctx);
		return status;
	}


	/* Create the LDAP GPO, including CN=User and CN=Machine */
	status = gp_create_ldap_gpo(gp_ctx, gpo);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to create LDAP group policy object\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Get the new security descriptor */
	status = gp_get_gpo_info(gp_ctx, gpo->dn, &gpo);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to fetch LDAP group policy object\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Create matching file and DS security descriptors */
	status = gp_create_gpt_security_descriptor(mem_ctx, gpo->security_descriptor, &sd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to convert ADS security descriptor to filesystem security descriptor\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Set the security descriptor on the filesystem for this GPO */
	status = gp_set_gpt_security_descriptor(gp_ctx, gpo, sd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set security descriptor (ACL) on the file system\n"));
		talloc_free(mem_ctx);
		return status;
	}

	talloc_free(mem_ctx);

	*ret = gpo;
	return NT_STATUS_OK;
}

NTSTATUS gp_set_acl (struct gp_context *gp_ctx, const char *dn_str, const struct security_descriptor *sd)
{
	TALLOC_CTX *mem_ctx;
	struct security_descriptor *fs_sd;
	struct gp_object *gpo;
	NTSTATUS status;

	/* Create a forked memory context, as a base for everything here */
	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	/* Set the ACL on LDAP database */
	status = gp_set_ads_acl(gp_ctx, dn_str, sd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set ACL on ADS\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Get the group policy object information, for filesystem location and merged sd */
	status = gp_get_gpo_info(gp_ctx, dn_str, &gpo);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set ACL on ADS\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Create matching file and DS security descriptors */
	status = gp_create_gpt_security_descriptor(mem_ctx, gpo->security_descriptor, &fs_sd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to convert ADS security descriptor to filesystem security descriptor\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Set the security descriptor on the filesystem for this GPO */
	status = gp_set_gpt_security_descriptor(gp_ctx, gpo, fs_sd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set security descriptor (ACL) on the file system\n"));
		talloc_free(mem_ctx);
		return status;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

NTSTATUS gp_push_gpo (struct gp_context *gp_ctx, const char *local_path, struct gp_object *gpo)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct gp_ini_context *ini;
	char *filename;

	mem_ctx = talloc_new(gp_ctx);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	/* Get version from ini file */
	/* FIXME: The local file system may be case sensitive */
	filename = talloc_asprintf(mem_ctx, "%s/%s", local_path, "GPT.INI");
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(filename, mem_ctx);
	status = gp_parse_ini(mem_ctx, gp_ctx, local_path, &ini);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to parse GPT.INI.\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Push the GPT to the remote sysvol */
	status = gp_push_gpt(gp_ctx, local_path, gpo->file_sys_path);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to push GPT to DC's sysvol share.\n"));
		talloc_free(mem_ctx);
		return status;
	}

	/* Write version to LDAP */
	status = gp_set_ldap_gpo(gp_ctx, gpo);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0, ("Failed to set GPO options in DC's LDAP.\n"));
		talloc_free(mem_ctx);
		return status;
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}
