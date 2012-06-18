/*
   Unix SMB/CIFS implementation.
   Password and authentication handling
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2007.

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
#include "smbd/globals.h"

enum server_allocated_state { SERVER_ALLOCATED_REQUIRED_YES,
				SERVER_ALLOCATED_REQUIRED_NO,
				SERVER_ALLOCATED_REQUIRED_ANY};

static user_struct *get_valid_user_struct_internal(
			struct smbd_server_connection *sconn,
			uint16 vuid,
			enum server_allocated_state server_allocated)
{
	user_struct *usp;
	int count=0;

	if (vuid == UID_FIELD_INVALID)
		return NULL;

	usp=sconn->smb1.sessions.validated_users;
	for (;usp;usp=usp->next,count++) {
		if (vuid == usp->vuid) {
			switch (server_allocated) {
				case SERVER_ALLOCATED_REQUIRED_YES:
					if (usp->server_info == NULL) {
						continue;
					}
					break;
				case SERVER_ALLOCATED_REQUIRED_NO:
					if (usp->server_info != NULL) {
						continue;
					}
				case SERVER_ALLOCATED_REQUIRED_ANY:
					break;
			}
			if (count > 10) {
				DLIST_PROMOTE(sconn->smb1.sessions.validated_users,
					      usp);
			}
			return usp;
		}
	}

	return NULL;
}

/****************************************************************************
 Check if a uid has been validated, and return an pointer to the user_struct
 if it has. NULL if not. vuid is biased by an offset. This allows us to
 tell random client vuid's (normally zero) from valid vuids.
****************************************************************************/

user_struct *get_valid_user_struct(struct smbd_server_connection *sconn,
				   uint16 vuid)
{
	return get_valid_user_struct_internal(sconn, vuid,
			SERVER_ALLOCATED_REQUIRED_YES);
}

bool is_partial_auth_vuid(struct smbd_server_connection *sconn, uint16 vuid)
{
	return (get_partial_auth_user_struct(sconn, vuid) != NULL);
}

/****************************************************************************
 Get the user struct of a partial NTLMSSP login
****************************************************************************/

user_struct *get_partial_auth_user_struct(struct smbd_server_connection *sconn,
					  uint16 vuid)
{
	return get_valid_user_struct_internal(sconn, vuid,
			SERVER_ALLOCATED_REQUIRED_NO);
}

/****************************************************************************
 Invalidate a uid.
****************************************************************************/

void invalidate_vuid(struct smbd_server_connection *sconn, uint16 vuid)
{
	user_struct *vuser = NULL;

	vuser = get_valid_user_struct_internal(sconn, vuid,
			SERVER_ALLOCATED_REQUIRED_ANY);
	if (vuser == NULL) {
		return;
	}

	session_yield(vuser);

	if (vuser->auth_ntlmssp_state) {
		auth_ntlmssp_end(&vuser->auth_ntlmssp_state);
	}

	DLIST_REMOVE(sconn->smb1.sessions.validated_users, vuser);

	/* clear the vuid from the 'cache' on each connection, and
	   from the vuid 'owner' of connections */
	conn_clear_vuid_caches(sconn, vuid);

	TALLOC_FREE(vuser);
	sconn->smb1.sessions.num_validated_vuids--;
}

/****************************************************************************
 Invalidate all vuid entries for this process.
****************************************************************************/

void invalidate_all_vuids(struct smbd_server_connection *sconn)
{
	if (sconn->allow_smb2) {
		return;
	}

	while (sconn->smb1.sessions.validated_users != NULL) {
		invalidate_vuid(sconn,
				sconn->smb1.sessions.validated_users->vuid);
	}
}

static void increment_next_vuid(uint16_t *vuid)
{
	*vuid += 1;

	/* Check for vuid wrap. */
	if (*vuid == UID_FIELD_INVALID) {
		*vuid = VUID_OFFSET;
	}
}

/****************************************************
 Create a new partial auth user struct.
*****************************************************/

int register_initial_vuid(struct smbd_server_connection *sconn)
{
	user_struct *vuser;

	/* Paranoia check. */
	if(lp_security() == SEC_SHARE) {
		smb_panic("register_initial_vuid: "
			"Tried to register uid in security=share");
	}

	/* Limit allowed vuids to 16bits - VUID_OFFSET. */
	if (sconn->smb1.sessions.num_validated_vuids >= 0xFFFF-VUID_OFFSET) {
		return UID_FIELD_INVALID;
	}

	if((vuser = talloc_zero(NULL, user_struct)) == NULL) {
		DEBUG(0,("register_initial_vuid: "
				"Failed to talloc users struct!\n"));
		return UID_FIELD_INVALID;
	}

	/* Allocate a free vuid. Yes this is a linear search... */
	while( get_valid_user_struct_internal(sconn,
			sconn->smb1.sessions.next_vuid,
			SERVER_ALLOCATED_REQUIRED_ANY) != NULL ) {
		increment_next_vuid(&sconn->smb1.sessions.next_vuid);
	}

	DEBUG(10,("register_initial_vuid: allocated vuid = %u\n",
		(unsigned int)sconn->smb1.sessions.next_vuid ));

	vuser->vuid = sconn->smb1.sessions.next_vuid;

	/*
	 * This happens in an unfinished NTLMSSP session setup. We
	 * need to allocate a vuid between the first and second calls
	 * to NTLMSSP.
	 */
	increment_next_vuid(&sconn->smb1.sessions.next_vuid);
	sconn->smb1.sessions.num_validated_vuids++;

	DLIST_ADD(sconn->smb1.sessions.validated_users, vuser);
	return vuser->vuid;
}

static int register_homes_share(const char *username)
{
	int result;
	struct passwd *pwd;

	result = lp_servicenumber(username);
	if (result != -1) {
		DEBUG(3, ("Using static (or previously created) service for "
			  "user '%s'; path = '%s'\n", username,
			  lp_pathname(result)));
		return result;
	}

	pwd = Get_Pwnam_alloc(talloc_tos(), username);

	if ((pwd == NULL) || (pwd->pw_dir[0] == '\0')) {
		DEBUG(3, ("No home directory defined for user '%s'\n",
			  username));
		TALLOC_FREE(pwd);
		return -1;
	}

	DEBUG(3, ("Adding homes service for user '%s' using home directory: "
		  "'%s'\n", username, pwd->pw_dir));

	result = add_home_service(username, username, pwd->pw_dir);

	TALLOC_FREE(pwd);
	return result;
}

/**
 *  register that a valid login has been performed, establish 'session'.
 *  @param server_info The token returned from the authentication process.
 *   (now 'owned' by register_existing_vuid)
 *
 *  @param session_key The User session key for the login session (now also
 *  'owned' by register_existing_vuid)
 *
 *  @param respose_blob The NT challenge-response, if available.  (May be
 *  freed after this call)
 *
 *  @param smb_name The untranslated name of the user
 *
 *  @return Newly allocated vuid, biased by an offset. (This allows us to
 *   tell random client vuid's (normally zero) from valid vuids.)
 *
 */

int register_existing_vuid(struct smbd_server_connection *sconn,
			uint16 vuid,
			auth_serversupplied_info *server_info,
			DATA_BLOB response_blob,
			const char *smb_name)
{
	fstring tmp;
	user_struct *vuser;

	vuser = get_partial_auth_user_struct(sconn, vuid);
	if (!vuser) {
		goto fail;
	}

	/* Use this to keep tabs on all our info from the authentication */
	vuser->server_info = talloc_move(vuser, &server_info);

	/* This is a potentially untrusted username */
	alpha_strcpy(tmp, smb_name, ". _-$", sizeof(tmp));

	vuser->server_info->sanitized_username = talloc_strdup(
		vuser->server_info, tmp);

	DEBUG(10,("register_existing_vuid: (%u,%u) %s %s %s guest=%d\n",
		  (unsigned int)vuser->server_info->utok.uid,
		  (unsigned int)vuser->server_info->utok.gid,
		  vuser->server_info->unix_name,
		  vuser->server_info->sanitized_username,
		  pdb_get_domain(vuser->server_info->sam_account),
		  vuser->server_info->guest ));

	DEBUG(3, ("register_existing_vuid: User name: %s\t"
		  "Real name: %s\n", vuser->server_info->unix_name,
		  pdb_get_fullname(vuser->server_info->sam_account)));

	if (!vuser->server_info->ptok) {
		DEBUG(1, ("register_existing_vuid: server_info does not "
			"contain a user_token - cannot continue\n"));
		goto fail;
	}

	DEBUG(3,("register_existing_vuid: UNIX uid %d is UNIX user %s, "
		"and will be vuid %u\n", (int)vuser->server_info->utok.uid,
		 vuser->server_info->unix_name, vuser->vuid));

	if (!session_claim(vuser)) {
		DEBUG(1, ("register_existing_vuid: Failed to claim session "
			"for vuid=%d\n",
			vuser->vuid));
		goto fail;
	}

	/* Register a home dir service for this user if
	(a) This is not a guest connection,
	(b) we have a home directory defined
	(c) there s not an existing static share by that name
	If a share exists by this name (autoloaded or not) reuse it . */

	vuser->homes_snum = -1;

	if (!vuser->server_info->guest) {
		vuser->homes_snum = register_homes_share(
			vuser->server_info->unix_name);
	}

	if (srv_is_signing_negotiated(smbd_server_conn) &&
	    !vuser->server_info->guest) {
		/* Try and turn on server signing on the first non-guest
		 * sessionsetup. */
		srv_set_signing(smbd_server_conn,
				vuser->server_info->user_session_key,
				response_blob);
	}

	/* fill in the current_user_info struct */
	set_current_user_info(
		vuser->server_info->sanitized_username,
		vuser->server_info->unix_name,
		pdb_get_domain(vuser->server_info->sam_account));

	return vuser->vuid;

  fail:

	if (vuser) {
		invalidate_vuid(sconn, vuid);
	}
	return UID_FIELD_INVALID;
}

/****************************************************************************
 Add a name to the session users list.
****************************************************************************/

void add_session_user(struct smbd_server_connection *sconn,
		      const char *user)
{
	struct passwd *pw;
	char *tmp;

	pw = Get_Pwnam_alloc(talloc_tos(), user);

	if (pw == NULL) {
		return;
	}

	if (sconn->smb1.sessions.session_userlist == NULL) {
		sconn->smb1.sessions.session_userlist = SMB_STRDUP(pw->pw_name);
		goto done;
	}

	if (in_list(pw->pw_name,sconn->smb1.sessions.session_userlist,false)) {
		goto done;
	}

	if (strlen(sconn->smb1.sessions.session_userlist) > 128 * 1024) {
		DEBUG(3,("add_session_user: session userlist already "
			 "too large.\n"));
		goto done;
	}

	if (asprintf(&tmp, "%s %s",
		     sconn->smb1.sessions.session_userlist, pw->pw_name) == -1) {
		DEBUG(3, ("asprintf failed\n"));
		goto done;
	}

	SAFE_FREE(sconn->smb1.sessions.session_userlist);
	sconn->smb1.sessions.session_userlist = tmp;
 done:
	TALLOC_FREE(pw);
}

/****************************************************************************
 In security=share mode we need to store the client workgroup, as that's
  what Vista uses for the NTLMv2 calculation.
****************************************************************************/

void add_session_workgroup(struct smbd_server_connection *sconn,
			   const char *workgroup)
{
	if (sconn->smb1.sessions.session_workgroup) {
		SAFE_FREE(sconn->smb1.sessions.session_workgroup);
	}
	sconn->smb1.sessions.session_workgroup = smb_xstrdup(workgroup);
}

/****************************************************************************
 In security=share mode we need to return the client workgroup, as that's
  what Vista uses for the NTLMv2 calculation.
****************************************************************************/

const char *get_session_workgroup(struct smbd_server_connection *sconn)
{
	return sconn->smb1.sessions.session_workgroup;
}

/****************************************************************************
 Check if a user is in a netgroup user list. If at first we don't succeed,
 try lower case.
****************************************************************************/

bool user_in_netgroup(struct smbd_server_connection *sconn,
		      const char *user, const char *ngname)
{
#ifdef HAVE_NETGROUP
	fstring lowercase_user;

	if (sconn->smb1.sessions.my_yp_domain == NULL) {
		yp_get_default_domain(&sconn->smb1.sessions.my_yp_domain);
	}

	if (sconn->smb1.sessions.my_yp_domain == NULL) {
		DEBUG(5,("Unable to get default yp domain, "
			"let's try without specifying it\n"));
	}

	DEBUG(5,("looking for user %s of domain %s in netgroup %s\n",
		user,
		sconn->smb1.sessions.my_yp_domain?
		sconn->smb1.sessions.my_yp_domain:"(ANY)",
		ngname));

	if (innetgr(ngname, NULL, user, sconn->smb1.sessions.my_yp_domain)) {
		DEBUG(5,("user_in_netgroup: Found\n"));
		return true;
	}

	/*
	 * Ok, innetgr is case sensitive. Try once more with lowercase
	 * just in case. Attempt to fix #703. JRA.
	 */
	fstrcpy(lowercase_user, user);
	strlower_m(lowercase_user);

	if (strcmp(user,lowercase_user) == 0) {
		/* user name was already lower case! */
		return false;
	}

	DEBUG(5,("looking for user %s of domain %s in netgroup %s\n",
		lowercase_user,
		sconn->smb1.sessions.my_yp_domain?
		sconn->smb1.sessions.my_yp_domain:"(ANY)",
		ngname));

	if (innetgr(ngname, NULL, lowercase_user,
		    sconn->smb1.sessions.my_yp_domain)) {
		DEBUG(5,("user_in_netgroup: Found\n"));
		return true;
	}
#endif /* HAVE_NETGROUP */
	return false;
}

/****************************************************************************
 Check if a user is in a user list - can check combinations of UNIX
 and netgroup lists.
****************************************************************************/

bool user_in_list(struct smbd_server_connection *sconn,
		  const char *user,const char **list)
{
	if (!list || !*list)
		return False;

	DEBUG(10,("user_in_list: checking user %s in list\n", user));

	while (*list) {

		DEBUG(10,("user_in_list: checking user |%s| against |%s|\n",
			  user, *list));

		/*
		 * Check raw username.
		 */
		if (strequal(user, *list))
			return(True);

		/*
		 * Now check to see if any combination
		 * of UNIX and netgroups has been specified.
		 */

		if(**list == '@') {
			/*
			 * Old behaviour. Check netgroup list
			 * followed by UNIX list.
			 */
			if(user_in_netgroup(sconn, user, *list +1))
				return True;
			if(user_in_group(user, *list +1))
				return True;
		} else if (**list == '+') {

			if((*(*list +1)) == '&') {
				/*
				 * Search UNIX list followed by netgroup.
				 */
				if(user_in_group(user, *list +2))
					return True;
				if(user_in_netgroup(sconn, user, *list +2))
					return True;

			} else {

				/*
				 * Just search UNIX list.
				 */

				if(user_in_group(user, *list +1))
					return True;
			}

		} else if (**list == '&') {

			if(*(*list +1) == '+') {
				/*
				 * Search netgroup list followed by UNIX list.
				 */
				if(user_in_netgroup(sconn, user, *list +2))
					return True;
				if(user_in_group(user, *list +2))
					return True;
			} else {
				/*
				 * Just search netgroup list.
				 */
				if(user_in_netgroup(sconn, user, *list +1))
					return True;
			}
		}

		list++;
	}
	return(False);
}

/****************************************************************************
 Check if a username is valid.
****************************************************************************/

static bool user_ok(struct smbd_server_connection *sconn,
		    const char *user, int snum)
{
	bool ret;

	ret = True;

	if (lp_invalid_users(snum)) {
		char **invalid = str_list_copy(talloc_tos(),
			lp_invalid_users(snum));
		if (invalid &&
		    str_list_substitute(invalid, "%S", lp_servicename(snum))) {

			/* This is used in sec=share only, so no current user
			 * around to pass to str_list_sub_basic() */

			if ( invalid && str_list_sub_basic(invalid, "", "") ) {
				ret = !user_in_list(sconn, user,
						    (const char **)invalid);
			}
		}
		TALLOC_FREE(invalid);
	}

	if (ret && lp_valid_users(snum)) {
		char **valid = str_list_copy(talloc_tos(),
			lp_valid_users(snum));
		if ( valid &&
		     str_list_substitute(valid, "%S", lp_servicename(snum)) ) {

			/* This is used in sec=share only, so no current user
			 * around to pass to str_list_sub_basic() */

			if ( valid && str_list_sub_basic(valid, "", "") ) {
				ret = user_in_list(sconn, user,
						   (const char **)valid);
			}
		}
		TALLOC_FREE(valid);
	}

	if (ret && lp_onlyuser(snum)) {
		char **user_list = str_list_make_v3(
			talloc_tos(), lp_username(snum), NULL);
		if (user_list &&
		    str_list_substitute(user_list, "%S",
					lp_servicename(snum))) {
			ret = user_in_list(sconn, user,
					   (const char **)user_list);
		}
		TALLOC_FREE(user_list);
	}

	return(ret);
}

/****************************************************************************
 Validate a group username entry. Return the username or NULL.
****************************************************************************/

static char *validate_group(struct smbd_server_connection *sconn,
			    char *group, DATA_BLOB password,int snum)
{
#ifdef HAVE_NETGROUP
	{
		char *host, *user, *domain;
		struct auth_context *actx = sconn->smb1.negprot.auth_context;
		bool enc = sconn->smb1.negprot.encrypted_passwords;
		setnetgrent(group);
		while (getnetgrent(&host, &user, &domain)) {
			if (user) {
				if (user_ok(sconn, user, snum) &&
				    password_ok(actx, enc,
						get_session_workgroup(sconn),
						user,password)) {
					endnetgrent();
					return(user);
				}
			}
		}
		endnetgrent();
	}
#endif

#ifdef HAVE_GETGRENT
	{
		struct group *gptr;
		struct auth_context *actx = sconn->smb1.negprot.auth_context;
		bool enc = sconn->smb1.negprot.encrypted_passwords;

		setgrent();
		while ((gptr = (struct group *)getgrent())) {
			if (strequal(gptr->gr_name,group))
				break;
		}

		/*
		 * As user_ok can recurse doing a getgrent(), we must
		 * copy the member list onto the heap before
		 * use. Bug pointed out by leon@eatworms.swmed.edu.
		 */

		if (gptr) {
			char *member_list = NULL;
			size_t list_len = 0;
			char *member;
			int i;

			for(i = 0; gptr->gr_mem && gptr->gr_mem[i]; i++) {
				list_len += strlen(gptr->gr_mem[i])+1;
			}
			list_len++;

			member_list = (char *)SMB_MALLOC(list_len);
			if (!member_list) {
				endgrent();
				return NULL;
			}

			*member_list = '\0';
			member = member_list;

			for(i = 0; gptr->gr_mem && gptr->gr_mem[i]; i++) {
				size_t member_len = strlen(gptr->gr_mem[i])+1;

				DEBUG(10,("validate_group: = gr_mem = "
					  "%s\n", gptr->gr_mem[i]));

				safe_strcpy(member, gptr->gr_mem[i],
					list_len - (member-member_list));
				member += member_len;
			}

			endgrent();

			member = member_list;
			while (*member) {
				if (user_ok(sconn, member,snum) &&
				    password_ok(actx, enc,
						get_session_workgroup(sconn),
						member,password)) {
					char *name = talloc_strdup(talloc_tos(),
								member);
					SAFE_FREE(member_list);
					return name;
				}

				DEBUG(10,("validate_group = member = %s\n",
					  member));

				member += strlen(member) + 1;
			}

			SAFE_FREE(member_list);
		} else {
			endgrent();
			return NULL;
		}
	}
#endif
	return(NULL);
}

/****************************************************************************
 Check for authority to login to a service with a given username/password.
 Note this is *NOT* used when logging on using sessionsetup_and_X.
****************************************************************************/

bool authorise_login(struct smbd_server_connection *sconn,
		     int snum, fstring user, DATA_BLOB password,
		     bool *guest)
{
	bool ok = False;
	struct auth_context *actx = sconn->smb1.negprot.auth_context;
	bool enc = sconn->smb1.negprot.encrypted_passwords;

#ifdef DEBUG_PASSWORD
	DEBUG(100,("authorise_login: checking authorisation on "
		   "user=%s pass=%s\n", user,password.data));
#endif

	*guest = False;

	/* there are several possibilities:
		1) login as the given user with given password
		2) login as a previously registered username with the given
		   password
		3) login as a session list username with the given password
		4) login as a previously validated user/password pair
		5) login as the "user =" user with given password
		6) login as the "user =" user with no password
		   (guest connection)
		7) login as guest user with no password

		if the service is guest_only then steps 1 to 5 are skipped
	*/

	/* now check the list of session users */
	if (!ok) {
		char *auser;
		char *user_list = NULL;
		char *saveptr;

		if (sconn->smb1.sessions.session_userlist)
			user_list = SMB_STRDUP(sconn->smb1.sessions.session_userlist);
		else
			user_list = SMB_STRDUP("");

		if (!user_list)
			return(False);

		for (auser = strtok_r(user_list, LIST_SEP, &saveptr);
		     !ok && auser;
		     auser = strtok_r(NULL, LIST_SEP, &saveptr)) {
			fstring user2;
			fstrcpy(user2,auser);
			if (!user_ok(sconn,user2,snum))
				continue;

			if (password_ok(actx, enc,
					get_session_workgroup(sconn),
					user2,password)) {
				ok = True;
				fstrcpy(user,user2);
				DEBUG(3,("authorise_login: ACCEPTED: session "
					 "list username (%s) and given "
					 "password ok\n", user));
			}
		}

		SAFE_FREE(user_list);
	}

	/* check the user= fields and the given password */
	if (!ok && lp_username(snum)) {
		TALLOC_CTX *ctx = talloc_tos();
		char *auser;
		char *user_list = talloc_strdup(ctx, lp_username(snum));
		char *saveptr;

		if (!user_list) {
			goto check_guest;
		}

		user_list = talloc_string_sub(ctx,
				user_list,
				"%S",
				lp_servicename(snum));

		if (!user_list) {
			goto check_guest;
		}

		for (auser = strtok_r(user_list, LIST_SEP, &saveptr);
		     auser && !ok;
		     auser = strtok_r(NULL, LIST_SEP, &saveptr)) {
			if (*auser == '@') {
				auser = validate_group(sconn,auser+1,
						       password,snum);
				if (auser) {
					ok = True;
					fstrcpy(user,auser);
					DEBUG(3,("authorise_login: ACCEPTED: "
						 "group username and given "
						 "password ok (%s)\n", user));
				}
			} else {
				fstring user2;
				fstrcpy(user2,auser);
				if (user_ok(sconn,user2,snum) &&
				    password_ok(actx, enc,
						get_session_workgroup(sconn),
						user2,password)) {
					ok = True;
					fstrcpy(user,user2);
					DEBUG(3,("authorise_login: ACCEPTED: "
						 "user list username and "
						 "given password ok (%s)\n",
						 user));
				}
			}
		}
	}

  check_guest:

	/* check for a normal guest connection */
	if (!ok && GUEST_OK(snum)) {
		struct passwd *guest_pw;
		fstring guestname;
		fstrcpy(guestname,lp_guestaccount());
		guest_pw = Get_Pwnam_alloc(talloc_tos(), guestname);
		if (guest_pw != NULL) {
			fstrcpy(user,guestname);
			ok = True;
			DEBUG(3,("authorise_login: ACCEPTED: guest account "
				 "and guest ok (%s)\n",	user));
		} else {
			DEBUG(0,("authorise_login: Invalid guest account "
				 "%s??\n",guestname));
		}
		TALLOC_FREE(guest_pw);
		*guest = True;
	}

	if (ok && !user_ok(sconn, user, snum)) {
		DEBUG(0,("authorise_login: rejected invalid user %s\n",user));
		ok = False;
	}

	return(ok);
}
