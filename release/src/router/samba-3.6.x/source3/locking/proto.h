/*
 *  Unix SMB/CIFS implementation.
 *  Locking functions
 *
 *  Copyright (C) Andrew Tridgell	1992-2000
 *  Copyright (C) Jeremy Allison	1992-2006
 *  Copyright (C) Volker Lendecke	2005
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LOCKING_PROTO_H_
#define _LOCKING_PROTO_H_

/* The following definitions come from locking/brlock.c  */

bool brl_same_context(const struct lock_context *ctx1,
			     const struct lock_context *ctx2);
NTSTATUS brl_lock_failed(files_struct *fsp, const struct lock_struct *lock, bool blocking_lock);
void brl_init(bool read_only);
void brl_shutdown(void);

NTSTATUS brl_lock_windows_default(struct byte_range_lock *br_lck,
		struct lock_struct *plock,
		bool blocking_lock);

NTSTATUS brl_lock(struct messaging_context *msg_ctx,
		struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size,
		enum brl_type lock_type,
		enum brl_flavour lock_flav,
		bool blocking_lock,
		uint64_t *psmblctx,
		struct blocking_lock_record *blr);
bool brl_unlock(struct messaging_context *msg_ctx,
		struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size,
		enum brl_flavour lock_flav);
bool brl_unlock_windows_default(struct messaging_context *msg_ctx,
			       struct byte_range_lock *br_lck,
			       const struct lock_struct *plock);
bool brl_locktest(struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size,
		enum brl_type lock_type,
		enum brl_flavour lock_flav);
NTSTATUS brl_lockquery(struct byte_range_lock *br_lck,
		uint64_t *psmblctx,
		struct server_id pid,
		br_off *pstart,
		br_off *psize,
		enum brl_type *plock_type,
		enum brl_flavour lock_flav);
bool brl_lock_cancel(struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size,
		enum brl_flavour lock_flav,
		struct blocking_lock_record *blr);
bool brl_lock_cancel_default(struct byte_range_lock *br_lck,
		struct lock_struct *plock);
void brl_close_fnum(struct messaging_context *msg_ctx,
		    struct byte_range_lock *br_lck);
int brl_forall(void (*fn)(struct file_id id, struct server_id pid,
			  enum brl_type lock_type,
			  enum brl_flavour lock_flav,
			  br_off start, br_off size,
			  void *private_data),
	       void *private_data);
struct byte_range_lock *brl_get_locks(TALLOC_CTX *mem_ctx,
					files_struct *fsp);
struct byte_range_lock *brl_get_locks_readonly(files_struct *fsp);
void brl_register_msgs(struct messaging_context *msg_ctx);

/* The following definitions come from locking/locking.c  */

const char *lock_type_name(enum brl_type lock_type);
const char *lock_flav_name(enum brl_flavour lock_flav);
void init_strict_lock_struct(files_struct *fsp,
				uint64_t smblctx,
				br_off start,
				br_off size,
				enum brl_type lock_type,
				struct lock_struct *plock);
bool strict_lock_default(files_struct *fsp,
				struct lock_struct *plock);
void strict_unlock_default(files_struct *fsp,
				struct lock_struct *plock);
NTSTATUS query_lock(files_struct *fsp,
			uint64_t *psmblctx,
			uint64_t *pcount,
			uint64_t *poffset,
			enum brl_type *plock_type,
			enum brl_flavour lock_flav);
struct byte_range_lock *do_lock(struct messaging_context *msg_ctx,
			files_struct *fsp,
			uint64_t smblctx,
			uint64_t count,
			uint64_t offset,
			enum brl_type lock_type,
			enum brl_flavour lock_flav,
			bool blocking_lock,
			NTSTATUS *perr,
			uint64_t *psmblctx,
			struct blocking_lock_record *blr);
NTSTATUS do_unlock(struct messaging_context *msg_ctx,
			files_struct *fsp,
			uint64_t smblctx,
			uint64_t count,
			uint64_t offset,
			enum brl_flavour lock_flav);
NTSTATUS do_lock_cancel(files_struct *fsp,
			uint64 smblctx,
			uint64_t count,
			uint64_t offset,
			enum brl_flavour lock_flav,
			struct blocking_lock_record *blr);
void locking_close_file(struct messaging_context *msg_ctx,
			files_struct *fsp,
			enum file_close_type close_type);
bool locking_init(void);
bool locking_init_readonly(void);
bool locking_end(void);
char *share_mode_str(TALLOC_CTX *ctx, int num, const struct share_mode_entry *e);
struct share_mode_lock *get_share_mode_lock(TALLOC_CTX *mem_ctx,
					    const struct file_id id,
					    const char *servicepath,
					    const struct smb_filename *smb_fname,
					    const struct timespec *old_write_time);
struct share_mode_lock *fetch_share_mode_unlocked(TALLOC_CTX *mem_ctx,
						  const struct file_id id);
bool rename_share_filename(struct messaging_context *msg_ctx,
			struct share_mode_lock *lck,
			const char *servicepath,
			uint32_t orig_name_hash,
			uint32_t new_name_hash,
			const struct smb_filename *smb_fname);
void get_file_infos(struct file_id id,
		    uint32_t name_hash,
		    bool *delete_on_close,
		    struct timespec *write_time);
bool is_valid_share_mode_entry(const struct share_mode_entry *e);
bool is_deferred_open_entry(const struct share_mode_entry *e);
bool is_unused_share_mode_entry(const struct share_mode_entry *e);
void set_share_mode(struct share_mode_lock *lck, files_struct *fsp,
		    uid_t uid, uint64_t mid, uint16 op_type);
void add_deferred_open(struct share_mode_lock *lck, uint64_t mid,
		       struct timeval request_time,
		       struct server_id pid, struct file_id id);
bool del_share_mode(struct share_mode_lock *lck, files_struct *fsp);
void del_deferred_open_entry(struct share_mode_lock *lck, uint64_t mid,
			     struct server_id pid);
bool remove_share_oplock(struct share_mode_lock *lck, files_struct *fsp);
bool downgrade_share_oplock(struct share_mode_lock *lck, files_struct *fsp);
NTSTATUS can_set_delete_on_close(files_struct *fsp, uint32 dosmode);
bool get_delete_on_close_token(struct share_mode_lock *lck,
			uint32_t name_hash,
			const struct security_token **pp_nt_tok,
			const struct security_unix_token **pp_tok);
void set_delete_on_close_lck(files_struct *fsp,
			struct share_mode_lock *lck,
			bool delete_on_close,
			const struct security_token *nt_tok,
			const struct security_unix_token *tok);
bool set_delete_on_close(files_struct *fsp, bool delete_on_close,
			const struct security_token *nt_tok,
			const struct security_unix_token *tok);
bool is_delete_on_close_set(struct share_mode_lock *lck, uint32_t name_hash);
bool set_sticky_write_time(struct file_id fileid, struct timespec write_time);
bool set_write_time(struct file_id fileid, struct timespec write_time);
int share_mode_forall(void (*fn)(const struct share_mode_entry *, const char *,
				 const char *, void *),
		      void *private_data);

/* The following definitions come from locking/posix.c  */

bool is_posix_locked(files_struct *fsp,
			uint64_t *pu_offset,
			uint64_t *pu_count,
			enum brl_type *plock_type,
			enum brl_flavour lock_flav);
bool posix_locking_init(bool read_only);
bool posix_locking_end(void);
void reduce_windows_lock_ref_count(files_struct *fsp, unsigned int dcount);
int fd_close_posix(struct files_struct *fsp);
bool set_posix_lock_windows_flavour(files_struct *fsp,
			uint64_t u_offset,
			uint64_t u_count,
			enum brl_type lock_type,
			const struct lock_context *lock_ctx,
			const struct lock_struct *plocks,
			int num_locks,
			int *errno_ret);
bool release_posix_lock_windows_flavour(files_struct *fsp,
				uint64_t u_offset,
				uint64_t u_count,
				enum brl_type deleted_lock_type,
				const struct lock_context *lock_ctx,
				const struct lock_struct *plocks,
				int num_locks);
bool set_posix_lock_posix_flavour(files_struct *fsp,
			uint64_t u_offset,
			uint64_t u_count,
			enum brl_type lock_type,
			int *errno_ret);
bool release_posix_lock_posix_flavour(files_struct *fsp,
				uint64_t u_offset,
				uint64_t u_count,
				const struct lock_context *lock_ctx,
				const struct lock_struct *plocks,
				int num_locks);

#endif /* _LOCKING_PROTO_H_ */
