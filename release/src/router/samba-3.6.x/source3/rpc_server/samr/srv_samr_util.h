/*
   Unix SMB/CIFS implementation.
   SAMR Pipe utility functions.

   Copyright (C) Luke Kenneth Casson Leighton 	1996-1998
   Copyright (C) Gerald (Jerry) Carter		2000-2001
   Copyright (C) Andrew Bartlett		2001-2002
   Copyright (C) Stefan (metze) Metzmacher	2002
   Copyright (C) Guenther Deschner		2008

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

/* The following definitions come from rpc_server/srv_samr_util.c  */

struct samu;

void copy_id2_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo2 *from);
void copy_id4_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo4 *from);
void copy_id6_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo6 *from);
void copy_id8_to_sam_passwd(struct samu *to,
			    struct samr_UserInfo8 *from);
void copy_id10_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo10 *from);
void copy_id11_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo11 *from);
void copy_id12_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo12 *from);
void copy_id13_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo13 *from);
void copy_id14_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo14 *from);
void copy_id16_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo16 *from);
void copy_id17_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo17 *from);
void copy_id18_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo18 *from);
void copy_id20_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo20 *from);
void copy_id21_to_sam_passwd(const char *log_prefix,
			     struct samu *to,
			     struct samr_UserInfo21 *from);
void copy_id23_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo23 *from);
void copy_id24_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo24 *from);
void copy_id25_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo25 *from);
void copy_id26_to_sam_passwd(struct samu *to,
			     struct samr_UserInfo26 *from);

/* The following definitions come from rpc_server/srv_samr_chgpasswd.c  */

bool chgpasswd(const char *name, const char *rhost, const struct passwd *pass,
	       const char *oldpass, const char *newpass, bool as_root);
NTSTATUS pass_oem_change(char *user, const char *rhost,
			 uchar password_encrypted_with_lm_hash[516],
			 const uchar old_lm_hash_encrypted[16],
			 uchar password_encrypted_with_nt_hash[516],
			 const uchar old_nt_hash_encrypted[16],
			 enum samPwdChangeReason *reject_reason);
NTSTATUS check_password_complexity(const char *username,
				   const char *password,
				   enum samPwdChangeReason *samr_reject_reason);
