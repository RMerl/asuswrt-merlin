/*
 * QNX4 file system, Linux implementation.
 *
 * Version : 0.2.1
 *
 * Using parts of the xiafs filesystem.
 *
 * History :
 *
 * 25-05-1998 by Richard Frowijn : first release.
 * 21-06-1998 by Frank Denis : wrote qnx4_readpage to use generic_file_read.
 * 27-06-1998 by Frank Denis : file overwriting.
 */

#include <linux/fs.h>
#include <linux/qnx4_fs.h>

/*
 * We have mostly NULL's here: the current defaults are ok for
 * the qnx4 filesystem.
 */
const struct file_operations qnx4_file_operations =
{
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.aio_read	= generic_file_aio_read,
	.mmap		= generic_file_mmap,
	.splice_read	= generic_file_splice_read,
#ifdef CONFIG_QNX4FS_RW
	.write		= do_sync_write,
	.aio_write	= generic_file_aio_write,
	.fsync		= qnx4_sync_file,
#endif
};

const struct inode_operations qnx4_file_inode_operations =
{
#ifdef CONFIG_QNX4FS_RW
	.truncate	= qnx4_truncate,
#endif
};
