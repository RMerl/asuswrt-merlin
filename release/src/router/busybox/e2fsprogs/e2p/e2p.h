/* vi: set sw=4 ts=4: */
#include "libbb.h"
#include <sys/types.h>		/* Needed by dirent.h on netbsd */
#include <stdio.h>
#include <dirent.h>

#include "../ext2fs/ext2_fs.h"

#define E2P_FEATURE_COMPAT	0
#define E2P_FEATURE_INCOMPAT	1
#define E2P_FEATURE_RO_INCOMPAT	2
#ifndef EXT3_FEATURE_INCOMPAT_EXTENTS
#define EXT3_FEATURE_INCOMPAT_EXTENTS           0x0040
#endif

/* `options' for print_e2flags() */

#define PFOPT_LONG  1 /* Must be 1 for compatibility with `int long_format'. */


/*void list_super(struct ext2_super_block * s);*/
void list_super2(struct ext2_super_block * s, FILE *f);
#define list_super(s) list_super2(s, stdout)
void print_fs_errors (FILE *f, unsigned short errors);
void print_fs_state (FILE *f, unsigned short state);

const char *e2p_feature2string(int compat, unsigned int mask);
int e2p_string2feature(char *string, int *compat, unsigned int *mask);
int e2p_edit_feature(const char *str, __u32 *compat_array, __u32 *ok_array);

const char *e2p_hash2string(int num);
int e2p_string2hash(char *string);

const char *e2p_mntopt2string(unsigned int mask);
int e2p_string2mntopt(char *string, unsigned int *mask);
int e2p_edit_mntopts(const char *str, __u32 *mntopts, __u32 ok);

unsigned long parse_num_blocks(const char *arg, int log_block_size);

char *e2p_os2string(int os_type);
int e2p_string2os(char *str);
