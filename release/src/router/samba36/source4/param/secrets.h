/*
 * Unix SMB/CIFS implementation. 
 * secrets.tdb file format info
 * Copyright (C) Andrew Tridgell              2000
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.  
 */

#ifndef _SECRETS_H
#define _SECRETS_H

#define SECRETS_PRIMARY_DOMAIN_DN "cn=Primary Domains"
#define SECRETS_PRINCIPALS_DN "cn=Principals"
#define SECRETS_PRIMARY_DOMAIN_FILTER "(&(flatname=%s)(objectclass=primaryDomain))"
#define SECRETS_PRIMARY_REALM_FILTER "(&(realm=%s)(objectclass=primaryDomain))"
#define SECRETS_KRBTGT_SEARCH "(&((|(realm=%s)(flatname=%s))(samAccountName=krbtgt)))"
#define SECRETS_PRINCIPAL_SEARCH "(&(|(realm=%s)(flatname=%s))(servicePrincipalName=%s))"
#define SECRETS_LDAP_FILTER "(&(objectclass=ldapSecret)(cn=SAMDB Credentials))"

/**
 * Use a TDB to store an incrementing random seed.
 *
 * Initialised to the current pid, the very first time Samba starts,
 * and incremented by one each time it is needed.  
 * 
 * @note Not called by systems with a working /dev/urandom.
 */
struct loadparm_context;
struct tevent_context;
struct ldb_message;
struct ldb_context;

#include "librpc/gen_ndr/misc.h"

struct tdb_wrap *secrets_init(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx);
struct ldb_context *secrets_db_connect(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx);
struct dom_sid *secrets_get_domain_sid(TALLOC_CTX *mem_ctx,
				       struct loadparm_context *lp_ctx,
				       const char *domain,
				       enum netr_SchannelType *sec_channel_type,
				       char **errstring);
char *keytab_name_from_msg(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, struct ldb_message *msg);


#endif /* _SECRETS_H */
