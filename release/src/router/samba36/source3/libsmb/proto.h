/*
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Bartlett 2001-2003
   Copyright (C) Andrew Tridgell 1994-1998,2000-2001
   Copyright (C) Gerald (Jerry) Carter 2004
   Copyright (C) Jelmer Vernooij 2003
   Copyright (C) Jeremy Allison 2001-2009,2011
   Copyright (C) Stefan Metzmacher 2003,2009
   Copyright (C) Volker Lendecke 2011

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

#ifndef _LIBSMB_PROTO_H_
#define _LIBSMB_PROTO_H_

/* The following definitions come from libsmb/cliconnect.c  */

ADS_STATUS cli_session_setup_spnego(struct cli_state *cli, const char *user,
			      const char *pass, const char *user_domain,
				    const char * dest_realm);

NTSTATUS cli_session_setup(struct cli_state *cli,
			   const char *user,
			   const char *pass, int passlen,
			   const char *ntpass, int ntpasslen,
			   const char *workgroup);
struct tevent_req *cli_session_setup_guest_create(TALLOC_CTX *mem_ctx,
						  struct event_context *ev,
						  struct cli_state *cli,
						  struct tevent_req **psmbreq);
struct tevent_req *cli_session_setup_guest_send(TALLOC_CTX *mem_ctx,
						struct event_context *ev,
						struct cli_state *cli);
NTSTATUS cli_session_setup_guest_recv(struct tevent_req *req);
struct tevent_req *cli_ulogoff_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct cli_state *cli);
NTSTATUS cli_ulogoff_recv(struct tevent_req *req);
NTSTATUS cli_ulogoff(struct cli_state *cli);
struct tevent_req *cli_tcon_andx_create(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *share, const char *dev,
					const char *pass, int passlen,
					struct tevent_req **psmbreq);
struct tevent_req *cli_tcon_andx_send(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct cli_state *cli,
				      const char *share, const char *dev,
				      const char *pass, int passlen);
NTSTATUS cli_tcon_andx_recv(struct tevent_req *req);
NTSTATUS cli_tcon_andx(struct cli_state *cli, const char *share,
		       const char *dev, const char *pass, int passlen);
struct tevent_req *cli_tdis_send(TALLOC_CTX *mem_ctx,
                                 struct tevent_context *ev,
                                 struct cli_state *cli);
NTSTATUS cli_tdis_recv(struct tevent_req *req);
NTSTATUS cli_tdis(struct cli_state *cli);
NTSTATUS cli_negprot(struct cli_state *cli);
struct tevent_req *cli_negprot_send(TALLOC_CTX *mem_ctx,
				    struct event_context *ev,
				    struct cli_state *cli);
NTSTATUS cli_negprot_recv(struct tevent_req *req);
bool cli_session_request(struct cli_state *cli,
			 struct nmb_name *calling, struct nmb_name *called);
NTSTATUS cli_connect(struct cli_state *cli,
		const char *host,
		struct sockaddr_storage *dest_ss);
NTSTATUS cli_start_connection(struct cli_state **output_cli,
			      const char *my_name,
			      const char *dest_host,
			      struct sockaddr_storage *dest_ss, int port,
			      int signing_state, int flags);
NTSTATUS cli_full_connection(struct cli_state **output_cli,
			     const char *my_name,
			     const char *dest_host,
			     struct sockaddr_storage *dest_ss, int port,
			     const char *service, const char *service_type,
			     const char *user, const char *domain,
			     const char *password, int flags,
			     int signing_state);
bool attempt_netbios_session_request(struct cli_state **ppcli, const char *srchost, const char *desthost,
                                     struct sockaddr_storage *pdest_ss);
NTSTATUS cli_raw_tcon(struct cli_state *cli,
		      const char *service, const char *pass, const char *dev,
		      uint16 *max_xmit, uint16 *tid);
struct cli_state *get_ipc_connect(char *server,
				struct sockaddr_storage *server_ss,
				const struct user_auth_info *user_info);
struct cli_state *get_ipc_connect_master_ip(TALLOC_CTX *ctx,
				struct sockaddr_storage *mb_ip,
				const struct user_auth_info *user_info,
				char **pp_workgroup_out);
struct cli_state *get_ipc_connect_master_ip_bcast(TALLOC_CTX *ctx,
					const struct user_auth_info *user_info,
					char **pp_workgroup_out);

/* The following definitions come from libsmb/clidfs.c  */

NTSTATUS cli_cm_force_encryption(struct cli_state *c,
			const char *username,
			const char *password,
			const char *domain,
			const char *sharename);
struct cli_state *cli_cm_open(TALLOC_CTX *ctx,
				struct cli_state *referring_cli,
				const char *server,
				const char *share,
				const struct user_auth_info *auth_info,
				bool show_hdr,
				bool force_encrypt,
				int max_protocol,
				int port,
				int name_type);
void cli_cm_display(const struct cli_state *c);
struct client_dfs_referral;
NTSTATUS cli_dfs_get_referral(TALLOC_CTX *ctx,
			struct cli_state *cli,
			const char *path,
			struct client_dfs_referral **refs,
			size_t *num_refs,
			size_t *consumed);
bool cli_resolve_path(TALLOC_CTX *ctx,
			const char *mountpt,
			const struct user_auth_info *dfs_auth_info,
			struct cli_state *rootcli,
			const char *path,
			struct cli_state **targetcli,
			char **pp_targetpath);

bool cli_check_msdfs_proxy(TALLOC_CTX *ctx,
			struct cli_state *cli,
			const char *sharename,
			char **pp_newserver,
			char **pp_newshare,
			bool force_encrypt,
			const char *username,
			const char *password,
			const char *domain);

/* The following definitions come from libsmb/clientgen.c  */

int cli_set_message(char *buf,int num_words,int num_bytes,bool zero);
unsigned int cli_set_timeout(struct cli_state *cli, unsigned int timeout);
void cli_set_port(struct cli_state *cli, int port);
bool cli_state_seqnum_persistent(struct cli_state *cli,
				 uint16_t mid);
bool cli_state_seqnum_remove(struct cli_state *cli,
			     uint16_t mid);
bool cli_receive_smb(struct cli_state *cli);
bool cli_send_smb(struct cli_state *cli);
void cli_setup_packet_buf(struct cli_state *cli, char *buf);
void cli_setup_packet(struct cli_state *cli);
void cli_setup_bcc(struct cli_state *cli, void *p);
NTSTATUS cli_set_domain(struct cli_state *cli, const char *domain);
NTSTATUS cli_set_username(struct cli_state *cli, const char *username);
NTSTATUS cli_set_password(struct cli_state *cli, const char *password);
NTSTATUS cli_init_creds(struct cli_state *cli, const char *username, const char *domain, const char *password);
struct cli_state *cli_initialise(void);
struct cli_state *cli_initialise_ex(int signing_state);
void cli_nt_pipes_close(struct cli_state *cli);
void cli_shutdown(struct cli_state *cli);
void cli_sockopt(struct cli_state *cli, const char *options);
uint16 cli_setpid(struct cli_state *cli, uint16 pid);
bool cli_set_case_sensitive(struct cli_state *cli, bool case_sensitive);
struct tevent_req *cli_echo_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct cli_state *cli, uint16_t num_echos,
				 DATA_BLOB data);
NTSTATUS cli_echo_recv(struct tevent_req *req);
NTSTATUS cli_echo(struct cli_state *cli, uint16_t num_echos, DATA_BLOB data);
bool cli_ucs2(struct cli_state *cli);
bool is_andx_req(uint8_t cmd);
NTSTATUS cli_smb(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		 uint8_t smb_command, uint8_t additional_flags,
		 uint8_t wct, uint16_t *vwv,
		 uint32_t num_bytes, const uint8_t *bytes,
		 struct tevent_req **result_parent,
		 uint8_t min_wct, uint8_t *pwct, uint16_t **pvwv,
		 uint32_t *pnum_bytes, uint8_t **pbytes);

/* The following definitions come from libsmb/clierror.c  */

const char *cli_errstr(struct cli_state *cli);
NTSTATUS cli_nt_error(struct cli_state *cli);
void cli_dos_error(struct cli_state *cli, uint8 *eclass, uint32 *ecode);
int cli_errno(struct cli_state *cli);
bool cli_is_error(struct cli_state *cli);
bool cli_is_nt_error(struct cli_state *cli);
bool cli_is_dos_error(struct cli_state *cli);
NTSTATUS cli_get_nt_error(struct cli_state *cli);
void cli_set_nt_error(struct cli_state *cli, NTSTATUS status);
void cli_reset_error(struct cli_state *cli);
bool cli_state_is_connected(struct cli_state *cli);

/* The following definitions come from libsmb/clifile.c  */

struct tevent_req *cli_setpathinfo_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					struct cli_state *cli,
					uint16_t level,
					const char *path,
					uint8_t *data,
					size_t data_len);
NTSTATUS cli_setpathinfo_recv(struct tevent_req *req);
NTSTATUS cli_setpathinfo(struct cli_state *cli,
			 uint16_t level,
			 const char *path,
			 uint8_t *data,
			 size_t data_len);

struct tevent_req *cli_posix_symlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *oldname,
					const char *newname);
NTSTATUS cli_posix_symlink_recv(struct tevent_req *req);
NTSTATUS cli_posix_symlink(struct cli_state *cli,
			const char *oldname,
			const char *newname);
struct tevent_req *cli_posix_readlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					size_t len);
NTSTATUS cli_posix_readlink_recv(struct tevent_req *req, struct cli_state *cli,
				char *retpath, size_t len);
NTSTATUS cli_posix_readlink(struct cli_state *cli, const char *fname,
			char *linkpath, size_t len);
struct tevent_req *cli_posix_hardlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *oldname,
					const char *newname);
NTSTATUS cli_posix_hardlink_recv(struct tevent_req *req);
NTSTATUS cli_posix_hardlink(struct cli_state *cli,
			const char *oldname,
			const char *newname);
uint32_t unix_perms_to_wire(mode_t perms);
mode_t wire_perms_to_unix(uint32_t perms);
struct tevent_req *cli_posix_getfacl_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname);
NTSTATUS cli_posix_getfacl_recv(struct tevent_req *req,
				TALLOC_CTX *mem_ctx,
				size_t *prb_size,
				char **retbuf);
NTSTATUS cli_posix_getfacl(struct cli_state *cli,
			const char *fname,
			TALLOC_CTX *mem_ctx,
			size_t *prb_size,
			char **retbuf);
struct tevent_req *cli_posix_stat_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname);
NTSTATUS cli_posix_stat_recv(struct tevent_req *req,
				SMB_STRUCT_STAT *sbuf);
NTSTATUS cli_posix_stat(struct cli_state *cli,
			const char *fname,
			SMB_STRUCT_STAT *sbuf);
struct tevent_req *cli_posix_chmod_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					mode_t mode);
NTSTATUS cli_posix_chmod_recv(struct tevent_req *req);
NTSTATUS cli_posix_chmod(struct cli_state *cli, const char *fname, mode_t mode);
struct tevent_req *cli_posix_chown_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					uid_t uid,
					gid_t gid);
NTSTATUS cli_posix_chown_recv(struct tevent_req *req);
NTSTATUS cli_posix_chown(struct cli_state *cli,
			const char *fname,
			uid_t uid,
			gid_t gid);
struct tevent_req *cli_rename_send(TALLOC_CTX *mem_ctx,
                                struct event_context *ev,
                                struct cli_state *cli,
                                const char *fname_src,
                                const char *fname_dst);
NTSTATUS cli_rename_recv(struct tevent_req *req);
NTSTATUS cli_rename(struct cli_state *cli, const char *fname_src, const char *fname_dst);
struct tevent_req *cli_ntrename_send(TALLOC_CTX *mem_ctx,
                                struct event_context *ev,
                                struct cli_state *cli,
                                const char *fname_src,
                                const char *fname_dst);
NTSTATUS cli_ntrename_recv(struct tevent_req *req);
NTSTATUS cli_ntrename(struct cli_state *cli, const char *fname_src, const char *fname_dst);

struct tevent_req *cli_nt_hardlink_send(TALLOC_CTX *mem_ctx,
                                struct event_context *ev,
                                struct cli_state *cli,
                                const char *fname_src,
                                const char *fname_dst);
NTSTATUS cli_nt_hardlink_recv(struct tevent_req *req);
NTSTATUS cli_nt_hardlink(struct cli_state *cli, const char *fname_src, const char *fname_dst);

struct tevent_req *cli_unlink_send(TALLOC_CTX *mem_ctx,
                                struct event_context *ev,
                                struct cli_state *cli,
                                const char *fname,
                                uint16_t mayhave_attrs);
NTSTATUS cli_unlink_recv(struct tevent_req *req);
NTSTATUS cli_unlink(struct cli_state *cli, const char *fname, uint16_t mayhave_attrs);

struct tevent_req *cli_mkdir_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  const char *dname);
NTSTATUS cli_mkdir_recv(struct tevent_req *req);
NTSTATUS cli_mkdir(struct cli_state *cli, const char *dname);
struct tevent_req *cli_rmdir_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  const char *dname);
NTSTATUS cli_rmdir_recv(struct tevent_req *req);
NTSTATUS cli_rmdir(struct cli_state *cli, const char *dname);
struct tevent_req *cli_nt_delete_on_close_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t fnum,
					bool flag);
NTSTATUS cli_nt_delete_on_close_recv(struct tevent_req *req);
NTSTATUS cli_nt_delete_on_close(struct cli_state *cli, uint16_t fnum, bool flag);
struct tevent_req *cli_ntcreate_send(TALLOC_CTX *mem_ctx,
				     struct event_context *ev,
				     struct cli_state *cli,
				     const char *fname,
				     uint32_t CreatFlags,
				     uint32_t DesiredAccess,
				     uint32_t FileAttributes,
				     uint32_t ShareAccess,
				     uint32_t CreateDisposition,
				     uint32_t CreateOptions,
				     uint8_t SecurityFlags);
NTSTATUS cli_ntcreate_recv(struct tevent_req *req, uint16_t *pfnum);
NTSTATUS cli_ntcreate(struct cli_state *cli,
		      const char *fname,
		      uint32_t CreatFlags,
		      uint32_t DesiredAccess,
		      uint32_t FileAttributes,
		      uint32_t ShareAccess,
		      uint32_t CreateDisposition,
		      uint32_t CreateOptions,
		      uint8_t SecurityFlags,
		      uint16_t *pfid);
uint8_t *smb_bytes_push_str(uint8_t *buf, bool ucs2, const char *str,
			    size_t str_len, size_t *pconverted_size);
uint8_t *smb_bytes_push_bytes(uint8_t *buf, uint8_t prefix,
			      const uint8_t *bytes, size_t num_bytes);
struct tevent_req *cli_open_create(TALLOC_CTX *mem_ctx,
				   struct event_context *ev,
				   struct cli_state *cli, const char *fname,
				   int flags, int share_mode,
				   struct tevent_req **psmbreq);
struct tevent_req *cli_open_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct cli_state *cli, const char *fname,
				 int flags, int share_mode);
NTSTATUS cli_open_recv(struct tevent_req *req, uint16_t *fnum);
NTSTATUS cli_open(struct cli_state *cli, const char *fname, int flags, int share_mode, uint16_t *pfnum);
struct tevent_req *cli_close_create(TALLOC_CTX *mem_ctx,
				    struct event_context *ev,
				    struct cli_state *cli, uint16_t fnum,
				    struct tevent_req **psubreq);
struct tevent_req *cli_close_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli, uint16_t fnum);
NTSTATUS cli_close_recv(struct tevent_req *req);
NTSTATUS cli_close(struct cli_state *cli, uint16_t fnum);
struct tevent_req *cli_ftruncate_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					uint16_t fnum,
					uint64_t size);
NTSTATUS cli_ftruncate_recv(struct tevent_req *req);
NTSTATUS cli_ftruncate(struct cli_state *cli, uint16_t fnum, uint64_t size);
NTSTATUS cli_locktype(struct cli_state *cli, uint16_t fnum,
		      uint32_t offset, uint32_t len,
		      int timeout, unsigned char locktype);
bool cli_lock(struct cli_state *cli, uint16_t fnum,
	      uint32_t offset, uint32_t len, int timeout, enum brl_type lock_type);
struct tevent_req *cli_unlock_send(TALLOC_CTX *mem_ctx,
                                struct event_context *ev,
                                struct cli_state *cli,
                                uint16_t fnum,
                                uint64_t offset,
                                uint64_t len);
NTSTATUS cli_unlock_recv(struct tevent_req *req);
NTSTATUS cli_unlock(struct cli_state *cli, uint16_t fnum, uint32_t offset, uint32_t len);
bool cli_lock64(struct cli_state *cli, uint16_t fnum,
		uint64_t offset, uint64_t len, int timeout, enum brl_type lock_type);
struct tevent_req *cli_unlock64_send(TALLOC_CTX *mem_ctx,
                                struct event_context *ev,
                                struct cli_state *cli,
                                uint16_t fnum,
                                uint64_t offset,
                                uint64_t len);
NTSTATUS cli_unlock64_recv(struct tevent_req *req);
NTSTATUS cli_unlock64(struct cli_state *cli, uint16_t fnum, uint64_t offset, uint64_t len);
struct tevent_req *cli_posix_lock_send(TALLOC_CTX *mem_ctx,
                                        struct event_context *ev,
                                        struct cli_state *cli,
                                        uint16_t fnum,
                                        uint64_t offset,
                                        uint64_t len,
                                        bool wait_lock,
                                        enum brl_type lock_type);
NTSTATUS cli_posix_lock_recv(struct tevent_req *req);
NTSTATUS cli_posix_lock(struct cli_state *cli, uint16_t fnum,
			uint64_t offset, uint64_t len,
			bool wait_lock, enum brl_type lock_type);
struct tevent_req *cli_posix_unlock_send(TALLOC_CTX *mem_ctx,
                                        struct event_context *ev,
                                        struct cli_state *cli,
                                        uint16_t fnum,
                                        uint64_t offset,
                                        uint64_t len);
NTSTATUS cli_posix_unlock_recv(struct tevent_req *req);
NTSTATUS cli_posix_unlock(struct cli_state *cli, uint16_t fnum, uint64_t offset, uint64_t len);
struct tevent_req *cli_getattrE_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
                                uint16_t fnum);
NTSTATUS cli_getattrE_recv(struct tevent_req *req,
                        uint16_t *attr,
                        SMB_OFF_T *size,
                        time_t *change_time,
                        time_t *access_time,
                        time_t *write_time);
NTSTATUS cli_getattrE(struct cli_state *cli,
			uint16_t fnum,
			uint16_t *attr,
			SMB_OFF_T *size,
			time_t *change_time,
			time_t *access_time,
			time_t *write_time);
struct tevent_req *cli_setattrE_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				uint16_t fnum,
				time_t change_time,
				time_t access_time,
				time_t write_time);
NTSTATUS cli_setattrE_recv(struct tevent_req *req);
NTSTATUS cli_setattrE(struct cli_state *cli,
			uint16_t fnum,
			time_t change_time,
			time_t access_time,
			time_t write_time);
struct tevent_req *cli_getatr_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname);
NTSTATUS cli_getatr_recv(struct tevent_req *req,
				uint16_t *attr,
				SMB_OFF_T *size,
				time_t *write_time);
NTSTATUS cli_getatr(struct cli_state *cli,
			const char *fname,
			uint16_t *attr,
			SMB_OFF_T *size,
			time_t *write_time);
struct tevent_req *cli_setatr_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *fname,
				uint16_t attr,
				time_t mtime);
NTSTATUS cli_setatr_recv(struct tevent_req *req);
NTSTATUS cli_setatr(struct cli_state *cli,
                const char *fname,
                uint16_t attr,
                time_t mtime);
struct tevent_req *cli_chkpath_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli,
				  const char *fname);
NTSTATUS cli_chkpath_recv(struct tevent_req *req);
NTSTATUS cli_chkpath(struct cli_state *cli, const char *path);
struct tevent_req *cli_dskattr_send(TALLOC_CTX *mem_ctx,
				  struct event_context *ev,
				  struct cli_state *cli);
NTSTATUS cli_dskattr_recv(struct tevent_req *req, int *bsize, int *total,
			  int *avail);
NTSTATUS cli_dskattr(struct cli_state *cli, int *bsize, int *total, int *avail);
struct tevent_req *cli_ctemp_send(TALLOC_CTX *mem_ctx,
				struct event_context *ev,
				struct cli_state *cli,
				const char *path);
NTSTATUS cli_ctemp_recv(struct tevent_req *req,
			TALLOC_CTX *ctx,
			uint16_t *pfnum,
			char **outfile);
NTSTATUS cli_ctemp(struct cli_state *cli,
			TALLOC_CTX *ctx,
			const char *path,
			uint16_t *pfnum,
			char **out_path);
NTSTATUS cli_raw_ioctl(struct cli_state *cli, uint16_t fnum, uint32_t code, DATA_BLOB *blob);
NTSTATUS cli_set_ea_path(struct cli_state *cli, const char *path,
			 const char *ea_name, const char *ea_val,
			 size_t ea_len);
NTSTATUS cli_set_ea_fnum(struct cli_state *cli, uint16_t fnum,
			 const char *ea_name, const char *ea_val,
			 size_t ea_len);
struct tevent_req *cli_get_ea_list_path_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct cli_state *cli,
					     const char *fname);
NTSTATUS cli_get_ea_list_path_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				   size_t *pnum_eas, struct ea_struct **peas);
NTSTATUS cli_get_ea_list_path(struct cli_state *cli, const char *path,
		TALLOC_CTX *ctx,
		size_t *pnum_eas,
		struct ea_struct **pea_list);
struct tevent_req *cli_posix_open_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname,
					int flags,
					mode_t mode);
NTSTATUS cli_posix_open_recv(struct tevent_req *req, uint16_t *pfnum);
NTSTATUS cli_posix_open(struct cli_state *cli, const char *fname,
			int flags, mode_t mode, uint16_t *fnum);
struct tevent_req *cli_posix_mkdir_send(TALLOC_CTX *mem_ctx,
                                        struct event_context *ev,
                                        struct cli_state *cli,
                                        const char *fname,
                                        mode_t mode);
NTSTATUS cli_posix_mkdir_recv(struct tevent_req *req);
NTSTATUS cli_posix_mkdir(struct cli_state *cli, const char *fname, mode_t mode);

struct tevent_req *cli_posix_unlink_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname);
NTSTATUS cli_posix_unlink_recv(struct tevent_req *req);
NTSTATUS cli_posix_unlink(struct cli_state *cli, const char *fname);

struct tevent_req *cli_posix_rmdir_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli,
					const char *fname);
NTSTATUS cli_posix_rmdir_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx);
NTSTATUS cli_posix_rmdir(struct cli_state *cli, const char *fname);
struct tevent_req *cli_notify_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   struct cli_state *cli, uint16_t fnum,
				   uint32_t buffer_size,
				   uint32_t completion_filter, bool recursive);
NTSTATUS cli_notify_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 uint32_t *pnum_changes,
			 struct notify_change **pchanges);

/* The following definitions come from libsmb/clifsinfo.c  */

struct tevent_req *cli_unix_extensions_version_send(TALLOC_CTX *mem_ctx,
						    struct tevent_context *ev,
						    struct cli_state *cli);
NTSTATUS cli_unix_extensions_version_recv(struct tevent_req *req,
					  uint16_t *pmajor, uint16_t *pminor,
					  uint32_t *pcaplow,
					  uint32_t *pcaphigh);
NTSTATUS cli_unix_extensions_version(struct cli_state *cli, uint16 *pmajor,
				     uint16 *pminor, uint32 *pcaplow,
				     uint32 *pcaphigh);
struct tevent_req *cli_set_unix_extensions_capabilities_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev, struct cli_state *cli,
	uint16_t major, uint16_t minor, uint32_t caplow, uint32_t caphigh);
NTSTATUS cli_set_unix_extensions_capabilities_recv(struct tevent_req *req);
NTSTATUS cli_set_unix_extensions_capabilities(struct cli_state *cli,
					      uint16 major, uint16 minor,
					      uint32 caplow, uint32 caphigh);
struct tevent_req *cli_get_fs_attr_info_send(TALLOC_CTX *mem_ctx,
					     struct tevent_context *ev,
					     struct cli_state *cli);
NTSTATUS cli_get_fs_attr_info_recv(struct tevent_req *req, uint32_t *fs_attr);
NTSTATUS cli_get_fs_attr_info(struct cli_state *cli, uint32_t *fs_attr);
NTSTATUS cli_get_fs_volume_info(struct cli_state *cli, fstring volume_name,
				uint32 *pserial_number, time_t *pdate);
NTSTATUS cli_get_fs_full_size_info(struct cli_state *cli,
				   uint64_t *total_allocation_units,
				   uint64_t *caller_allocation_units,
				   uint64_t *actual_allocation_units,
				   uint64_t *sectors_per_allocation_unit,
				   uint64_t *bytes_per_sector);
NTSTATUS cli_get_posix_fs_info(struct cli_state *cli,
			       uint32 *optimal_transfer_size,
			       uint32 *block_size,
			       uint64_t *total_blocks,
			       uint64_t *blocks_available,
			       uint64_t *user_blocks_available,
			       uint64_t *total_file_nodes,
			       uint64_t *free_file_nodes,
			       uint64_t *fs_identifier);
NTSTATUS cli_raw_ntlm_smb_encryption_start(struct cli_state *cli,
				const char *user,
				const char *pass,
				const char *domain);
NTSTATUS cli_gss_smb_encryption_start(struct cli_state *cli);
NTSTATUS cli_force_encryption(struct cli_state *c,
			const char *username,
			const char *password,
			const char *domain);

/* The following definitions come from libsmb/clilist.c  */

NTSTATUS cli_list_old(struct cli_state *cli,const char *Mask,uint16 attribute,
		      NTSTATUS (*fn)(const char *, struct file_info *,
				 const char *, void *), void *state);
NTSTATUS cli_list_trans(struct cli_state *cli, const char *mask,
			uint16_t attribute, int info_level,
			NTSTATUS (*fn)(const char *mnt, struct file_info *finfo,
				   const char *mask, void *private_data),
			void *private_data);
struct tevent_req *cli_list_send(TALLOC_CTX *mem_ctx,
				 struct tevent_context *ev,
				 struct cli_state *cli,
				 const char *mask,
				 uint16_t attribute,
				 uint16_t info_level);
NTSTATUS cli_list_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
		       struct file_info **finfo, size_t *num_finfo);
NTSTATUS cli_list(struct cli_state *cli,const char *Mask,uint16 attribute,
		  NTSTATUS (*fn)(const char *, struct file_info *, const char *,
			     void *), void *state);

/* The following definitions come from libsmb/climessage.c  */

struct tevent_req *cli_message_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct cli_state *cli,
				    const char *host, const char *username,
				    const char *message);
NTSTATUS cli_message_recv(struct tevent_req *req);
NTSTATUS cli_message(struct cli_state *cli, const char *host,
		     const char *username, const char *message);

/* The following definitions come from libsmb/clioplock.c  */

struct tevent_req *cli_oplock_ack_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct cli_state *cli,
				       uint16_t fnum, uint8_t level);
NTSTATUS cli_oplock_ack_recv(struct tevent_req *req);
NTSTATUS cli_oplock_ack(struct cli_state *cli, uint16_t fnum, unsigned char level);
void cli_oplock_handler(struct cli_state *cli,
			NTSTATUS (*handler)(struct cli_state *, uint16_t, unsigned char));

/* The following definitions come from libsmb/cliprint.c  */

int cli_print_queue(struct cli_state *cli,
		    void (*fn)(struct print_job_info *));
int cli_printjob_del(struct cli_state *cli, int job);

/* The following definitions come from libsmb/cliquota.c  */

NTSTATUS cli_get_quota_handle(struct cli_state *cli, uint16_t *quota_fnum);
void free_ntquota_list(SMB_NTQUOTA_LIST **qt_list);
NTSTATUS cli_get_user_quota(struct cli_state *cli, int quota_fnum,
			    SMB_NTQUOTA_STRUCT *pqt);
NTSTATUS cli_set_user_quota(struct cli_state *cli, int quota_fnum,
			    SMB_NTQUOTA_STRUCT *pqt);
NTSTATUS cli_list_user_quota(struct cli_state *cli, int quota_fnum,
			     SMB_NTQUOTA_LIST **pqt_list);
NTSTATUS cli_get_fs_quota_info(struct cli_state *cli, int quota_fnum,
			       SMB_NTQUOTA_STRUCT *pqt);
NTSTATUS cli_set_fs_quota_info(struct cli_state *cli, int quota_fnum,
			       SMB_NTQUOTA_STRUCT *pqt);

/* The following definitions come from libsmb/clireadwrite.c  */

struct tevent_req *cli_read_andx_create(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					struct cli_state *cli, uint16_t fnum,
					off_t offset, size_t size,
					struct tevent_req **psmbreq);
struct tevent_req *cli_read_andx_send(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct cli_state *cli, uint16_t fnum,
				      off_t offset, size_t size);
NTSTATUS cli_read_andx_recv(struct tevent_req *req, ssize_t *received,
			    uint8_t **rcvbuf);
struct tevent_req *cli_pull_send(TALLOC_CTX *mem_ctx,
				 struct event_context *ev,
				 struct cli_state *cli,
				 uint16_t fnum, off_t start_offset,
				 SMB_OFF_T size, size_t window_size,
				 NTSTATUS (*sink)(char *buf, size_t n,
						  void *priv),
				 void *priv);
NTSTATUS cli_pull_recv(struct tevent_req *req, SMB_OFF_T *received);
NTSTATUS cli_pull(struct cli_state *cli, uint16_t fnum,
		  off_t start_offset, SMB_OFF_T size, size_t window_size,
		  NTSTATUS (*sink)(char *buf, size_t n, void *priv),
		  void *priv, SMB_OFF_T *received);
ssize_t cli_read(struct cli_state *cli, uint16_t fnum, char *buf,
		 off_t offset, size_t size);
NTSTATUS cli_smbwrite(struct cli_state *cli, uint16_t fnum, char *buf,
		      off_t offset, size_t size1, size_t *ptotal);
struct tevent_req *cli_write_andx_create(TALLOC_CTX *mem_ctx,
					 struct event_context *ev,
					 struct cli_state *cli, uint16_t fnum,
					 uint16_t mode, const uint8_t *buf,
					 off_t offset, size_t size,
					 struct tevent_req **reqs_before,
					 int num_reqs_before,
					 struct tevent_req **psmbreq);
struct tevent_req *cli_write_andx_send(TALLOC_CTX *mem_ctx,
				       struct event_context *ev,
				       struct cli_state *cli, uint16_t fnum,
				       uint16_t mode, const uint8_t *buf,
				       off_t offset, size_t size);
NTSTATUS cli_write_andx_recv(struct tevent_req *req, size_t *pwritten);

NTSTATUS cli_writeall(struct cli_state *cli, uint16_t fnum, uint16_t mode,
		      const uint8_t *buf, off_t offset, size_t size,
		      size_t *pwritten);

struct tevent_req *cli_push_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct cli_state *cli,
				 uint16_t fnum, uint16_t mode,
				 off_t start_offset, size_t window_size,
				 size_t (*source)(uint8_t *buf, size_t n,
						  void *priv),
				 void *priv);
NTSTATUS cli_push_recv(struct tevent_req *req);
NTSTATUS cli_push(struct cli_state *cli, uint16_t fnum, uint16_t mode,
		  off_t start_offset, size_t window_size,
		  size_t (*source)(uint8_t *buf, size_t n, void *priv),
		  void *priv);

/* The following definitions come from libsmb/clisecdesc.c  */

struct security_descriptor *cli_query_secdesc(struct cli_state *cli, uint16_t fnum,
			    TALLOC_CTX *mem_ctx);
NTSTATUS cli_set_secdesc(struct cli_state *cli, uint16_t fnum,
			 struct security_descriptor *sd);

/* The following definitions come from libsmb/clistr.c  */

size_t clistr_push_fn(const char *function,
			unsigned int line,
			struct cli_state *cli,
			void *dest,
			const char *src,
			int dest_len,
			int flags);
size_t clistr_pull_fn(const char *function,
			unsigned int line,
			const char *inbuf,
			char *dest,
			const void *src,
			int dest_len,
			int src_len,
			int flags);
size_t clistr_pull_talloc_fn(const char *function,
				unsigned int line,
				TALLOC_CTX *ctx,
				const char *base,
				uint16_t flags2,
				char **pp_dest,
				const void *src,
				int src_len,
				int flags);
size_t clistr_align_out(struct cli_state *cli, const void *p, int flags);

/* The following definitions come from libsmb/clitrans.c  */

struct tevent_req *cli_trans_send(
	TALLOC_CTX *mem_ctx, struct event_context *ev,
	struct cli_state *cli, uint8_t cmd,
	const char *pipe_name, uint16_t fid, uint16_t function, int flags,
	uint16_t *setup, uint8_t num_setup, uint8_t max_setup,
	uint8_t *param, uint32_t num_param, uint32_t max_param,
	uint8_t *data, uint32_t num_data, uint32_t max_data);
NTSTATUS cli_trans_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			uint16_t *recv_flags2,
			uint16_t **setup, uint8_t min_setup,
			uint8_t *num_setup,
			uint8_t **param, uint32_t min_param,
			uint32_t *num_param,
			uint8_t **data, uint32_t min_data,
			uint32_t *num_data);
NTSTATUS cli_trans(TALLOC_CTX *mem_ctx, struct cli_state *cli,
		   uint8_t trans_cmd,
		   const char *pipe_name, uint16_t fid, uint16_t function,
		   int flags,
		   uint16_t *setup, uint8_t num_setup, uint8_t max_setup,
		   uint8_t *param, uint32_t num_param, uint32_t max_param,
		   uint8_t *data, uint32_t num_data, uint32_t max_data,
		   uint16_t *recv_flags2,
		   uint16_t **rsetup, uint8_t min_rsetup, uint8_t *num_rsetup,
		   uint8_t **rparam, uint32_t min_rparam, uint32_t *num_rparam,
		   uint8_t **rdata, uint32_t min_rdata, uint32_t *num_rdata);

/* The following definitions come from libsmb/smb_seal.c  */

NTSTATUS get_enc_ctx_num(const uint8_t *buf, uint16 *p_enc_ctx_num);
bool common_encryption_on(struct smb_trans_enc_state *es);
NTSTATUS common_ntlm_decrypt_buffer(struct ntlmssp_state *ntlmssp_state, char *buf);
NTSTATUS common_ntlm_encrypt_buffer(struct ntlmssp_state *ntlmssp_state,
				uint16 enc_ctx_num,
				char *buf,
				char **ppbuf_out);
NTSTATUS common_encrypt_buffer(struct smb_trans_enc_state *es, char *buffer, char **buf_out);
NTSTATUS common_decrypt_buffer(struct smb_trans_enc_state *es, char *buf);
void common_free_encryption_state(struct smb_trans_enc_state **pp_es);
void common_free_enc_buffer(struct smb_trans_enc_state *es, char *buf);
bool cli_encryption_on(struct cli_state *cli);
void cli_free_encryption_context(struct cli_state *cli);
void cli_free_enc_buffer(struct cli_state *cli, char *buf);
NTSTATUS cli_decrypt_message(struct cli_state *cli);
NTSTATUS cli_encrypt_message(struct cli_state *cli, char *buf, char **buf_out);

/* The following definitions come from libsmb/clisigning.c  */

bool cli_simple_set_signing(struct cli_state *cli,
			    const DATA_BLOB user_session_key,
			    const DATA_BLOB response);
bool cli_temp_set_signing(struct cli_state *cli);
void cli_calculate_sign_mac(struct cli_state *cli, char *buf, uint32_t *seqnum);
bool cli_check_sign_mac(struct cli_state *cli, const char *buf, uint32_t seqnum);
bool client_is_signing_on(struct cli_state *cli);
bool client_is_signing_allowed(struct cli_state *cli);
bool client_is_signing_mandatory(struct cli_state *cli);
void cli_set_signing_negotiated(struct cli_state *cli);

#endif /* _LIBSMB_PROTO_H_ */
