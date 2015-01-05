#ifndef VSF_SYSSTR_H
#define VSF_SYSSTR_H

/* Forward declarations */
struct mystr;
struct vsf_sysutil_statbuf;
struct vsf_sysutil_dir;
struct vsf_sysutil_user;

void str_getcwd(struct mystr* p_str);
int str_readlink(struct mystr* p_str, const struct mystr* p_filename_str);
int str_write_loop(const struct mystr* p_str, const int fd);
int str_read_loop(struct mystr* p_str, const int fd);
int str_mkdir(const struct mystr* p_str, const unsigned int mode);
int str_rmdir(const struct mystr* p_str);
int str_unlink(const struct mystr* p_str);
int str_chdir(const struct mystr* p_str);
enum EVSFSysStrOpenMode
{
  kVSFSysStrOpenUnknown = 0,
  kVSFSysStrOpenReadOnly = 1
};
int str_open(const struct mystr* p_str, const enum EVSFSysStrOpenMode mode);
int str_create(const struct mystr* p_str);
int str_create_exclusive(const struct mystr* p_str);
int str_chmod(const struct mystr* p_str, unsigned int mode);
int str_stat(const struct mystr* p_str, struct vsf_sysutil_statbuf** p_ptr);
int str_lstat(const struct mystr* p_str, struct vsf_sysutil_statbuf** p_ptr);
int str_rename(const struct mystr* p_from_str, const struct mystr* p_to_str);
struct vsf_sysutil_dir* str_opendir(const struct mystr* p_str);
void str_next_dirent(const char* session_user,
                     const char* base_dir,
                     struct mystr* p_filename_str,
                     struct vsf_sysutil_dir* p_dir);

struct vsf_sysutil_user* str_getpwnam(const struct mystr* p_user_str);

void str_syslog(const struct mystr* p_str, int severe);

#endif /* VSF_SYSSTR_H */

