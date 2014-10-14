/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef	__XFS_LOG_PRIV_H__
#define __XFS_LOG_PRIV_H__

struct xfs_buf;
struct ktrace;
struct log;
struct xlog_ticket;
struct xfs_buf_cancel;
struct xfs_mount;

/*
 * Macros, structures, prototypes for internal log manager use.
 */

#define XLOG_MIN_ICLOGS		2
#define XLOG_MED_ICLOGS		4
#define XLOG_MAX_ICLOGS		8
#define XLOG_HEADER_MAGIC_NUM	0xFEEDbabe	/* Invalid cycle number */
#define XLOG_VERSION_1		1
#define XLOG_VERSION_2		2		/* Large IClogs, Log sunit */
#define XLOG_VERSION_OKBITS	(XLOG_VERSION_1 | XLOG_VERSION_2)
#define XLOG_RECORD_BSIZE	(16*1024)	/* eventually 32k */
#define XLOG_BIG_RECORD_BSIZE	(32*1024)	/* 32k buffers */
#define XLOG_MAX_RECORD_BSIZE	(256*1024)
#define XLOG_HEADER_CYCLE_SIZE	(32*1024)	/* cycle data in header */
#define XLOG_RECORD_BSHIFT	14		/* 16384 == 1 << 14 */
#define XLOG_BIG_RECORD_BSHIFT	15		/* 32k == 1 << 15 */
#define XLOG_MAX_RECORD_BSHIFT	18		/* 256k == 1 << 18 */
#define XLOG_BTOLSUNIT(log, b)  (((b)+(log)->l_mp->m_sb.sb_logsunit-1) / \
                                 (log)->l_mp->m_sb.sb_logsunit)
#define XLOG_LSUNITTOB(log, su) ((su) * (log)->l_mp->m_sb.sb_logsunit)

#define XLOG_HEADER_SIZE	512

#define XLOG_REC_SHIFT(log) \
	BTOBB(1 << (XFS_SB_VERSION_HASLOGV2(&log->l_mp->m_sb) ? \
	 XLOG_MAX_RECORD_BSHIFT : XLOG_BIG_RECORD_BSHIFT))
#define XLOG_TOTAL_REC_SHIFT(log) \
	BTOBB(XLOG_MAX_ICLOGS << (XFS_SB_VERSION_HASLOGV2(&log->l_mp->m_sb) ? \
	 XLOG_MAX_RECORD_BSHIFT : XLOG_BIG_RECORD_BSHIFT))

/*
 *  set lsns
 */

#define ASSIGN_ANY_LSN_HOST(lsn,cycle,block)  \
    { \
	(lsn) = ((xfs_lsn_t)(cycle)<<32)|(block); \
    }
#define ASSIGN_ANY_LSN_DISK(lsn,cycle,block)  \
    { \
	INT_SET(((uint *)&(lsn))[0], ARCH_CONVERT, (cycle)); \
	INT_SET(((uint *)&(lsn))[1], ARCH_CONVERT, (block)); \
    }
#define ASSIGN_LSN(lsn,log) \
    ASSIGN_ANY_LSN_DISK(lsn,(log)->l_curr_cycle,(log)->l_curr_block);

#define XLOG_SET(f,b)		(((f) & (b)) == (b))

#define GET_CYCLE(ptr, arch) \
    (INT_GET(*(uint *)(ptr), arch) == XLOG_HEADER_MAGIC_NUM ? \
	 INT_GET(*((uint *)(ptr)+1), arch) : \
	 INT_GET(*(uint *)(ptr), arch) \
    )

#define BLK_AVG(blk1, blk2)	((blk1+blk2) >> 1)


#ifdef __KERNEL__

/*
 * get client id from packed copy.
 *
 * this hack is here because the xlog_pack code copies four bytes
 * of xlog_op_header containing the fields oh_clientid, oh_flags
 * and oh_res2 into the packed copy.
 *
 * later on this four byte chunk is treated as an int and the
 * client id is pulled out.
 *
 * this has endian issues, of course.
 */

#ifndef XFS_NATIVE_HOST
#define GET_CLIENT_ID(i,arch) \
    ((i) & 0xff)
#else
#define GET_CLIENT_ID(i,arch) \
    ((i) >> 24)
#endif

#define GRANT_LOCK(log)		mutex_spinlock(&(log)->l_grant_lock)
#define GRANT_UNLOCK(log, s)	mutex_spinunlock(&(log)->l_grant_lock, s)
#define LOG_LOCK(log)		mutex_spinlock(&(log)->l_icloglock)
#define LOG_UNLOCK(log, s)	mutex_spinunlock(&(log)->l_icloglock, s)

#define xlog_panic(args...)	cmn_err(CE_PANIC, ## args)
#define xlog_exit(args...)	cmn_err(CE_PANIC, ## args)
#define xlog_warn(args...)	cmn_err(CE_WARN, ## args)

/*
 * In core log state
 */
#define XLOG_STATE_ACTIVE    0x0001 /* Current IC log being written to */
#define XLOG_STATE_WANT_SYNC 0x0002 /* Want to sync this iclog; no more writes */
#define XLOG_STATE_SYNCING   0x0004 /* This IC log is syncing */
#define XLOG_STATE_DONE_SYNC 0x0008 /* Done syncing to disk */
#define XLOG_STATE_DO_CALLBACK \
			     0x0010 /* Process callback functions */
#define XLOG_STATE_CALLBACK  0x0020 /* Callback functions now */
#define XLOG_STATE_DIRTY     0x0040 /* Dirty IC log, not ready for ACTIVE status*/
#define XLOG_STATE_IOERROR   0x0080 /* IO error happened in sync'ing log */
#define XLOG_STATE_ALL	     0x7FFF /* All possible valid flags */
#define XLOG_STATE_NOTUSED   0x8000 /* This IC log not being used */
#endif	/* __KERNEL__ */

/*
 * Flags to log operation header
 *
 * The first write of a new transaction will be preceded with a start
 * record, XLOG_START_TRANS.  Once a transaction is committed, a commit
 * record is written, XLOG_COMMIT_TRANS.  If a single region can not fit into
 * the remainder of the current active in-core log, it is split up into
 * multiple regions.  Each partial region will be marked with a
 * XLOG_CONTINUE_TRANS until the last one, which gets marked with XLOG_END_TRANS.
 *
 */
#define XLOG_START_TRANS	0x01	/* Start a new transaction */
#define XLOG_COMMIT_TRANS	0x02	/* Commit this transaction */
#define XLOG_CONTINUE_TRANS	0x04	/* Cont this trans into new region */
#define XLOG_WAS_CONT_TRANS	0x08	/* Cont this trans into new region */
#define XLOG_END_TRANS		0x10	/* End a continued transaction */
#define XLOG_UNMOUNT_TRANS	0x20	/* Unmount a filesystem transaction */

#ifdef __KERNEL__
/*
 * Flags to log ticket
 */
#define XLOG_TIC_INITED		0x1	/* has been initialized */
#define XLOG_TIC_PERM_RESERV	0x2	/* permanent reservation */
#define XLOG_TIC_IN_Q		0x4
#endif	/* __KERNEL__ */

#define XLOG_UNMOUNT_TYPE	0x556e	/* Un for Unmount */

/*
 * Flags for log structure
 */
#define XLOG_CHKSUM_MISMATCH	0x1	/* used only during recovery */
#define XLOG_ACTIVE_RECOVERY	0x2	/* in the middle of recovery */
#define	XLOG_RECOVERY_NEEDED	0x4	/* log was recovered */
#define XLOG_IO_ERROR		0x8	/* log hit an I/O error, and being
					   shutdown */
typedef __uint32_t xlog_tid_t;


#ifdef __KERNEL__
/*
 * Below are states for covering allocation transactions.
 * By covering, we mean changing the h_tail_lsn in the last on-disk
 * log write such that no allocation transactions will be re-done during
 * recovery after a system crash. Recovery starts at the last on-disk
 * log write.
 *
 * These states are used to insert dummy log entries to cover
 * space allocation transactions which can undo non-transactional changes
 * after a crash. Writes to a file with space
 * already allocated do not result in any transactions. Allocations
 * might include space beyond the EOF. So if we just push the EOF a
 * little, the last transaction for the file could contain the wrong
 * size. If there is no file system activity, after an allocation
 * transaction, and the system crashes, the allocation transaction
 * will get replayed and the file will be truncated. This could
 * be hours/days/... after the allocation occurred.
 *
 * The fix for this is to do two dummy transactions when the
 * system is idle. We need two dummy transaction because the h_tail_lsn
 * in the log record header needs to point beyond the last possible
 * non-dummy transaction. The first dummy changes the h_tail_lsn to
 * the first transaction before the dummy. The second dummy causes
 * h_tail_lsn to point to the first dummy. Recovery starts at h_tail_lsn.
 *
 * These dummy transactions get committed when everything
 * is idle (after there has been some activity).
 *
 * There are 5 states used to control this.
 *
 *  IDLE -- no logging has been done on the file system or
 *		we are done covering previous transactions.
 *  NEED -- logging has occurred and we need a dummy transaction
 *		when the log becomes idle.
 *  DONE -- we were in the NEED state and have committed a dummy
 *		transaction.
 *  NEED2 -- we detected that a dummy transaction has gone to the
 *		on disk log with no other transactions.
 *  DONE2 -- we committed a dummy transaction when in the NEED2 state.
 *
 * There are two places where we switch states:
 *
 * 1.) In xfs_sync, when we detect an idle log and are in NEED or NEED2.
 *	We commit the dummy transaction and switch to DONE or DONE2,
 *	respectively. In all other states, we don't do anything.
 *
 * 2.) When we finish writing the on-disk log (xlog_state_clean_log).
 *
 *	No matter what state we are in, if this isn't the dummy
 *	transaction going out, the next state is NEED.
 *	So, if we aren't in the DONE or DONE2 states, the next state
 *	is NEED. We can't be finishing a write of the dummy record
 *	unless it was committed and the state switched to DONE or DONE2.
 *
 *	If we are in the DONE state and this was a write of the
 *		dummy transaction, we move to NEED2.
 *
 *	If we are in the DONE2 state and this was a write of the
 *		dummy transaction, we move to IDLE.
 *
 *
 * Writing only one dummy transaction can get appended to
 * one file space allocation. When this happens, the log recovery
 * code replays the space allocation and a file could be truncated.
 * This is why we have the NEED2 and DONE2 states before going idle.
 */

#define XLOG_STATE_COVER_IDLE	0
#define XLOG_STATE_COVER_NEED	1
#define XLOG_STATE_COVER_DONE	2
#define XLOG_STATE_COVER_NEED2	3
#define XLOG_STATE_COVER_DONE2	4

#define XLOG_COVER_OPS		5


/* Ticket reservation region accounting */ 
#define XLOG_TIC_LEN_MAX	15
#define XLOG_TIC_RESET_RES(t) ((t)->t_res_num = \
				(t)->t_res_arr_sum = (t)->t_res_num_ophdrs = 0)
#define XLOG_TIC_ADD_OPHDR(t) ((t)->t_res_num_ophdrs++)
#define XLOG_TIC_ADD_REGION(t, len, type)				\
	do {								\
		if ((t)->t_res_num == XLOG_TIC_LEN_MAX) { 		\
			/* add to overflow and start again */		\
			(t)->t_res_o_flow += (t)->t_res_arr_sum;	\
			(t)->t_res_num = 0;				\
			(t)->t_res_arr_sum = 0;				\
		}							\
		(t)->t_res_arr[(t)->t_res_num].r_len = (len);		\
		(t)->t_res_arr[(t)->t_res_num].r_type = (type);		\
		(t)->t_res_arr_sum += (len);				\
		(t)->t_res_num++;					\
	} while (0)

/*
 * Reservation region
 * As would be stored in xfs_log_iovec but without the i_addr which
 * we don't care about.
 */
typedef struct xlog_res {
	uint	r_len;	/* region length		:4 */
	uint	r_type;	/* region's transaction type	:4 */
} xlog_res_t;

typedef struct xlog_ticket {
	sv_t		   t_sema;	 /* sleep on this semaphore      : 20 */
 	struct xlog_ticket *t_next;	 /*			         :4|8 */
	struct xlog_ticket *t_prev;	 /*				 :4|8 */
	xlog_tid_t	   t_tid;	 /* transaction identifier	 : 4  */
	int		   t_curr_res;	 /* current reservation in bytes : 4  */
	int		   t_unit_res;	 /* unit reservation in bytes    : 4  */
	char		   t_ocnt;	 /* original count		 : 1  */
	char		   t_cnt;	 /* current count		 : 1  */
	char		   t_clientid;	 /* who does this belong to;	 : 1  */
	char		   t_flags;	 /* properties of reservation	 : 1  */
	uint		   t_trans_type; /* transaction type             : 4  */

        /* reservation array fields */
	uint		   t_res_num;                    /* num in array : 4 */
	uint		   t_res_num_ophdrs;		 /* num op hdrs  : 4 */
	uint		   t_res_arr_sum;		 /* array sum    : 4 */
	uint		   t_res_o_flow;		 /* sum overflow : 4 */
	xlog_res_t	   t_res_arr[XLOG_TIC_LEN_MAX];  /* array of res : 8 * 15 */ 
} xlog_ticket_t;

#endif


typedef struct xlog_op_header {
	xlog_tid_t oh_tid;	/* transaction id of operation	:  4 b */
	int	   oh_len;	/* bytes in data region		:  4 b */
	__uint8_t  oh_clientid;	/* who sent me this		:  1 b */
	__uint8_t  oh_flags;	/*				:  1 b */
	ushort	   oh_res2;	/* 32 bit align			:  2 b */
} xlog_op_header_t;


/* valid values for h_fmt */
#define XLOG_FMT_UNKNOWN  0
#define XLOG_FMT_LINUX_LE 1
#define XLOG_FMT_LINUX_BE 2
#define XLOG_FMT_IRIX_BE  3

/* our fmt */
#ifdef XFS_NATIVE_HOST
#define XLOG_FMT XLOG_FMT_LINUX_BE
#else
#define XLOG_FMT XLOG_FMT_LINUX_LE
#endif

typedef struct xlog_rec_header {
	uint	  h_magicno;	/* log record (LR) identifier		:  4 */
	uint	  h_cycle;	/* write cycle of log			:  4 */
	int	  h_version;	/* LR version				:  4 */
	int	  h_len;	/* len in bytes; should be 64-bit aligned: 4 */
	xfs_lsn_t h_lsn;	/* lsn of this LR			:  8 */
	xfs_lsn_t h_tail_lsn;	/* lsn of 1st LR w/ buffers not committed: 8 */
	uint	  h_chksum;	/* may not be used; non-zero if used	:  4 */
	int	  h_prev_block; /* block number to previous LR		:  4 */
	int	  h_num_logops;	/* number of log operations in this LR	:  4 */
	uint	  h_cycle_data[XLOG_HEADER_CYCLE_SIZE / BBSIZE];
	/* new fields */
	int       h_fmt;        /* format of log record                 :  4 */
	uuid_t    h_fs_uuid;    /* uuid of FS                           : 16 */
	int       h_size;	/* iclog size				:  4 */
} xlog_rec_header_t;

typedef struct xlog_rec_ext_header {
	uint	  xh_cycle;	/* write cycle of log			: 4 */
	uint	  xh_cycle_data[XLOG_HEADER_CYCLE_SIZE / BBSIZE]; /*	: 256 */
} xlog_rec_ext_header_t;

#ifdef __KERNEL__
/*
 * - A log record header is 512 bytes.  There is plenty of room to grow the
 *	xlog_rec_header_t into the reserved space.
 * - ic_data follows, so a write to disk can start at the beginning of
 *	the iclog.
 * - ic_forcesema is used to implement synchronous forcing of the iclog to disk.
 * - ic_next is the pointer to the next iclog in the ring.
 * - ic_bp is a pointer to the buffer used to write this incore log to disk.
 * - ic_log is a pointer back to the global log structure.
 * - ic_callback is a linked list of callback function/argument pairs to be
 *	called after an iclog finishes writing.
 * - ic_size is the full size of the header plus data.
 * - ic_offset is the current number of bytes written to in this iclog.
 * - ic_refcnt is bumped when someone is writing to the log.
 * - ic_state is the state of the iclog.
 */
typedef struct xlog_iclog_fields {
	sv_t			ic_forcesema;
	sv_t			ic_writesema;
	struct xlog_in_core	*ic_next;
	struct xlog_in_core	*ic_prev;
	struct xfs_buf		*ic_bp;
	struct log		*ic_log;
	xfs_log_callback_t	*ic_callback;
	xfs_log_callback_t	**ic_callback_tail;
#ifdef XFS_LOG_TRACE
	struct ktrace		*ic_trace;
#endif
	int			ic_size;
	int			ic_offset;
	int			ic_refcnt;
	int			ic_bwritecnt;
	ushort_t		ic_state;
	char			*ic_datap;	/* pointer to iclog data */
} xlog_iclog_fields_t;

typedef union xlog_in_core2 {
	xlog_rec_header_t	hic_header;
	xlog_rec_ext_header_t	hic_xheader;
	char			hic_sector[XLOG_HEADER_SIZE];
} xlog_in_core_2_t;

typedef struct xlog_in_core {
	xlog_iclog_fields_t	hic_fields;
	xlog_in_core_2_t	*hic_data;
} xlog_in_core_t;

/*
 * Defines to save our code from this glop.
 */
#define	ic_forcesema	hic_fields.ic_forcesema
#define ic_writesema	hic_fields.ic_writesema
#define	ic_next		hic_fields.ic_next
#define	ic_prev		hic_fields.ic_prev
#define	ic_bp		hic_fields.ic_bp
#define	ic_log		hic_fields.ic_log
#define	ic_callback	hic_fields.ic_callback
#define	ic_callback_tail hic_fields.ic_callback_tail
#define	ic_trace	hic_fields.ic_trace
#define	ic_size		hic_fields.ic_size
#define	ic_offset	hic_fields.ic_offset
#define	ic_refcnt	hic_fields.ic_refcnt
#define	ic_bwritecnt	hic_fields.ic_bwritecnt
#define	ic_state	hic_fields.ic_state
#define ic_datap	hic_fields.ic_datap
#define ic_header	hic_data->hic_header

/*
 * The reservation head lsn is not made up of a cycle number and block number.
 * Instead, it uses a cycle number and byte number.  Logs don't expect to
 * overflow 31 bits worth of byte offset, so using a byte number will mean
 * that round off problems won't occur when releasing partial reservations.
 */
typedef struct log {
	/* The following block of fields are changed while holding icloglock */
	sema_t			l_flushsema;    /* iclog flushing semaphore */
	int			l_flushcnt;	/* # of procs waiting on this
						 * sema */
	int			l_ticket_cnt;	/* free ticket count */
	int			l_ticket_tcnt;	/* total ticket count */
	int			l_covered_state;/* state of "covering disk
						 * log entries" */
	xlog_ticket_t		*l_freelist;    /* free list of tickets */
	xlog_ticket_t		*l_unmount_free;/* kmem_free these addresses */
	xlog_ticket_t		*l_tail;        /* free list of tickets */
	xlog_in_core_t		*l_iclog;       /* head log queue	*/
	lock_t			l_icloglock;    /* grab to change iclog state */
	xfs_lsn_t		l_tail_lsn;     /* lsn of 1st LR with unflushed
						 * buffers */
	xfs_lsn_t		l_last_sync_lsn;/* lsn of last LR on disk */
	struct xfs_mount	*l_mp;	        /* mount point */
	struct xfs_buf		*l_xbuf;        /* extra buffer for log
						 * wrapping */
	struct xfs_buftarg	*l_targ;        /* buftarg of log */
	xfs_daddr_t		l_logBBstart;   /* start block of log */
	int			l_logsize;      /* size of log in bytes */
	int			l_logBBsize;    /* size of log in BB chunks */
	int			l_curr_cycle;   /* Cycle number of log writes */
	int			l_prev_cycle;   /* Cycle number before last
						 * block increment */
	int			l_curr_block;   /* current logical log block */
	int			l_prev_block;   /* previous logical log block */
	int			l_iclog_size;	/* size of log in bytes */
	int			l_iclog_size_log; /* log power size of log */
	int			l_iclog_bufs;	/* number of iclog buffers */

	/* The following field are used for debugging; need to hold icloglock */
	char			*l_iclog_bak[XLOG_MAX_ICLOGS];

	/* The following block of fields are changed while holding grant_lock */
	lock_t			l_grant_lock;
	xlog_ticket_t		*l_reserve_headq;
	xlog_ticket_t		*l_write_headq;
	int			l_grant_reserve_cycle;
	int			l_grant_reserve_bytes;
	int			l_grant_write_cycle;
	int			l_grant_write_bytes;

	/* The following fields don't need locking */
#ifdef XFS_LOG_TRACE
	struct ktrace		*l_trace;
	struct ktrace		*l_grant_trace;
#endif
	uint			l_flags;
	uint			l_quotaoffs_flag; /* XFS_DQ_*, for QUOTAOFFs */
	struct xfs_buf_cancel	**l_buf_cancel_table;
	int			l_iclog_hsize;  /* size of iclog header */
	int			l_iclog_heads;  /* # of iclog header sectors */
	uint			l_sectbb_log;   /* log2 of sector size in BBs */
	uint			l_sectbb_mask;  /* sector size (in BBs)
						 * alignment mask */
} xlog_t;

#define XLOG_FORCED_SHUTDOWN(log)	((log)->l_flags & XLOG_IO_ERROR)


/* common routines */
extern xfs_lsn_t xlog_assign_tail_lsn(struct xfs_mount *mp);
extern int	 xlog_find_tail(xlog_t	*log,
				xfs_daddr_t *head_blk,
				xfs_daddr_t *tail_blk);
extern int	 xlog_recover(xlog_t *log);
extern int	 xlog_recover_finish(xlog_t *log, int mfsi_flags);
extern void	 xlog_pack_data(xlog_t *log, xlog_in_core_t *iclog, int);
extern void	 xlog_recover_process_iunlinks(xlog_t *log);

extern struct xfs_buf *xlog_get_bp(xlog_t *, int);
extern void	 xlog_put_bp(struct xfs_buf *);
extern int	 xlog_bread(xlog_t *, xfs_daddr_t, int, struct xfs_buf *);

/* iclog tracing */
#define XLOG_TRACE_GRAB_FLUSH  1
#define XLOG_TRACE_REL_FLUSH   2
#define XLOG_TRACE_SLEEP_FLUSH 3
#define XLOG_TRACE_WAKE_FLUSH  4

/*
 * Unmount record type is used as a pseudo transaction type for the ticket.
 * It's value must be outside the range of XFS_TRANS_* values.
 */
#define XLOG_UNMOUNT_REC_TYPE	(-1U)

#endif	/* __KERNEL__ */

#endif	/* __XFS_LOG_PRIV_H__ */
