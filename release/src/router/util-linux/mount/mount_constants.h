#ifndef MS_RDONLY
#define MS_RDONLY	 1	/* Mount read-only */
#endif
#ifndef MS_NOSUID
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#endif
#ifndef MS_NODEV
#define MS_NODEV	 4	/* Disallow access to device special files */
#endif
#ifndef MS_NOEXEC
#define MS_NOEXEC	 8	/* Disallow program execution */
#endif
#ifndef MS_SYNCHRONOUS
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#endif
#ifndef MS_REMOUNT
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#endif
#ifndef MS_MANDLOCK
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#endif
#ifndef MS_DIRSYNC
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
#endif
#ifndef MS_NOATIME
#define MS_NOATIME	0x400	/* 1024: Do not update access times. */
#endif
#ifndef MS_NODIRATIME
#define MS_NODIRATIME   0x800	/* 2048: Don't update directory access times */
#endif
#ifndef MS_BIND
#define	MS_BIND		0x1000	/* 4096: Mount existing tree also elsewhere */
#endif
#ifndef MS_MOVE
#define MS_MOVE		0x2000	/* 8192: Atomically move tree */
#endif
#ifndef MS_REC
#define MS_REC		0x4000	/* 16384: Recursive loopback */
#endif
#ifndef MS_SILENT
#define MS_SILENT	0x8000	/* 32768 (was MS_VERBOSE) */
#endif
#ifndef MS_RELATIME
#define MS_RELATIME	0x200000 /* 200000: Update access times relative
                                  to mtime/ctime */
#endif
#ifndef MS_UNBINDABLE
#define MS_UNBINDABLE	(1<<17)	/* 131072 unbindable*/
#endif
#ifndef MS_PRIVATE
#define MS_PRIVATE	(1<<18)	/* 262144 Private*/
#endif
#ifndef MS_SLAVE
#define MS_SLAVE	(1<<19)	/* 524288 Slave*/
#endif
#ifndef MS_SHARED
#define MS_SHARED	(1<<20)	/* 1048576 Shared*/
#endif
#ifndef MS_I_VERSION
#define MS_I_VERSION	(1<<23)	/* update inode I_version field */
#endif
#ifndef MS_STRICTATIME
#define MS_STRICTATIME	(1<<24) /* strict atime semantics */
#endif
/*
 * Magic mount flag number. Had to be or-ed to the flag values.
 */
#ifndef MS_MGC_VAL
#define MS_MGC_VAL 0xC0ED0000	/* magic flag number to indicate "new" flags */
#endif
#ifndef MS_MGC_MSK
#define MS_MGC_MSK 0xffff0000	/* magic flag number mask */
#endif
