
#ifndef _JFS_COMPAT_H
#define _JFS_COMPAT_H

#include "kernel-list.h"
#include <errno.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#define printk printf
#define KERN_ERR ""
#define KERN_DEBUG ""

#define READ 0
#define WRITE 1

#define cpu_to_be32(n) htonl(n)
#define be32_to_cpu(n) ntohl(n)

typedef unsigned int tid_t;
typedef struct journal_s journal_t;

struct buffer_head;
struct inode;

struct journal_s
{
	unsigned long		j_flags;
	int			j_errno;
	struct buffer_head *	j_sb_buffer;
	struct journal_superblock_s *j_superblock;
	int			j_format_version;
	unsigned long		j_head;
	unsigned long		j_tail;
	unsigned long		j_free;
	unsigned long		j_first, j_last;
	kdev_t			j_dev;
	kdev_t			j_fs_dev;
	int			j_blocksize;
	unsigned int		j_blk_offset;
	unsigned int		j_maxlen;
	struct inode *		j_inode;
	tid_t			j_tail_sequence;
	tid_t			j_transaction_sequence;
	__u8			j_uuid[16];
	struct jbd_revoke_table_s *j_revoke;
	tid_t			j_failed_commit;
};

#define J_ASSERT(assert)						\
	do { if (!(assert)) {						\
		printf ("Assertion failure in %s() at %s line %d: "	\
			"\"%s\"\n",					\
			__FUNCTION__, __FILE__, __LINE__, # assert);	\
		fatal_error(e2fsck_global_ctx, 0);			\
	} } while (0)

#define is_journal_abort(x) 0

#define BUFFER_TRACE(bh, info)	do {} while (0)

/* Need this so we can compile with configure --enable-gcc-wall */
#ifdef NO_INLINE_FUNCS
#define inline
#endif

#endif /* _JFS_COMPAT_H */
