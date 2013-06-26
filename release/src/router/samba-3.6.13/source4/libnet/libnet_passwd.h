/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   
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

/* struct and enum for doing a remote password change */
enum libnet_ChangePassword_level {
	LIBNET_CHANGE_PASSWORD_GENERIC,
	LIBNET_CHANGE_PASSWORD_SAMR,
	LIBNET_CHANGE_PASSWORD_KRB5,
	LIBNET_CHANGE_PASSWORD_LDAP,
	LIBNET_CHANGE_PASSWORD_RAP
};

union libnet_ChangePassword {
	struct {
		enum libnet_ChangePassword_level level;

		struct _libnet_ChangePassword_in {
			const char *account_name;
			const char *domain_name;
			const char *oldpassword;
			const char *newpassword;
		} in;

		struct _libnet_ChangePassword_out {
			const char *error_string;
		} out;
	} generic;

	struct {
		enum libnet_ChangePassword_level level;
		struct _libnet_ChangePassword_in in;
		struct _libnet_ChangePassword_out out;
	} samr;

	struct {
		enum libnet_ChangePassword_level level;
		struct _libnet_ChangePassword_in in;
		struct _libnet_ChangePassword_out out;
	} krb5;

	struct {
		enum libnet_ChangePassword_level level;
		struct _libnet_ChangePassword_in in;
		struct _libnet_ChangePassword_out out;
	} ldap;

	struct {
		enum libnet_ChangePassword_level level;
		struct _libnet_ChangePassword_in in;
		struct _libnet_ChangePassword_out out;
	} rap;
};

/* struct and enum for doing a remote password set */
enum libnet_SetPassword_level {
	LIBNET_SET_PASSWORD_GENERIC,
	LIBNET_SET_PASSWORD_SAMR,
	LIBNET_SET_PASSWORD_SAMR_HANDLE,
	LIBNET_SET_PASSWORD_SAMR_HANDLE_26,
	LIBNET_SET_PASSWORD_SAMR_HANDLE_25,
	LIBNET_SET_PASSWORD_SAMR_HANDLE_24,
	LIBNET_SET_PASSWORD_SAMR_HANDLE_23,
	LIBNET_SET_PASSWORD_KRB5,
	LIBNET_SET_PASSWORD_LDAP,
	LIBNET_SET_PASSWORD_RAP
};

union libnet_SetPassword {
	struct {
		enum libnet_SetPassword_level level;

		struct _libnet_SetPassword_in {
			const char *account_name;
			const char *domain_name;
			const char *newpassword;
		} in;

		struct _libnet_SetPassword_out {
			const char *error_string;
		} out;
	} generic;

	struct {
		enum libnet_SetPassword_level level;
		struct _libnet_SetPassword_samr_handle_in {
			const char           *account_name; /* for debug only */
			struct policy_handle *user_handle;
			struct dcerpc_pipe   *dcerpc_pipe;
			const char           *newpassword;
			struct samr_UserInfo21 *info21; /* can be NULL,
			                                 * for level 26,24 it must be NULL
			                                 * for level 25,23 it must be non-NULL
							 */
		} in;
		struct _libnet_SetPassword_out out;
	} samr_handle;

	struct {
		enum libnet_SetPassword_level level;
		struct _libnet_SetPassword_in in;
		struct _libnet_SetPassword_out out;
	} samr;

	struct {
		enum libnet_SetPassword_level level;
		struct _libnet_SetPassword_in in;
		struct _libnet_SetPassword_out out;
	} krb5;

	struct {
		enum libnet_SetPassword_level level;
		struct _libnet_SetPassword_in in;
		struct _libnet_SetPassword_out out;
	} ldap;

	struct {
		enum libnet_ChangePassword_level level;
		struct _libnet_SetPassword_in in;
		struct _libnet_SetPassword_out out;
	} rap;
};
