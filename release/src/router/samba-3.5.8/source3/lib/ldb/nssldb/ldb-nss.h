/* 
   LDB nsswitch module

   Copyright (C) Simo Sorce 2006
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LDB_NSS
#define _LDB_NSS

#include "includes.h"
#include "ldb/include/includes.h"

#include <nss.h>
#include <pwd.h>
#include <grp.h>

#define _LDB_NSS_URL "etc/users.ldb"
#define _LDB_NSS_BASEDN "CN=Users,CN=System"
#define _LDB_NSS_PWENT_FILTER "(&(objectClass=posixAccount)(!(uidNumber=0))(!(gidNumber=0)))"
#define _LDB_NSS_PWUID_FILTER "(&(objectClass=posixAccount)(uidNumber=%d)(!(gidNumber=0)))"
#define _LDB_NSS_PWNAM_FILTER "(&(objectClass=posixAccount)(uid=%s)(!(uidNumber=0))(!(gidNumber=0)))"

#define _LDB_NSS_GRENT_FILTER "(&(objectClass=posixGroup)(!(gidNumber=0)))"
#define _LDB_NSS_GRGID_FILTER "(&(objectClass=posixGroup)(gidNumber=%d)))"
#define _LDB_NSS_GRNAM_FILTER "(&(objectClass=posixGroup)(cn=%s)(!(gidNumber=0)))"

typedef enum nss_status NSS_STATUS;

struct _ldb_nss_context {

	pid_t pid;

	struct ldb_context *ldb;
	const struct ldb_dn *base;

	int pw_cur;
	struct ldb_result *pw_res;

	int gr_cur;
	struct ldb_result *gr_res;
};
	
NSS_STATUS _ldb_nss_init(void);

NSS_STATUS _ldb_nss_fill_passwd(struct passwd *result,
				char *buffer,
				int buflen,
				int *errnop,
				struct ldb_message *msg);

NSS_STATUS _ldb_nss_fill_group(struct group *result,
				char *buffer,
				int buflen,
				int *errnop,
				struct ldb_message *group,
				struct ldb_result *members);

NSS_STATUS _ldb_nss_fill_initgr(gid_t group,
				long int limit,
				long int *start,
				long int *size,
				gid_t **groups,
				int *errnop,
				struct ldb_result *grlist);

NSS_STATUS _ldb_nss_group_request(struct ldb_result **res,
					struct ldb_dn *group_dn,
					const char * const *attrs,
					const char *mattr);

#endif /* _LDB_NSS */
