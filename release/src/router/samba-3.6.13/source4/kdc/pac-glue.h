/*
   Unix SMB/CIFS implementation.

   PAC Glue between Samba and the KDC

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005-2009
   Copyright (C) Simo Sorce <idra@samba.org> 2010

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

krb5_error_code samba_make_krb5_pac(krb5_context context,
				    DATA_BLOB *pac_blob,
				    krb5_pac *pac);

bool samba_princ_needs_pac(struct hdb_entry_ex *princ);

bool samba_krbtgt_was_untrusted_rodc(struct hdb_entry_ex *princ);

NTSTATUS samba_kdc_get_pac_blob(TALLOC_CTX *mem_ctx,
				struct hdb_entry_ex *client,
				DATA_BLOB **_pac_blob);

NTSTATUS samba_kdc_update_pac_blob(TALLOC_CTX *mem_ctx,
				   krb5_context context,
				   krb5_pac *pac, DATA_BLOB *pac_blob);

void samba_kdc_build_edata_reply(NTSTATUS nt_status, DATA_BLOB *e_data);

krb5_error_code samba_kdc_map_policy_err(NTSTATUS nt_status);

NTSTATUS samba_kdc_check_client_access(struct samba_kdc_entry *kdc_entry,
				       const char *client_name,
				       const char *workstation,
				       bool password_change);
