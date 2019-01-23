/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2007
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) James Peach 2006

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
#include "popt_common.h"
#include "secrets.h"

/**************************************************************************n
  Code to cope with username/password auth options from the commandline.
  Used mainly in client tools.
****************************************************************************/

struct user_auth_info *user_auth_info_init(TALLOC_CTX *mem_ctx)
{
	struct user_auth_info *result;

	result = TALLOC_ZERO_P(mem_ctx, struct user_auth_info);
	if (result == NULL) {
		return NULL;
	}

	result->signing_state = Undefined;
	return result;
}

const char *get_cmdline_auth_info_username(const struct user_auth_info *auth_info)
{
	if (!auth_info->username) {
		return "";
	}
	return auth_info->username;
}

void set_cmdline_auth_info_username(struct user_auth_info *auth_info,
				    const char *username)
{
	TALLOC_FREE(auth_info->username);
	auth_info->username = talloc_strdup(auth_info, username);
	if (!auth_info->username) {
		exit(ENOMEM);
	}
}

const char *get_cmdline_auth_info_domain(const struct user_auth_info *auth_info)
{
	if (!auth_info->domain) {
		return "";
	}
	return auth_info->domain;
}

void set_cmdline_auth_info_domain(struct user_auth_info *auth_info,
				  const char *domain)
{
	TALLOC_FREE(auth_info->domain);
	auth_info->domain = talloc_strdup(auth_info, domain);
	if (!auth_info->domain) {
		exit(ENOMEM);
	}
}

const char *get_cmdline_auth_info_password(const struct user_auth_info *auth_info)
{
	if (!auth_info->password) {
		return "";
	}
	return auth_info->password;
}

void set_cmdline_auth_info_password(struct user_auth_info *auth_info,
				    const char *password)
{
	TALLOC_FREE(auth_info->password);
	if (password == NULL) {
		password = "";
	}
	auth_info->password = talloc_strdup(auth_info, password);
	if (!auth_info->password) {
		exit(ENOMEM);
	}
	auth_info->got_pass = true;
}

bool set_cmdline_auth_info_signing_state(struct user_auth_info *auth_info,
					 const char *arg)
{
	auth_info->signing_state = -1;
	if (strequal(arg, "off") || strequal(arg, "no") ||
			strequal(arg, "false")) {
		auth_info->signing_state = false;
	} else if (strequal(arg, "on") || strequal(arg, "yes") ||
			strequal(arg, "true") || strequal(arg, "auto")) {
		auth_info->signing_state = true;
	} else if (strequal(arg, "force") || strequal(arg, "required") ||
			strequal(arg, "forced")) {
		auth_info->signing_state = Required;
	} else {
		return false;
	}
	return true;
}

int get_cmdline_auth_info_signing_state(const struct user_auth_info *auth_info)
{
	return auth_info->signing_state;
}

void set_cmdline_auth_info_use_ccache(struct user_auth_info *auth_info, bool b)
{
        auth_info->use_ccache = b;
}

bool get_cmdline_auth_info_use_ccache(const struct user_auth_info *auth_info)
{
	return auth_info->use_ccache;
}

void set_cmdline_auth_info_use_kerberos(struct user_auth_info *auth_info,
					bool b)
{
        auth_info->use_kerberos = b;
}

bool get_cmdline_auth_info_use_kerberos(const struct user_auth_info *auth_info)
{
	return auth_info->use_kerberos;
}

void set_cmdline_auth_info_fallback_after_kerberos(struct user_auth_info *auth_info,
					bool b)
{
	auth_info->fallback_after_kerberos = b;
}

bool get_cmdline_auth_info_fallback_after_kerberos(const struct user_auth_info *auth_info)
{
	return auth_info->fallback_after_kerberos;
}

/* This should only be used by lib/popt_common.c JRA */
void set_cmdline_auth_info_use_krb5_ticket(struct user_auth_info *auth_info)
{
	auth_info->use_kerberos = true;
	auth_info->got_pass = true;
}

/* This should only be used by lib/popt_common.c JRA */
void set_cmdline_auth_info_smb_encrypt(struct user_auth_info *auth_info)
{
	auth_info->smb_encrypt = true;
}

void set_cmdline_auth_info_use_machine_account(struct user_auth_info *auth_info)
{
	auth_info->use_machine_account = true;
}

bool get_cmdline_auth_info_got_pass(const struct user_auth_info *auth_info)
{
	return auth_info->got_pass;
}

bool get_cmdline_auth_info_smb_encrypt(const struct user_auth_info *auth_info)
{
	return auth_info->smb_encrypt;
}

bool get_cmdline_auth_info_use_machine_account(const struct user_auth_info *auth_info)
{
	return auth_info->use_machine_account;
}

struct user_auth_info *get_cmdline_auth_info_copy(TALLOC_CTX *mem_ctx,
						  const struct user_auth_info *src)
{
	struct user_auth_info *result;

	result = user_auth_info_init(mem_ctx);
	if (result == NULL) {
		return NULL;
	}

	*result = *src;

	result->username = talloc_strdup(
		result, get_cmdline_auth_info_username(src));
	result->password = talloc_strdup(
		result, get_cmdline_auth_info_password(src));
	if ((result->username == NULL) || (result->password == NULL)) {
		TALLOC_FREE(result);
		return NULL;
	}

	return result;
}

bool set_cmdline_auth_info_machine_account_creds(struct user_auth_info *auth_info)
{
	char *pass = NULL;
	char *account = NULL;

	if (!get_cmdline_auth_info_use_machine_account(auth_info)) {
		return false;
	}

	if (!secrets_init()) {
		d_printf("ERROR: Unable to open secrets database\n");
		return false;
	}

	if (asprintf(&account, "%s$@%s", global_myname(), lp_realm()) < 0) {
		return false;
	}

	pass = secrets_fetch_machine_password(lp_workgroup(), NULL, NULL);
	if (!pass) {
		d_printf("ERROR: Unable to fetch machine password for "
			"%s in domain %s\n",
			account, lp_workgroup());
		SAFE_FREE(account);
		return false;
	}

	set_cmdline_auth_info_username(auth_info, account);
	set_cmdline_auth_info_password(auth_info, pass);

	SAFE_FREE(account);
	SAFE_FREE(pass);

	return true;
}

/****************************************************************************
 Ensure we have a password if one not given.
****************************************************************************/

void set_cmdline_auth_info_getpass(struct user_auth_info *auth_info)
{
	char *label = NULL;
	char *pass;
	TALLOC_CTX *frame;

	if (get_cmdline_auth_info_got_pass(auth_info) ||
			get_cmdline_auth_info_use_kerberos(auth_info)) {
		/* Already got one... */
		return;
	}

	frame = talloc_stackframe();
	label = talloc_asprintf(frame, "Enter %s's password: ",
			get_cmdline_auth_info_username(auth_info));
	pass = getpass(label);
	if (pass) {
		set_cmdline_auth_info_password(auth_info, pass);
	}
	TALLOC_FREE(frame);
}
