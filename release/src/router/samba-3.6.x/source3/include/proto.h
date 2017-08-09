/*
 * Unix SMB/CIFS implementation.
 * collected prototypes header
 *
 * frozen from "make proto" in May 2008
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PROTO_H_
#define _PROTO_H_

/* The following definitions come from lib/access.c  */

bool client_match(const char *tok, const void *item);
bool list_match(const char **list,const void *item,
		bool (*match_fn)(const char *, const void *));
bool allow_access(const char **deny_list,
		const char **allow_list,
		const char *cname,
		const char *caddr);

/* The following definitions come from lib/adt_tree.c  */


/* The following definitions come from lib/afs.c  */

char *afs_createtoken_str(const char *username, const char *cell);
bool afs_login(connection_struct *conn);
bool afs_login(connection_struct *conn);
char *afs_createtoken_str(const char *username, const char *cell);

/* The following definitions come from lib/afs_settoken.c  */

int afs_syscall( int subcall,
	  char * path,
	  int cmd,
	  char * cmarg,
	  int follow);
bool afs_settoken_str(const char *token_string);
bool afs_settoken_str(const char *token_string);

/* The following definitions come from lib/audit.c  */

const char *audit_category_str(uint32 category);
const char *audit_param_str(uint32 category);
const char *audit_description_str(uint32 category);
bool get_audit_category_from_param(const char *param, uint32 *audit_category);
const char *audit_policy_str(TALLOC_CTX *mem_ctx, uint32 policy);

/* The following definitions come from lib/charcnv.c  */

char lp_failed_convert_char(void);
void lazy_initialize_conv(void);
void gfree_charcnv(void);
void init_iconv(void);
size_t convert_string(charset_t from, charset_t to,
		      void const *src, size_t srclen, 
		      void *dest, size_t destlen, bool allow_bad_conv);
size_t unix_strupper(const char *src, size_t srclen, char *dest, size_t destlen);
char *talloc_strdup_upper(TALLOC_CTX *ctx, const char *s);
char *strupper_talloc(TALLOC_CTX *ctx, const char *s);
size_t unix_strlower(const char *src, size_t srclen, char *dest, size_t destlen);
char *talloc_strdup_lower(TALLOC_CTX *ctx, const char *s);
char *strlower_talloc(TALLOC_CTX *ctx, const char *s);
size_t ucs2_align(const void *base_ptr, const void *p, int flags);
size_t push_ascii(void *dest, const char *src, size_t dest_len, int flags);
size_t push_ascii_fstring(void *dest, const char *src);
size_t push_ascii_nstring(void *dest, const char *src);
size_t pull_ascii(char *dest, const void *src, size_t dest_len, size_t src_len, int flags);
size_t pull_ascii_fstring(char *dest, const void *src);
size_t pull_ascii_nstring(char *dest, size_t dest_len, const void *src);
size_t push_ucs2(const void *base_ptr, void *dest, const char *src, size_t dest_len, int flags);
size_t push_utf8_fstring(void *dest, const char *src);
bool push_utf8_talloc(TALLOC_CTX *ctx, char **dest, const char *src,
		      size_t *converted_size);
size_t pull_ucs2(const void *base_ptr, char *dest, const void *src, size_t dest_len, size_t src_len, int flags);
size_t pull_ucs2_base_talloc(TALLOC_CTX *ctx,
			const void *base_ptr,
			char **ppdest,
			const void *src,
			size_t src_len,
			int flags);
size_t pull_ucs2_fstring(char *dest, const void *src);
bool push_ucs2_talloc(TALLOC_CTX *ctx, smb_ucs2_t **dest, const char *src,
		      size_t *converted_size);
bool pull_utf8_talloc(TALLOC_CTX *ctx, char **dest, const char *src,
		      size_t *converted_size);
bool pull_ucs2_talloc(TALLOC_CTX *ctx, char **dest, const smb_ucs2_t *src,
		      size_t *converted_size);
bool pull_ascii_talloc(TALLOC_CTX *ctx, char **dest, const char *src,
		       size_t *converted_size);
size_t push_string_check_fn(const char *function, unsigned int line,
			    void *dest, const char *src,
			    size_t dest_len, int flags);
size_t push_string_base(const char *function, unsigned int line,
			const char *base, uint16 flags2, 
			void *dest, const char *src,
			size_t dest_len, int flags);
size_t pull_string_fn(const char *function,
			unsigned int line,
			const void *base_ptr,
			uint16 smb_flags2,
			char *dest,
			const void *src,
			size_t dest_len,
			size_t src_len,
			int flags);
size_t pull_string_talloc_fn(const char *function,
			unsigned int line,
			TALLOC_CTX *ctx,
			const void *base_ptr,
			uint16 smb_flags2,
			char **ppdest,
			const void *src,
			size_t src_len,
			int flags);
size_t align_string(const void *base_ptr, const char *p, int flags);

/* The following definitions come from lib/clobber.c  */

void clobber_region(const char *fn, unsigned int line, char *dest, size_t len);

/* The following definitions come from lib/conn_tdb.c  */

struct db_record *connections_fetch_entry(TALLOC_CTX *mem_ctx,
					  connection_struct *conn,
					  const char *name);
int connections_traverse(int (*fn)(struct db_record *rec,
				   void *private_data),
			 void *private_data);
int connections_forall(int (*fn)(struct db_record *rec,
				 const struct connections_key *key,
				 const struct connections_data *data,
				 void *private_data),
		       void *private_data);
int connections_forall_read(int (*fn)(const struct connections_key *key,
				      const struct connections_data *data,
				      void *private_data),
			    void *private_data);
bool connections_init(bool rw);

/* The following definitions come from lib/dmallocmsg.c  */

void register_dmalloc_msgs(struct messaging_context *msg_ctx);

/* The following definitions come from lib/dprintf.c  */

void display_set_stderr(void);

/* The following definitions come from lib/errmap_unix.c  */

NTSTATUS map_nt_error_from_unix(int unix_error);
int map_errno_from_nt_status(NTSTATUS status);

/* The following definitions come from lib/fault.c  */
void fault_setup(void (*fn)(void *));
void dump_core_setup(const char *progname);

/* The following definitions come from lib/file_id.c  */

struct file_id vfs_file_id_from_sbuf(connection_struct *conn, const SMB_STRUCT_STAT *sbuf);
bool file_id_equal(const struct file_id *id1, const struct file_id *id2);
const char *file_id_string_tos(const struct file_id *id);
void push_file_id_16(char *buf, const struct file_id *id);
void push_file_id_24(char *buf, const struct file_id *id);
void pull_file_id_24(char *buf, struct file_id *id);

/* The following definitions come from lib/gencache.c  */

bool gencache_set(const char *keystr, const char *value, time_t timeout);
bool gencache_del(const char *keystr);
bool gencache_get(const char *keystr, char **valstr, time_t *timeout);
bool gencache_parse(const char *keystr,
		    void (*parser)(time_t timeout, DATA_BLOB blob,
				   void *private_data),
		    void *private_data);
bool gencache_get_data_blob(const char *keystr, DATA_BLOB *blob,
			    time_t *timeout, bool *was_expired);
bool gencache_stabilize(void);
bool gencache_set_data_blob(const char *keystr, const DATA_BLOB *blob, time_t timeout);
void gencache_iterate_blobs(void (*fn)(const char *key, DATA_BLOB value,
				       time_t timeout, void *private_data),
			    void *private_data, const char *pattern);
void gencache_iterate(void (*fn)(const char* key, const char *value, time_t timeout, void* dptr),
                      void* data, const char* keystr_pattern);

/* The following definitions come from lib/interface.c  */

bool ismyaddr(const struct sockaddr *ip);
bool ismyip_v4(struct in_addr ip);
bool is_local_net(const struct sockaddr *from);
void setup_linklocal_scope_id(struct sockaddr *pss);
bool is_local_net_v4(struct in_addr from);
int iface_count(void);
int iface_count_v4_nl(void);
const struct in_addr *first_ipv4_iface(void);
struct interface *get_interface(int n);
const struct sockaddr_storage *iface_n_sockaddr_storage(int n);
const struct in_addr *iface_n_ip_v4(int n);
const struct in_addr *iface_n_bcast_v4(int n);
const struct sockaddr_storage *iface_n_bcast(int n);
const struct sockaddr_storage *iface_ip(const struct sockaddr *ip);
bool iface_local(const struct sockaddr *ip);
void load_interfaces(void);
void gfree_interfaces(void);
bool interfaces_changed(void);

/* The following definitions come from lib/ldap_debug_handler.c  */

void init_ldap_debugging(void);

/* The following definitions come from lib/ldap_escape.c  */

char *escape_ldap_string(TALLOC_CTX *mem_ctx, const char *s);
char *escape_rdn_val_string_alloc(const char *s);

/* The following definitions come from lib/module.c  */

NTSTATUS smb_load_module(const char *module_name);
int smb_load_modules(const char **modules);
NTSTATUS smb_probe_module(const char *subsystem, const char *module);
NTSTATUS smb_load_module(const char *module_name);
int smb_load_modules(const char **modules);
NTSTATUS smb_probe_module(const char *subsystem, const char *module);
void init_modules(void);

/* The following definitions come from lib/ms_fnmatch.c  */

int ms_fnmatch(const char *pattern, const char *string, bool translate_pattern,
	       bool is_case_sensitive);
int gen_fnmatch(const char *pattern, const char *string);

/* The following definitions come from lib/pidfile.c  */

pid_t pidfile_pid(const char *name);
void pidfile_create(const char *program_name);
void pidfile_unlink(void);

/* The following definitions come from lib/recvfile.c  */

ssize_t sys_recvfile(int fromfd,
			int tofd,
			SMB_OFF_T offset,
			size_t count);
ssize_t sys_recvfile(int fromfd,
			int tofd,
			SMB_OFF_T offset,
			size_t count);
ssize_t drain_socket(int sockfd, size_t count);

/* The following definitions come from lib/secdesc.c  */

uint32_t get_sec_info(const struct security_descriptor *sd);
struct security_descriptor *sec_desc_merge(TALLOC_CTX *ctx, struct security_descriptor *new_sdb, struct security_descriptor *old_sdb);
struct sec_desc_buf *sec_desc_merge_buf(TALLOC_CTX *ctx, struct sec_desc_buf *new_sdb, struct sec_desc_buf *old_sdb);
struct security_descriptor *make_sec_desc(TALLOC_CTX *ctx,
			enum security_descriptor_revision revision,
			uint16 type,
			const struct dom_sid *owner_sid, const struct dom_sid *grp_sid,
			struct security_acl *sacl, struct security_acl *dacl, size_t *sd_size);
struct security_descriptor *dup_sec_desc(TALLOC_CTX *ctx, const struct security_descriptor *src);
NTSTATUS marshall_sec_desc(TALLOC_CTX *mem_ctx,
			   struct security_descriptor *secdesc,
			   uint8 **data, size_t *len);
NTSTATUS marshall_sec_desc_buf(TALLOC_CTX *mem_ctx,
			       struct sec_desc_buf *secdesc_buf,
			       uint8_t **data, size_t *len);
NTSTATUS unmarshall_sec_desc(TALLOC_CTX *mem_ctx, uint8 *data, size_t len,
			     struct security_descriptor **psecdesc);
NTSTATUS unmarshall_sec_desc_buf(TALLOC_CTX *mem_ctx, uint8_t *data, size_t len,
				 struct sec_desc_buf **psecdesc_buf);
struct security_descriptor *make_standard_sec_desc(TALLOC_CTX *ctx, const struct dom_sid *owner_sid, const struct dom_sid *grp_sid,
				 struct security_acl *dacl, size_t *sd_size);
struct sec_desc_buf *make_sec_desc_buf(TALLOC_CTX *ctx, size_t len, struct security_descriptor *sec_desc);
struct sec_desc_buf *dup_sec_desc_buf(TALLOC_CTX *ctx, struct sec_desc_buf *src);
NTSTATUS sec_desc_add_sid(TALLOC_CTX *ctx, struct security_descriptor **psd, const struct dom_sid *sid, uint32 mask, size_t *sd_size);
NTSTATUS sec_desc_mod_sid(struct security_descriptor *sd, struct dom_sid *sid, uint32 mask);
NTSTATUS sec_desc_del_sid(TALLOC_CTX *ctx, struct security_descriptor **psd, struct dom_sid *sid, size_t *sd_size);
bool sd_has_inheritable_components(const struct security_descriptor *parent_ctr, bool container);
NTSTATUS se_create_child_secdesc(TALLOC_CTX *ctx,
                                        struct security_descriptor **ppsd,
					size_t *psize,
                                        const struct security_descriptor *parent_ctr,
                                        const struct dom_sid *owner_sid,
                                        const struct dom_sid *group_sid,
                                        bool container);
NTSTATUS se_create_child_secdesc_buf(TALLOC_CTX *ctx,
					struct sec_desc_buf **ppsdb,
					const struct security_descriptor *parent_ctr,
					bool container);

/* The following definitions come from lib/sendfile.c  */

ssize_t sys_sendfile(int tofd, int fromfd, const DATA_BLOB *header, SMB_OFF_T offset, size_t count);

/* The following definitions come from lib/server_mutex.c  */

struct named_mutex *grab_named_mutex(TALLOC_CTX *mem_ctx, const char *name,
				     int timeout);

/* The following definitions come from lib/sharesec.c  */

bool share_info_db_init(void);
struct security_descriptor *get_share_security_default( TALLOC_CTX *ctx, size_t *psize, uint32 def_access);
struct security_descriptor *get_share_security( TALLOC_CTX *ctx, const char *servicename,
			      size_t *psize);
bool set_share_security(const char *share_name, struct security_descriptor *psd);
bool delete_share_security(const char *servicename);
bool share_access_check(const struct security_token *token,
			const char *sharename,
			uint32 desired_access,
			uint32_t *pgranted);
bool parse_usershare_acl(TALLOC_CTX *ctx, const char *acl_str, struct security_descriptor **ppsd);

/* The following definitions come from lib/smbrun.c  */

int smbrun_no_sanitize(const char *cmd, int *outfd);
int smbrun(const char *cmd, int *outfd);
int smbrunsecret(const char *cmd, const char *secret);

/* The following definitions come from lib/sock_exec.c  */

int sock_exec(const char *prog);

/* The following definitions come from lib/substitute.c  */

void free_local_machine_name(void);
bool set_local_machine_name(const char *local_name, bool perm);
const char *get_local_machine_name(void);
bool set_remote_machine_name(const char *remote_name, bool perm);
const char *get_remote_machine_name(void);
void sub_set_smb_name(const char *name);
void set_current_user_info(const char *smb_name, const char *unix_name,
			   const char *domain);
void sub_set_socket_ids(const char *peeraddr, const char *peername,
			const char *sockaddr);
const char *get_current_username(void);
void standard_sub_basic(const char *smb_name, const char *domain_name,
			char *str, size_t len);
char *talloc_sub_basic(TALLOC_CTX *mem_ctx, const char *smb_name,
		       const char *domain_name, const char *str);
char *talloc_sub_specified(TALLOC_CTX *mem_ctx,
			const char *input_string,
			const char *username,
			const char *domain,
			uid_t uid,
			gid_t gid);
char *talloc_sub_advanced(TALLOC_CTX *mem_ctx,
			  const char *servicename, const char *user,
			  const char *connectpath, gid_t gid,
			  const char *smb_name, const char *domain_name,
			  const char *str);
void standard_sub_advanced(const char *servicename, const char *user,
			   const char *connectpath, gid_t gid,
			   const char *smb_name, const char *domain_name,
			   char *str, size_t len);
char *standard_sub_conn(TALLOC_CTX *ctx, connection_struct *conn, const char *str);

/* The following definitions come from lib/sysacls.c  */

int sys_acl_get_entry(SMB_ACL_T acl_d, int entry_id, SMB_ACL_ENTRY_T *entry_p);
int sys_acl_get_tag_type(SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T *type_p);
int sys_acl_get_permset(SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T *permset_p);
void *sys_acl_get_qualifier(SMB_ACL_ENTRY_T entry_d);
int sys_acl_clear_perms(SMB_ACL_PERMSET_T permset_d);
int sys_acl_add_perm(SMB_ACL_PERMSET_T permset_d, SMB_ACL_PERM_T perm);
int sys_acl_get_perm(SMB_ACL_PERMSET_T permset_d, SMB_ACL_PERM_T perm);
char *sys_acl_to_text(SMB_ACL_T acl_d, ssize_t *len_p);
SMB_ACL_T sys_acl_init(int count);
int sys_acl_create_entry(SMB_ACL_T *acl_p, SMB_ACL_ENTRY_T *entry_p);
int sys_acl_set_tag_type(SMB_ACL_ENTRY_T entry_d, SMB_ACL_TAG_T tag_type);
int sys_acl_set_qualifier(SMB_ACL_ENTRY_T entry_d, void *qual_p);
int sys_acl_set_permset(SMB_ACL_ENTRY_T entry_d, SMB_ACL_PERMSET_T permset_d);
int sys_acl_free_text(char *text);
int sys_acl_free_acl(SMB_ACL_T acl_d) ;
int sys_acl_free_qualifier(void *qual, SMB_ACL_TAG_T tagtype);
int sys_acl_valid(SMB_ACL_T acl_d);
SMB_ACL_T sys_acl_get_file(vfs_handle_struct *handle, 
			   const char *path_p, SMB_ACL_TYPE_T type);
SMB_ACL_T sys_acl_get_fd(vfs_handle_struct *handle, files_struct *fsp);
int sys_acl_set_file(vfs_handle_struct *handle,
		     const char *name, SMB_ACL_TYPE_T type, SMB_ACL_T acl_d);
int sys_acl_set_fd(vfs_handle_struct *handle, files_struct *fsp,
		   SMB_ACL_T acl_d);
int sys_acl_delete_def_file(vfs_handle_struct *handle,
			    const char *path);
int no_acl_syscall_error(int err);

/* The following definitions come from lib/sysquotas.c  */

int sys_get_quota(const char *path, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dp);
int sys_set_quota(const char *path, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dp);

/* The following definitions come from lib/sysquotas_*.c  */

int sys_get_vfs_quota(const char *path, const char *bdev, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dp);
int sys_set_vfs_quota(const char *path, const char *bdev, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dp);

int sys_get_xfs_quota(const char *path, const char *bdev, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dp);
int sys_set_xfs_quota(const char *path, const char *bdev, enum SMB_QUOTA_TYPE qtype, unid_t id, SMB_DISK_QUOTA *dp);

int sys_get_nfs_quota(const char *path, const char *bdev,
		      enum SMB_QUOTA_TYPE qtype,
		      unid_t id, SMB_DISK_QUOTA *dp);
int sys_set_nfs_quota(const char *path, const char *bdev,
		      enum SMB_QUOTA_TYPE qtype,
		      unid_t id, SMB_DISK_QUOTA *dp);

/* The following definitions come from lib/system.c  */

void *sys_memalign( size_t align, size_t size );
int sys_usleep(long usecs);
ssize_t sys_read(int fd, void *buf, size_t count);
ssize_t sys_write(int fd, const void *buf, size_t count);
ssize_t sys_writev(int fd, const struct iovec *iov, int iovcnt);
ssize_t sys_pread(int fd, void *buf, size_t count, SMB_OFF_T off);
ssize_t sys_pwrite(int fd, const void *buf, size_t count, SMB_OFF_T off);
ssize_t sys_send(int s, const void *msg, size_t len, int flags);
ssize_t sys_sendto(int s,  const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t sys_recv(int fd, void *buf, size_t count, int flags);
ssize_t sys_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
int sys_fcntl_ptr(int fd, int cmd, void *arg);
int sys_fcntl_long(int fd, int cmd, long arg);
void update_stat_ex_mtime(struct stat_ex *dst, struct timespec write_ts);
void update_stat_ex_create_time(struct stat_ex *dst, struct timespec create_time);
int sys_stat(const char *fname, SMB_STRUCT_STAT *sbuf,
	     bool fake_dir_create_times);
int sys_fstat(int fd, SMB_STRUCT_STAT *sbuf,
	      bool fake_dir_create_times);
int sys_lstat(const char *fname,SMB_STRUCT_STAT *sbuf,
	      bool fake_dir_create_times);
int sys_ftruncate(int fd, SMB_OFF_T offset);
int sys_posix_fallocate(int fd, SMB_OFF_T offset, SMB_OFF_T len);
int sys_fallocate(int fd, enum vfs_fallocate_mode mode, SMB_OFF_T offset, SMB_OFF_T len);
SMB_OFF_T sys_lseek(int fd, SMB_OFF_T offset, int whence);
int sys_fseek(FILE *fp, SMB_OFF_T offset, int whence);
SMB_OFF_T sys_ftell(FILE *fp);
int sys_creat(const char *path, mode_t mode);
int sys_open(const char *path, int oflag, mode_t mode);
FILE *sys_fopen(const char *path, const char *type);
void kernel_flock(int fd, uint32 share_mode, uint32 access_mask);
SMB_STRUCT_DIR *sys_opendir(const char *name);
SMB_STRUCT_DIR *sys_fdopendir(int fd);
SMB_STRUCT_DIRENT *sys_readdir(SMB_STRUCT_DIR *dirp);
void sys_seekdir(SMB_STRUCT_DIR *dirp, long offset);
long sys_telldir(SMB_STRUCT_DIR *dirp);
void sys_rewinddir(SMB_STRUCT_DIR *dirp);
int sys_closedir(SMB_STRUCT_DIR *dirp);
int sys_mknod(const char *path, mode_t mode, SMB_DEV_T dev);
int sys_waitpid(pid_t pid,int *status,int options);
char *sys_getwd(char *s);
void set_effective_capability(enum smbd_capability capability);
void drop_effective_capability(enum smbd_capability capability);
long sys_random(void);
void sys_srandom(unsigned int seed);
int groups_max(void);
int sys_getgroups(int setlen, gid_t *gidset);
int sys_setgroups(gid_t UNUSED(primary_gid), int setlen, gid_t *gidset);
int sys_popen(const char *command);
int sys_pclose(int fd);
ssize_t sys_getxattr (const char *path, const char *name, void *value, size_t size);
ssize_t sys_lgetxattr (const char *path, const char *name, void *value, size_t size);
ssize_t sys_fgetxattr (int filedes, const char *name, void *value, size_t size);
ssize_t sys_listxattr (const char *path, char *list, size_t size);
ssize_t sys_llistxattr (const char *path, char *list, size_t size);
ssize_t sys_flistxattr (int filedes, char *list, size_t size);
int sys_removexattr (const char *path, const char *name);
int sys_lremovexattr (const char *path, const char *name);
int sys_fremovexattr (int filedes, const char *name);
int sys_setxattr (const char *path, const char *name, const void *value, size_t size, int flags);
int sys_lsetxattr (const char *path, const char *name, const void *value, size_t size, int flags);
int sys_fsetxattr (int filedes, const char *name, const void *value, size_t size, int flags);
uint32 unix_dev_major(SMB_DEV_T dev);
uint32 unix_dev_minor(SMB_DEV_T dev);
int sys_aio_read(SMB_STRUCT_AIOCB *aiocb);
int sys_aio_write(SMB_STRUCT_AIOCB *aiocb);
ssize_t sys_aio_return(SMB_STRUCT_AIOCB *aiocb);
int sys_aio_cancel(int fd, SMB_STRUCT_AIOCB *aiocb);
int sys_aio_error(const SMB_STRUCT_AIOCB *aiocb);
int sys_aio_fsync(int op, SMB_STRUCT_AIOCB *aiocb);
int sys_aio_suspend(const SMB_STRUCT_AIOCB * const cblist[], int n, const struct timespec *timeout);
int sys_aio_read(SMB_STRUCT_AIOCB *aiocb);
int sys_aio_write(SMB_STRUCT_AIOCB *aiocb);
ssize_t sys_aio_return(SMB_STRUCT_AIOCB *aiocb);
int sys_aio_cancel(int fd, SMB_STRUCT_AIOCB *aiocb);
int sys_aio_error(const SMB_STRUCT_AIOCB *aiocb);
int sys_aio_fsync(int op, SMB_STRUCT_AIOCB *aiocb);
int sys_aio_suspend(const SMB_STRUCT_AIOCB * const cblist[], int n, const struct timespec *timeout);
/* The following definitions come from lib/system_smbd.c  */

bool getgroups_unix_user(TALLOC_CTX *mem_ctx, const char *user,
			 gid_t primary_gid,
			 gid_t **ret_groups, uint32_t *p_ngroups);

/* The following definitions come from lib/tallocmsg.c  */

void register_msg_pool_usage(struct messaging_context *msg_ctx);

/* The following definitions come from lib/time.c  */

void push_dos_date(uint8_t *buf, int offset, time_t unixdate, int zone_offset);
void push_dos_date2(uint8_t *buf,int offset,time_t unixdate, int zone_offset);
void push_dos_date3(uint8_t *buf,int offset,time_t unixdate, int zone_offset);
uint32_t convert_time_t_to_uint32_t(time_t t);
time_t convert_uint32_t_to_time_t(uint32_t u);
bool nt_time_is_zero(const NTTIME *nt);
time_t generalized_to_unix_time(const char *str);
int get_server_zone_offset(void);
int set_server_zone_offset(time_t t);
char *timeval_string(TALLOC_CTX *ctx, const struct timeval *tp, bool hires);
char *current_timestring(TALLOC_CTX *ctx, bool hires);
void srv_put_dos_date(char *buf,int offset,time_t unixdate);
void srv_put_dos_date2(char *buf,int offset, time_t unixdate);
void srv_put_dos_date3(char *buf,int offset,time_t unixdate);
void round_timespec(enum timestamp_set_resolution res, struct timespec *ts);
void put_long_date_timespec(enum timestamp_set_resolution res, char *p, struct timespec ts);
void put_long_date(char *p, time_t t);
void dos_filetime_timespec(struct timespec *tsp);
time_t make_unix_date(const void *date_ptr, int zone_offset);
time_t make_unix_date2(const void *date_ptr, int zone_offset);
time_t make_unix_date3(const void *date_ptr, int zone_offset);
time_t srv_make_unix_date(const void *date_ptr);
time_t srv_make_unix_date2(const void *date_ptr);
time_t srv_make_unix_date3(const void *date_ptr);
struct timespec convert_time_t_to_timespec(time_t t);
struct timespec convert_timeval_to_timespec(const struct timeval tv);
struct timeval convert_timespec_to_timeval(const struct timespec ts);
struct timespec timespec_current(void);
struct timespec timespec_min(const struct timespec *ts1,
			   const struct timespec *ts2);
int timespec_compare(const struct timespec *ts1, const struct timespec *ts2);
void round_timespec_to_sec(struct timespec *ts);
void round_timespec_to_usec(struct timespec *ts);
struct timespec interpret_long_date(const char *p);
void TimeInit(void);
void get_process_uptime(struct timeval *ret_time);
void get_startup_time(struct timeval *ret_time);
time_t nt_time_to_unix_abs(const NTTIME *nt);
time_t uint64s_nt_time_to_unix_abs(const uint64_t *src);
void unix_timespec_to_nt_time(NTTIME *nt, struct timespec ts);
void unix_to_nt_time_abs(NTTIME *nt, time_t t);
const char *time_to_asc(const time_t t);
const char *display_time(NTTIME nttime);
bool nt_time_is_set(const NTTIME *nt);

/* The following definitions come from lib/username.c  */

void flush_pwnam_cache(void);
char *get_user_home_dir(TALLOC_CTX *mem_ctx, const char *user);
struct passwd *Get_Pwnam_alloc(TALLOC_CTX *mem_ctx, const char *user);

/* The following definitions come from lib/util_names.c  */
void gfree_netbios_names(void);
bool set_global_myname(const char *myname);
const char *global_myname(void);
bool set_global_myworkgroup(const char *myworkgroup);
const char *lp_workgroup(void);
const char *get_global_sam_name(void);

/* The following definitions come from lib/util.c  */

enum protocol_types get_Protocol(void);
void set_Protocol(enum protocol_types  p);
bool all_zero(const uint8_t *ptr, size_t size);
bool set_global_scope(const char *scope);
const char *global_scope(void);
void gfree_names(void);
void gfree_all( void );
const char *my_netbios_names(int i);
bool set_netbios_aliases(const char **str_array);
bool init_names(void);
bool file_exist_stat(const char *fname,SMB_STRUCT_STAT *sbuf,
		     bool fake_dir_create_times);
bool socket_exist(const char *fname);
uint64_t get_file_size_stat(const SMB_STRUCT_STAT *sbuf);
SMB_OFF_T get_file_size(char *file_name);
char *attrib_string(uint16 mode);
void show_msg(char *buf);
void smb_set_enclen(char *buf,int len,uint16 enc_ctx_num);
void smb_setlen(char *buf,int len);
int set_message_bcc(char *buf,int num_bytes);
ssize_t message_push_blob(uint8 **outbuf, DATA_BLOB blob);
char *unix_clean_name(TALLOC_CTX *ctx, const char *s);
char *clean_name(TALLOC_CTX *ctx, const char *s);
ssize_t write_data_at_offset(int fd, const char *buffer, size_t N, SMB_OFF_T pos);
int set_blocking(int fd, bool set);
NTSTATUS reinit_after_fork(struct messaging_context *msg_ctx,
			   struct event_context *ev_ctx,
			   struct server_id id,
			   bool parent_longlived);
void *malloc_(size_t size);
void *memalign_array(size_t el_size, size_t align, unsigned int count);
void *calloc_array(size_t size, size_t nmemb);
void *Realloc(void *p, size_t size, bool free_old_on_error);
void add_to_large_array(TALLOC_CTX *mem_ctx, size_t element_size,
			void *element, void *_array, uint32 *num_elements,
			ssize_t *array_size);
char *get_myname(TALLOC_CTX *ctx);
char *get_mydnsdomname(TALLOC_CTX *ctx);
int interpret_protocol(const char *str,int def);
char *automount_lookup(TALLOC_CTX *ctx, const char *user_name);
char *automount_lookup(TALLOC_CTX *ctx, const char *user_name);
bool process_exists(const struct server_id pid);
const char *uidtoname(uid_t uid);
char *gidtoname(gid_t gid);
uid_t nametouid(const char *name);
gid_t nametogid(const char *name);
void smb_panic(const char *const why);
void log_stack_trace(void);
const char *readdirname(SMB_STRUCT_DIR *p);
bool is_in_path(const char *name, name_compare_entry *namelist, bool case_sensitive);
void set_namearray(name_compare_entry **ppname_array, const char *namelist);
void free_namearray(name_compare_entry *name_array);
bool fcntl_lock(int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type);
bool fcntl_getlock(int fd, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid);
bool is_myname(const char *s);
bool is_myworkgroup(const char *s);
void ra_lanman_string( const char *native_lanman );
const char *get_remote_arch_str(void);
void set_remote_arch(enum remote_arch_types type);
enum remote_arch_types get_remote_arch(void);
const char *tab_depth(int level, int depth);
int str_checksum(const char *s);
void zero_free(void *p, size_t size);
int set_maxfiles(int requested_max);
int smb_mkstemp(char *name_template);
void *smb_xmalloc_array(size_t size, unsigned int count);
char *myhostname(void);
char *lock_path(const char *name);
char *pid_path(const char *name);
char *lib_path(const char *name);
char *modules_path(const char *name);
char *data_path(const char *name);
char *state_path(const char *name);
char *cache_path(const char *name);
const char *shlib_ext(void);
bool parent_dirname(TALLOC_CTX *mem_ctx, const char *dir, char **parent,
		    const char **name);
bool ms_has_wild(const char *s);
bool ms_has_wild_w(const smb_ucs2_t *s);
bool mask_match(const char *string, const char *pattern, bool is_case_sensitive);
bool mask_match_search(const char *string, const char *pattern, bool is_case_sensitive);
bool mask_match_list(const char *string, char **list, int listLen, bool is_case_sensitive);
bool unix_wild_match(const char *pattern, const char *string);
bool name_to_fqdn(fstring fqdn, const char *name);
void *talloc_append_blob(TALLOC_CTX *mem_ctx, void *buf, DATA_BLOB blob);
uint32 map_share_mode_to_deny_mode(uint32 share_access, uint32 private_options);
pid_t procid_to_pid(const struct server_id *proc);
void set_my_vnn(uint32 vnn);
uint32 get_my_vnn(void);
void set_my_unique_id(uint64_t unique_id);
struct server_id pid_to_procid(pid_t pid);
struct server_id procid_self(void);
bool procid_equal(const struct server_id *p1, const struct server_id *p2);
bool cluster_id_equal(const struct server_id *id1,
		      const struct server_id *id2);
bool procid_is_me(const struct server_id *pid);
struct server_id interpret_pid(const char *pid_string);
char *procid_str(TALLOC_CTX *mem_ctx, const struct server_id *pid);
char *procid_str_static(const struct server_id *pid);
bool procid_valid(const struct server_id *pid);
bool procid_is_local(const struct server_id *pid);
bool trans_oob(uint32_t bufsize, uint32_t offset, uint32_t length);
bool is_offset_safe(const char *buf_base, size_t buf_len, char *ptr, size_t off);
char *get_safe_ptr(const char *buf_base, size_t buf_len, char *ptr, size_t off);
char *get_safe_str_ptr(const char *buf_base, size_t buf_len, char *ptr, size_t off);
int get_safe_SVAL(const char *buf_base, size_t buf_len, char *ptr, size_t off, int failval);
int get_safe_IVAL(const char *buf_base, size_t buf_len, char *ptr, size_t off, int failval);
void split_domain_user(TALLOC_CTX *mem_ctx,
		       const char *full_name,
		       char **domain,
		       char **user);
void *_talloc_zero_zeronull(const void *ctx, size_t size, const char *name);
void *_talloc_memdup_zeronull(const void *t, const void *p, size_t size, const char *name);
void *_talloc_array_zeronull(const void *ctx, size_t el_size, unsigned count, const char *name);
void *_talloc_zero_array_zeronull(const void *ctx, size_t el_size, unsigned count, const char *name);
void *talloc_zeronull(const void *context, size_t size, const char *name);
const char *strip_hostname(const char *s);
bool tevent_req_poll_ntstatus(struct tevent_req *req,
			      struct tevent_context *ev,
			      NTSTATUS *status);
bool any_nt_status_not_ok(NTSTATUS err1, NTSTATUS err2, NTSTATUS *result);
int timeval_to_msec(struct timeval t);
char *valid_share_pathname(TALLOC_CTX *ctx, const char *dos_pathname);
bool is_executable(const char *fname);
bool map_open_params_to_ntcreate(const char *smb_base_fname,
				 int deny_mode, int open_func,
				 uint32 *paccess_mask,
				 uint32 *pshare_mode,
				 uint32 *pcreate_disposition,
				 uint32 *pcreate_options,
				 uint32_t *pprivate_flags);

/* The following definitions come from lib/util_cmdline.c  */

struct user_auth_info *user_auth_info_init(TALLOC_CTX *mem_ctx);
const char *get_cmdline_auth_info_username(const struct user_auth_info *auth_info);
void set_cmdline_auth_info_username(struct user_auth_info *auth_info,
				    const char *username);
const char *get_cmdline_auth_info_domain(const struct user_auth_info *auth_info);
void set_cmdline_auth_info_domain(struct user_auth_info *auth_info,
				  const char *domain);
void set_cmdline_auth_info_password(struct user_auth_info *auth_info,
				    const char *password);
const char *get_cmdline_auth_info_password(const struct user_auth_info *auth_info);
bool set_cmdline_auth_info_signing_state(struct user_auth_info *auth_info,
					 const char *arg);
int get_cmdline_auth_info_signing_state(const struct user_auth_info *auth_info);
void set_cmdline_auth_info_use_ccache(struct user_auth_info *auth_info,
				      bool b);
bool get_cmdline_auth_info_use_ccache(const struct user_auth_info *auth_info);
void set_cmdline_auth_info_use_kerberos(struct user_auth_info *auth_info,
					bool b);
bool get_cmdline_auth_info_use_kerberos(const struct user_auth_info *auth_info);
void set_cmdline_auth_info_fallback_after_kerberos(struct user_auth_info *auth_info,
					bool b);
bool get_cmdline_auth_info_fallback_after_kerberos(const struct user_auth_info *auth_info);
void set_cmdline_auth_info_use_krb5_ticket(struct user_auth_info *auth_info);
void set_cmdline_auth_info_smb_encrypt(struct user_auth_info *auth_info);
void set_cmdline_auth_info_use_machine_account(struct user_auth_info *auth_info);
bool get_cmdline_auth_info_got_pass(const struct user_auth_info *auth_info);
bool get_cmdline_auth_info_smb_encrypt(const struct user_auth_info *auth_info);
bool get_cmdline_auth_info_use_machine_account(const struct user_auth_info *auth_info);
struct user_auth_info *get_cmdline_auth_info_copy(TALLOC_CTX *mem_ctx,
						 const struct user_auth_info *info);
bool set_cmdline_auth_info_machine_account_creds(struct user_auth_info *auth_info);
void set_cmdline_auth_info_getpass(struct user_auth_info *auth_info);

/* The following definitions come from lib/util_builtin.c  */

bool lookup_builtin_rid(TALLOC_CTX *mem_ctx, uint32 rid, const char **name);
bool lookup_builtin_name(const char *name, uint32 *rid);
const char *builtin_domain_name(void);
bool sid_check_is_builtin(const struct dom_sid *sid);
bool sid_check_is_in_builtin(const struct dom_sid *sid);

/* The following definitions come from lib/util_file.c  */

char **file_lines_pload(const char *syscmd, int *numlines);
void file_lines_free(char **lines);

/* The following definitions come from lib/util_nscd.c  */

void smb_nscd_flush_user_cache(void);
void smb_nscd_flush_group_cache(void);

/* The following definitions come from lib/util_nttoken.c  */

struct security_token *dup_nt_token(TALLOC_CTX *mem_ctx, const struct security_token *ptoken);
NTSTATUS merge_nt_token(TALLOC_CTX *mem_ctx,
			const struct security_token *token_1,
			const struct security_token *token_2,
			struct security_token **token_out);
bool token_sid_in_ace(const struct security_token *token, const struct security_ace *ace);

/* The following definitions come from lib/util_sec.c  */

void sec_init(void);
uid_t sec_initial_uid(void);
gid_t sec_initial_gid(void);
bool non_root_mode(void);
void gain_root_privilege(void);
void gain_root_group_privilege(void);
void set_effective_uid(uid_t uid);
void set_effective_gid(gid_t gid);
void save_re_uid(void);
void restore_re_uid_fromroot(void);
void restore_re_uid(void);
void save_re_gid(void);
void restore_re_gid(void);
int set_re_uid(void);
void become_user_permanently(uid_t uid, gid_t gid);
bool is_setuid_root(void) ;

/* The following definitions come from lib/util_sid.c  */

char *sid_to_fstring(fstring sidstr_out, const struct dom_sid *sid);
char *sid_string_talloc(TALLOC_CTX *mem_ctx, const struct dom_sid *sid);
char *sid_string_dbg(const struct dom_sid *sid);
char *sid_string_tos(const struct dom_sid *sid);
bool sid_linearize(char *outbuf, size_t len, const struct dom_sid *sid);
bool non_mappable_sid(struct dom_sid *sid);
char *sid_binstring_hex(const struct dom_sid *sid);
struct netr_SamInfo3;
NTSTATUS sid_array_from_info3(TALLOC_CTX *mem_ctx,
			      const struct netr_SamInfo3 *info3,
			      struct dom_sid **user_sids,
			      uint32_t *num_user_sids,
			      bool include_user_group_rid);

/* The following definitions come from lib/util_sock.c  */

bool is_broadcast_addr(const struct sockaddr *pss);
bool is_loopback_ip_v4(struct in_addr ip);
bool is_loopback_addr(const struct sockaddr *pss);
bool is_zero_addr(const struct sockaddr_storage *pss);
void zero_ip_v4(struct in_addr *ip);
void in_addr_to_sockaddr_storage(struct sockaddr_storage *ss,
		struct in_addr ip);
bool same_net(const struct sockaddr *ip1,
		const struct sockaddr *ip2,
		const struct sockaddr *mask);
bool sockaddr_equal(const struct sockaddr *ip1,
		const struct sockaddr *ip2);
bool is_address_any(const struct sockaddr *psa);
uint16_t get_sockaddr_port(const struct sockaddr_storage *pss);
char *print_sockaddr(char *dest,
			size_t destlen,
			const struct sockaddr_storage *psa);
char *print_canonical_sockaddr(TALLOC_CTX *ctx,
			const struct sockaddr_storage *pss);
const char *client_name(int fd);
int get_socket_port(int fd);
const char *client_addr(int fd, char *addr, size_t addrlen);
const char *client_socket_addr(int fd, char *addr, size_t addr_len);
int client_socket_port(int fd);
void set_smb_read_error(enum smb_read_errors *pre,
			enum smb_read_errors newerr);
void cond_set_smb_read_error(enum smb_read_errors *pre,
			enum smb_read_errors newerr);
bool is_a_socket(int fd);
void set_socket_options(int fd, const char *options);
ssize_t read_udp_v4_socket(int fd,
			char *buf,
			size_t len,
			struct sockaddr_storage *psa);
NTSTATUS read_fd_with_timeout(int fd, char *buf,
				  size_t mincnt, size_t maxcnt,
				  unsigned int time_out,
				  size_t *size_ret);
NTSTATUS read_data(int fd, char *buffer, size_t N);
ssize_t write_data(int fd, const char *buffer, size_t N);
ssize_t write_data_iov(int fd, const struct iovec *orig_iov, int iovcnt);
bool send_keepalive(int client);
NTSTATUS read_smb_length_return_keepalive(int fd, char *inbuf,
					  unsigned int timeout,
					  size_t *len);
NTSTATUS receive_smb_raw(int fd,
			char *buffer,
			size_t buflen,
			unsigned int timeout,
			size_t maxlen,
			size_t *p_len);
int open_socket_in(int type,
		uint16_t port,
		int dlevel,
		const struct sockaddr_storage *psock,
		bool rebind);
NTSTATUS open_socket_out(const struct sockaddr_storage *pss, uint16_t port,
			 int timeout, int *pfd);
struct tevent_req *open_socket_out_send(TALLOC_CTX *mem_ctx,
					struct event_context *ev,
					const struct sockaddr_storage *pss,
					uint16_t port,
					int timeout);
NTSTATUS open_socket_out_recv(struct tevent_req *req, int *pfd);
struct tevent_req *open_socket_out_defer_send(TALLOC_CTX *mem_ctx,
					      struct event_context *ev,
					      struct timeval wait_time,
					      const struct sockaddr_storage *pss,
					      uint16_t port,
					      int timeout);
NTSTATUS open_socket_out_defer_recv(struct tevent_req *req, int *pfd);
int open_udp_socket(const char *host, int port);
const char *get_peer_name(int fd, bool force_lookup);
const char *get_peer_addr(int fd, char *addr, size_t addr_len);
int create_pipe_sock(const char *socket_dir,
		     const char *socket_name,
		     mode_t dir_perms);
const char *get_mydnsfullname(void);
bool is_myname_or_ipaddr(const char *s);
struct tevent_req *getaddrinfo_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct fncall_context *ctx,
				    const char *node,
				    const char *service,
				    const struct addrinfo *hints);
int getaddrinfo_recv(struct tevent_req *req, struct addrinfo **res);
int poll_one_fd(int fd, int events, int timeout, int *revents);
int poll_intr_one_fd(int fd, int events, int timeout, int *revents);
struct tstream_context;
struct tevent_req *tstream_read_packet_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    struct tstream_context *stream,
					    size_t initial,
					    ssize_t (*more)(uint8_t *buf,
							    size_t buflen,
							    void *private_data),
					    void *private_data);
ssize_t tstream_read_packet_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				 uint8_t **pbuf, int *perrno);

/* The following definitions come from lib/util_str.c  */

bool next_token(const char **ptr, char *buff, const char *sep, size_t bufsize);
int StrCaseCmp(const char *s, const char *t);
int StrnCaseCmp(const char *s, const char *t, size_t len);
bool strnequal(const char *s1,const char *s2,size_t n);
bool strcsequal(const char *s1,const char *s2);
void strnorm(char *s, int case_default);
bool strisnormal(const char *s, int case_default);
char *push_skip_string(char *buf);
char *skip_string(const char *base, size_t len, char *buf);
size_t str_charnum(const char *s);
size_t str_ascii_charnum(const char *s);
bool trim_char(char *s,char cfront,char cback);
bool strhasupper(const char *s);
bool strhaslower(const char *s);
char *safe_strcpy_fn(const char *fn,
		int line,
		char *dest,
		const char *src,
		size_t maxlength);
char *safe_strcat_fn(const char *fn,
		int line,
		char *dest,
		const char *src,
		size_t maxlength);
char *alpha_strcpy_fn(const char *fn,
		int line,
		char *dest,
		const char *src,
		const char *other_safe_chars,
		size_t maxlength);
char *StrnCpy_fn(const char *fn, int line,char *dest,const char *src,size_t n);
bool in_list(const char *s, const char *list, bool casesensitive);
void string_free(char **s);
bool string_set(char **dest,const char *src);
void string_sub2(char *s,const char *pattern, const char *insert, size_t len,
		 bool remove_unsafe_characters, bool replace_once,
		 bool allow_trailing_dollar);
void string_sub_once(char *s, const char *pattern,
		const char *insert, size_t len);
void string_sub(char *s,const char *pattern, const char *insert, size_t len);
void fstring_sub(char *s,const char *pattern,const char *insert);
char *realloc_string_sub2(char *string,
			const char *pattern,
			const char *insert,
			bool remove_unsafe_characters,
			bool allow_trailing_dollar);
char *realloc_string_sub(char *string,
			const char *pattern,
			const char *insert);
char *talloc_string_sub2(TALLOC_CTX *mem_ctx, const char *src,
			const char *pattern,
			const char *insert,
			bool remove_unsafe_characters,
			bool replace_once,
			bool allow_trailing_dollar);
char *talloc_string_sub(TALLOC_CTX *mem_ctx,
			const char *src,
			const char *pattern,
			const char *insert);
void all_string_sub(char *s,const char *pattern,const char *insert, size_t len);
char *talloc_all_string_sub(TALLOC_CTX *ctx,
				const char *src,
				const char *pattern,
				const char *insert);
char *octal_string(int i);
char *string_truncate(char *s, unsigned int length);
char *strchr_m(const char *src, char c);
char *strrchr_m(const char *s, char c);
char *strnrchr_m(const char *s, char c, unsigned int n);
char *strstr_m(const char *src, const char *findstr);
void strlower_m(char *s);
void strupper_m(char *s);
size_t strlen_m(const char *s);
size_t strlen_m_term(const char *s);
size_t strlen_m_term_null(const char *s);
int fstr_sprintf(fstring s, const char *fmt, ...);
bool str_list_sub_basic( char **list, const char *smb_name,
			 const char *domain_name );
bool str_list_substitute(char **list, const char *pattern, const char *insert);

char *ipstr_list_make(char **ipstr_list,
			const struct ip_service *ip_list,
			int ip_count);
int ipstr_list_parse(const char *ipstr_list, struct ip_service **ip_list);
void ipstr_list_free(char* ipstr_list);
DATA_BLOB base64_decode_data_blob(const char *s);
void base64_decode_inplace(char *s);
char *base64_encode_data_blob(TALLOC_CTX *mem_ctx, DATA_BLOB data);
uint64_t STR_TO_SMB_BIG_UINT(const char *nptr, const char **entptr);
SMB_OFF_T conv_str_size(const char * str);
void string_append(char **left, const char *right);
bool add_string_to_array(TALLOC_CTX *mem_ctx,
			 const char *str, const char ***strings,
			 int *num);
void sprintf_append(TALLOC_CTX *mem_ctx, char **string, ssize_t *len,
		    size_t *bufsize, const char *fmt, ...);
int asprintf_strupper_m(char **strp, const char *fmt, ...);
char *talloc_asprintf_strupper_m(TALLOC_CTX *t, const char *fmt, ...);
char *talloc_asprintf_strlower_m(TALLOC_CTX *t, const char *fmt, ...);
char *sstring_sub(const char *src, char front, char back);
bool validate_net_name( const char *name,
		const char *invalid_chars,
		int max_len);
char *escape_shell_string(const char *src);
char **str_list_make_v3(TALLOC_CTX *mem_ctx, const char *string, const char *sep);
char *sanitize_username(TALLOC_CTX *mem_ctx, const char *username);

/* The following definitions come from lib/util_unistr.c  */

void gfree_case_tables(void);
void load_case_tables(void);
size_t dos_PutUniCode(char *dst,const char *src, size_t len, bool null_terminate);
int rpcstr_push(void *dest, const char *src, size_t dest_len, int flags);
int rpcstr_push_talloc(TALLOC_CTX *ctx, smb_ucs2_t **dest, const char *src);
bool isvalid83_w(smb_ucs2_t c);
size_t strlen_w(const smb_ucs2_t *src);
size_t strnlen_w(const smb_ucs2_t *src, size_t max);
smb_ucs2_t *strchr_w(const smb_ucs2_t *s, smb_ucs2_t c);
smb_ucs2_t *strchr_wa(const smb_ucs2_t *s, char c);
smb_ucs2_t *strrchr_w(const smb_ucs2_t *s, smb_ucs2_t c);
smb_ucs2_t *strnrchr_w(const smb_ucs2_t *s, smb_ucs2_t c, unsigned int n);
smb_ucs2_t *strstr_w(const smb_ucs2_t *s, const smb_ucs2_t *ins);
bool strlower_w(smb_ucs2_t *s);
bool strupper_w(smb_ucs2_t *s);
void strnorm_w(smb_ucs2_t *s, int case_default);
int strcmp_w(const smb_ucs2_t *a, const smb_ucs2_t *b);
int strncmp_w(const smb_ucs2_t *a, const smb_ucs2_t *b, size_t len);
int strcasecmp_w(const smb_ucs2_t *a, const smb_ucs2_t *b);
int strncasecmp_w(const smb_ucs2_t *a, const smb_ucs2_t *b, size_t len);
bool strequal_w(const smb_ucs2_t *s1, const smb_ucs2_t *s2);
bool strnequal_w(const smb_ucs2_t *s1,const smb_ucs2_t *s2,size_t n);
smb_ucs2_t *strdup_w(const smb_ucs2_t *src);
smb_ucs2_t *strndup_w(const smb_ucs2_t *src, size_t len);
smb_ucs2_t *strncpy_w(smb_ucs2_t *dest, const smb_ucs2_t *src, const size_t max);
smb_ucs2_t *strncat_w(smb_ucs2_t *dest, const smb_ucs2_t *src, const size_t max);
smb_ucs2_t *strcat_w(smb_ucs2_t *dest, const smb_ucs2_t *src);
void string_replace_w(smb_ucs2_t *s, smb_ucs2_t oldc, smb_ucs2_t newc);
bool trim_string_w(smb_ucs2_t *s, const smb_ucs2_t *front,
				  const smb_ucs2_t *back);
int strcmp_wa(const smb_ucs2_t *a, const char *b);
int strncmp_wa(const smb_ucs2_t *a, const char *b, size_t len);
smb_ucs2_t *strpbrk_wa(const smb_ucs2_t *s, const char *p);
smb_ucs2_t *strstr_wa(const smb_ucs2_t *s, const char *ins);
smb_ucs2_t toupper_w(smb_ucs2_t v);
bool isupper_w(smb_ucs2_t v);
smb_ucs2_t tolower_w(smb_ucs2_t v);
bool islower_w(smb_ucs2_t v);

/* The following definitions come from lib/version.c  */

const char *samba_version_string(void);

/* The following definitions come from lib/wins_srv.c  */

bool wins_srv_is_dead(struct in_addr wins_ip, struct in_addr src_ip);
void wins_srv_alive(struct in_addr wins_ip, struct in_addr src_ip);
void wins_srv_died(struct in_addr wins_ip, struct in_addr src_ip);
unsigned wins_srv_count(void);
char **wins_srv_tags(void);
void wins_srv_tags_free(char **list);
struct in_addr wins_srv_ip_tag(const char *tag, struct in_addr src_ip);
unsigned wins_srv_count_tag(const char *tag);

/* The following definitions come from libsmb/clispnego.c  */

DATA_BLOB spnego_gen_negTokenInit(TALLOC_CTX *ctx,
				  const char *OIDs[],
				  DATA_BLOB *psecblob,
				  const char *principal);

#ifndef ASN1_MAX_OIDS
#define ASN1_MAX_OIDS 20
#endif
bool spnego_parse_negTokenInit(TALLOC_CTX *ctx,
			       DATA_BLOB blob,
			       char *OIDs[ASN1_MAX_OIDS],
			       char **principal,
			       DATA_BLOB *secblob);
DATA_BLOB spnego_gen_krb5_wrap(TALLOC_CTX *ctx, const DATA_BLOB ticket, const uint8 tok_id[2]);
bool spnego_parse_krb5_wrap(TALLOC_CTX *ctx, DATA_BLOB blob, DATA_BLOB *ticket, uint8 tok_id[2]);
int spnego_gen_krb5_negTokenInit(TALLOC_CTX *ctx,
			    const char *principal, int time_offset,
			    DATA_BLOB *targ,
			    DATA_BLOB *session_key_krb5, uint32 extra_ap_opts,
			    time_t *expire_time);
bool spnego_parse_challenge(TALLOC_CTX *ctx, const DATA_BLOB blob,
			    DATA_BLOB *chal1, DATA_BLOB *chal2);
DATA_BLOB spnego_gen_auth(TALLOC_CTX *ctx, DATA_BLOB blob);
bool spnego_parse_auth(TALLOC_CTX *ctx, DATA_BLOB blob, DATA_BLOB *auth);
DATA_BLOB spnego_gen_auth_response(TALLOC_CTX *ctx, DATA_BLOB *reply, NTSTATUS nt_status,
				   const char *mechOID);
bool spnego_parse_auth_response(TALLOC_CTX *ctx,
				DATA_BLOB blob, NTSTATUS nt_status,
				const char *mechOID,
				DATA_BLOB *auth);

bool spnego_parse_auth_and_mic(TALLOC_CTX *ctx, DATA_BLOB blob,
				DATA_BLOB *auth, DATA_BLOB *signature);
DATA_BLOB spnego_gen_auth_response_and_mic(TALLOC_CTX *ctx,
					   NTSTATUS nt_status,
					   const char *mechOID,
					   DATA_BLOB *reply,
					   DATA_BLOB *mechlistMIC);
bool spnego_mech_list_blob(TALLOC_CTX *mem_ctx,
			   char **oid_list, DATA_BLOB *data);

/* The following definitions come from libsmb/conncache.c  */

NTSTATUS check_negative_conn_cache( const char *domain, const char *server);
void add_failed_connection_entry(const char *domain, const char *server, NTSTATUS result) ;
void flush_negative_conn_cache_for_domain(const char *domain);

/* The following definitions come from libsmb/dsgetdcname.c  */

struct netr_DsRGetDCNameInfo;

void debug_dsdcinfo_flags(int lvl, uint32_t flags);
NTSTATUS dsgetdcname(TALLOC_CTX *mem_ctx,
		     struct messaging_context *msg_ctx,
		     const char *domain_name,
		     const struct GUID *domain_guid,
		     const char *site_name,
		     uint32_t flags,
		     struct netr_DsRGetDCNameInfo **info);

/* The following definitions come from libsmb/errormap.c  */

NTSTATUS dos_to_ntstatus(uint8 eclass, uint32 ecode);
void ntstatus_to_dos(NTSTATUS ntstatus, uint8 *eclass, uint32 *ecode);
NTSTATUS werror_to_ntstatus(WERROR error);
WERROR ntstatus_to_werror(NTSTATUS error);
NTSTATUS map_nt_error_from_gss(uint32 gss_maj, uint32 minor);

/* The following definitions come from libsmb/namecache.c  */

bool namecache_store(const char *name,
			int name_type,
			int num_names,
			struct ip_service *ip_list);
bool namecache_fetch(const char *name,
			int name_type,
			struct ip_service **ip_list,
			int *num_names);
bool namecache_delete(const char *name, int name_type);
void namecache_flush(void);
bool namecache_status_store(const char *keyname, int keyname_type,
		int name_type, const struct sockaddr_storage *keyip,
		const char *srvname);
bool namecache_status_fetch(const char *keyname,
				int keyname_type,
				int name_type,
				const struct sockaddr_storage *keyip,
				char *srvname_out);

/* The following definitions come from libsmb/namequery.c  */

bool saf_store( const char *domain, const char *servername );
bool saf_join_store( const char *domain, const char *servername );
bool saf_delete( const char *domain );
char *saf_fetch( const char *domain );
struct tevent_req *node_status_query_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct nmb_name *name,
					  const struct sockaddr_storage *addr);
NTSTATUS node_status_query_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
				struct node_status **pnode_status,
				int *pnum_names,
				struct node_status_extra *extra);
NTSTATUS node_status_query(TALLOC_CTX *mem_ctx, struct nmb_name *name,
			   const struct sockaddr_storage *addr,
			   struct node_status **pnode_status,
			   int *pnum_names,
			   struct node_status_extra *extra);
bool name_status_find(const char *q_name,
			int q_type,
			int type,
			const struct sockaddr_storage *to_ss,
			fstring name);
int ip_service_compare(struct ip_service *ss1, struct ip_service *ss2);
struct tevent_req *name_query_send(TALLOC_CTX *mem_ctx,
				   struct tevent_context *ev,
				   const char *name, int name_type,
				   bool bcast, bool recurse,
				   const struct sockaddr_storage *addr);
NTSTATUS name_query_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			 struct sockaddr_storage **addrs, int *num_addrs,
			 uint8_t *flags);
NTSTATUS name_query(const char *name, int name_type,
		    bool bcast, bool recurse,
		    const struct sockaddr_storage *to_ss,
		    TALLOC_CTX *mem_ctx,
		    struct sockaddr_storage **addrs,
		    int *num_addrs, uint8_t *flags);
NTSTATUS name_resolve_bcast(const char *name,
			int name_type,
			TALLOC_CTX *mem_ctx,
			struct sockaddr_storage **return_iplist,
			int *return_count);
NTSTATUS resolve_wins(const char *name,
		int name_type,
		struct ip_service **return_iplist,
		int *return_count);
NTSTATUS internal_resolve_name(const char *name,
			        int name_type,
				const char *sitename,
				struct ip_service **return_iplist,
				int *return_count,
				const char *resolve_order);
bool resolve_name(const char *name,
		struct sockaddr_storage *return_ss,
		int name_type,
		bool prefer_ipv4);
NTSTATUS resolve_name_list(TALLOC_CTX *ctx,
		const char *name,
		int name_type,
		struct sockaddr_storage **return_ss_arr,
		unsigned int *p_num_entries);
bool find_master_ip(const char *group, struct sockaddr_storage *master_ss);
bool get_pdc_ip(const char *domain, struct sockaddr_storage *pss);
NTSTATUS get_sorted_dc_list( const char *domain,
			const char *sitename,
			struct ip_service **ip_list,
			int *count,
			bool ads_only );
NTSTATUS get_kdc_list( const char *realm,
			const char *sitename,
			struct ip_service **ip_list,
			int *count);

/* The following definitions come from libsmb/namequery_dc.c  */

bool get_dc_name(const char *domain,
		const char *realm,
		fstring srv_name,
		struct sockaddr_storage *ss_out);

/* The following definitions come from libsmb/nterr.c  */

const char *nt_errstr(NTSTATUS nt_code);
const char *get_friendly_nt_error_msg(NTSTATUS nt_code);
const char *get_nt_error_c_code(NTSTATUS nt_code);
NTSTATUS nt_status_string_to_code(const char *nt_status_str);
NTSTATUS nt_status_squash(NTSTATUS nt_status);

/* The following definitions come from libsmb/ntlmssp.c  */
struct ntlmssp_state;
NTSTATUS ntlmssp_set_username(struct ntlmssp_state *ntlmssp_state, const char *user) ;
NTSTATUS ntlmssp_set_hashes(struct ntlmssp_state *ntlmssp_state,
			    const uint8_t lm_hash[16],
			    const uint8_t nt_hash[16]) ;
NTSTATUS ntlmssp_set_password(struct ntlmssp_state *ntlmssp_state, const char *password) ;
NTSTATUS ntlmssp_set_domain(struct ntlmssp_state *ntlmssp_state, const char *domain) ;
void ntlmssp_want_feature_list(struct ntlmssp_state *ntlmssp_state, char *feature_list);
void ntlmssp_want_feature(struct ntlmssp_state *ntlmssp_state, uint32_t feature);
bool ntlmssp_have_feature(struct ntlmssp_state *ntlmssp_state, uint32_t feature);
NTSTATUS ntlmssp_update(struct ntlmssp_state *ntlmssp_state,
			const DATA_BLOB in, DATA_BLOB *out) ;
NTSTATUS ntlmssp_server_start(TALLOC_CTX *mem_ctx,
			      bool is_standalone,
			      const char *netbios_name,
			      const char *netbios_domain,
			      const char *dns_name,
			      const char *dns_domain,
			      struct ntlmssp_state **ntlmssp_state);
NTSTATUS ntlmssp_client_start(TALLOC_CTX *mem_ctx,
			      const char *netbios_name,
			      const char *netbios_domain,
			      bool use_ntlmv2,
			      struct ntlmssp_state **_ntlmssp_state);

/* The following definitions come from libsmb/passchange.c  */

NTSTATUS remote_password_change(const char *remote_machine, const char *user_name, 
				const char *old_passwd, const char *new_passwd,
				char **err_str);

/* The following definitions come from libsmb/samlogon_cache.c  */

bool netsamlogon_cache_init(void);
bool netsamlogon_cache_shutdown(void);
void netsamlogon_clear_cached_user(const struct dom_sid *user_sid);
bool netsamlogon_cache_store(const char *username, struct netr_SamInfo3 *info3);
struct netr_SamInfo3 *netsamlogon_cache_get(TALLOC_CTX *mem_ctx, const struct dom_sid *user_sid);
bool netsamlogon_cache_have(const struct dom_sid *user_sid);

/* The following definitions come from libsmb/smberr.c  */

const char *smb_dos_err_name(uint8 e_class, uint16 num);
const char *get_dos_error_msg(WERROR result);
const char *smb_dos_err_class(uint8 e_class);
char *smb_dos_errstr(char *inbuf);
WERROR map_werror_from_unix(int error);

/* The following definitions come from libsmb/trustdom_cache.c  */

bool trustdom_cache_enable(void);
bool trustdom_cache_shutdown(void);
bool trustdom_cache_store(char* name, char* alt_name, const struct dom_sid *sid,
                          time_t timeout);
bool trustdom_cache_fetch(const char* name, struct dom_sid* sid);
uint32 trustdom_cache_fetch_timestamp( void );
bool trustdom_cache_store_timestamp( uint32 t, time_t timeout );
void trustdom_cache_flush(void);
void update_trustdom_cache( void );

/* The following definitions come from libsmb/trusts_util.c  */

NTSTATUS trust_pw_change_and_store_it(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx, 
				      const char *domain,
				      const char *account_name,
				      unsigned char orig_trust_passwd_hash[16],
				      enum netr_SchannelType sec_channel_type);
NTSTATUS trust_pw_find_change_and_store_it(struct rpc_pipe_client *cli, 
					   TALLOC_CTX *mem_ctx, 
					   const char *domain) ;
bool enumerate_domain_trusts( TALLOC_CTX *mem_ctx, const char *domain,
                                     char ***domain_names, uint32 *num_domains,
				     struct dom_sid **sids );
NTSTATUS change_trust_account_password( const char *domain, const char *remote_machine);

/* The following definitions come from param/loadparm.c  */

char *lp_smb_ports(void);
const char *lp_dos_charset(void);
const char *lp_unix_charset(void);
const char *lp_display_charset(void);
char *lp_logfile(void);
char *lp_configfile(void);
char *lp_smb_passwd_file(void);
char *lp_private_dir(void);
char *lp_serverstring(void);
int lp_printcap_cache_time(void);
char *lp_addport_cmd(void);
char *lp_enumports_cmd(void);
char *lp_addprinter_cmd(void);
char *lp_deleteprinter_cmd(void);
char *lp_os2_driver_map(void);
char *lp_lockdir(void);
char *lp_statedir(void);
char *lp_cachedir(void);
char *lp_piddir(void);
char *lp_mangling_method(void);
int lp_mangle_prefix(void);
char *lp_utmpdir(void);
char *lp_wtmpdir(void);
bool lp_utmp(void);
char *lp_rootdir(void);
char *lp_defaultservice(void);
char *lp_msg_command(void);
char *lp_get_quota_command(void);
char *lp_set_quota_command(void);
char *lp_auto_services(void);
char *lp_passwd_program(void);
char *lp_passwd_chat(void);
char *lp_passwordserver(void);
char *lp_name_resolve_order(void);
char *lp_realm(void);
const char *lp_afs_username_map(void);
int lp_afs_token_lifetime(void);
char *lp_log_nt_token_command(void);
char *lp_username_map(void);
const char *lp_logon_script(void);
const char *lp_logon_path(void);
const char *lp_logon_drive(void);
const char *lp_logon_home(void);
char *lp_remote_announce(void);
char *lp_remote_browse_sync(void);
bool lp_nmbd_bind_explicit_broadcast(void);
const char **lp_wins_server_list(void);
const char **lp_interfaces(void);
const char *lp_socket_address(void);
char *lp_nis_home_map_name(void);
const char **lp_netbios_aliases(void);
const char *lp_passdb_backend(void);
const char **lp_preload_modules(void);
char *lp_panic_action(void);
char *lp_adduser_script(void);
char *lp_renameuser_script(void);
char *lp_deluser_script(void);
const char *lp_guestaccount(void);
char *lp_addgroup_script(void);
char *lp_delgroup_script(void);
char *lp_addusertogroup_script(void);
char *lp_deluserfromgroup_script(void);
char *lp_setprimarygroup_script(void);
char *lp_addmachine_script(void);
char *lp_shutdown_script(void);
char *lp_abort_shutdown_script(void);
char *lp_username_map_script(void);
int lp_username_map_cache_time(void);
char *lp_check_password_script(void);
char *lp_wins_hook(void);
const char *lp_template_homedir(void);
const char *lp_template_shell(void);
const char *lp_winbind_separator(void);
int lp_acl_compatibility(void);
bool lp_winbind_enum_users(void);
bool lp_winbind_enum_groups(void);
bool lp_winbind_use_default_domain(void);
bool lp_winbind_trusted_domains_only(void);
bool lp_winbind_nested_groups(void);
int lp_winbind_expand_groups(void);
bool lp_winbind_refresh_tickets(void);
bool lp_winbind_offline_logon(void);
bool lp_winbind_normalize_names(void);
bool lp_winbind_rpc_only(void);
bool lp_create_krb5_conf(void);
int lp_winbind_max_domain_connections(void);
const char *lp_idmap_backend(void);
int lp_idmap_cache_time(void);
int lp_idmap_negative_cache_time(void);
int lp_keepalive(void);
bool lp_passdb_expand_explicit(void);
char *lp_ldap_suffix(void);
char *lp_ldap_admin_dn(void);
int lp_ldap_ssl(void);
bool lp_ldap_ssl_ads(void);
int lp_ldap_deref(void);
int lp_ldap_follow_referral(void);
int lp_ldap_passwd_sync(void);
bool lp_ldap_delete_dn(void);
int lp_ldap_replication_sleep(void);
int lp_ldap_timeout(void);
int lp_ldap_connection_timeout(void);
int lp_ldap_page_size(void);
int lp_ldap_debug_level(void);
int lp_ldap_debug_threshold(void);
char *lp_add_share_cmd(void);
char *lp_change_share_cmd(void);
char *lp_delete_share_cmd(void);
char *lp_usershare_path(void);
const char **lp_usershare_prefix_allow_list(void);
const char **lp_usershare_prefix_deny_list(void);
const char **lp_eventlog_list(void);
bool lp_registry_shares(void);
bool lp_usershare_allow_guests(void);
bool lp_usershare_owner_only(void);
bool lp_disable_netbios(void);
bool lp_reset_on_zero_vc(void);
bool lp_log_writeable_files_on_exit(void);
bool lp_ms_add_printer_wizard(void);
bool lp_dns_proxy(void);
bool lp_wins_support(void);
bool lp_we_are_a_wins_server(void);
bool lp_wins_proxy(void);
bool lp_local_master(void);
bool lp_domain_logons(void);
const char **lp_init_logon_delayed_hosts(void);
int lp_init_logon_delay(void);
bool lp_load_printers(void);
bool lp_readraw(void);
bool _lp_readraw(void);
bool lp_large_readwrite(void);
bool lp_writeraw(void);
bool _lp_writeraw(void);
bool lp_null_passwords(void);
bool lp_obey_pam_restrictions(void);
bool lp_encrypted_passwords(void);
int lp_client_schannel(void);
int lp_server_schannel(void);
bool lp_syslog_only(void);
bool lp_timestamp_logs(void);
bool lp_debug_prefix_timestamp(void);
bool lp_debug_hires_timestamp(void);
bool lp_debug_pid(void);
bool lp_debug_uid(void);
bool lp_debug_class(void);
bool lp_enable_core_files(void);
bool lp_browse_list(void);
bool lp_nis_home_map(void);
bool lp_bind_interfaces_only(void);
bool lp_pam_password_change(void);
bool lp_unix_password_sync(void);
bool lp_passwd_chat_debug(void);
int lp_passwd_chat_timeout(void);
bool lp_nt_pipe_support(void);
bool lp_nt_status_support(void);
bool lp_stat_cache(void);
int lp_max_stat_cache_size(void);
bool lp_allow_trusted_domains(void);
bool lp_map_untrusted_to_domain(void);
int lp_restrict_anonymous(void);
bool lp_lanman_auth(void);
bool lp_ntlm_auth(void);
bool lp_raw_ntlmv2_auth(void);
bool lp_client_plaintext_auth(void);
bool lp_client_lanman_auth(void);
bool lp_client_ntlmv2_auth(void);
bool lp_host_msdfs(void);
bool lp_kernel_oplocks(void);
bool lp_enhanced_browsing(void);
bool lp_use_mmap(void);
bool lp_unix_extensions(void);
bool lp_use_spnego(void);
bool lp_client_use_spnego(void);
bool lp_client_use_spnego_principal(void);
bool lp_send_spnego_principal(void);
bool lp_hostname_lookups(void);
bool lp_change_notify(const struct share_params *p );
bool lp_kernel_change_notify(const struct share_params *p );
char * lp_dedicated_keytab_file(void);
int lp_kerberos_method(void);
bool lp_defer_sharing_violations(void);
bool lp_enable_privileges(void);
bool lp_enable_asu_support(void);
int lp_os_level(void);
int lp_max_ttl(void);
int lp_max_wins_ttl(void);
int lp_min_wins_ttl(void);
int lp_max_log_size(void);
int lp_max_open_files(void);
int lp_open_files_db_hash_size(void);
int lp_maxxmit(void);
int lp_maxmux(void);
int lp_passwordlevel(void);
int lp_usernamelevel(void);
int lp_deadtime(void);
bool lp_getwd_cache(void);
int lp_maxprotocol(void);
int lp_minprotocol(void);
int lp_security(void);
const char **lp_auth_methods(void);
bool lp_paranoid_server_security(void);
int lp_maxdisksize(void);
int lp_lpqcachetime(void);
int lp_max_smbd_processes(void);
bool _lp_disable_spoolss(void);
int lp_syslog(void);
int lp_lm_announce(void);
int lp_lm_interval(void);
int lp_machine_password_timeout(void);
int lp_map_to_guest(void);
int lp_oplock_break_wait_time(void);
int lp_lock_spin_time(void);
int lp_usershare_max_shares(void);
const char *lp_socket_options(void);
int lp_config_backend(void);
int lp_smb2_max_read(void);
int lp_smb2_max_write(void);
int lp_smb2_max_trans(void);
int lp_smb2_max_credits(void);
char *lp_preexec(int );
char *lp_postexec(int );
char *lp_rootpreexec(int );
char *lp_rootpostexec(int );
char *lp_servicename(int );
const char *lp_const_servicename(int );
char *lp_pathname(int );
char *lp_dontdescend(int );
char *lp_username(int );
const char **lp_invalid_users(int );
const char **lp_valid_users(int );
const char **lp_admin_users(int );
const char **lp_svcctl_list(void);
char *lp_cups_options(int );
char *lp_cups_server(void);
int lp_cups_encrypt(void);
char *lp_iprint_server(void);
int lp_cups_connection_timeout(void);
const char *lp_ctdbd_socket(void);
const char **lp_cluster_addresses(void);
bool lp_clustering(void);
int lp_ctdb_timeout(void);
int lp_ctdb_locktime_warn_threshold(void);
char *lp_printcommand(int );
char *lp_lpqcommand(int );
char *lp_lprmcommand(int );
char *lp_lppausecommand(int );
char *lp_lpresumecommand(int );
char *lp_queuepausecommand(int );
char *lp_queueresumecommand(int );
const char *lp_printjob_username(int );
const char **lp_hostsallow(int );
const char **lp_hostsdeny(int );
char *lp_magicscript(int );
char *lp_magicoutput(int );
char *lp_comment(int );
char *lp_force_user(int );
char *lp_force_group(int );
const char **lp_readlist(int );
const char **lp_writelist(int );
const char **lp_printer_admin(int );
char *lp_fstype(int );
const char **lp_vfs_objects(int );
char *lp_msdfs_proxy(int );
char *lp_veto_files(int );
char *lp_hide_files(int );
char *lp_veto_oplocks(int );
bool lp_msdfs_root(int );
char *lp_aio_write_behind(int );
char *lp_dfree_command(int );
bool lp_autoloaded(int );
bool lp_preexec_close(int );
bool lp_rootpreexec_close(int );
int lp_casesensitive(int );
bool lp_preservecase(int );
bool lp_shortpreservecase(int );
bool lp_hide_dot_files(int );
bool lp_hide_special_files(int );
bool lp_hideunreadable(int );
bool lp_hideunwriteable_files(int );
bool lp_browseable(int );
bool lp_access_based_share_enum(int );
bool lp_readonly(int );
bool lp_no_set_dir(int );
bool lp_guest_ok(int );
bool lp_guest_only(int );
bool lp_administrative_share(int );
bool lp_print_ok(int );
bool lp_print_notify_backchannel(int );
bool lp_map_hidden(int );
bool lp_map_archive(int );
bool lp_store_dos_attributes(int );
bool lp_dmapi_support(int );
bool lp_locking(const struct share_params *p );
int lp_strict_locking(const struct share_params *p );
bool lp_posix_locking(const struct share_params *p );
bool lp_share_modes(int );
bool lp_oplocks(int );
bool lp_level2_oplocks(int );
bool lp_onlyuser(int );
bool lp_manglednames(const struct share_params *p );
bool lp_allow_insecure_widelinks(void);
bool lp_widelinks(int );
bool lp_symlinks(int );
bool lp_syncalways(int );
bool lp_strict_allocate(int );
bool lp_strict_sync(int );
bool lp_map_system(int );
bool lp_delete_readonly(int );
bool lp_fake_oplocks(int );
bool lp_recursive_veto_delete(int );
bool lp_dos_filemode(int );
bool lp_dos_filetimes(int );
bool lp_dos_filetime_resolution(int );
bool lp_fake_dir_create_times(int);
bool lp_async_smb_echo_handler(void);
bool lp_multicast_dns_register(void);
bool lp_blocking_locks(int );
bool lp_inherit_perms(int );
bool lp_inherit_acls(int );
bool lp_inherit_owner(int );
bool lp_use_client_driver(int );
bool lp_default_devmode(int );
bool lp_force_printername(int );
bool lp_nt_acl_support(int );
bool lp_force_unknown_acl_user(int );
bool lp_ea_support(int );
bool _lp_use_sendfile(int );
bool lp_profile_acls(int );
bool lp_map_acl_inherit(int );
bool lp_afs_share(int );
bool lp_acl_check_permissions(int );
bool lp_acl_group_control(int );
bool lp_acl_map_full_control(int );
int lp_create_mask(int );
int lp_force_create_mode(int );
int lp_security_mask(int );
int lp_force_security_mode(int );
int lp_dir_mask(int );
int lp_force_dir_mode(int );
int lp_dir_security_mask(int );
int lp_force_dir_security_mode(int );
int lp_max_connections(int );
int lp_defaultcase(int );
int lp_minprintspace(int );
int lp_printing(int );
int lp_max_reported_jobs(int );
int lp_oplock_contention_limit(int );
int lp_csc_policy(int );
int lp_write_cache_size(int );
int lp_block_size(int );
int lp_dfree_cache_time(int );
int lp_allocation_roundup_size(int );
int lp_aio_read_size(int );
int lp_aio_write_size(int );
int lp_map_readonly(int );
int lp_directory_name_cache_size(int );
int lp_smb_encrypt(int );
char lp_magicchar(const struct share_params *p );
int lp_winbind_cache_time(void);
int lp_winbind_reconnect_delay(void);
int lp_winbind_max_clients(void);
const char **lp_winbind_nss_info(void);
bool lp_winbind_sealed_pipes(void);
int lp_algorithmic_rid_base(void);
int lp_name_cache_timeout(void);
int lp_client_signing(void);
int lp_client_ipc_signing(void);
int lp_server_signing(void);
int lp_client_ldap_sasl_wrapping(void);
char *lp_parm_talloc_string(int snum, const char *type, const char *option, const char *def);
const char *lp_parm_const_string(int snum, const char *type, const char *option, const char *def);
const char **lp_parm_string_list(int snum, const char *type, const char *option, const char **def);
int lp_parm_int(int snum, const char *type, const char *option, int def);
unsigned long lp_parm_ulong(int snum, const char *type, const char *option, unsigned long def);
bool lp_parm_bool(int snum, const char *type, const char *option, bool def);
int lp_parm_enum(int snum, const char *type, const char *option,
		 const struct enum_list *_enum, int def);
char *canonicalize_servicename(TALLOC_CTX *ctx, const char *src);
bool lp_add_home(const char *pszHomename, int iDefaultService,
		 const char *user, const char *pszHomedir);
int lp_add_service(const char *pszService, int iDefaultService);
bool lp_add_printer(const char *pszPrintername, int iDefaultService);
bool lp_parameter_is_valid(const char *pszParmName);
bool lp_parameter_is_global(const char *pszParmName);
bool lp_parameter_is_canonical(const char *parm_name);
bool lp_canonicalize_parameter(const char *parm_name, const char **canon_parm,
			       bool *inverse);
bool lp_canonicalize_parameter_with_value(const char *parm_name,
					  const char *val,
					  const char **canon_parm,
					  const char **canon_val);
void show_parameter_list(void);
bool lp_string_is_valid_boolean(const char *parm_value);
bool lp_invert_boolean(const char *str, const char **inverse_str);
bool lp_canonicalize_boolean(const char *str, const char**canon_str);
bool service_ok(int iService);
bool process_registry_service(const char *service_name);
bool process_registry_shares(void);
bool lp_config_backend_is_registry(void);
bool lp_config_backend_is_file(void);
bool lp_file_list_changed(void);
bool lp_idmap_uid(uid_t *low, uid_t *high);
bool lp_idmap_gid(gid_t *low, gid_t *high);
const char *lp_ldap_machine_suffix(void);
const char *lp_ldap_user_suffix(void);
const char *lp_ldap_group_suffix(void);
const char *lp_ldap_idmap_suffix(void);
void *lp_local_ptr_by_snum(int snum, void *ptr);
bool lp_do_parameter(int snum, const char *pszParmName, const char *pszParmValue);
bool lp_set_cmdline(const char *pszParmName, const char *pszParmValue);
bool lp_set_option(const char *option);
void init_locals(void);
bool lp_is_default(int snum, struct parm_struct *parm);
bool dump_a_parameter(int snum, char *parm_name, FILE * f, bool isGlobal);
struct parm_struct *lp_get_parameter(const char *param_name);
struct parm_struct *lp_next_parameter(int snum, int *i, int allparameters);
bool lp_snum_ok(int iService);
void lp_add_one_printer(const char *name, const char *comment,
			const char *location, void *pdata);
bool lp_loaded(void);
void lp_killunused(bool (*snumused) (int));
void lp_kill_all_services(void);
void lp_killservice(int iServiceIn);
const char* server_role_str(uint32 role);
enum usershare_err parse_usershare_file(TALLOC_CTX *ctx,
			SMB_STRUCT_STAT *psbuf,
			const char *servicename,
			int snum,
			char **lines,
			int numlines,
			char **pp_sharepath,
			char **pp_comment,
			char **pp_cp_share_name,
			struct security_descriptor **ppsd,
			bool *pallow_guest);
int load_usershare_service(const char *servicename);
int load_usershare_shares(void);
void gfree_loadparm(void);
void lp_set_in_client(bool b);
bool lp_is_in_client(void);
bool lp_load(const char *pszFname,
	     bool global_only,
	     bool save_defaults,
	     bool add_ipc,
	     bool initialize_globals);
bool lp_load_initial_only(const char *pszFname);
bool lp_load_with_registry_shares(const char *pszFname,
				  bool global_only,
				  bool save_defaults,
				  bool add_ipc,
				  bool initialize_globals);
int lp_numservices(void);
void lp_dump(FILE *f, bool show_defaults, int maxtoprint);
void lp_dump_one(FILE * f, bool show_defaults, int snum);
int lp_servicenumber(const char *pszServiceName);
bool share_defined(const char *service_name);
struct share_params *get_share_params(TALLOC_CTX *mem_ctx,
				      const char *sharename);
struct share_iterator *share_list_all(TALLOC_CTX *mem_ctx);
struct share_params *next_share(struct share_iterator *list);
struct share_params *next_printer(struct share_iterator *list);
struct share_params *snum2params_static(int snum);
const char *volume_label(int snum);
bool lp_domain_master(void);
bool lp_domain_master_true_or_auto(void);
bool lp_preferred_master(void);
void lp_remove_service(int snum);
void lp_copy_service(int snum, const char *new_name);
int lp_default_server_announce(void);
int lp_major_announce_version(void);
int lp_minor_announce_version(void);
void lp_set_name_resolve_order(const char *new_order);
const char *lp_printername(int snum);
void lp_set_logfile(const char *name);
int lp_maxprintjobs(int snum);
const char *lp_printcapname(void);
bool lp_disable_spoolss( void );
void lp_set_spoolss_state( uint32 state );
uint32 lp_get_spoolss_state( void );
struct smb_signing_state;
bool lp_use_sendfile(int snum, struct smb_signing_state *signing_state);
void set_use_sendfile(int snum, bool val);
void set_store_dos_attributes(int snum, bool val);
void lp_set_mangling_method(const char *new_method);
bool lp_posix_pathnames(void);
void lp_set_posix_pathnames(void);
enum brl_flavour lp_posix_cifsu_locktype(files_struct *fsp);
void lp_set_posix_default_cifsx_readwrite_locktype(enum brl_flavour val);
int lp_min_receive_file_size(void);
char* lp_perfcount_module(void);
void lp_set_passdb_backend(const char *backend);
void widelinks_warning(int snum);
char *lp_ncalrpc_dir(void);
bool lp_allow_dcerpc_auth_level_connect(void);

/* The following definitions come from param/loadparm_server_role.c  */

int lp_server_role(void);
void set_server_role(void);

/* The following definitions come from param/util.c  */

uint32 get_int_param( const char* param );
char* get_string_param( const char* param );

/* The following definitions come from lib/server_contexts.c  */
struct tevent_context *server_event_context(void);
void server_event_context_free(void);
struct messaging_context *server_messaging_context(void);
void server_messaging_context_free(void);

/* The following definitions come from lib/sessionid_tdb.c  */
struct sessionid;
bool sessionid_init(void);
bool sessionid_init_readonly(void);
struct db_record *sessionid_fetch_record(TALLOC_CTX *mem_ctx, const char *key);
int sessionid_traverse(int (*fn)(struct db_record *rec, const char *key,
				 struct sessionid *session,
				 void *private_data),
		       void *private_data);
int sessionid_traverse_read(int (*fn)(const char *key,
				      struct sessionid *session,
				      void *private_data),
			    void *private_data);

/* The following definitions come from utils/passwd_util.c  */

char *stdin_new_passwd( void);
char *get_pass( const char *prompt, bool stdin_get);

/* The following definitions come from lib/avahi.c */

struct AvahiPoll *tevent_avahi_poll(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev);

/* The following definitions come from lib/fncall.c */

struct fncall_context *fncall_context_init(TALLOC_CTX *mem_ctx,
					   int max_threads);
struct tevent_req *fncall_send(TALLOC_CTX *mem_ctx, struct tevent_context *ev,
			       struct fncall_context *ctx,
			       void (*fn)(void *private_data),
			       void *private_data);
int fncall_recv(struct tevent_req *req, int *perr);

/* The following definitions come from libsmb/smbsock_connect.c */

struct tevent_req *smbsock_connect_send(TALLOC_CTX *mem_ctx,
					struct tevent_context *ev,
					const struct sockaddr_storage *addr,
					uint16_t port,
					const char *called_name,
					int called_type,
					const char *calling_name,
					int calling_type);
NTSTATUS smbsock_connect_recv(struct tevent_req *req, int *sock,
			      uint16_t *ret_port);
NTSTATUS smbsock_connect(const struct sockaddr_storage *addr, uint16_t port,
			 const char *called_name, int called_type,
			 const char *calling_name, int calling_type,
			 int *pfd, uint16_t *ret_port, int sec_timeout);

struct tevent_req *smbsock_any_connect_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    const struct sockaddr_storage *addrs,
					    const char **called_names,
					    int *called_types,
					    const char **calling_names,
					    int *calling_types,
					    size_t num_addrs, uint16_t port);
NTSTATUS smbsock_any_connect_recv(struct tevent_req *req, int *pfd,
				  size_t *chosen_index, uint16_t *chosen_port);
NTSTATUS smbsock_any_connect(const struct sockaddr_storage *addrs,
			     const char **called_names,
			     int *called_types,
			     const char **calling_names,
			     int *calling_types,
			     size_t num_addrs,
			     uint16_t port,
			     int sec_timeout,
			     int *pfd, size_t *chosen_index,
			     uint16_t *chosen_port);

/* The following definitions come from lib/util_wellknown.c  */

bool sid_check_is_wellknown_domain(const struct dom_sid *sid, const char **name);
bool sid_check_is_in_wellknown_domain(const struct dom_sid *sid);
bool lookup_wellknown_sid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			  const char **domain, const char **name);
bool lookup_wellknown_name(TALLOC_CTX *mem_ctx, const char *name,
			   struct dom_sid *sid, const char **domain);

/* The following definitions come from lib/util_unixsids.c  */

bool sid_check_is_unix_users(const struct dom_sid *sid);
bool sid_check_is_in_unix_users(const struct dom_sid *sid);
void uid_to_unix_users_sid(uid_t uid, struct dom_sid *sid);
void gid_to_unix_groups_sid(gid_t gid, struct dom_sid *sid);
const char *unix_users_domain_name(void);
bool lookup_unix_user_name(const char *name, struct dom_sid *sid);
bool sid_check_is_unix_groups(const struct dom_sid *sid);
bool sid_check_is_in_unix_groups(const struct dom_sid *sid);
const char *unix_groups_domain_name(void);
bool lookup_unix_group_name(const char *name, struct dom_sid *sid);

/* The following definitions come from lib/filename_util.c */

NTSTATUS get_full_smb_filename(TALLOC_CTX *ctx, const struct smb_filename *smb_fname,
			      char **full_name);
NTSTATUS create_synthetic_smb_fname(TALLOC_CTX *ctx, const char *base_name,
				    const char *stream_name,
				    const SMB_STRUCT_STAT *psbuf,
				    struct smb_filename **smb_fname_out);
NTSTATUS create_synthetic_smb_fname_split(TALLOC_CTX *ctx,
					  const char *fname,
					  const SMB_STRUCT_STAT *psbuf,
					  struct smb_filename **smb_fname_out);
const char *smb_fname_str_dbg(const struct smb_filename *smb_fname);
const char *fsp_str_dbg(const struct files_struct *fsp);
NTSTATUS copy_smb_filename(TALLOC_CTX *ctx,
			   const struct smb_filename *smb_fname_in,
			   struct smb_filename **smb_fname_out);
bool is_ntfs_stream_smb_fname(const struct smb_filename *smb_fname);
bool is_ntfs_default_stream_smb_fname(const struct smb_filename *smb_fname);

/* The following definitions come from lib/dummyroot.c */

void become_root(void);
void unbecome_root(void);

/* The following definitions come from lib/dummysmbd.c */

int find_service(TALLOC_CTX *ctx, const char *service_in, char **p_service_out);
bool conn_snum_used(int snum);
void cancel_pending_lock_requests_by_fid(files_struct *fsp,
			struct byte_range_lock *br_lck,
			enum file_close_type close_type);
void send_stat_cache_delete_message(struct messaging_context *msg_ctx,
				    const char *name);
NTSTATUS can_delete_directory_fsp(files_struct *fsp);
bool change_to_root_user(void);
struct event_context *smbd_event_context(void);
void contend_level2_oplocks_begin(files_struct *fsp,
				  enum level2_contention_type type);
void contend_level2_oplocks_end(files_struct *fsp,
				enum level2_contention_type type);

#endif /*  _PROTO_H_  */
