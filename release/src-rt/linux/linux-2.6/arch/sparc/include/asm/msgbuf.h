#ifndef _SPARC_MSGBUF_H
#define _SPARC_MSGBUF_H

/*
 * The msqid64_ds structure for sparc64 architecture.
 * Note extra padding because this structure is passed back and forth
 * between kernel and user space.
 *
 * Pad space is left for:
 * - 64-bit time_t to solve y2038 problem
 * - 2 miscellaneous 32-bit values
 */

#if defined(__sparc__) && defined(__arch64__)
# define PADDING(x)
#else
# define PADDING(x) unsigned int x;
#endif


struct msqid64_ds {
	struct ipc64_perm msg_perm;
	PADDING(__pad1)
	__kernel_time_t msg_stime;	/* last msgsnd time */
	PADDING(__pad2)
	__kernel_time_t msg_rtime;	/* last msgrcv time */
	PADDING(__pad3)
	__kernel_time_t msg_ctime;	/* last change time */
	unsigned long  msg_cbytes;	/* current number of bytes on queue */
	unsigned long  msg_qnum;	/* number of messages in queue */
	unsigned long  msg_qbytes;	/* max number of bytes on queue */
	__kernel_pid_t msg_lspid;	/* pid of last msgsnd */
	__kernel_pid_t msg_lrpid;	/* last receive pid */
	unsigned long  __unused1;
	unsigned long  __unused2;
};
#undef PADDING
#endif /* _SPARC_MSGBUF_H */
