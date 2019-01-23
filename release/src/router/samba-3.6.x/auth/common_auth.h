/*
   Unix SMB/CIFS implementation.
   Standardised Authentication types
   Copyright (C) Andrew Bartlett 2001-2010

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

#define USER_INFO_CASE_INSENSITIVE_USERNAME 0x01 /* username may be in any case */
#define USER_INFO_CASE_INSENSITIVE_PASSWORD 0x02 /* password may be in any case */
#define USER_INFO_DONT_CHECK_UNIX_ACCOUNT   0x04 /* don't check unix account status */
#define USER_INFO_INTERACTIVE_LOGON         0x08 /* don't check unix account status */

enum auth_password_state {
	AUTH_PASSWORD_PLAIN = 1,
	AUTH_PASSWORD_HASH = 2,
	AUTH_PASSWORD_RESPONSE = 3
};

struct auth_usersupplied_info
{
	const char *workstation_name;
	const struct tsocket_address *remote_host;

	uint32_t logon_parameters;

	bool mapped_state;
	bool was_mapped;
	/* the values the client gives us */
	struct {
		const char *account_name;
		const char *domain_name;
	} client, mapped;

	enum auth_password_state password_state;

	struct {
		struct {
			DATA_BLOB lanman;
			DATA_BLOB nt;
		} response;
		struct {
			struct samr_Password *lanman;
			struct samr_Password *nt;
		} hash;

		char *plaintext;
	} password;
	uint32_t flags;
};
