/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2007-2008
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


/****************************************************************
****************************************************************/

struct nt_user_token *registry_create_system_token(TALLOC_CTX *mem_ctx)
{
	struct nt_user_token *token = NULL;

	token = TALLOC_ZERO_P(mem_ctx, struct nt_user_token);
	if (!token) {
		DEBUG(1,("talloc failed\n"));
		return NULL;
	}

	token->privileges = se_priv_all;

	if (!NT_STATUS_IS_OK(add_sid_to_array(token, &global_sid_System,
			 &token->user_sids, &token->num_sids))) {
		DEBUG(1,("Error adding nt-authority system sid to token\n"));
		return NULL;
	}

	return token;
}

/****************************************************************
****************************************************************/

WERROR gp_init_reg_ctx(TALLOC_CTX *mem_ctx,
		       const char *initial_path,
		       uint32_t desired_access,
		       const struct nt_user_token *token,
		       struct gp_registry_context **reg_ctx)
{
	struct gp_registry_context *tmp_ctx;
	WERROR werr;

	if (!reg_ctx) {
		return WERR_INVALID_PARAM;
	}

	werr = registry_init_basic();
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	tmp_ctx = TALLOC_ZERO_P(mem_ctx, struct gp_registry_context);
	W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

	if (token) {
		tmp_ctx->token = token;
	} else {
		tmp_ctx->token = registry_create_system_token(mem_ctx);
	}
	if (!tmp_ctx->token) {
		TALLOC_FREE(tmp_ctx);
		return WERR_NOMEM;
	}

	werr = regdb_open();
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	if (initial_path) {
		tmp_ctx->path = talloc_strdup(mem_ctx, initial_path);
		if (!tmp_ctx->path) {
			TALLOC_FREE(tmp_ctx);
			return WERR_NOMEM;
		}

		werr = reg_open_path(mem_ctx, tmp_ctx->path, desired_access,
				     tmp_ctx->token, &tmp_ctx->curr_key);
		if (!W_ERROR_IS_OK(werr)) {
			TALLOC_FREE(tmp_ctx);
			return werr;
		}
	}

	*reg_ctx = tmp_ctx;

	return WERR_OK;
}

/****************************************************************
****************************************************************/

void gp_free_reg_ctx(struct gp_registry_context *reg_ctx)
{
	TALLOC_FREE(reg_ctx);
}

/****************************************************************
****************************************************************/

WERROR gp_store_reg_subkey(TALLOC_CTX *mem_ctx,
			   const char *subkeyname,
			   struct registry_key *curr_key,
			   struct registry_key **new_key)
{
	enum winreg_CreateAction action = REG_ACTION_NONE;
	WERROR werr;

	werr = reg_createkey(mem_ctx, curr_key, subkeyname,
			     REG_KEY_WRITE, new_key, &action);
	if (W_ERROR_IS_OK(werr) && (action != REG_CREATED_NEW_KEY)) {
		return WERR_OK;
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR gp_read_reg_subkey(TALLOC_CTX *mem_ctx,
			  struct gp_registry_context *reg_ctx,
			  const char *subkeyname,
			  struct registry_key **key)
{
	const char *tmp = NULL;

	if (!reg_ctx || !subkeyname || !key) {
		return WERR_INVALID_PARAM;
	}

	tmp = talloc_asprintf(mem_ctx, "%s\\%s", reg_ctx->path, subkeyname);
	W_ERROR_HAVE_NO_MEMORY(tmp);

	return reg_open_path(mem_ctx, tmp, REG_KEY_READ,
			     reg_ctx->token, key);
}

/****************************************************************
****************************************************************/

WERROR gp_store_reg_val_sz(TALLOC_CTX *mem_ctx,
			   struct registry_key *key,
			   const char *val_name,
			   const char *val)
{
	struct registry_value reg_val;
	ZERO_STRUCT(reg_val);

	/* FIXME: hack */
	val = val ? val : " ";

	reg_val.type = REG_SZ;
	reg_val.v.sz.len = strlen(val);
	reg_val.v.sz.str = talloc_strdup(mem_ctx, val);
	W_ERROR_HAVE_NO_MEMORY(reg_val.v.sz.str);

	return reg_setvalue(key, val_name, &reg_val);
}

/****************************************************************
****************************************************************/

static WERROR gp_store_reg_val_dword(TALLOC_CTX *mem_ctx,
				     struct registry_key *key,
				     const char *val_name,
				     uint32_t val)
{
	struct registry_value reg_val;
	ZERO_STRUCT(reg_val);

	reg_val.type = REG_DWORD;
	reg_val.v.dword = val;

	return reg_setvalue(key, val_name, &reg_val);
}

/****************************************************************
****************************************************************/

WERROR gp_read_reg_val_sz(TALLOC_CTX *mem_ctx,
			  struct registry_key *key,
			  const char *val_name,
			  const char **val)
{
	WERROR werr;
	struct registry_value *reg_val = NULL;

	werr = reg_queryvalue(mem_ctx, key, val_name, &reg_val);
	W_ERROR_NOT_OK_RETURN(werr);

	if (reg_val->type != REG_SZ) {
		return WERR_INVALID_DATATYPE;
	}

	*val = talloc_strdup(mem_ctx, reg_val->v.sz.str);
	W_ERROR_HAVE_NO_MEMORY(*val);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR gp_read_reg_val_dword(TALLOC_CTX *mem_ctx,
				    struct registry_key *key,
				    const char *val_name,
				    uint32_t *val)
{
	WERROR werr;
	struct registry_value *reg_val = NULL;

	werr = reg_queryvalue(mem_ctx, key, val_name, &reg_val);
	W_ERROR_NOT_OK_RETURN(werr);

	if (reg_val->type != REG_DWORD) {
		return WERR_INVALID_DATATYPE;
	}

	*val = reg_val->v.dword;

	return WERR_OK;
}

/****************************************************************
****************************************************************/

static WERROR gp_store_reg_gpovals(TALLOC_CTX *mem_ctx,
				   struct registry_key *key,
				   struct GROUP_POLICY_OBJECT *gpo)
{
	WERROR werr;

	if (!key || !gpo) {
		return WERR_INVALID_PARAM;
	}

	werr = gp_store_reg_val_dword(mem_ctx, key, "Version",
				      gpo->version);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_dword(mem_ctx, key, "WQLFilterPass",
				      true); /* fake */
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_dword(mem_ctx, key, "AccessDenied",
				      false); /* fake */
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_dword(mem_ctx, key, "GPO-Disabled",
				      (gpo->options & GPO_FLAG_DISABLE));
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_dword(mem_ctx, key, "Options",
				      gpo->options);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_sz(mem_ctx, key, "GPOID",
				   gpo->name);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_sz(mem_ctx, key, "SOM",
				   gpo->link);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_sz(mem_ctx, key, "DisplayName",
				   gpo->display_name);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_store_reg_val_sz(mem_ctx, key, "WQL-Id",
				   NULL);
	W_ERROR_NOT_OK_RETURN(werr);

	return werr;
}

/****************************************************************
****************************************************************/

static const char *gp_reg_groupmembership_path(TALLOC_CTX *mem_ctx,
					       const DOM_SID *sid,
					       uint32_t flags)
{
	if (flags & GPO_LIST_FLAG_MACHINE) {
		return "GroupMembership";
	}

	return talloc_asprintf(mem_ctx, "%s\\%s", sid_string_tos(sid),
			       "GroupMembership");
}

/****************************************************************
****************************************************************/

static WERROR gp_reg_del_groupmembership(TALLOC_CTX *mem_ctx,
					 struct registry_key *key,
					 const struct nt_user_token *token,
					 uint32_t flags)
{
	const char *path = NULL;

	path = gp_reg_groupmembership_path(mem_ctx, &token->user_sids[0],
					   flags);
	W_ERROR_HAVE_NO_MEMORY(path);

	return reg_deletekey_recursive(mem_ctx, key, path);

}

/****************************************************************
****************************************************************/

static WERROR gp_reg_store_groupmembership(TALLOC_CTX *mem_ctx,
					   struct gp_registry_context *reg_ctx,
					   const struct nt_user_token *token,
					   uint32_t flags)
{
	struct registry_key *key = NULL;
	WERROR werr;
	int i = 0;
	const char *valname = NULL;
	const char *path = NULL;
	const char *val = NULL;
	int count = 0;

	path = gp_reg_groupmembership_path(mem_ctx, &token->user_sids[0],
					   flags);
	W_ERROR_HAVE_NO_MEMORY(path);

	gp_reg_del_groupmembership(mem_ctx, reg_ctx->curr_key, token, flags);

	werr = gp_store_reg_subkey(mem_ctx, path,
				   reg_ctx->curr_key, &key);
	W_ERROR_NOT_OK_RETURN(werr);

	for (i=0; i<token->num_sids; i++) {

		valname = talloc_asprintf(mem_ctx, "Group%d", count++);
		W_ERROR_HAVE_NO_MEMORY(valname);

		val = sid_string_talloc(mem_ctx, &token->user_sids[i]);
		W_ERROR_HAVE_NO_MEMORY(val);
		werr = gp_store_reg_val_sz(mem_ctx, key, valname, val);
		W_ERROR_NOT_OK_RETURN(werr);
	}

	werr = gp_store_reg_val_dword(mem_ctx, key, "Count", count);
	W_ERROR_NOT_OK_RETURN(werr);

	return WERR_OK;
}

/****************************************************************
****************************************************************/
#if 0
/* not used yet */
static WERROR gp_reg_read_groupmembership(TALLOC_CTX *mem_ctx,
					  struct gp_registry_context *reg_ctx,
					  const DOM_SID *object_sid,
					  struct nt_user_token **token,
					  uint32_t flags)
{
	struct registry_key *key = NULL;
	WERROR werr;
	int i = 0;
	const char *valname = NULL;
	const char *val = NULL;
	const char *path = NULL;
	uint32_t count = 0;
	int num_token_sids = 0;
	struct nt_user_token *tmp_token = NULL;

	tmp_token = TALLOC_ZERO_P(mem_ctx, struct nt_user_token);
	W_ERROR_HAVE_NO_MEMORY(tmp_token);

	path = gp_reg_groupmembership_path(mem_ctx, object_sid, flags);
	W_ERROR_HAVE_NO_MEMORY(path);

	werr = gp_read_reg_subkey(mem_ctx, reg_ctx, path, &key);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_read_reg_val_dword(mem_ctx, key, "Count", &count);
	W_ERROR_NOT_OK_RETURN(werr);

	for (i=0; i<count; i++) {

		valname = talloc_asprintf(mem_ctx, "Group%d", i);
		W_ERROR_HAVE_NO_MEMORY(valname);

		werr = gp_read_reg_val_sz(mem_ctx, key, valname, &val);
		W_ERROR_NOT_OK_RETURN(werr);

		if (!string_to_sid(&tmp_token->user_sids[num_token_sids++],
				   val)) {
			return WERR_INSUFFICIENT_BUFFER;
		}
	}

	tmp_token->num_sids = num_token_sids;

	*token = tmp_token;

	return WERR_OK;
}
#endif
/****************************************************************
****************************************************************/

static const char *gp_req_state_path(TALLOC_CTX *mem_ctx,
				     const DOM_SID *sid,
				     uint32_t flags)
{
	if (flags & GPO_LIST_FLAG_MACHINE) {
		return GPO_REG_STATE_MACHINE;
	}

	return talloc_asprintf(mem_ctx, "%s\\%s", "State", sid_string_tos(sid));
}

/****************************************************************
****************************************************************/

static WERROR gp_del_reg_state(TALLOC_CTX *mem_ctx,
			       struct registry_key *key,
			       const char *path)
{
	return reg_deletesubkeys_recursive(mem_ctx, key, path);
}

/****************************************************************
****************************************************************/

WERROR gp_reg_state_store(TALLOC_CTX *mem_ctx,
			  uint32_t flags,
			  const char *dn,
			  const struct nt_user_token *token,
			  struct GROUP_POLICY_OBJECT *gpo_list)
{
	struct gp_registry_context *reg_ctx = NULL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *subkeyname = NULL;
	struct GROUP_POLICY_OBJECT *gpo;
	int count = 0;
	struct registry_key *key;

	werr = gp_init_reg_ctx(mem_ctx, KEY_GROUP_POLICY, REG_KEY_WRITE,
			       token, &reg_ctx);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_secure_key(mem_ctx, flags, reg_ctx->curr_key,
			     &token->user_sids[0]);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("failed to secure key: %s\n", win_errstr(werr)));
		goto done;
	}

	werr = gp_reg_store_groupmembership(mem_ctx, reg_ctx, token, flags);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("failed to store group membership: %s\n", win_errstr(werr)));
		goto done;
	}

	subkeyname = gp_req_state_path(mem_ctx, &token->user_sids[0], flags);
	if (!subkeyname) {
		werr = WERR_NOMEM;
		goto done;
	}

	werr = gp_del_reg_state(mem_ctx, reg_ctx->curr_key, subkeyname);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("failed to delete old state: %s\n", win_errstr(werr)));
		/* goto done; */
	}

	werr = gp_store_reg_subkey(mem_ctx, subkeyname,
				   reg_ctx->curr_key, &reg_ctx->curr_key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = gp_store_reg_val_sz(mem_ctx, reg_ctx->curr_key,
				   "Distinguished-Name", dn);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	/* store link list */

	werr = gp_store_reg_subkey(mem_ctx, "GPLink-List",
				   reg_ctx->curr_key, &key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	/* store gpo list */

	werr = gp_store_reg_subkey(mem_ctx, "GPO-List",
				   reg_ctx->curr_key, &reg_ctx->curr_key);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	for (gpo = gpo_list; gpo; gpo = gpo->next) {

		subkeyname = talloc_asprintf(mem_ctx, "%d", count++);
		if (!subkeyname) {
			werr = WERR_NOMEM;
			goto done;
		}

		werr = gp_store_reg_subkey(mem_ctx, subkeyname,
					   reg_ctx->curr_key, &key);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}

		werr = gp_store_reg_gpovals(mem_ctx, key, gpo);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("gp_reg_state_store: "
				"gpo_store_reg_gpovals failed for %s: %s\n",
				gpo->display_name, win_errstr(werr)));
			goto done;
		}
	}
 done:
	gp_free_reg_ctx(reg_ctx);
	return werr;
}

/****************************************************************
****************************************************************/

static WERROR gp_read_reg_gpovals(TALLOC_CTX *mem_ctx,
				  struct registry_key *key,
				  struct GROUP_POLICY_OBJECT *gpo)
{
	WERROR werr;

	if (!key || !gpo) {
		return WERR_INVALID_PARAM;
	}

	werr = gp_read_reg_val_dword(mem_ctx, key, "Version",
				     &gpo->version);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_read_reg_val_dword(mem_ctx, key, "Options",
				     &gpo->options);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_read_reg_val_sz(mem_ctx, key, "GPOID",
				  &gpo->name);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_read_reg_val_sz(mem_ctx, key, "SOM",
				  &gpo->link);
	W_ERROR_NOT_OK_RETURN(werr);

	werr = gp_read_reg_val_sz(mem_ctx, key, "DisplayName",
				  &gpo->display_name);
	W_ERROR_NOT_OK_RETURN(werr);

	return werr;
}

/****************************************************************
****************************************************************/

static WERROR gp_read_reg_gpo(TALLOC_CTX *mem_ctx,
			      struct registry_key *key,
			      struct GROUP_POLICY_OBJECT **gpo_ret)
{
	struct GROUP_POLICY_OBJECT *gpo = NULL;
	WERROR werr;

	if (!gpo_ret || !key) {
		return WERR_INVALID_PARAM;
	}

	gpo = TALLOC_ZERO_P(mem_ctx, struct GROUP_POLICY_OBJECT);
	W_ERROR_HAVE_NO_MEMORY(gpo);

	werr = gp_read_reg_gpovals(mem_ctx, key, gpo);
	W_ERROR_NOT_OK_RETURN(werr);

	*gpo_ret = gpo;

	return werr;
}

/****************************************************************
****************************************************************/

WERROR gp_reg_state_read(TALLOC_CTX *mem_ctx,
			 uint32_t flags,
			 const DOM_SID *sid,
			 struct GROUP_POLICY_OBJECT **gpo_list)
{
	struct gp_registry_context *reg_ctx = NULL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *subkeyname = NULL;
	struct GROUP_POLICY_OBJECT *gpo = NULL;
	int count = 0;
	struct registry_key *key = NULL;
	const char *path = NULL;
	const char *gp_state_path = NULL;

	if (!gpo_list) {
		return WERR_INVALID_PARAM;
	}

	ZERO_STRUCTP(gpo_list);

	gp_state_path = gp_req_state_path(mem_ctx, sid, flags);
	if (!gp_state_path) {
		werr = WERR_NOMEM;
		goto done;
	}

	path = talloc_asprintf(mem_ctx, "%s\\%s\\%s",
			       KEY_GROUP_POLICY,
			       gp_state_path,
			       "GPO-List");
	if (!path) {
		werr = WERR_NOMEM;
		goto done;
	}

	werr = gp_init_reg_ctx(mem_ctx, path, REG_KEY_READ, NULL, &reg_ctx);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	while (1) {

		subkeyname = talloc_asprintf(mem_ctx, "%d", count++);
		if (!subkeyname) {
			werr = WERR_NOMEM;
			goto done;
		}

		werr = gp_read_reg_subkey(mem_ctx, reg_ctx, subkeyname, &key);
		if (W_ERROR_EQUAL(werr, WERR_BADFILE)) {
			werr = WERR_OK;
			break;
		}
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,("gp_reg_state_read: "
				"gp_read_reg_subkey gave: %s\n",
				win_errstr(werr)));
			goto done;
		}

		werr = gp_read_reg_gpo(mem_ctx, key, &gpo);
		if (!W_ERROR_IS_OK(werr)) {
			goto done;
		}

		DLIST_ADD(*gpo_list, gpo);
	}

 done:
	gp_free_reg_ctx(reg_ctx);
	return werr;
}

/****************************************************************
****************************************************************/

static WERROR gp_reg_generate_sd(TALLOC_CTX *mem_ctx,
				 const DOM_SID *sid,
				 struct security_descriptor **sd,
				 size_t *sd_size)
{
	SEC_ACE ace[6];
	uint32_t mask;

	SEC_ACL *theacl = NULL;

	uint8_t inherit_flags;

	mask = REG_KEY_ALL;
	init_sec_ace(&ace[0],
		     &global_sid_System,
		     SEC_ACE_TYPE_ACCESS_ALLOWED,
		     mask, 0);

	mask = REG_KEY_ALL;
	init_sec_ace(&ace[1],
		     &global_sid_Builtin_Administrators,
		     SEC_ACE_TYPE_ACCESS_ALLOWED,
		     mask, 0);

	mask = REG_KEY_READ;
	init_sec_ace(&ace[2],
		     sid ? sid : &global_sid_Authenticated_Users,
		     SEC_ACE_TYPE_ACCESS_ALLOWED,
		     mask, 0);

	inherit_flags = SEC_ACE_FLAG_OBJECT_INHERIT |
			SEC_ACE_FLAG_CONTAINER_INHERIT |
			SEC_ACE_FLAG_INHERIT_ONLY;

	mask = REG_KEY_ALL;
	init_sec_ace(&ace[3],
		     &global_sid_System,
		     SEC_ACE_TYPE_ACCESS_ALLOWED,
		     mask, inherit_flags);

	mask = REG_KEY_ALL;
	init_sec_ace(&ace[4],
		     &global_sid_Builtin_Administrators,
		     SEC_ACE_TYPE_ACCESS_ALLOWED,
		     mask, inherit_flags);

	mask = REG_KEY_READ;
	init_sec_ace(&ace[5],
		     sid ? sid : &global_sid_Authenticated_Users,
		     SEC_ACE_TYPE_ACCESS_ALLOWED,
		     mask, inherit_flags);

	theacl = make_sec_acl(mem_ctx, NT4_ACL_REVISION, 6, ace);
	W_ERROR_HAVE_NO_MEMORY(theacl);

	*sd = make_sec_desc(mem_ctx, SEC_DESC_REVISION,
			    SEC_DESC_SELF_RELATIVE |
			    SEC_DESC_DACL_AUTO_INHERITED | /* really ? */
			    SEC_DESC_DACL_AUTO_INHERIT_REQ, /* really ? */
			    NULL, NULL, NULL,
			    theacl, sd_size);
	W_ERROR_HAVE_NO_MEMORY(*sd);

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR gp_secure_key(TALLOC_CTX *mem_ctx,
		     uint32_t flags,
		     struct registry_key *key,
		     const DOM_SID *sid)
{
	struct security_descriptor *sd = NULL;
	size_t sd_size = 0;
	const DOM_SID *sd_sid = NULL;
	WERROR werr;

	if (!(flags & GPO_LIST_FLAG_MACHINE)) {
		sd_sid = sid;
	}

	werr = gp_reg_generate_sd(mem_ctx, sd_sid, &sd, &sd_size);
	W_ERROR_NOT_OK_RETURN(werr);

	return reg_setkeysecurity(key, sd);
}

/****************************************************************
****************************************************************/

void dump_reg_val(int lvl, const char *direction,
		  const char *key, const char *subkey,
		  struct registry_value *val)
{
	int i = 0;
	const char *type_str = NULL;

	if (!val) {
		DEBUG(lvl,("no val!\n"));
		return;
	}

	type_str = reg_type_lookup(val->type);

	DEBUG(lvl,("\tdump_reg_val:\t%s '%s'\n\t\t\t'%s' %s: ",
		direction, key, subkey, type_str));

	switch (val->type) {
		case REG_DWORD:
			DEBUG(lvl,("%d (0x%08x)\n",
				(int)val->v.dword, val->v.dword));
			break;
		case REG_QWORD:
			DEBUG(lvl,("%d (0x%016llx)\n",
				(int)val->v.qword,
				(unsigned long long)val->v.qword));
			break;
		case REG_SZ:
			DEBUG(lvl,("%s (length: %d)\n",
				   val->v.sz.str,
				   (int)val->v.sz.len));
			break;
		case REG_MULTI_SZ:
			DEBUG(lvl,("(num_strings: %d)\n",
				   val->v.multi_sz.num_strings));
			for (i=0; i < val->v.multi_sz.num_strings; i++) {
				DEBUGADD(lvl,("\t%s\n",
					val->v.multi_sz.strings[i]));
			}
			break;
		case REG_NONE:
			DEBUG(lvl,("\n"));
			break;
		case REG_BINARY:
			dump_data(lvl, val->v.binary.data,
				  val->v.binary.length);
			break;
		default:
			DEBUG(lvl,("unsupported type: %d\n", val->type));
			break;
	}
}

/****************************************************************
****************************************************************/

void dump_reg_entry(uint32_t flags,
		    const char *dir,
		    struct gp_registry_entry *entry)
{
	if (!(flags & GPO_INFO_FLAG_VERBOSE))
		return;

	dump_reg_val(1, dir,
		     entry->key,
		     entry->value,
		     entry->data);
}

/****************************************************************
****************************************************************/

void dump_reg_entries(uint32_t flags,
		      const char *dir,
		      struct gp_registry_entry *entries,
		      size_t num_entries)
{
	size_t i;

	if (!(flags & GPO_INFO_FLAG_VERBOSE))
		return;

	for (i=0; i < num_entries; i++) {
		dump_reg_entry(flags, dir, &entries[i]);
	}
}

/****************************************************************
****************************************************************/

bool add_gp_registry_entry_to_array(TALLOC_CTX *mem_ctx,
				    struct gp_registry_entry *entry,
				    struct gp_registry_entry **entries,
				    size_t *num)
{
	*entries = TALLOC_REALLOC_ARRAY(mem_ctx, *entries,
					struct gp_registry_entry,
					(*num)+1);

	if (*entries == NULL) {
		*num = 0;
		return false;
	}

	(*entries)[*num].action = entry->action;
	(*entries)[*num].key = entry->key;
	(*entries)[*num].value = entry->value;
	(*entries)[*num].data = entry->data;

	*num += 1;
	return true;
}

/****************************************************************
****************************************************************/

static const char *gp_reg_action_str(enum gp_reg_action action)
{
	switch (action) {
		case GP_REG_ACTION_NONE:
			return "GP_REG_ACTION_NONE";
		case GP_REG_ACTION_ADD_VALUE:
			return "GP_REG_ACTION_ADD_VALUE";
		case GP_REG_ACTION_ADD_KEY:
			return "GP_REG_ACTION_ADD_KEY";
		case GP_REG_ACTION_DEL_VALUES:
			return "GP_REG_ACTION_DEL_VALUES";
		case GP_REG_ACTION_DEL_VALUE:
			return "GP_REG_ACTION_DEL_VALUE";
		case GP_REG_ACTION_DEL_ALL_VALUES:
			return "GP_REG_ACTION_DEL_ALL_VALUES";
		case GP_REG_ACTION_DEL_KEYS:
			return "GP_REG_ACTION_DEL_KEYS";
		case GP_REG_ACTION_SEC_KEY_SET:
			return "GP_REG_ACTION_SEC_KEY_SET";
		case GP_REG_ACTION_SEC_KEY_RESET:
			return "GP_REG_ACTION_SEC_KEY_RESET";
		default:
			return "unknown";
	}
}

/****************************************************************
****************************************************************/

WERROR reg_apply_registry_entry(TALLOC_CTX *mem_ctx,
				struct registry_key *root_key,
				struct gp_registry_context *reg_ctx,
				struct gp_registry_entry *entry,
				const struct nt_user_token *token,
				uint32_t flags)
{
	WERROR werr;
	struct registry_key *key = NULL;

	if (flags & GPO_INFO_FLAG_VERBOSE) {
		printf("about to store key:    [%s]\n", entry->key);
		printf("               value:  [%s]\n", entry->value);
		printf("               data:   [%s]\n", reg_type_lookup(entry->data->type));
		printf("               action: [%s]\n", gp_reg_action_str(entry->action));
	}

	werr = gp_store_reg_subkey(mem_ctx, entry->key,
				   root_key, &key);
				   /* reg_ctx->curr_key, &key); */
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("gp_store_reg_subkey failed: %s\n", win_errstr(werr)));
		return werr;
	}

	switch (entry->action) {
		case GP_REG_ACTION_NONE:
		case GP_REG_ACTION_ADD_KEY:
			return WERR_OK;

		case GP_REG_ACTION_SEC_KEY_SET:
			werr = gp_secure_key(mem_ctx, flags,
					     key,
					     &token->user_sids[0]);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,("reg_apply_registry_entry: "
					"gp_secure_key failed: %s\n",
					win_errstr(werr)));
				return werr;
			}
			break;
		case GP_REG_ACTION_ADD_VALUE:
			werr = reg_setvalue(key, entry->value, entry->data);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,("reg_apply_registry_entry: "
					"reg_setvalue failed: %s\n",
					win_errstr(werr)));
				dump_reg_entry(flags, "STORE", entry);
				return werr;
			}
			break;
		case GP_REG_ACTION_DEL_VALUE:
			werr = reg_deletevalue(key, entry->value);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,("reg_apply_registry_entry: "
					"reg_deletevalue failed: %s\n",
					win_errstr(werr)));
				dump_reg_entry(flags, "STORE", entry);
				return werr;
			}
			break;
		case GP_REG_ACTION_DEL_ALL_VALUES:
			werr = reg_deleteallvalues(key);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,("reg_apply_registry_entry: "
					"reg_deleteallvalues failed: %s\n",
					win_errstr(werr)));
				dump_reg_entry(flags, "STORE", entry);
				return werr;
			}
			break;
		case GP_REG_ACTION_DEL_VALUES:
		case GP_REG_ACTION_DEL_KEYS:
		case GP_REG_ACTION_SEC_KEY_RESET:
			DEBUG(0,("reg_apply_registry_entry: "
				"not yet supported: %s (%d)\n",
				gp_reg_action_str(entry->action),
				entry->action));
			return WERR_NOT_SUPPORTED;
		default:
			DEBUG(0,("invalid action: %d\n", entry->action));
			return WERR_INVALID_PARAM;
	}

	return werr;
}

