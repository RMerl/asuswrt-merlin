/*
   Samba CIFS implementation
   ADS convenience functions for GPO

   Copyright (C) 2008 Jelmer Vernooij, jelmer@samba.org
   Copyright (C) 2008 Wilco Baan Hofman, wilco@baanhofman.nl

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

#ifndef __ADS_CONVENIENCE_H__
#define __ADS_CONVENIENCE_H__

#include "librpc/gen_ndr/security.h"

#define ADS_ERR_OK(status) ((status.error_type == ENUM_ADS_ERROR_NT) ? NT_STATUS_IS_OK(status.err.nt_status):(status.err.rc == 0))
#define ADS_ERROR(rc) ads_build_ldap_error(rc)
#define ADS_ERROR_NT(rc) ads_build_nt_error(rc)
#define ADS_SUCCESS ADS_ERROR(0)

#define ADS_ERROR_HAVE_NO_MEMORY(x) do { \
        if (!(x)) {\
                return ADS_ERROR_NT(NT_STATUS_NO_MEMORY);\
        }\
} while (0)

#define LDAP_SCOPE_BASE		LDB_SCOPE_BASE
#define LDAP_SCOPE_SUBTREE	LDB_SCOPE_SUBTREE
#define LDAP_SCOPE_ONELEVEL	LDB_SCOPE_ONELEVEL




typedef struct {
	struct libnet_context *netctx;
	struct ldb_context *ldbctx;
	char *ldap_server_name;

	/* State information for the smb connection */
	struct cli_credentials *credentials;
	struct smbcli_state *cli;
} ADS_STRUCT;


typedef struct security_token NT_USER_TOKEN;

typedef struct ldb_result LDAPMessage;
typedef void ** ADS_MODLIST;

/* there are 3 possible types of errors the ads subsystem can produce */
enum ads_error_type { ENUM_ADS_ERROR_LDAP, ENUM_ADS_ERROR_SYSTEM, ENUM_ADS_ERROR_NT};

typedef struct {
	enum ads_error_type error_type;
	union err_state{
		int rc;
		NTSTATUS nt_status;
	} err;
	int minor_status;
} ADS_STATUS;


/* Prototypes from ads_convenience.c */
ADS_STATUS ads_build_nt_error(NTSTATUS);
ADS_STATUS ads_build_ldap_error(int);

ADS_STATUS ads_startup (struct libnet_context *netctx, ADS_STRUCT **ads);
const char *ads_errstr(ADS_STATUS status);
const char * ads_get_dn(ADS_STRUCT *ads, LDAPMessage *res);
bool ads_pull_sd(ADS_STRUCT *ads, TALLOC_CTX *ctx, LDAPMessage *res, const char *field, struct security_descriptor **sd);
const char * ads_pull_string(ADS_STRUCT *ads, TALLOC_CTX *ctx, LDAPMessage *res, const char *field);
bool ads_pull_uint32(ADS_STRUCT *ads, LDAPMessage *res, const char *field, uint32_t *ret);
int ads_count_replies(ADS_STRUCT *ads, LDAPMessage *res);
ADS_STATUS ads_do_search_all_sd_flags (ADS_STRUCT *ads, const char *dn, int scope,
                                              const char *filter, const char **attrs,
                                              uint32_t sd_flags, LDAPMessage **res);
ADS_STATUS ads_search_dn(ADS_STRUCT *ads, LDAPMessage **res,
                         const char *dn, const char **attrs);
ADS_STATUS ads_search_retry_dn_sd_flags(ADS_STRUCT *ads, LDAPMessage **res, uint32_t sd_flags,
                                        const char *dn, const char **attrs);
ADS_STATUS ads_msgfree(ADS_STRUCT *ads, LDAPMessage *res);
NTSTATUS ads_ntstatus(ADS_STATUS status);
ADS_STATUS ads_build_ldap_error(int ldb_error);
ADS_STATUS ads_build_nt_error(NTSTATUS nt_status);
bool nt_token_check_sid( const struct dom_sid *sid, const NT_USER_TOKEN *token);
ADS_MODLIST ads_init_mods(TALLOC_CTX *ctx);
ADS_STATUS ads_mod_str(TALLOC_CTX *ctx, ADS_MODLIST *mods, const char *name, const char *val);
ADS_STATUS ads_gen_mod(ADS_STRUCT *ads, const char *mod_dn, ADS_MODLIST mods);
const char *ads_get_ldap_server_name(ADS_STRUCT *ads);


#endif
