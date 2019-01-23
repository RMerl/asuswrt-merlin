/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2006
   
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

#define TEST_USERNAME  "libnetusertest"

#define continue_if_field_set(field) \
	if (field != 0) { \
		i--; \
		continue; \
	}


#define USER_FIELD_FIRST	acct_name
#define USER_FIELD_LAST 	acct_flags

enum test_fields { none = 0,
		   acct_name, acct_full_name, acct_description,
		   acct_home_directory, acct_home_drive, acct_comment,
		   acct_logon_script, acct_profile_path, acct_expiry, acct_flags };


#define TEST_CHG_ACCOUNTNAME   "newlibnetusertest%02d"
#define TEST_CHG_DESCRIPTION   "Sample description %ld"
#define TEST_CHG_FULLNAME      "First%04x Last%04x"
#define TEST_CHG_COMMENT       "Comment[%04lu%04lu]"
#define TEST_CHG_PROFILEPATH   "\\\\srv%04ld\\profile%02u\\prof"
