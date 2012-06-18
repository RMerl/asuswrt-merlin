/*
   Unix SMB/CIFS implementation.
   dump the remote SAM using rpc samsync operations

   Copyright (C) Andrew Tridgell 2002
   Copyright (C) Tim Potter 2001,2002
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2005
   Modified by Volker Lendecke 2002
   Copyright (C) Jeremy Allison 2005.
   Copyright (C) Guenther Deschner 2008.

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
#include "libnet/libnet.h"

static void display_group_mem_info(uint32_t rid,
				   struct netr_DELTA_GROUP_MEMBER *r)
{
	int i;
	d_printf("Group mem %u: ", rid);
	for (i=0; i< r->num_rids; i++) {
		d_printf("%u ", r->rids[i]);
	}
	d_printf("\n");
}

static void display_alias_info(uint32_t rid,
			       struct netr_DELTA_ALIAS *r)
{
	d_printf("Alias '%s' ", r->alias_name.string);
	d_printf("desc='%s' rid=%u\n", r->description.string, r->rid);
}

static void display_alias_mem(uint32_t rid,
			      struct netr_DELTA_ALIAS_MEMBER *r)
{
	int i;
	d_printf("Alias rid %u: ", rid);
	for (i=0; i< r->sids.num_sids; i++) {
		d_printf("%s ", sid_string_tos(r->sids.sids[i].sid));
	}
	d_printf("\n");
}

static void display_account_info(uint32_t rid,
				 struct netr_DELTA_USER *r)
{
	fstring hex_nt_passwd, hex_lm_passwd;
	uchar zero_buf[16];

	memset(zero_buf, '\0', sizeof(zero_buf));

	/* Decode hashes from password hash (if they are not NULL) */

	if (memcmp(r->lmpassword.hash, zero_buf, 16) != 0) {
		pdb_sethexpwd(hex_lm_passwd, r->lmpassword.hash, r->acct_flags);
	} else {
		pdb_sethexpwd(hex_lm_passwd, NULL, 0);
	}

	if (memcmp(r->ntpassword.hash, zero_buf, 16) != 0) {
		pdb_sethexpwd(hex_nt_passwd, r->ntpassword.hash, r->acct_flags);
	} else {
		pdb_sethexpwd(hex_nt_passwd, NULL, 0);
	}

	printf("%s:%d:%s:%s:%s:LCT-0\n",
		r->account_name.string,
		r->rid, hex_lm_passwd, hex_nt_passwd,
		pdb_encode_acct_ctrl(r->acct_flags, NEW_PW_FORMAT_SPACE_PADDED_LEN));
}

static void display_domain_info(struct netr_DELTA_DOMAIN *r)
{
	time_t u_logout;
	struct netr_AcctLockStr *lockstr = NULL;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = talloc_tos();

	status = pull_netr_AcctLockStr(mem_ctx, &r->account_lockout,
				       &lockstr);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("failed to pull account lockout string: %s\n",
			nt_errstr(status));
	}

	u_logout = uint64s_nt_time_to_unix_abs((const uint64 *)&r->force_logoff_time);

	d_printf("Domain name: %s\n", r->domain_name.string);

	d_printf("Minimal Password Length: %d\n", r->min_password_length);
	d_printf("Password History Length: %d\n", r->password_history_length);

	d_printf("Force Logoff: %d\n", (int)u_logout);

	d_printf("Max Password Age: %s\n", display_time(r->max_password_age));
	d_printf("Min Password Age: %s\n", display_time(r->min_password_age));

	if (lockstr) {
		d_printf("Lockout Time: %s\n", display_time((NTTIME)lockstr->lockout_duration));
		d_printf("Lockout Reset Time: %s\n", display_time((NTTIME)lockstr->reset_count));
		d_printf("Bad Attempt Lockout: %d\n", lockstr->bad_attempt_lockout);
	}

	d_printf("User must logon to change password: %d\n", r->logon_to_chgpass);
}

static void display_group_info(uint32_t rid, struct netr_DELTA_GROUP *r)
{
	d_printf("Group '%s' ", r->group_name.string);
	d_printf("desc='%s', rid=%u\n", r->description.string, rid);
}

static void display_delete_group(uint32_t rid)
{
	d_printf("Delete Group '%d'\n", rid);
}

static void display_rename_group(uint32_t rid, struct netr_DELTA_RENAME *r)
{
	d_printf("Rename Group '%d' ", rid);
	d_printf("Rename Group: %s -> %s\n",
		r->OldName.string, r->NewName.string);
}

static void display_delete_user(uint32_t rid)
{
	d_printf("Delete User '%d'\n", rid);
}

static void display_rename_user(uint32_t rid, struct netr_DELTA_RENAME *r)
{
	d_printf("Rename User '%d' ", rid);
	d_printf("Rename User: %s -> %s\n",
		r->OldName.string, r->NewName.string);
}

static void display_delete_alias(uint32_t rid)
{
	d_printf("Delete Alias '%d'\n", rid);
}

static void display_rename_alias(uint32_t rid, struct netr_DELTA_RENAME *r)
{
	d_printf("Rename Alias '%d' ", rid);
	d_printf("Rename Alias: %s -> %s\n",
		r->OldName.string, r->NewName.string);
}

static NTSTATUS display_sam_entry(TALLOC_CTX *mem_ctx,
				  enum netr_SamDatabaseID database_id,
				  struct netr_DELTA_ENUM *r,
				  struct samsync_context *ctx)
{
	union netr_DELTA_UNION u = r->delta_union;
	union netr_DELTA_ID_UNION id = r->delta_id_union;

	switch (r->delta_type) {
	case NETR_DELTA_DOMAIN:
		display_domain_info(u.domain);
		break;
	case NETR_DELTA_GROUP:
		display_group_info(id.rid, u.group);
		break;
	case NETR_DELTA_DELETE_GROUP:
		display_delete_group(id.rid);
		break;
	case NETR_DELTA_RENAME_GROUP:
		display_rename_group(id.rid, u.rename_group);
		break;
	case NETR_DELTA_USER:
		display_account_info(id.rid, u.user);
		break;
	case NETR_DELTA_DELETE_USER:
		display_delete_user(id.rid);
		break;
	case NETR_DELTA_RENAME_USER:
		display_rename_user(id.rid, u.rename_user);
		break;
	case NETR_DELTA_GROUP_MEMBER:
		display_group_mem_info(id.rid, u.group_member);
		break;
	case NETR_DELTA_ALIAS:
		display_alias_info(id.rid, u.alias);
		break;
	case NETR_DELTA_DELETE_ALIAS:
		display_delete_alias(id.rid);
		break;
	case NETR_DELTA_RENAME_ALIAS:
		display_rename_alias(id.rid, u.rename_alias);
		break;
	case NETR_DELTA_ALIAS_MEMBER:
		display_alias_mem(id.rid, u.alias_member);
		break;
	case NETR_DELTA_POLICY:
		printf("Policy\n");
		break;
	case NETR_DELTA_TRUSTED_DOMAIN:
		printf("Trusted Domain: %s\n",
			u.trusted_domain->domain_name.string);
		break;
	case NETR_DELTA_DELETE_TRUST:
		printf("Delete Trust: %d\n",
			u.delete_trust.unknown);
		break;
	case NETR_DELTA_ACCOUNT:
		printf("Account\n");
		break;
	case NETR_DELTA_DELETE_ACCOUNT:
		printf("Delete Account: %d\n",
			u.delete_account.unknown);
		break;
	case NETR_DELTA_SECRET:
		printf("Secret\n");
		break;
	case NETR_DELTA_DELETE_SECRET:
		printf("Delete Secret: %d\n",
			u.delete_secret.unknown);
		break;
	case NETR_DELTA_DELETE_GROUP2:
		printf("Delete Group2: %s\n",
			u.delete_group->account_name);
		break;
	case NETR_DELTA_DELETE_USER2:
		printf("Delete User2: %s\n",
			u.delete_user->account_name);
		break;
	case NETR_DELTA_MODIFY_COUNT:
		printf("sam sequence update: 0x%016llx\n",
			(unsigned long long) *u.modified_count);
		break;
#if 0
	/* The following types are recognised but not handled */
	case NETR_DELTA_POLICY:
		d_printf("NETR_DELTA_POLICY not handled\n");
		break;
	case NETR_DELTA_TRUSTED_DOMAIN:
		d_printf("NETR_DELTA_TRUSTED_DOMAIN not handled\n");
		break;
	case NETR_DELTA_ACCOUNT:
		d_printf("NETR_DELTA_ACCOUNT not handled\n");
		break;
	case NETR_DELTA_SECRET:
		d_printf("NETR_DELTA_SECRET not handled\n");
		break;
	case NETR_DELTA_MODIFY_COUNT:
		d_printf("NETR_DELTA_MODIFY_COUNT not handled\n");
		break;
	case NETR_DELTA_DELETE_TRUST:
		d_printf("NETR_DELTA_DELETE_TRUST not handled\n");
		break;
	case NETR_DELTA_DELETE_ACCOUNT:
		d_printf("NETR_DELTA_DELETE_ACCOUNT not handled\n");
		break;
	case NETR_DELTA_DELETE_SECRET:
		d_printf("NETR_DELTA_DELETE_SECRET not handled\n");
		break;
	case NETR_DELTA_DELETE_GROUP2:
		d_printf("NETR_DELTA_DELETE_GROUP2 not handled\n");
		break;
	case NETR_DELTA_DELETE_USER2:
		d_printf("NETR_DELTA_DELETE_USER2 not handled\n");
		break;
#endif
	default:
		printf("unknown delta type 0x%02x\n",
			r->delta_type);
		break;
	}

	return NT_STATUS_OK;
}

static NTSTATUS display_sam_entries(TALLOC_CTX *mem_ctx,
				    enum netr_SamDatabaseID database_id,
				    struct netr_DELTA_ENUM_ARRAY *r,
				    uint64_t *sequence_num,
				    struct samsync_context *ctx)
{
	int i;

	for (i = 0; i < r->num_deltas; i++) {
		display_sam_entry(mem_ctx, database_id, &r->delta_enum[i],
				  ctx);
	}

	return NT_STATUS_OK;
}

const struct samsync_ops libnet_samsync_display_ops = {
	.process_objects	= display_sam_entries,
};
