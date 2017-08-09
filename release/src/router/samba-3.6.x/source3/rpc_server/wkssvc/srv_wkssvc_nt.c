/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *
 *  Copyright (C) Andrew Tridgell		1992-1997,
 *  Copyright (C) Gerald (Jerry) Carter		2006.
 *  Copyright (C) Guenther Deschner		2007-2008.
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

/* This is the implementation of the wks interface. */

#include "includes.h"
#include "ntdomain.h"
#include "librpc/gen_ndr/libnet_join.h"
#include "libnet/libnet_join.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/srv_wkssvc.h"
#include "../libcli/security/security.h"
#include "session.h"
#include "smbd/smbd.h"
#include "auth.h"
#include "krb5_env.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

struct dom_usr {
	char *name;
	char *domain;
	time_t login_time;
};

#ifdef HAVE_GETUTXENT

#include <utmpx.h>

struct usrinfo {
	char *name;
	struct timeval login_time;
};

static int usr_info_cmp(const struct usrinfo *usr1, const struct usrinfo *usr2)
{
	/* Called from qsort to compare two users in a usrinfo_t array for
	 * sorting by login time. Return >0 if usr1 login time was later than
	 * usr2 login time, <0 if it was earlier */
	return timeval_compare(&usr1->login_time, &usr2->login_time);
}

/*******************************************************************
 Get a list of the names of all users logged into this machine
 ********************************************************************/

static char **get_logged_on_userlist(TALLOC_CTX *mem_ctx)
{
	char **users;
	int i, num_users = 0;
	struct usrinfo *usr_infos = NULL;
	struct utmpx *u;

	while ((u = getutxent()) != NULL) {
		struct usrinfo *tmp;
		if (u->ut_type != USER_PROCESS) {
			continue;
		}
		for (i = 0; i < num_users; i++) {
			/* getutxent can return multiple user entries for the
			 * same user, so ignore any dups */
			if (strcmp(u->ut_user, usr_infos[i].name) == 0) {
				break;
			}
		}
		if (i < num_users) {
			continue;
		}

		tmp = talloc_realloc(mem_ctx, usr_infos, struct usrinfo,
				     num_users+1);
		if (tmp == NULL) {
			TALLOC_FREE(tmp);
			endutxent();
			return NULL;
		}
		usr_infos = tmp;
		usr_infos[num_users].name = talloc_strdup(usr_infos,
							  u->ut_user);
		if (usr_infos[num_users].name == NULL) {
			TALLOC_FREE(usr_infos);
			endutxent();
			return NULL;
		}
		usr_infos[num_users].login_time.tv_sec = u->ut_tv.tv_sec;
		usr_infos[num_users].login_time.tv_usec = u->ut_tv.tv_usec;
		num_users += 1;
	}

	/* Sort the user list by time, oldest first */
	TYPESAFE_QSORT(usr_infos, num_users, usr_info_cmp);

	users = (char**)talloc_array(mem_ctx, char*, num_users);
	if (users) {
		for (i = 0; i < num_users; i++) {
			users[i] = talloc_move(users, &usr_infos[i].name);
		}
	}
	TALLOC_FREE(usr_infos);
	endutxent();
	errno = 0;
	return users;
}

#else

static char **get_logged_on_userlist(TALLOC_CTX *mem_ctx)
{
	return NULL;
}

#endif

static int dom_user_cmp(const struct dom_usr *usr1, const struct dom_usr *usr2)
{
	/* Called from qsort to compare two domain users in a dom_usr_t array
	 * for sorting by login time. Return >0 if usr1 login time was later
	 * than usr2 login time, <0 if it was earlier */
	return (usr1->login_time - usr2->login_time);
}

/*******************************************************************
 Get a list of the names of all users of this machine who are
 logged into the domain.

 This should return a list of the users on this machine who are
 logged into the domain (i.e. have been authenticated by the domain's
 password server) but that doesn't fit well with the normal Samba
 scenario where accesses out to the domain are made through smbclient
 with each such session individually authenticated. So about the best
 we can do currently is to list sessions of local users connected to
 this server, which means that to get themself included in the list a
 local user must create a session to the local samba server by running:
     smbclient \\\\localhost\\share

 FIXME: find a better way to get local users logged into the domain
 in this list.
 ********************************************************************/

static struct dom_usr *get_domain_userlist(TALLOC_CTX *mem_ctx)
{
	struct sessionid *session_list = NULL;
	char *machine_name, *p, *nm;
	const char *sep;
	struct dom_usr *users, *tmp;
	int i, num_users, num_sessions;

	sep = lp_winbind_separator();
	if (!sep) {
		sep = "\\";
	}

	num_sessions = list_sessions(mem_ctx, &session_list);
	if (num_sessions == 0) {
		errno = 0;
		return NULL;
	}

	users = talloc_array(mem_ctx, struct dom_usr, num_sessions);
	if (users == NULL) {
		TALLOC_FREE(session_list);
		return NULL;
	}

	for (i=num_users=0; i<num_sessions; i++) {
		if (!session_list[i].username
		    || !session_list[i].remote_machine) {
			continue;
		}
		p = strpbrk(session_list[i].remote_machine, "./");
		if (p) {
			*p = '\0';
		}
		machine_name = talloc_asprintf_strupper_m(
			users, "%s", session_list[i].remote_machine);
		if (machine_name == NULL) {
			DEBUG(10, ("talloc_asprintf failed\n"));
			continue;
		}
		if (strcmp(machine_name, global_myname()) == 0) {
			p = session_list[i].username;
			nm = strstr(p, sep);
			if (nm) {
				/*
				 * "domain+name" format so split domain and
				 * name components
				 */
				*nm = '\0';
				nm += strlen(sep);
				users[num_users].domain =
					talloc_asprintf_strupper_m(users,
								   "%s", p);
				users[num_users].name = talloc_strdup(users,
								      nm);
			} else {
				/*
				 * Simple user name so get domain from smb.conf
				 */
				users[num_users].domain =
					talloc_strdup(users, lp_workgroup());
				users[num_users].name = talloc_strdup(users,
								      p);
			}
			users[num_users].login_time =
				session_list[i].connect_start;
			num_users++;
		}
		TALLOC_FREE(machine_name);
	}
	TALLOC_FREE(session_list);

	tmp = talloc_realloc(mem_ctx, users, struct dom_usr, num_users);
	if (tmp == NULL) {
		return NULL;
	}
	users = tmp;

	/* Sort the user list by time, oldest first */
	TYPESAFE_QSORT(users, num_users, dom_user_cmp);

	errno = 0;
	return users;
}

/*******************************************************************
 RPC Workstation Service request NetWkstaGetInfo with level 100.
 Returns to the requester:
  - The machine name.
  - The smb version number
  - The domain name.
 Returns a filled in wkssvc_NetWkstaInfo100 struct.
 ********************************************************************/

static struct wkssvc_NetWkstaInfo100 *create_wks_info_100(TALLOC_CTX *mem_ctx)
{
	struct wkssvc_NetWkstaInfo100 *info100;

	info100 = talloc(mem_ctx, struct wkssvc_NetWkstaInfo100);
	if (info100 == NULL) {
		return NULL;
	}

	info100->platform_id	 = PLATFORM_ID_NT;	/* unknown */
	info100->version_major	 = lp_major_announce_version();
	info100->version_minor	 = lp_minor_announce_version();

	info100->server_name = talloc_asprintf_strupper_m(
		info100, "%s", global_myname());
	info100->domain_name = talloc_asprintf_strupper_m(
		info100, "%s", lp_workgroup());

	return info100;
}

/*******************************************************************
 RPC Workstation Service request NetWkstaGetInfo with level 101.
 Returns to the requester:
  - As per NetWkstaGetInfo with level 100, plus:
  - The LANMAN directory path (not currently supported).
 Returns a filled in wkssvc_NetWkstaInfo101 struct.
 ********************************************************************/

static struct wkssvc_NetWkstaInfo101 *create_wks_info_101(TALLOC_CTX *mem_ctx)
{
	struct wkssvc_NetWkstaInfo101 *info101;

	info101 = talloc(mem_ctx, struct wkssvc_NetWkstaInfo101);
	if (info101 == NULL) {
		return NULL;
	}

	info101->platform_id	 = PLATFORM_ID_NT;	/* unknown */
	info101->version_major	 = lp_major_announce_version();
	info101->version_minor	 = lp_minor_announce_version();

	info101->server_name = talloc_asprintf_strupper_m(
		info101, "%s", global_myname());
	info101->domain_name = talloc_asprintf_strupper_m(
		info101, "%s", lp_workgroup());
	info101->lan_root = "";

	return info101;
}

/*******************************************************************
 RPC Workstation Service request NetWkstaGetInfo with level 102.
 Returns to the requester:
  - As per NetWkstaGetInfo with level 101, plus:
  - The number of logged in users.
 Returns a filled in wkssvc_NetWkstaInfo102 struct.
 ********************************************************************/

static struct wkssvc_NetWkstaInfo102 *create_wks_info_102(TALLOC_CTX *mem_ctx)
{
	struct wkssvc_NetWkstaInfo102 *info102;
	char **users;

	info102 = talloc(mem_ctx, struct wkssvc_NetWkstaInfo102);
	if (info102 == NULL) {
		return NULL;
	}

	info102->platform_id	 = PLATFORM_ID_NT;	/* unknown */
	info102->version_major	 = lp_major_announce_version();
	info102->version_minor	 = lp_minor_announce_version();

	info102->server_name = talloc_asprintf_strupper_m(
		info102, "%s", global_myname());
	info102->domain_name = talloc_asprintf_strupper_m(
		info102, "%s", lp_workgroup());
	info102->lan_root = "";

	users = get_logged_on_userlist(talloc_tos());
	info102->logged_on_users = talloc_array_length(users);

	TALLOC_FREE(users);

	return info102;
}

/********************************************************************
 Handling for RPC Workstation Service request NetWkstaGetInfo
 ********************************************************************/

WERROR _wkssvc_NetWkstaGetInfo(struct pipes_struct *p,
			       struct wkssvc_NetWkstaGetInfo *r)
{
	switch (r->in.level) {
	case 100:
		/* Level 100 can be allowed from anyone including anonymous
		 * so no access checks are needed for this case */
		r->out.info->info100 = create_wks_info_100(p->mem_ctx);
		if (r->out.info->info100 == NULL) {
			return WERR_NOMEM;
		}
		break;
	case 101:
		/* Level 101 can be allowed from any logged in user */
		if (!nt_token_check_sid(&global_sid_Authenticated_Users,
					p->session_info->security_token)) {
			DEBUG(1,("User not allowed for NetWkstaGetInfo level "
				 "101\n"));
			DEBUGADD(3,(" - does not have sid for Authenticated "
				    "Users %s:\n",
				    sid_string_dbg(
					    &global_sid_Authenticated_Users)));
			security_token_debug(DBGC_CLASS, 3,
					    p->session_info->security_token);
			return WERR_ACCESS_DENIED;
		}
		r->out.info->info101 = create_wks_info_101(p->mem_ctx);
		if (r->out.info->info101 == NULL) {
			return WERR_NOMEM;
		}
		break;
	case 102:
		/* Level 102 Should only be allowed from a domain administrator */
		if (!nt_token_check_sid(&global_sid_Builtin_Administrators,
					p->session_info->security_token)) {
			DEBUG(1,("User not allowed for NetWkstaGetInfo level "
				 "102\n"));
			DEBUGADD(3,(" - does not have sid for Administrators "
				    "group %s, sids are:\n",
				    sid_string_dbg(&global_sid_Builtin_Administrators)));
			security_token_debug(DBGC_CLASS, 3,
					    p->session_info->security_token);
			return WERR_ACCESS_DENIED;
		}
		r->out.info->info102 = create_wks_info_102(p->mem_ctx);
		if (r->out.info->info102 == NULL) {
			return WERR_NOMEM;
		}
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_OK;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetWkstaSetInfo(struct pipes_struct *p,
			       struct wkssvc_NetWkstaSetInfo *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 RPC Workstation Service request NetWkstaEnumUsers with level 0:
 Returns to the requester:
  - the user names of the logged in users.
 Returns a filled in wkssvc_NetWkstaEnumUsersCtr0 struct.
 ********************************************************************/

static struct wkssvc_NetWkstaEnumUsersCtr0 *create_enum_users0(
	TALLOC_CTX *mem_ctx)
{
	struct wkssvc_NetWkstaEnumUsersCtr0 *ctr0;
	char **users;
	int i, num_users;

	ctr0 = talloc(mem_ctx, struct wkssvc_NetWkstaEnumUsersCtr0);
	if (ctr0 == NULL) {
		return NULL;
	}

	users = get_logged_on_userlist(talloc_tos());
	if (users == NULL && errno != 0) {
		DEBUG(1,("get_logged_on_userlist error %d: %s\n",
			errno, strerror(errno)));
		TALLOC_FREE(ctr0);
		return NULL;
	}

	num_users = talloc_array_length(users);
	ctr0->entries_read = num_users;
	ctr0->user0 = talloc_array(ctr0, struct wkssvc_NetrWkstaUserInfo0,
				   num_users);
	if (ctr0->user0 == NULL) {
		TALLOC_FREE(ctr0);
		TALLOC_FREE(users);
		return NULL;
	}

	for (i=0; i<num_users; i++) {
		ctr0->user0[i].user_name = talloc_move(ctr0->user0, &users[i]);
	}
	TALLOC_FREE(users);
	return ctr0;
}

/********************************************************************
 RPC Workstation Service request NetWkstaEnumUsers with level 1.
 Returns to the requester:
  - the user names of the logged in users,
  - the domain or machine each is logged into,
  - the password server that was used to authenticate each,
  - other domains each user is logged into (not currently supported).
 Returns a filled in wkssvc_NetWkstaEnumUsersCtr1 struct.
 ********************************************************************/

static struct wkssvc_NetWkstaEnumUsersCtr1 *create_enum_users1(
	TALLOC_CTX *mem_ctx)
{
	struct wkssvc_NetWkstaEnumUsersCtr1 *ctr1;
	char **users;
	struct dom_usr *dom_users;
	const char *pwd_server;
	char *pwd_tmp;
	int i, j, num_users, num_dom_users;

	ctr1 = talloc(mem_ctx, struct wkssvc_NetWkstaEnumUsersCtr1);
	if (ctr1 == NULL) {
		return NULL;
	}

	users = get_logged_on_userlist(talloc_tos());
	if (users == NULL && errno != 0) {
		DEBUG(1,("get_logged_on_userlist error %d: %s\n",
			errno, strerror(errno)));
		TALLOC_FREE(ctr1);
		return NULL;
	}
	num_users = talloc_array_length(users);

	dom_users = get_domain_userlist(talloc_tos());
	if (dom_users == NULL && errno != 0) {
		TALLOC_FREE(ctr1);
		TALLOC_FREE(users);
		return NULL;
	}
	num_dom_users = talloc_array_length(dom_users);

	ctr1->user1 = talloc_array(ctr1, struct wkssvc_NetrWkstaUserInfo1,
				   num_users+num_dom_users);
	if (ctr1->user1 == NULL) {
		TALLOC_FREE(ctr1);
		TALLOC_FREE(users);
		TALLOC_FREE(dom_users);
		return NULL;
	}

	pwd_server = "";

	if ((pwd_tmp = talloc_strdup(ctr1->user1, lp_passwordserver()))) {
		/* The configured password server is a full DNS name but
		 * for the logon server we need to return just the first
		 * component (machine name) of it in upper-case */
		char *p = strchr(pwd_tmp, '.');
		if (p) {
			*p = '\0';
		} else {
			p = pwd_tmp + strlen(pwd_tmp);
		}
		while (--p >= pwd_tmp) {
			*p = toupper(*p);
		}
		pwd_server = pwd_tmp;
	}

	/* Put in local users first */
	for (i=0; i<num_users; i++) {
		ctr1->user1[i].user_name = talloc_move(ctr1->user1, &users[i]);

		/* For a local user the domain name and logon server are
		 * both returned as the local machine's NetBIOS name */
		ctr1->user1[i].logon_domain = ctr1->user1[i].logon_server =
			talloc_asprintf_strupper_m(ctr1->user1, "%s", global_myname());

		ctr1->user1[i].other_domains = NULL;	/* Maybe in future? */
	}

	/* Now domain users */
	for (j=0; j<num_dom_users; j++) {
		ctr1->user1[i].user_name =
				talloc_strdup(ctr1->user1, dom_users[j].name);
		ctr1->user1[i].logon_domain =
				talloc_strdup(ctr1->user1, dom_users[j].domain);
		ctr1->user1[i].logon_server = pwd_server;

		ctr1->user1[i++].other_domains = NULL;	/* Maybe in future? */
	}

	ctr1->entries_read = i;

	TALLOC_FREE(users);
	TALLOC_FREE(dom_users);
	return ctr1;
}

/********************************************************************
 Handling for RPC Workstation Service request NetWkstaEnumUsers
 (a.k.a Windows NetWkstaUserEnum)
 ********************************************************************/

WERROR _wkssvc_NetWkstaEnumUsers(struct pipes_struct *p,
				 struct wkssvc_NetWkstaEnumUsers *r)
{
	/* This with any level should only be allowed from a domain administrator */
	if (!nt_token_check_sid(&global_sid_Builtin_Administrators,
				p->session_info->security_token)) {
		DEBUG(1,("User not allowed for NetWkstaEnumUsers\n"));
		DEBUGADD(3,(" - does not have sid for Administrators group "
			    "%s\n", sid_string_dbg(
				    &global_sid_Builtin_Administrators)));
		security_token_debug(DBGC_CLASS, 3, p->session_info->security_token);
		return WERR_ACCESS_DENIED;
	}

	switch (r->in.info->level) {
	case 0:
		r->out.info->ctr.user0 = create_enum_users0(p->mem_ctx);
		if (r->out.info->ctr.user0 == NULL) {
			return WERR_NOMEM;
		}
		r->out.info->level = r->in.info->level;
		*r->out.entries_read = r->out.info->ctr.user0->entries_read;
		if (r->out.resume_handle != NULL) {
			*r->out.resume_handle = 0;
		}
		break;
	case 1:
		r->out.info->ctr.user1 = create_enum_users1(p->mem_ctx);
		if (r->out.info->ctr.user1 == NULL) {
			return WERR_NOMEM;
		}
		r->out.info->level = r->in.info->level;
		*r->out.entries_read = r->out.info->ctr.user1->entries_read;
		if (r->out.resume_handle != NULL) {
			*r->out.resume_handle = 0;
		}
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	return WERR_OK;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrWkstaUserGetInfo(struct pipes_struct *p,
				    struct wkssvc_NetrWkstaUserGetInfo *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrWkstaUserSetInfo(struct pipes_struct *p,
				    struct wkssvc_NetrWkstaUserSetInfo *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetWkstaTransportEnum(struct pipes_struct *p,
				     struct wkssvc_NetWkstaTransportEnum *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrWkstaTransportAdd(struct pipes_struct *p,
				     struct wkssvc_NetrWkstaTransportAdd *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrWkstaTransportDel(struct pipes_struct *p,
				     struct wkssvc_NetrWkstaTransportDel *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrUseAdd(struct pipes_struct *p,
			  struct wkssvc_NetrUseAdd *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrUseGetInfo(struct pipes_struct *p,
			      struct wkssvc_NetrUseGetInfo *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrUseDel(struct pipes_struct *p,
			  struct wkssvc_NetrUseDel *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrUseEnum(struct pipes_struct *p,
			   struct wkssvc_NetrUseEnum *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrMessageBufferSend(struct pipes_struct *p,
				     struct wkssvc_NetrMessageBufferSend *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrWorkstationStatisticsGet(struct pipes_struct *p,
					    struct wkssvc_NetrWorkstationStatisticsGet *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrLogonDomainNameAdd(struct pipes_struct *p,
				      struct wkssvc_NetrLogonDomainNameAdd *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrLogonDomainNameDel(struct pipes_struct *p,
				      struct wkssvc_NetrLogonDomainNameDel *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrJoinDomain(struct pipes_struct *p,
			      struct wkssvc_NetrJoinDomain *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrUnjoinDomain(struct pipes_struct *p,
				struct wkssvc_NetrUnjoinDomain *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrRenameMachineInDomain(struct pipes_struct *p,
					 struct wkssvc_NetrRenameMachineInDomain *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrValidateName(struct pipes_struct *p,
				struct wkssvc_NetrValidateName *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrGetJoinInformation(struct pipes_struct *p,
				      struct wkssvc_NetrGetJoinInformation *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrGetJoinableOus(struct pipes_struct *p,
				  struct wkssvc_NetrGetJoinableOus *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 _wkssvc_NetrJoinDomain2
 ********************************************************************/

WERROR _wkssvc_NetrJoinDomain2(struct pipes_struct *p,
			       struct wkssvc_NetrJoinDomain2 *r)
{
	struct libnet_JoinCtx *j = NULL;
	char *cleartext_pwd = NULL;
	char *admin_domain = NULL;
	char *admin_account = NULL;
	WERROR werr;
	struct security_token *token = p->session_info->security_token;

#ifndef NETLOGON_SUPPORT
	return WERR_NOT_SUPPORTED;
#endif

	if (!r->in.domain_name) {
		return WERR_INVALID_PARAM;
	}

	if (!r->in.admin_account || !r->in.encrypted_password) {
		return WERR_INVALID_PARAM;
	}

	if (!security_token_has_privilege(token, SEC_PRIV_MACHINE_ACCOUNT) &&
	    !nt_token_check_domain_rid(token, DOMAIN_RID_ADMINS) &&
	    !nt_token_check_sid(&global_sid_Builtin_Administrators, token)) {
		DEBUG(5,("_wkssvc_NetrJoinDomain2: account doesn't have "
			"sufficient privileges\n"));
		return WERR_ACCESS_DENIED;
	}

	if ((r->in.join_flags & WKSSVC_JOIN_FLAGS_MACHINE_PWD_PASSED) ||
	    (r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_UNSECURE)) {
		return WERR_NOT_SUPPORTED;
	}

	werr = decode_wkssvc_join_password_buffer(
		p->mem_ctx, r->in.encrypted_password,
		&p->session_info->user_session_key, &cleartext_pwd);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	split_domain_user(p->mem_ctx,
			  r->in.admin_account,
			  &admin_domain,
			  &admin_account);

	werr = libnet_init_JoinCtx(p->mem_ctx, &j);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	j->in.domain_name	= r->in.domain_name;
	j->in.account_ou	= r->in.account_ou;
	j->in.join_flags	= r->in.join_flags;
	j->in.admin_account	= admin_account;
	j->in.admin_password	= cleartext_pwd;
	j->in.debug		= true;
	j->in.modify_config     = lp_config_backend_is_registry();
	j->in.msg_ctx		= p->msg_ctx;

	become_root();
	setenv(KRB5_ENV_CCNAME, "MEMORY:_wkssvc_NetrJoinDomain2", 1);
	werr = libnet_Join(p->mem_ctx, j);
	unsetenv(KRB5_ENV_CCNAME);
	unbecome_root();

	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5,("_wkssvc_NetrJoinDomain2: libnet_Join failed with: %s\n",
			j->out.error_string ? j->out.error_string :
			win_errstr(werr)));
	}

	TALLOC_FREE(j);
	return werr;
}

/********************************************************************
 _wkssvc_NetrUnjoinDomain2
 ********************************************************************/

WERROR _wkssvc_NetrUnjoinDomain2(struct pipes_struct *p,
				 struct wkssvc_NetrUnjoinDomain2 *r)
{
	struct libnet_UnjoinCtx *u = NULL;
	char *cleartext_pwd = NULL;
	char *admin_domain = NULL;
	char *admin_account = NULL;
	WERROR werr;
	struct security_token *token = p->session_info->security_token;

#ifndef NETLOGON_SUPPORT
	return WERR_NOT_SUPPORTED;
#endif

	if (!r->in.account || !r->in.encrypted_password) {
		return WERR_INVALID_PARAM;
	}

	if (!security_token_has_privilege(token, SEC_PRIV_MACHINE_ACCOUNT) &&
	    !nt_token_check_domain_rid(token, DOMAIN_RID_ADMINS) &&
	    !nt_token_check_sid(&global_sid_Builtin_Administrators, token)) {
		DEBUG(5,("_wkssvc_NetrUnjoinDomain2: account doesn't have "
			"sufficient privileges\n"));
		return WERR_ACCESS_DENIED;
	}

	werr = decode_wkssvc_join_password_buffer(
		p->mem_ctx, r->in.encrypted_password,
		&p->session_info->user_session_key, &cleartext_pwd);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	split_domain_user(p->mem_ctx,
			  r->in.account,
			  &admin_domain,
			  &admin_account);

	werr = libnet_init_UnjoinCtx(p->mem_ctx, &u);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	u->in.domain_name	= lp_realm();
	u->in.unjoin_flags	= r->in.unjoin_flags |
				  WKSSVC_JOIN_FLAGS_JOIN_TYPE;
	u->in.admin_account	= admin_account;
	u->in.admin_password	= cleartext_pwd;
	u->in.debug		= true;
	u->in.modify_config     = lp_config_backend_is_registry();
	u->in.msg_ctx		= p->msg_ctx;

	become_root();
	setenv(KRB5_ENV_CCNAME, "MEMORY:_wkssvc_NetrUnjoinDomain2", 1);
	werr = libnet_Unjoin(p->mem_ctx, u);
	unsetenv(KRB5_ENV_CCNAME);
	unbecome_root();

	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(5,("_wkssvc_NetrUnjoinDomain2: libnet_Unjoin failed with: %s\n",
			u->out.error_string ? u->out.error_string :
			win_errstr(werr)));
	}

	TALLOC_FREE(u);
	return werr;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrRenameMachineInDomain2(struct pipes_struct *p,
					  struct wkssvc_NetrRenameMachineInDomain2 *r)
{
	/* for now just return not supported */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrValidateName2(struct pipes_struct *p,
				 struct wkssvc_NetrValidateName2 *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrGetJoinableOus2(struct pipes_struct *p,
				   struct wkssvc_NetrGetJoinableOus2 *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrAddAlternateComputerName(struct pipes_struct *p,
					    struct wkssvc_NetrAddAlternateComputerName *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrRemoveAlternateComputerName(struct pipes_struct *p,
					       struct wkssvc_NetrRemoveAlternateComputerName *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrSetPrimaryComputername(struct pipes_struct *p,
					  struct wkssvc_NetrSetPrimaryComputername *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/********************************************************************
 ********************************************************************/

WERROR _wkssvc_NetrEnumerateComputerNames(struct pipes_struct *p,
					  struct wkssvc_NetrEnumerateComputerNames *r)
{
	/* FIXME: Add implementation code here */
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}
