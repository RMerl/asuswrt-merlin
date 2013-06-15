/* 
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Guenther Deschner 2005-2006
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "iniparser/src/iniparser.h"

/****************************************************************
 parse the local gpt.ini file
****************************************************************/

#define GPT_INI_SECTION_GENERAL "General"
#define GPT_INI_PARAMETER_VERSION "Version"
#define GPT_INI_PARAMETER_DISPLAYNAME "displayName"

NTSTATUS parse_gpt_ini(TALLOC_CTX *mem_ctx, const char *filename, uint32 *version, char **display_name)
{
	NTSTATUS result;
	uint32 v;
	char *name = NULL;
	dictionary *d;

	d = iniparser_load(filename);
	if (d == NULL) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	if ((name = iniparser_getstring(d, GPT_INI_SECTION_GENERAL
			":"GPT_INI_PARAMETER_DISPLAYNAME, NULL)) == NULL) {
		/* the default domain policy and the default domain controller
		 * policy never have a displayname in their gpt.ini file */
		DEBUG(10,("parse_gpt_ini: no name in %s\n", filename));
	}

	if (name && display_name) {
		*display_name = talloc_strdup(mem_ctx, name);
		if (*display_name == NULL) {
			result = NT_STATUS_NO_MEMORY;
			goto out;
		}
	}

	if ((v = iniparser_getint(d, GPT_INI_SECTION_GENERAL
			":"GPT_INI_PARAMETER_VERSION, Undefined)) == Undefined) {
		DEBUG(10,("parse_gpt_ini: no version\n"));
		result = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto out;
	}

	if (version) {
		*version = v;
	}

	result = NT_STATUS_OK;
 out:
 	if (d) {
		iniparser_freedict(d);
	}

	return result;
}

#if 0 /* not yet */

/****************************************************************
 parse the Version section from gpttmpl file
****************************************************************/

#define GPTTMPL_SECTION_VERSION "Version"
#define GPTTMPL_PARAMETER_REVISION "Revision"
#define GPTTMPL_PARAMETER_SIGNATURE "signature"
#define GPTTMPL_CHICAGO "$CHICAGO$" /* whatever this is good for... */
#define GPTTMPL_SECTION_UNICODE "Unicode"
#define GPTTMPL_PARAMETER_UNICODE "Unicode"

static NTSTATUS parse_gpttmpl(dictionary *d, uint32 *version_out)
{
	const char *signature = NULL;
	uint32 version;

	if ((signature = iniparser_getstring(d, GPTTMPL_SECTION_VERSION
			":"GPTTMPL_PARAMETER_SIGNATURE, NULL)) == NULL) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (!strequal(signature, GPTTMPL_CHICAGO)) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if ((version = iniparser_getint(d, GPTTMPL_SECTION_VERSION
			":"GPTTMPL_PARAMETER_REVISION, Undefined)) == Undefined) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	if (version_out) {
		*version_out = version;
	}

	/* treat that as boolean */
	if ((!iniparser_getboolean(d, GPTTMPL_SECTION_UNICODE
			":"GPTTMPL_PARAMETER_UNICODE, Undefined)) == Undefined) {
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	return NT_STATUS_OK;
}

/****************************************************************
 parse the "System Access" section from gpttmpl file
****************************************************************/

#define GPTTMPL_SECTION_SYSTEM_ACCESS "System Access"
#define GPTTMPL_PARAMETER_MINPWDAGE "MinimumPasswordAge"
#define GPTTMPL_PARAMETER_MAXPWDAGE "MaximumPasswordAge"
#define GPTTMPL_PARAMETER_MINPWDLEN "MinimumPasswordLength"
#define GPTTMPL_PARAMETER_PWDCOMPLEX "PasswordComplexity"
#define GPTTMPL_PARAMETER_PWDHISTORY "PasswordHistorySize"
#define GPTTMPL_PARAMETER_LOCKOUTCOUNT "LockoutBadCount"

static NTSTATUS parse_gpttmpl_system_access(const char *filename)
{
	NTSTATUS status;
	dictionary *d = NULL;
	uint32 pwd_min_age, pwd_max_age, pwd_min_len, pwd_history;
	uint32 lockout_count;
	BOOL pwd_complex;
	uint32 version;

	d = iniparser_load(filename);
	if (d == NULL) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	status = parse_gpttmpl(d, &version);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	status = NT_STATUS_INVALID_PARAMETER;

	if ((pwd_min_age = iniparser_getint(d, GPTTMPL_SECTION_SYSTEM_ACCESS
			":"GPTTMPL_PARAMETER_MINPWDAGE, Undefined)) == Undefined) {
		goto out;
	}

	if ((pwd_max_age = iniparser_getint(d, GPTTMPL_SECTION_SYSTEM_ACCESS
			":"GPTTMPL_PARAMETER_MINPWDAGE, Undefined)) == Undefined) {
		goto out;
	}

	if ((pwd_min_len = iniparser_getint(d, GPTTMPL_SECTION_SYSTEM_ACCESS
			":"GPTTMPL_PARAMETER_MINPWDLEN, Undefined)) == Undefined) {
		goto out;
	}

	if ((pwd_complex = iniparser_getboolean(d, GPTTMPL_SECTION_SYSTEM_ACCESS
			":"GPTTMPL_PARAMETER_PWDCOMPLEX, Undefined)) == Undefined) {
		goto out;
	}

	if ((pwd_history = iniparser_getint(d, GPTTMPL_SECTION_SYSTEM_ACCESS
			":"GPTTMPL_PARAMETER_PWDHISTORY, Undefined)) == Undefined) {
		goto out;
	}

	if ((lockout_count = iniparser_getint(d, GPTTMPL_SECTION_SYSTEM_ACCESS
			":"GPTTMPL_PARAMETER_LOCKOUTCOUNT, Undefined)) == Undefined) {
		goto out;
	}

	/* TODO ? 
	RequireLogonToChangePassword = 0
	ForceLogoffWhenHourExpire = 0
	ClearTextPassword = 0
	*/

	status = NT_STATUS_OK;

 out:
	if (d) {
		iniparser_freedict(d);
	}

 	return status;
}

/****************************************************************
 parse the "Kerberos Policy" section from gpttmpl file
****************************************************************/

#define GPTTMPL_SECTION_KERBEROS_POLICY "Kerberos Policy"
#define GPTTMPL_PARAMETER_MAXTKTAGE "MaxTicketAge"
#define GPTTMPL_PARAMETER_MAXRENEWAGE "MaxRenewAge"
#define GPTTMPL_PARAMETER_MAXTGSAGE "MaxServiceAge"
#define GPTTMPL_PARAMETER_MAXCLOCKSKEW "MaxClockSkew"
#define GPTTMPL_PARAMETER_TKTVALIDATECLIENT "TicketValidateClient"

static NTSTATUS parse_gpttmpl_kerberos_policy(const char *filename)
{
	NTSTATUS status;
	dictionary *d = NULL;
	uint32 tkt_max_age, tkt_max_renew, tgs_max_age, max_clock_skew;
	BOOL tkt_validate;
	uint32 version;

	d = iniparser_load(filename);
	if (d == NULL) {
		return NT_STATUS_NO_SUCH_FILE;
	}

	status = parse_gpttmpl(d, &version);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	status = NT_STATUS_INVALID_PARAMETER;

	if ((tkt_max_age = iniparser_getint(d, GPTTMPL_SECTION_KERBEROS_POLICY
			":"GPTTMPL_PARAMETER_MAXTKTAGE, Undefined)) != Undefined) {
		goto out;
	}

	if ((tkt_max_renew = iniparser_getint(d, GPTTMPL_SECTION_KERBEROS_POLICY
			":"GPTTMPL_PARAMETER_MAXRENEWAGE, Undefined)) != Undefined) {
		goto out;
	}

	if ((tgs_max_age = iniparser_getint(d, GPTTMPL_SECTION_KERBEROS_POLICY
			":"GPTTMPL_PARAMETER_MAXTGSAGE, Undefined)) != Undefined) {
		goto out;
	}

	if ((max_clock_skew = iniparser_getint(d, GPTTMPL_SECTION_KERBEROS_POLICY
			":"GPTTMPL_PARAMETER_MAXCLOCKSKEW, Undefined)) != Undefined) {
		goto out;
	}

	if ((tkt_validate = iniparser_getboolean(d, GPTTMPL_SECTION_KERBEROS_POLICY
			":"GPTTMPL_PARAMETER_TKTVALIDATECLIENT, Undefined)) != Undefined) {
		goto out;
	}

	status = NT_STATUS_OK;

 out:
	if (d) {
		iniparser_freedict(d);
	}

 	return status;
}

#endif

/*

perfectly parseable with iniparser:

{GUID}/Machine/Microsoft/Windows NT/SecEdit/GptTmpl.inf


[Unicode]
Unicode=yes
[System Access]
MinimumPasswordAge = 1
MaximumPasswordAge = 42
MinimumPasswordLength = 7
PasswordComplexity = 1
PasswordHistorySize = 24
LockoutBadCount = 0
RequireLogonToChangePassword = 0
ForceLogoffWhenHourExpire = 0
ClearTextPassword = 0
[Kerberos Policy]
MaxTicketAge = 10
MaxRenewAge = 7
MaxServiceAge = 600
MaxClockSkew = 5
TicketValidateClient = 1
[Version]
signature="$CHICAGO$"
Revision=1
*/
