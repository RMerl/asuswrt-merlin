/*
 * debugfs.h --- header file for the debugfs program
 */

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"

#ifdef __STDC__
#define NOARGS void
#else
#define NOARGS
#define const
#endif

/*
 * Flags used by the common argument processing functions
 */
#define CHECK_FS_RW		0x0001
#define CHECK_FS_BITMAPS	0x0002
#define CHECK_FS_NOTOPEN	0x0004

extern ext2_filsys current_fs;
extern ext2_ino_t	root, cwd;

extern void reset_getopt(void);
extern FILE *open_pager(void);
extern void close_pager(FILE *stream);
extern int check_fs_open(char *name);
extern int check_fs_not_open(char *name);
extern int check_fs_read_write(char *name);
extern int check_fs_bitmaps(char *name);
extern ext2_ino_t string_to_inode(char *str);
extern char *time_to_string(__u32);
extern time_t string_to_time(const char *);
extern unsigned long parse_ulong(const char *str, const char *cmd,
				 const char *descr, int *err);
extern int strtoblk(const char *cmd, const char *str, blk_t *ret);
extern int common_args_process(int argc, char *argv[], int min_argc,
			       int max_argc, const char *cmd,
			       const char *usage, int flags);
extern int common_inode_args_process(int argc, char *argv[],
				     ext2_ino_t *inode, int flags);
extern int common_block_args_process(int argc, char *argv[],
				     blk_t *block, blk_t *count);
extern int debugfs_read_inode(ext2_ino_t ino, struct ext2_inode * inode,
			      const char *cmd);
extern int debugfs_read_inode_full(ext2_ino_t ino, struct ext2_inode * inode,
				   const char *cmd, int bufsize);
extern int debugfs_write_inode(ext2_ino_t ino, struct ext2_inode * inode,
			       const char *cmd);
extern int debugfs_write_new_inode(ext2_ino_t ino, struct ext2_inode * inode,
				   const char *cmd);

/* ss command functions */

/* dump.c */
extern void do_dump(int argc, char **argv);
extern void do_cat(int argc, char **argv);
extern void do_rdump(int argc, char **argv);

/* htree.c */
extern void do_htree_dump(int argc, char **argv);
extern void do_dx_hash(int argc, char **argv);
extern void do_dirsearch(int argc, char **argv);

/* logdump.c */
extern void do_logdump(int argc, char **argv);

/* lsdel.c */
extern void do_lsdel(int argc, char **argv);

/* icheck.c */
extern void do_icheck(int argc, char **argv);

/* ncheck.c */
extern void do_ncheck(int argc, char **argv);

/* set_fields.c */
extern void do_set_super(int argc, char **);
extern void do_set_inode(int argc, char **);
extern void do_set_block_group_descriptor(int argc, char **);

/* unused.c */
extern void do_dump_unused(int argc, char **argv);

/* debugfs.c */
extern void internal_dump_inode(FILE *, const char *, ext2_ino_t,
				struct ext2_inode *, int);

extern void do_dirty_filesys(int argc, char **argv);
extern void do_open_filesys(int argc, char **argv);
extern void do_close_filesys(int argc, char **argv);
extern void do_lcd(int argc, char **argv);
extern void do_init_filesys(int argc, char **argv);
extern void do_show_super_stats(int argc, char **argv);
extern void do_kill_file(int argc, char **argv);
extern void do_rm(int argc, char **argv);
extern void do_link(int argc, char **argv);
extern void do_undel(int argc, char **argv);
extern void do_unlink(int argc, char **argv);
extern void do_find_free_block(int argc, char **argv);
extern void do_find_free_inode(int argc, char **argv);
extern void do_stat(int argc, char **argv);

extern void do_chroot(int argc, char **argv);
extern void do_clri(int argc, char **argv);
extern void do_freei(int argc, char **argv);
extern void do_seti(int argc, char **argv);
extern void do_testi(int argc, char **argv);
extern void do_freeb(int argc, char **argv);
extern void do_setb(int argc, char **argv);
extern void do_testb(int argc, char **argv);
extern void do_modify_inode(int argc, char **argv);
extern void do_list_dir(int argc, char **argv);
extern void do_change_working_dir(int argc, char **argv);
extern void do_print_working_directory(int argc, char **argv);
extern void do_write(int argc, char **argv);
extern void do_mknod(int argc, char **argv);
extern void do_mkdir(int argc, char **argv);
extern void do_rmdir(int argc, char **argv);
extern void do_show_debugfs_params(int argc, char **argv);
extern void do_expand_dir(int argc, char **argv);
extern void do_features(int argc, char **argv);
extern void do_bmap(int argc, char **argv);
extern void do_imap(int argc, char **argv);
extern void do_set_current_time(int argc, char **argv);
extern void do_supported_features(int argc, char **argv);

