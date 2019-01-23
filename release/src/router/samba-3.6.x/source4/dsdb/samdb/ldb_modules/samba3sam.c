/*
   ldb database library - Samba3 SAM compatibility backend

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Martin Kuehl <mkhl@samba.org> 2006
*/

#include "includes.h"
#include "ldb_module.h"
#include "ldb/ldb_map/ldb_map.h"
#include "system/passwd.h"

#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "librpc/ndr/libndr.h"
#include "libcli/security/security.h"
#include "lib/samba3/samba3.h"

/*
 * sambaSID -> member  (dn!)
 * sambaSIDList -> member (dn!)
 * sambaDomainName -> name
 * sambaTrustPassword
 * sambaUnixIdPool
 * sambaIdmapEntry
 * sambaSidEntry
 * sambaAcctFlags -> systemFlags ?
 * sambaPasswordHistory  -> ntPwdHistory*/

/* Not necessary:
 * sambaConfig
 * sambaShare
 * sambaConfigOption
 * sambaNextGroupRid
 * sambaNextUserRid
 * sambaAlgorithmicRidBase
 */

/* Not in Samba4:
 * sambaKickoffTime
 * sambaPwdCanChange
 * sambaPwdMustChange
 * sambaHomePath
 * sambaHomeDrive
 * sambaLogonScript
 * sambaProfilePath
 * sambaUserWorkstations
 * sambaMungedDial
 * sambaLogonHours */

/* In Samba4 but not in Samba3:
*/

/* From a sambaPrimaryGroupSID, generate a primaryGroupID (integer) attribute */
static struct ldb_message_element *generate_primaryGroupID(struct ldb_module *module, TALLOC_CTX *ctx, const char *local_attr, const struct ldb_message *remote)
{
	struct ldb_message_element *el;
	const char *sid = ldb_msg_find_attr_as_string(remote, "sambaPrimaryGroupSID", NULL);
	const char *p;
	
	if (!sid)
		return NULL;

	p = strrchr(sid, '-');
	if (!p)
		return NULL;

	el = talloc_zero(ctx, struct ldb_message_element);
	el->name = talloc_strdup(ctx, "primaryGroupID");
	el->num_values = 1;
	el->values = talloc_array(ctx, struct ldb_val, 1);
	el->values[0].data = (uint8_t *)talloc_strdup(el->values, p+1);
	el->values[0].length = strlen((char *)el->values[0].data);

	return el;
}

static void generate_sambaPrimaryGroupSID(struct ldb_module *module, const char *local_attr, const struct ldb_message *local, struct ldb_message *remote_mp, struct ldb_message *remote_fb)
{
	const struct ldb_val *sidval;
	char *sidstring;
	struct dom_sid *sid;
	enum ndr_err_code ndr_err;

	/* We need the domain, so we get it from the objectSid that we hope is here... */
	sidval = ldb_msg_find_ldb_val(local, "objectSid");

	if (!sidval)
		return; /* Sorry, no SID today.. */

	sid = talloc(remote_mp, struct dom_sid);
	if (sid == NULL) {
		return;
	}

	ndr_err = ndr_pull_struct_blob(sidval, sid, sid, (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(sid);
		return;
	}

	if (!ldb_msg_find_ldb_val(local, "primaryGroupID"))
		return; /* Sorry, no SID today.. */

	sid->num_auths--;

	sidstring = dom_sid_string(remote_mp, sid);
	talloc_free(sid);
	ldb_msg_add_fmt(remote_mp, "sambaPrimaryGroupSID", "%s-%u", sidstring,
			ldb_msg_find_attr_as_uint(local, "primaryGroupID", 0));
	talloc_free(sidstring);
}

/* Just copy the old value. */
static struct ldb_val convert_uid_samaccount(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out = data_blob(NULL, 0);
	out = ldb_val_dup(ctx, val);

	return out;
}

static struct ldb_val lookup_homedir(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_context *ldb;
	struct passwd *pwd; 
	struct ldb_val retval;

	ldb = ldb_module_get_ctx(module);

	pwd = getpwnam((char *)val->data);

	if (!pwd) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "Unable to lookup '%s' in passwd", (char *)val->data);
		return *talloc_zero(ctx, struct ldb_val);
	}

	retval.data = (uint8_t *)talloc_strdup(ctx, pwd->pw_dir);
	retval.length = strlen((char *)retval.data);

	return retval;
}

static struct ldb_val lookup_gid(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct passwd *pwd; 
	struct ldb_val retval;

	pwd = getpwnam((char *)val->data);

	if (!pwd) {
		return *talloc_zero(ctx, struct ldb_val);
	}

	/* "pw_gid" is per POSIX definition "unsigned".
	 * But write it out as "signed" for LDAP compliance. */
	retval.data = (uint8_t *)talloc_asprintf(ctx, "%d", (int) pwd->pw_gid);
	retval.length = strlen((char *)retval.data);

	return retval;
}

static struct ldb_val lookup_uid(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct passwd *pwd; 
	struct ldb_val retval;
	
	pwd = getpwnam((char *)val->data);

	if (!pwd) {
		return *talloc_zero(ctx, struct ldb_val);
	}

	/* "pw_uid" is per POSIX definition "unsigned".
	 * But write it out as "signed" for LDAP compliance. */
	retval.data = (uint8_t *)talloc_asprintf(ctx, "%d", (int) pwd->pw_uid);
	retval.length = strlen((char *)retval.data);

	return retval;
}

/* Encode a sambaSID to an objectSid. */
static struct ldb_val encode_sid(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out = data_blob(NULL, 0);
	struct dom_sid *sid;
	enum ndr_err_code ndr_err;

	sid = dom_sid_parse_talloc(ctx, (char *)val->data);
	if (sid == NULL) {
		return out;
	}

	ndr_err = ndr_push_struct_blob(&out, ctx, 
				       sid, (ndr_push_flags_fn_t)ndr_push_dom_sid);
	talloc_free(sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return out;
	}

	return out;
}

/* Decode an objectSid to a sambaSID. */
static struct ldb_val decode_sid(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out = data_blob(NULL, 0);
	struct dom_sid *sid;
	enum ndr_err_code ndr_err;

	sid = talloc(ctx, struct dom_sid);
	if (sid == NULL) {
		return out;
	}

	ndr_err = ndr_pull_struct_blob(val, sid, sid,
				       (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		goto done;
	}

	out.data = (uint8_t *)dom_sid_string(ctx, sid);
	if (out.data == NULL) {
		goto done;
	}
	out.length = strlen((const char *)out.data);

done:
	talloc_free(sid);
	return out;
}

/* Convert 16 bytes to 32 hex digits. */
static struct ldb_val bin2hex(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out;
	struct samr_Password pwd;
	if (val->length != sizeof(pwd.hash)) {
		return data_blob(NULL, 0);
	}
	memcpy(pwd.hash, val->data, sizeof(pwd.hash));
	out = data_blob_string_const(smbpasswd_sethexpwd(ctx, &pwd, 0));
	if (!out.data) {
		return data_blob(NULL, 0);
	}
	return out;
}

/* Convert 32 hex digits to 16 bytes. */
static struct ldb_val hex2bin(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_val *val)
{
	struct ldb_val out;
	struct samr_Password *pwd;
	pwd = smbpasswd_gethexpwd(ctx, (const char *)val->data);
	if (!pwd) {
		return data_blob(NULL, 0);
	}
	out = data_blob_talloc(ctx, pwd->hash, sizeof(pwd->hash));
	return out;
}

const struct ldb_map_objectclass samba3_objectclasses[] = {
	{
		.local_name = "user",
		.remote_name = "posixAccount",
		.base_classes = { "top", NULL },
		.musts = { "cn", "uid", "uidNumber", "gidNumber", "homeDirectory", NULL },
		.mays = { "userPassword", "loginShell", "gecos", "description", NULL },
	},
	{
		.local_name = "group",
		.remote_name = "posixGroup",
		.base_classes = { "top", NULL },
		.musts = { "cn", "gidNumber", NULL },
		.mays = { "userPassword", "memberUid", "description", NULL },
	},
	{
		.local_name = "group",
		.remote_name = "sambaGroupMapping",
		.base_classes = { "top", "posixGroup", NULL },
		.musts = { "gidNumber", "sambaSID", "sambaGroupType", NULL },
		.mays = { "displayName", "description", "sambaSIDList", NULL },
	},
	{
		.local_name = "user",
		.remote_name = "sambaSAMAccount",
		.base_classes = { "top", "posixAccount", NULL },
		.musts = { "uid", "sambaSID", NULL },
		.mays = { "cn", "sambaLMPassword", "sambaNTPassword",
			"sambaPwdLastSet", "sambaLogonTime", "sambaLogoffTime",
			"sambaKickoffTime", "sambaPwdCanChange", "sambaPwdMustChange",
			"sambaAcctFlags", "displayName", "sambaHomePath", "sambaHomeDrive",
			"sambaLogonScript", "sambaProfilePath", "description", "sambaUserWorkstations",
			"sambaPrimaryGroupSID", "sambaDomainName", "sambaMungedDial",
			"sambaBadPasswordCount", "sambaBadPasswordTime",
		"sambaPasswordHistory", "sambaLogonHours", NULL }

	},
	{
		.local_name = "domain",
		.remote_name = "sambaDomain",
		.base_classes = { "top", NULL },
		.musts = { "sambaDomainName", "sambaSID", NULL },
		.mays = { "sambaNextRid", "sambaNextGroupRid", "sambaNextUserRid", "sambaAlgorithmicRidBase", NULL },
	},
		{ NULL, NULL }
};

const struct ldb_map_attribute samba3_attributes[] =
{
	/* sambaNextRid -> nextRid */
	{
		.local_name = "nextRid",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "sambaNextRid",
			},
		},
	},

	/* sambaBadPasswordTime -> badPasswordtime*/
	{
		.local_name = "badPasswordTime",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "sambaBadPasswordTime",
			},
		},
	},

	/* sambaLMPassword -> lmPwdHash*/
	{
		.local_name = "dBCSPwd",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "sambaLMPassword",
				.convert_local = bin2hex,
				.convert_remote = hex2bin,
			},
		},
	},

	/* sambaGroupType -> groupType */
	{
		.local_name = "groupType",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "sambaGroupType",
			},
		},
	},

	/* sambaNTPassword -> ntPwdHash*/
	{
		.local_name = "ntpwdhash",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "sambaNTPassword",
				.convert_local = bin2hex,
				.convert_remote = hex2bin,
			},
		},
	},

	/* sambaPrimaryGroupSID -> primaryGroupID */
	{
		.local_name = "primaryGroupID",
		.type = LDB_MAP_GENERATE,
		.u = {
			.generate = {
				.remote_names = { "sambaPrimaryGroupSID", NULL },
				.generate_local = generate_primaryGroupID,
				.generate_remote = generate_sambaPrimaryGroupSID,
			},
		},
	},

	/* sambaBadPasswordCount -> badPwdCount */
	{
		.local_name = "badPwdCount",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "sambaBadPasswordCount",
			},
		},
	},

	/* sambaLogonTime -> lastLogon*/
	{
		.local_name = "lastLogon",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "sambaLogonTime",
			},
		},
	},

	/* sambaLogoffTime -> lastLogoff*/
	{
		.local_name = "lastLogoff",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "sambaLogoffTime",
			},
		},
	},

	/* uid -> unixName */
	{
		.local_name = "unixName",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "uid",
			},
		},
	},

	/* displayName -> name */
	{
		.local_name = "name",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "displayName",
			},
		},
	},

	/* cn */
	{
		.local_name = "cn",
		.type = LDB_MAP_KEEP,
	},

	/* sAMAccountName -> cn */
	{
		.local_name = "sAMAccountName",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "uid",
				.convert_remote = convert_uid_samaccount,
			},
		},
	},

	/* objectCategory */
	{
		.local_name = "objectCategory",
		.type = LDB_MAP_IGNORE,
	},

	/* objectGUID */
	{
		.local_name = "objectGUID",
		.type = LDB_MAP_IGNORE,
	},

	/* objectVersion */
	{
		.local_name = "objectVersion",
		.type = LDB_MAP_IGNORE,
	},

	/* codePage */
	{
		.local_name = "codePage",
		.type = LDB_MAP_IGNORE,
	},

	/* dNSHostName */
	{
		.local_name = "dNSHostName",
		.type = LDB_MAP_IGNORE,
	},


	/* dnsDomain */
	{
		.local_name = "dnsDomain",
		.type = LDB_MAP_IGNORE,
	},

	/* dnsRoot */
	{
		.local_name = "dnsRoot",
		.type = LDB_MAP_IGNORE,
	},

	/* countryCode */
	{
		.local_name = "countryCode",
		.type = LDB_MAP_IGNORE,
	},

	/* nTMixedDomain */
	{
		.local_name = "nTMixedDomain",
		.type = LDB_MAP_IGNORE,
	},

	/* operatingSystem */
	{
		.local_name = "operatingSystem",
		.type = LDB_MAP_IGNORE,
	},

	/* operatingSystemVersion */
	{
		.local_name = "operatingSystemVersion",
		.type = LDB_MAP_IGNORE,
	},


	/* servicePrincipalName */
	{
		.local_name = "servicePrincipalName",
		.type = LDB_MAP_IGNORE,
	},

	/* msDS-Behavior-Version */
	{
		.local_name = "msDS-Behavior-Version",
		.type = LDB_MAP_IGNORE,
	},

	/* msDS-KeyVersionNumber */
	{
		.local_name = "msDS-KeyVersionNumber",
		.type = LDB_MAP_IGNORE,
	},

	/* msDs-masteredBy */
	{
		.local_name = "msDs-masteredBy",
		.type = LDB_MAP_IGNORE,
	},

	/* ou */
	{
		.local_name = "ou",
		.type = LDB_MAP_KEEP,
	},

	/* dc */
	{
		.local_name = "dc",
		.type = LDB_MAP_KEEP,
	},

	/* description */
	{
		.local_name = "description",
		.type = LDB_MAP_KEEP,
	},

	/* sambaSID -> objectSid*/
	{
		.local_name = "objectSid",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "sambaSID",
				.convert_local = decode_sid,
				.convert_remote = encode_sid,
			},
		},
	},

	/* sambaPwdLastSet -> pwdLastSet */
	{
		.local_name = "pwdLastSet",
		.type = LDB_MAP_RENAME,
		.u = {
			.rename = {
				.remote_name = "sambaPwdLastSet",
			},
		},
	},

	/* accountExpires */
	{
		.local_name = "accountExpires",
		.type = LDB_MAP_IGNORE,
	},

	/* adminCount */
	{
		.local_name = "adminCount",
		.type = LDB_MAP_IGNORE,
	},

	/* canonicalName */
	{
		.local_name = "canonicalName",
		.type = LDB_MAP_IGNORE,
	},

	/* createTimestamp */
	{
		.local_name = "createTimestamp",
		.type = LDB_MAP_IGNORE,
	},

	/* creationTime */
	{
		.local_name = "creationTime",
		.type = LDB_MAP_IGNORE,
	},

	/* dMDLocation */
	{
		.local_name = "dMDLocation",
		.type = LDB_MAP_IGNORE,
	},

	/* fSMORoleOwner */
	{
		.local_name = "fSMORoleOwner",
		.type = LDB_MAP_IGNORE,
	},

	/* forceLogoff */
	{
		.local_name = "forceLogoff",
		.type = LDB_MAP_IGNORE,
	},

	/* instanceType */
	{
		.local_name = "instanceType",
		.type = LDB_MAP_IGNORE,
	},

	/* invocationId */
	{
		.local_name = "invocationId",
		.type = LDB_MAP_IGNORE,
	},

	/* isCriticalSystemObject */
	{
		.local_name = "isCriticalSystemObject",
		.type = LDB_MAP_IGNORE,
	},

	/* localPolicyFlags */
	{
		.local_name = "localPolicyFlags",
		.type = LDB_MAP_IGNORE,
	},

	/* lockOutObservationWindow */
	{
		.local_name = "lockOutObservationWindow",
		.type = LDB_MAP_IGNORE,
	},

	/* lockoutDuration */
	{
		.local_name = "lockoutDuration",
		.type = LDB_MAP_IGNORE,
	},

	/* lockoutThreshold */
	{
		.local_name = "lockoutThreshold",
		.type = LDB_MAP_IGNORE,
	},

	/* logonCount */
	{
		.local_name = "logonCount",
		.type = LDB_MAP_IGNORE,
	},

	/* masteredBy */
	{
		.local_name = "masteredBy",
		.type = LDB_MAP_IGNORE,
	},

	/* maxPwdAge */
	{
		.local_name = "maxPwdAge",
		.type = LDB_MAP_IGNORE,
	},

	/* member */
	{
		.local_name = "member",
		.type = LDB_MAP_IGNORE,
	},

	/* memberOf */
	{
		.local_name = "memberOf",
		.type = LDB_MAP_IGNORE,
	},

	/* minPwdAge */
	{
		.local_name = "minPwdAge",
		.type = LDB_MAP_IGNORE,
	},

	/* minPwdLength */
	{
		.local_name = "minPwdLength",
		.type = LDB_MAP_IGNORE,
	},

	/* modifiedCount */
	{
		.local_name = "modifiedCount",
		.type = LDB_MAP_IGNORE,
	},

	/* modifiedCountAtLastProm */
	{
		.local_name = "modifiedCountAtLastProm",
		.type = LDB_MAP_IGNORE,
	},

	/* modifyTimestamp */
	{
		.local_name = "modifyTimestamp",
		.type = LDB_MAP_IGNORE,
	},

	/* nCName */
	{
		.local_name = "nCName",
		.type = LDB_MAP_IGNORE,
	},

	/* nETBIOSName */
	{
		.local_name = "nETBIOSName",
		.type = LDB_MAP_IGNORE,
	},

	/* oEMInformation */
	{
		.local_name = "oEMInformation",
		.type = LDB_MAP_IGNORE,
	},

	/* privilege */
	{
		.local_name = "privilege",
		.type = LDB_MAP_IGNORE,
	},

	/* pwdHistoryLength */
	{
		.local_name = "pwdHistoryLength",
		.type = LDB_MAP_IGNORE,
	},

	/* pwdProperties */
	{
		.local_name = "pwdProperties",
		.type = LDB_MAP_IGNORE,
	},

	/* rIDAvailablePool */
	{
		.local_name = "rIDAvailablePool",
		.type = LDB_MAP_IGNORE,
	},

	/* revision */
	{
		.local_name = "revision",
		.type = LDB_MAP_IGNORE,
	},

	/* ridManagerReference */
	{
		.local_name = "ridManagerReference",
		.type = LDB_MAP_IGNORE,
	},

	/* sAMAccountType */
	{
		.local_name = "sAMAccountType",
		.type = LDB_MAP_IGNORE,
	},

	/* sPNMappings */
	{
		.local_name = "sPNMappings",
		.type = LDB_MAP_IGNORE,
	},

	/* serverReference */
	{
		.local_name = "serverReference",
		.type = LDB_MAP_IGNORE,
	},

	/* serverState */
	{
		.local_name = "serverState",
		.type = LDB_MAP_IGNORE,
	},

	/* showInAdvancedViewOnly */
	{
		.local_name = "showInAdvancedViewOnly",
		.type = LDB_MAP_IGNORE,
	},

	/* subRefs */
	{
		.local_name = "subRefs",
		.type = LDB_MAP_IGNORE,
	},

	/* systemFlags */
	{
		.local_name = "systemFlags",
		.type = LDB_MAP_IGNORE,
	},

	/* uASCompat */
	{
		.local_name = "uASCompat",
		.type = LDB_MAP_IGNORE,
	},

	/* uSNChanged */
	{
		.local_name = "uSNChanged",
		.type = LDB_MAP_IGNORE,
	},

	/* uSNCreated */
	{
		.local_name = "uSNCreated",
		.type = LDB_MAP_IGNORE,
	},

	/* userPassword */
	{
		.local_name = "userPassword",
		.type = LDB_MAP_IGNORE,
	},

	/* userAccountControl */
	{
		.local_name = "userAccountControl",
		.type = LDB_MAP_IGNORE,
	},

	/* whenChanged */
	{
		.local_name = "whenChanged",
		.type = LDB_MAP_IGNORE,
	},

	/* whenCreated */
	{
		.local_name = "whenCreated",
		.type = LDB_MAP_IGNORE,
	},

	/* uidNumber */
	{
		.local_name = "unixName",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "uidNumber",
				.convert_local = lookup_uid,
			},
		},
	},

	/* gidNumber. Perhaps make into generate so we can distinguish between 
	 * groups and accounts? */
	{
		.local_name = "unixName",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "gidNumber",
				.convert_local = lookup_gid,
			},
		},
	},

	/* homeDirectory */
	{
		.local_name = "unixName",
		.type = LDB_MAP_CONVERT,
		.u = {
			.convert = {
				.remote_name = "homeDirectory",
				.convert_local = lookup_homedir,
			},
		},
	},
	{
		.local_name = NULL,
	}
};

/* the context init function */
static int samba3sam_init(struct ldb_module *module)
{
	int ret;

	ret = ldb_map_init(module, samba3_attributes, samba3_objectclasses, NULL, NULL, "samba3sam");
	if (ret != LDB_SUCCESS)
		return ret;

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_samba3sam_module_ops = {
	LDB_MAP_OPS
	.name		   = "samba3sam",
	.init_context	   = samba3sam_init,
};

int ldb_samba3sam_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_samba3sam_module_ops);
}
