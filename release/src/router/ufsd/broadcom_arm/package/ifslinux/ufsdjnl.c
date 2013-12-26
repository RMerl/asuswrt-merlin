/////////////////////////////////////////////////////////////////////////////
// February of 2012. (C) Paragon Software
//
//
// Written by Alexander Mamaev
/////////////////////////////////////////////////////////////////////////////
// This file is under the terms of the GNU General Public License, version 2.
// http://www.gnu.org/licenses/gpl-2.0.html
// Filesystem journal-writing code for HFS+ (based on jbd2 implementation in Kernel).
/////////////////////////////////////////////////////////////////////////////
//
// This field is updated by CVS
//
static const char s_FileVer[] = "$Id: ufsdjnl.c 216367 2013-11-08 22:10:46Z shura $";

#include "config.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/proc_fs.h> // proc_mkdir
#include <linux/seq_file.h>

#include "jnl.h"


//
// Current module is supported in 2.6.20+ only
//
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)

const struct jnl_operations jnl_op = {0};
static int __init
jnl_init( void ){ return 0; }
static void __exit
jnl_exit( void ){}

#else

#include <linux/freezer.h>
#include <linux/bit_spinlock.h>

#if defined HAVE_DECL_KMAP_ATOMIC_V1 && HAVE_DECL_KMAP_ATOMIC_V1
  #define atomic_kmap(p)    kmap_atomic( (p), KM_USER0 )
  #define atomic_kunmap(p)  kunmap_atomic( (p), KM_USER0 )
#else
  #define atomic_kmap(p)    kmap_atomic( (p) )
  #define atomic_kunmap(p)  kunmap_atomic( (p) )
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
  #define kmem_cache_allocZ(c)    kmem_cache_alloc( (c), GFP_NOFS | __GFP_ZERO )
#elif defined HAVE_DECL_KMEM_CACHE_ZALLOC && HAVE_DECL_KMEM_CACHE_ZALLOC
  #define kmem_cache_allocZ(c)    kmem_cache_zalloc( (c), GFP_NOFS )
#else
  #error "Unknown version of kmem_cache_zalloc"
//  void *p = kmem_cache_alloc( (c), GFP_KERNEL );
  //if ( NULL != p )
    //memset( p, 0, kmem_cache_size( (c) ) );
#endif

#if defined CONFIG_LBD | defined CONFIG_LBDAF
  // sector_t - 64 bit value
  #define PSCT      "ll"
#else
  // sector_t - 32 bit value
  #define PSCT      "l"
#endif

#define IN
#define OUT

//#define async_commit

//#define JNL_PARANOID_IOFAIL

#if defined UFSD_TRACE && defined UFSD_DEBUG

  // Trace get_write/get_create access
  #define JNL_TRACE_LEVEL_ACCESS      0x00000001
  #define JNL_TRACE_LEVEL_DIRTY_META  0x00000002
  #define JNL_TRACE_LEVEL_HANDLE      0x00000004
  #define JNL_TRACE_LEVEL_COMMIT      0x00000008
  #define JNL_TRACE_LEVEL_HDR         0x00000010
  #define JNL_TRACE_LEVEL_THREAD      0x00000020
  #define JNL_TRACE_LEVEL_CHECK       0x00000040
  #define JNL_TRACE_LEVEL_JHDR        0x00000080
  #define JNL_TRACE_LEVEL_IO          0x00000100
  #define JNL_TRACE_LEVEL_REVOKE      0x00000200

  #define JNL_EXPENSIVE_CHECKING "enable more expensive internal consistency checks"

  static unsigned jnl_enable_debug = 0;//~(JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_HANDLE|JNL_TRACE_LEVEL_JHDR|JNL_TRACE_LEVEL_DIRTY_META);
//  KERN_DEBUG
  #define jnl_debug( n, f, a... )                         \
    do {                                                  \
      if ( 0 == (n) || 0 != ((n) & jnl_enable_debug) ) {  \
        printk( KERN_NOTICE f, ## a );                    \
      }                                                   \
    } while ( 0 )

  #define J_ASSERT( x )  BUG_ON( !(x) )

#else

  //
  // Release version
  //
  #define jnl_debug( n, f, a... )
  #define J_ASSERT( x )  BUG_ON( !(x) )
//  #define J_ASSERT( x )
  #undef TRACE_ONLY
  #define TRACE_ONLY(x)
  #undef UFSD_TRACE

#endif

#ifdef JNL_PARANOID_IOFAIL
  #define J_EXPECT( expr, why... )        J_ASSERT( expr )
#else
  #define __journal_expect( expr, why... )  \
    ( {                                \
        int val = ( expr );             \
        if ( !val ) {                   \
          printk( KERN_ERR"JNL unexpected failure: %s: %s;\n", __func__, #expr );  \
          printk( KERN_ERR why "\n" );  \
        }                             \
        val;                          \
    } )
  #define J_EXPECT( expr, why... )  __journal_expect( expr, ## why )
#endif


//===============================================
//           GLOBALS
//===============================================

#define UFSD_PACKAGE_STAMP ", " __DATE__" "__TIME__

static const char s_DriverVer[] = PACKAGE_VERSION
#ifdef PACKAGE_TAG
   " " PACKAGE_TAG
#else
   UFSD_PACKAGE_STAMP
#endif
#if defined CONFIG_LBD | defined CONFIG_LBDAF
  ", LBD=ON"
#else
  ", LBD=OFF"
#endif
  ;


// jnl_head storage management
static struct kmem_cache *jnl_handle_cache;
static struct kmem_cache *jnl_head_cache;
static struct kmem_cache *jnl_revoke_record_cache;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
static struct lock_class_key jnl_handle_key;
#endif

#define JNL_MAX_SLABS     9
// supports block sizes [512 128k] : [2^9 - 2^17]
static struct kmem_cache *jnl_slab[JNL_MAX_SLABS];

static const char jnl_slab_names[9][JNL_MAX_SLABS] = {
  "jnl_0k", "jnl_1k", "jnl_2k", "jnl_4k", "jnl_8k", "jnl_16k", "jnl_32k", "jnl_64k", "jnl_128k"
};

#define journal_oom_retry 1

#define JOURNAL_REVOKE_DEFAULT_HASH       256
#define JOURNAL_REVOKE_DEFAULT_HASH_SHIFT 8

// The default maximum commit age, in seconds.
#define JNL_DEFAULT_MAX_COMMIT_AGE 5

#define JNL_MIN_JOURNAL_BLOCKS 1024


//=========================================================
//
//       On disk structs
//
// All fields always in current endian
//=========================================================

typedef struct block_info{
  u64   bnum;   // 0x00: block # on the file system device
  u32   bsize;  // 0x08: in bytes
  u32   cksum;  // 0x0C
} block_info;

// number of bytes to checksum in a block_list_header
#define BLHDR_CHECKSUM_SIZE     0x20

#define BLHDR_FLAGS_CHECK_CHECKSUMS   0x0001
#define BLHDR_FLAGS_FIRST_HEADER      0x0002

typedef struct block_list_header{

  u16 max_blocks;     // 0x00: max number of blocks in this chunk
  u16 num_blocks;     // 0x02: number of valid block numbers plus one in 'binfo'
  u32 bytes_used;     // 0x04: how many bytes of this tbuffer are used
  u32 checksum;       // 0x08: checksum of first 0x20 bytes
  u32 flags;          // 0x0C: BLHDR_FLAGS_XXX

  u64 next;           // 0x10: if 0 then this is the last block list
  u32 zero;           // 0x18:
  u32 sequence_num;   // 0x1C:

  //
  // To iterate over binfo use the following cycle
  // for ( i = 0; i < num_blocks - 1; i++ )
  //   binfo[i]
  //
  block_info  binfo[1];   // 0x20:

} block_list_header;


#define JOURNAL_HEADER_MAGIC      0x4a4e4c78  // 'JNLx'

//
// The journal header
//
typedef struct jnl_header {
  u32 magic;          // 0x00: 'JNLx' or 'JHDR'
  u32 endian;         // 0x04: ENDIAN_MAGIC
  u64 start;          // 0x08: zero-based byte offset of the start of the first transaction
  u64 end;            // 0x10: zero-based byte offset of where free space begins
  u64 size;           // 0x18: size in bytes of the entire journal
  u32 blhdr_size;     // 0x20: size in bytes of each block_list_header in the journal
  u32 checksum;       // 0x24: checksum of first 0x2C bytes
  u32 jhdr_size;      // 0x28: block size ( in bytes ) of the journal header
  u32 sequence;       // 0x2C: a monotonically increasing value assigned to all txn's
  TRACE_ONLY( u32 overlaps; )       // 0x30: to debug

} jnl_header;


#define JOURNAL_HEADER_CKSUM_SIZE  ( offsetof( struct jnl_header, sequence ) )


//=========================================================
//
//       In memory structs
//
//=========================================================

// journaling buffer types  jlist
#define BJ_None     0 // Not journaled
#define BJ_Metadata 1 // Normal journaled metadata
#define BJ_Forget   2 // Buffer superseded by this transaction
#define BJ_IO       3 // Buffer is for temporary IO use
#define BJ_Shadow   4 // Buffer contents being shadowed to the log
#define BJ_LogCtl   5 // Buffer contains log blhdr's
#define BJ_Reserved 6 // Buffer is reserved for access by journal
#define BJ_Types    7


typedef struct jnl_head {

  // Points back to our buffer_head. [jnl_lock_bh_hdr()]
  struct buffer_head *bh;

  // Reference count. [jnl_lock_bh_hdr()]
  int jcount;

  // Journaling list for this buffer. See BJ_XXX [jnl_lock_bh_state()]
  unsigned jlist;

  // This flag signals the buffer has been modified by the currently running transaction [jnl_lock_bh_state()]
  unsigned modified;

  // This field tracks the last transaction id in which this buffer has been cowed [jnl_lock_bh_state()]
//  unsigned cow_tid;

  // Copy of the buffer data frozen for writing to the log. [jnl_lock_bh_state()]
  char *frozen_data;

  //
  // Pointer to a saved copy of the buffer containing no uncommitted deallocation references,
  // so that allocations can avoid overwriting uncommitted deletes. [jnl_lock_bh_state()]
  char *committed_data;

  //
  // Pointer to the compound transaction which owns this buffer's metadata: either the running transaction or the committing
  // transaction ( if there is one ).  Only applies to buffers on a transaction's data or metadata journaling list.
  // [list_lock] [jnl_lock_bh_state()]
  //
  struct transaction *tr;

  //
  // Pointer to the running compound transaction which is currently modifying the buffer's metadata,
  // if there was already a transaction committing it when the new transaction touched it.
  // [list_lock] [jnl_lock_bh_state()]
  struct transaction *next_tr;

  //
  // Doubly-linked list of buffers on a transaction's data, metadata or forget queue.
  // [list_lock] [jnl_lock_bh_state()]
  struct jnl_head *tnext, *tprev;

  //
  // Pointer to the compound transaction against which this buffer is checkpointed. Only dirty buffers can be checkpointed
  // [list_lock]
  struct transaction *cp_tr;

  //
  // Doubly-linked list of buffers still remaining to be flushed before an old transaction can be checkpointed.
  // [list_lock]
  struct jnl_head *cpnext, *cpprev;

} jnl_head;


typedef struct jnl_revoke_record
{
  struct list_head  hash;
//  unsigned int  sequence; // Used for recovery only
  unsigned long blocknr;
} jnl_revoke_record;


// The revoke table is just a simple hash table of revoke records
typedef struct list_head jnl_revoke_table;


struct jnl_handle
{
  // Which compound transaction is this update a part of?
  struct transaction *tr;

  // Number of remaining buffers we are allowed to dirty:
  int     buffer_credits;

  TRACE_ONLY( int credits_used; )

  // Field for caller's use to track errors through large fs operations
  int     err;

  // Flags [no locking]
  unsigned int  sync:1;     // sync-on-close
  unsigned int  jdata:1;    // force data journaling
  unsigned int  aborted:1;  // fatal error on handle
  unsigned int  cowing:1;   // COWing block to snapshot

  // Number of buffers requested by user: ( before adding the COW credits factor )
  unsigned int  base_credits:14;

  // Number of buffers the user is allowed to dirty: ( counts only buffers dirtied when !h_cowing )
  unsigned int  user_credits:14;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
  struct lockdep_map  lockdep_map;
#endif

#ifdef UFSD_DEBUG
  // COW debugging counters:
  unsigned int cow_moved;     // blocks moved to snapshot
  unsigned int cow_copied;    // blocks copied to snapshot
  unsigned int cow_ok_jh;     // blocks already COWed during current transaction
  unsigned int cow_ok_bitmap; // blocks not set in COW bitmap
  unsigned int cow_ok_mapped; // blocks already mapped in snapshot
  unsigned int cow_bitmaps;   // COW bitmaps created
  unsigned int cow_excluded;  // blocks set in exclude bitmap
#endif
};


//
// Some stats for checkpoint phase
//
#ifdef USE_JNL_STATS
struct transaction_chp_stats {
  unsigned long chp_time;
  unsigned      forced_to_close;
  unsigned      written;
  unsigned      dropped;
};
#endif


//
// The transaction_t type is the guts of the journaling mechanism. It tracks a compound transaction through its various states:
//
// - RUNNING: accepting new updates
// - LOCKED:  Updates still running but we don't accept new ones
// - RUNDOWN: Updates are tidying up but have finished requesting new buffers to modify ( state not used for now )
// - FLUSH:   All updates complete, but we are still writing to disk
// - COMMIT:  All data on disk, writing commit record
// - FINISHED:  We still have to keep the transaction for checkpointing.
//
// The transaction keeps track of all of the buffers modified by a running transaction, and all of the buffers committed but not yet
// flushed to home for finished transactions.
//
// Lock ranking:
//
//    list_lock
//      ->jnl_lock_bh_hdr()  ( This is "innermost" )
//
//    state_lock
//      ->jnl_lock_bh_state()
//
//    jnl_lock_bh_state()
//      ->list_lock
//
//    state_lock
//      ->handle_lock
//
//    state_lock
//      ->list_lock     ( journal_unmap_buffer )
//

typedef struct transaction
{
  // Pointer to the journal for this transaction. [no locking]
  jnl *journal;

  // Sequence number for this transaction [no locking]
  unsigned int    tid;

  //
  // Transaction's current state
  // [no locking - only jnl_thread alters this]
  // [list_lock] guards transition of a transaction into T_FINISHED
  // state and subsequent call of __jnl_drop_transaction()
  // FIXME: needs barriers
  // KLUDGE: [use state_lock]
  //
  enum {
    T_RUNNING,
    T_LOCKED,
    T_FLUSH,
    T_COMMIT,
    T_COMMIT_DFLUSH,
    T_COMMIT_JFLUSH,
    T_FINISHED
  } state;


  // Where in the log does this transaction's commit start? [no locking]
  unsigned long log_start, log_end;

  // Number of buffers on the buffers list [list_lock]
  int       nr_buffers;

  //
  // Doubly-linked circular list of all buffers reserved but not yet modified by this transaction [list_lock]
  //
  jnl_head *reserved_list;

  //
  // Doubly-linked circular list of all metadata buffers owned by this transaction [list_lock]
  //
  jnl_head *buffers;

  //
  // Doubly-linked circular list of all forget buffers ( superseded buffers which we can un-checkpoint once this transaction commits ) [list_lock]
  //
  jnl_head *forget;

  //
  // Doubly-linked circular list of all buffers still to be flushed before this transaction can be checkpointed. [list_lock]
  //
  jnl_head *checkpoint_list;

  //
  //Doubly-linked circular list of all buffers submitted for IO while checkpointing. [list_lock]
  //
  jnl_head *checkpoint_io_list;

  //
  // Doubly-linked circular list of temporary buffers currently undergoing IO in the log [list_lock]
  //
  jnl_head *iobuf_list;

  //
  // Doubly-linked circular list of metadata buffers being shadowed by log IO.
  // The IO buffers on the iobuf list and the shadow buffers on this list match each other one for one at all times. [list_lock]
  //
  jnl_head *shadow_list;

  //
  // Doubly-linked circular list of control buffers being written to the log. [list_lock]
  //
  jnl_head *log_list;

  //
  // Protects info related to handles
  //
  spinlock_t      handle_lock;

  //
  // Longest time some handle had to wait for running transaction
  //
  unsigned long   max_wait;

  //
  // When transaction started
  //
  unsigned long   start_jiffies;

#ifdef USE_JNL_STATS
  //
  // Checkpointing stats [j_checkpoint_sem]
  //
  struct transaction_chp_stats chp_stats;
#endif

  //
  // Number of outstanding updates running on this transaction [handle_lock]
  //
  atomic_t    updates;

  //
  // Number of buffers reserved for use by all handles in this transaction handle but not yet modified. [handle_lock]
  //
  atomic_t    outstanding_credits;

  //
  // Forward and backward links for the circular list of all transactions awaiting checkpoint. [list_lock]
  //
  struct transaction *cpnext, *cpprev;

  //
  // When will the transaction expire ( become due for commit ), in jiffies? [no locking]
  //
  unsigned long   expires;

  //
  // When this transaction started, in nanoseconds [no locking]
  //
  ktime_t     start_time;

  //
  // How many handles used this transaction? [handle_lock]
  //
  atomic_t    handle_count;

  //
  // This transaction is being forced and some process is waiting for it to finish.
  //
  unsigned int synchronous_commit:1;

  // Disk flush needs to be sent to fs partition [no locking]
  int     need_data_flush;

  //
  // For use by the filesystem to store fs-specific data structures associated with the transaction
  //
  struct list_head  private_list;

} transaction;


#ifdef USE_JNL_STATS
typedef struct transaction_stats {
  unsigned long   tid;
  struct {
    unsigned long   wait;
    unsigned long   running;
    unsigned long   locked;
    unsigned long   flushing;
    unsigned long   logging;

    unsigned  handle_count;
    unsigned  blocks;
    unsigned  blocks_logged;
  } run;
} transaction_stats;
#endif


#define JNL_NR_BATCH 64

//
// Journal flags
//
#define JNL_FLAGS_UNMOUNT  0x001   // Journal thread is being destroyed
#define JNL_FLAGS_ABORT    0x002   // Journaling has been aborted for errors
#define JNL_FLAGS_ACK_ERR  0x004   // The errno in the sb has been acked
#define JNL_FLAGS_FLUSHED  0x008   // The journal header has been flushed
//#define JNL_FLAGS_LOADED   0x010   // The journal header has been loaded
#define JNL_FLAGS_BARRIER  0x020   // Use IDE barriers
#define JNL_FLAGS_ABORT_ON_SYNCDATA_ERR  0x040 // Abort the journal on file data write error in ordered mode

struct jnl_control {

  // General journaling state flags [state_lock]. See JNL_FLAGS_XX
  unsigned long   flags;

  // Is there an outstanding uncleared error on the journal ( from a prior abort )? [state_lock]
  int             errno;

  // The header buffer
  struct buffer_head *hdr_buffer;
  struct jnl_header *hdr;

  // Protect the various scalars in the journal
  rwlock_t        state_lock;

  // Number of processes waiting to create a barrier lock [state_lock]
  int             barrier_count;

  // The barrier lock itself
  struct mutex    barrier;

  // The current running transaction... [state_lock] [caller holding open handle]
  transaction     *running_tr;

  // The transaction we are pushing to disk [state_lock] [caller holding open handle]
  transaction     *committing_tr;

  // and a linked circular list of all transactions waiting for checkpointing. [list_lock]
  transaction     *checkpoint_tr;

  // Wait queue for waiting for a locked transaction to start committing, or for a barrier lock to be released
  wait_queue_head_t wait_transaction_locked;

  // Wait queue for waiting for checkpointing to complete
  wait_queue_head_t wait_logspace;

  // Wait queue for waiting for commit to complete
  wait_queue_head_t wait_done_commit;

  // Wait queue to trigger checkpointing
  wait_queue_head_t wait_checkpoint;

  // Wait queue to trigger commit
  wait_queue_head_t wait_commit;

  // Wait queue to wait for updates to complete
  wait_queue_head_t wait_updates;

  // Semaphore for locking against concurrent checkpoints
  struct mutex    checkpoint_mutex;

  //
  // List of buffer heads used by the checkpoint routine.  This was moved from jnl_log_do_checkpoint() to reduce stack usage
  // Access to this array is controlled by the checkpoint_mutex.  [checkpoint_mutex]
  //
  struct buffer_head  *chkpt_bhs[JNL_NR_BATCH];

  // Journal head: identifies the first unused block in the journal. [state_lock]
  unsigned long   head;

  // Journal tail: identifies the oldest still-used block in the journal. [state_lock]
  unsigned long   tail;

  // Journal free: how many free blocks are there in the journal? [state_lock]
  unsigned long   free;

  //
  // Journal start and end: the block numbers of the first usable block
  // and one beyond the last usable block in the journal. [state_lock]
  //
  unsigned long   first, last;

  // Device, blocksize and starting block offset for the location where we store the journal
  struct block_device *dev;
  unsigned long long  blk_offset;
  unsigned int        blocksize;
  unsigned char       blocksize_bits;
  unsigned char       blocksize_bits_9;
  char                devname[BDEVNAME_SIZE+24];

  // Device which holds the client fs.  For internal journal this will be equal to dev
  struct block_device *fs_dev;

  // Total maximum capacity of the journal region on disk
  unsigned int      maxlen;

  // Protects the buffer lists and internal buffer state
  spinlock_t        list_lock;

  // Optional inode where we store the journal.  If present, all journal block numbers are mapped into this inode via bmap()
//  struct inode    *inode;

  // Sequence number of the oldest transaction in the log [state_lock]
  unsigned int      tail_sequence;

  // Sequence number of the next transaction to grant [state_lock]
  unsigned int      tr_sequence;

  // Sequence number of the most recently committed transaction [state_lock]
  unsigned int      commit_sequence;

  // Sequence number of the most recent transaction wanting commit [state_lock]
  unsigned int      commit_request;

  // Pointer to the current commit thread for this journal
  struct task_struct  *task;

  // Maximum number of metadata buffers to allow in a single compound commit transaction
  int     max_transaction_buffers;

  // What is the maximum transaction lifetime before we begin a commit?
  unsigned long   commit_interval;

  // The timer used to wakeup the commit thread
  struct timer_list commit_timer;

  //
  // The revoke table: maintains the list of revoked blocks in the current transaction.  [revoke_lock]
  //
  spinlock_t    revoke_lock;
  jnl_revoke_table  *revoke;
  jnl_revoke_table  *revoke_table[2];

  // array of bhs for jnl_commit_transaction
  struct buffer_head  **wbuf;
  int     wbufsize;

  // this is the pid of the last person to run a synchronous operation through the journal
  pid_t   last_sync_writer;

  // the average amount of time in nanoseconds it takes to commit a transaction to disk. [state_lock]
  u64     average_commit_time;

  // minimum/maximum times that we should wait for additional filesystem operations to get batched into a synchronous handle in microseconds
  u32     min_batch_time;
  u32     max_batch_time;

  // This function is called when a transaction is closed
  void  (*commit_callback)( void  *sb, struct list_head *head );

  //
  // Journal statistics
  //
  spinlock_t        history_lock;
#ifdef USE_JNL_STATS
  transaction_stats stats;
#endif

  TRACE_ONLY( int max_credits_used; )

  // Failed journal commit ID
  unsigned int    failed_commit;

  //
  // An opaque pointer to fs-private information
  //
  void  *private;

};


// Comparison functions for transaction IDs: perform comparisons using modulo arithmetic so that they work over sequence number wraps
static inline int tid_gt( unsigned int x, unsigned int y )
{
  int difference = x - y;
  return difference > 0;
}

static inline int tid_geq( unsigned int x, unsigned int y )
{
  int difference = x - y;
  return difference >= 0;
}


static inline unsigned long
jnl_time_diff( unsigned long start, unsigned long end )
{
  if ( end >= start )
    return end - start;

  return end + ( MAX_JIFFY_OFFSET - start );
}

//=========================================================
//
//           Add missing functions
//
//=========================================================
#if !(defined HAVE_DECL_BLK_START_PLUG  && HAVE_DECL_BLK_START_PLUG)
  struct blk_plug{};
  static inline void blk_start_plug( struct blk_plug *plug ){}
  static inline void blk_finish_plug( struct blk_plug *plug ){}
#endif

#if defined HAVE_DECL_BLKDEV_ISSUE_FLUSH_V1 && HAVE_DECL_BLKDEV_ISSUE_FLUSH_V1
  #define Blkdev_issue_flush( d ) blkdev_issue_flush( (d), NULL )
#elif defined HAVE_DECL_BLKDEV_ISSUE_FLUSH_V2 && HAVE_DECL_BLKDEV_ISSUE_FLUSH_V2
  #define Blkdev_issue_flush( d ) blkdev_issue_flush( (d), GFP_KERNEL, NULL, 0 )
#elif defined HAVE_DECL_BLKDEV_ISSUE_FLUSH_V3 && HAVE_DECL_BLKDEV_ISSUE_FLUSH_V3
  #define Blkdev_issue_flush( d ) blkdev_issue_flush( (d), GFP_KERNEL, NULL )
#else
  #error "Unknown version of blkdev_issue_flush"
#endif

#if !(defined HAVE_DECL_WRITE_DIRTY_BUFFER && HAVE_DECL_WRITE_DIRTY_BUFFER)
static void write_dirty_buffer( struct buffer_head *bh, int rw )
{
  lock_buffer( bh );
  if ( !test_clear_buffer_dirty( bh ) )
    unlock_buffer( bh );
  else {
    bh->b_end_io = end_buffer_write_sync;
    get_bh( bh );
    submit_bh( rw, bh );
  }
}
#endif

#ifndef SLAB_TEMPORARY
  #define SLAB_TEMPORARY  0
#endif

#if defined HAVE_DECL_KMEM_CACHE_CREATE_V1 && HAVE_DECL_KMEM_CACHE_CREATE_V1
  #define Kmem_cache_create(n, s, a, f) kmem_cache_create( n, s, a, f, NULL, NULL )
#else
  #define Kmem_cache_create(n, s, a, f) kmem_cache_create( n, s, a, f, NULL )
#endif

#ifndef order_base_2
 #define order_base_2(n) ilog2(roundup_pow_of_two(n))
#endif

#ifdef need_lockbreak
  #define spin_needbreak(s) need_lockbreak(s)
#endif

#if !defined CONFIG_DEBUG_LOCK_ALLOC && !defined lock_map_release
  #define lock_map_acquire(l)                    do { } while (0)
  #define lock_map_release(l)                    do { } while (0)
#endif

//=========================================================
//
//           JNL API functions
//
//=========================================================


///////////////////////////////////////////////////////////
// jnl_is_aborted
//
//
///////////////////////////////////////////////////////////
static int
jnl_is_aborted(
    IN jnl *j
    )
{
  return j->flags & JNL_FLAGS_ABORT;
}


///////////////////////////////////////////////////////////
// jnl_is_handle_aborted
//
//
///////////////////////////////////////////////////////////
static int
jnl_is_handle_aborted(
    IN handle_j *handle
    )
{
  if ( handle->aborted )
    return 1;
  return jnl_is_aborted( handle->tr->journal );
}


///////////////////////////////////////////////////////////
// jnl_abort_handle
//
//
///////////////////////////////////////////////////////////
static void
jnl_abort_handle(
    handle_j *handle
    )
{
  handle->aborted = 1;
}


///////////////////////////////////////////////////////////
// jnl_space_needed
//
// Return the minimum number of blocks which must be free in the journal before a new transaction may be started
// Must be called under state_lock.
///////////////////////////////////////////////////////////
static inline int
jnl_space_needed(
    IN jnl *j
    )
{
  int nblocks = j->max_transaction_buffers;
  if ( NULL != j->committing_tr )
    nblocks += atomic_read( &j->committing_tr->outstanding_credits );
  return nblocks;
}



enum jnl_state_bits {
  BH_JNL = BH_PrivateStart+1, // Has an attached jnl_head
  BH_JWrite,                // Being written to log ( @@@ DEBUGGING )
  BH_Freed,                 // Has been freed ( truncated )
  BH_Revoked,               // Has been revoked from the log
  BH_RevokeValid,           // Revoked flag is valid
  BH_JNLDirty,              // Is dirty but journaled
  BH_State,                 // Pins most jnl_head state
  BH_JournalHead,           // Pins bh->b_private and jh->bh
  BH_Unshadow,              // Dummy bit, for BJ_Shadow wakeup filtering
  BH_JNLPrivateStart,       // First bit available for private use by FS
};

BUFFER_FNS( JNL, jnl )
BUFFER_FNS( JWrite, jwrite )
BUFFER_FNS( JNLDirty, jnldirty )
TAS_BUFFER_FNS( JNLDirty, jnldirty )
BUFFER_FNS( Revoked, revoked )
TAS_BUFFER_FNS( Revoked, revoked )
BUFFER_FNS( RevokeValid, revokevalid )
TAS_BUFFER_FNS( RevokeValid, revokevalid )
BUFFER_FNS( Freed, freed )

static inline struct buffer_head *jh2bh( jnl_head *jh )
{
  return jh->bh;
}

static inline jnl_head *bh2jh( struct buffer_head *bh )
{
  return bh->b_private;
}

static inline void jnl_lock_bh_state( struct buffer_head *bh )
{
  bit_spin_lock( BH_State, &bh->b_state );
}

static inline int jnl_trylock_bh_state( struct buffer_head *bh )
{
  return bit_spin_trylock( BH_State, &bh->b_state );
}

static inline int jnl_is_locked_bh_state( struct buffer_head *bh )
{
  return bit_spin_is_locked( BH_State, &bh->b_state );
}

static inline void jnl_unlock_bh_state( struct buffer_head *bh )
{
  bit_spin_unlock( BH_State, &bh->b_state );
}

static inline void jnl_lock_bh_hdr( struct buffer_head *bh )
{
  bit_spin_lock( BH_JournalHead, &bh->b_state );
}

static inline void jnl_unlock_bh_hdr( struct buffer_head *bh )
{
  bit_spin_unlock( BH_JournalHead, &bh->b_state );
}


///////////////////////////////////////////////////////////
// CalcCheckSumEx
//
//
///////////////////////////////////////////////////////////
static inline unsigned int
CalcCheckSumEx(
    IN const void   *ptr,
    IN size_t       len,
    IN unsigned int cksum
    )
{
  size_t i;
  const unsigned char *p = ptr;

  // this is a lame checksum but for now it'll do
  for( i = 0; i < len; i++, p++ )
    cksum = ( cksum << 8 ) ^ ( cksum + *p );

  return cksum;
}


///////////////////////////////////////////////////////////
// CalcCheckSum
//
//
///////////////////////////////////////////////////////////
static inline unsigned int
CalcCheckSum(
    IN const void *ptr,
    IN size_t     len
    )
{
  return ~CalcCheckSumEx( ptr, len, 0 );
}


///////////////////////////////////////////////////////////
// __jnl_log_start_commit
//
// Called with state_lock locked for writing
// Returns true if a transaction commit was started
///////////////////////////////////////////////////////////
static int
__jnl_log_start_commit(
    IN jnl *j,
    IN unsigned int target
    )
{
  //
  // The only transaction we can possibly wait upon is the currently running transaction ( if it exists )
  // Otherwise, the target tid must be an old one
  //
  if ( NULL != j->running_tr && j->running_tr->tid == target ) {
    //
    // We want a new commit: OK, mark the request and wakeup the commit thread.  We do _not_ do the commit ourselves.
    //
    j->commit_request = target;
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "JNL(%s): commit %x requesting %x\n", current->comm, j->commit_sequence, j->commit_request );
    wake_up( &j->wait_commit );
    return 1;
  }

  if ( !tid_geq( j->commit_request, target ) ) {
#ifdef WARN_ONCE
    // This should never happen, but if it does, preserve the evidence before kjournald goes into a loop and
    // increments commit_sequence beyond all recognition
    WARN_ONCE( 1, "jnl: bad log_start_commit: %x %x %x %x\n", j->commit_sequence, j->commit_request, target, j->running_tr? j->running_tr->tid : 0 );
#else
    static int __warned;
    if ( !__warned ) {
      printk( KERN_WARNING "WARNING: jnl: bad log_start_commit: %x %x %x %x: %pS()\n",
              j->commit_sequence, j->commit_request, target, j->running_tr? j->running_tr->tid : 0 , __builtin_return_address(0) );
      __warned = 1;
    }
#endif
  }
  return 0;
}


///////////////////////////////////////////////////////////
// jnl_log_start_commit
//
//
///////////////////////////////////////////////////////////
static int
jnl_log_start_commit(
    IN jnl *j,
    IN unsigned int tid
    )
{
  int ret;
  write_lock( &j->state_lock );
  ret = __jnl_log_start_commit( j, tid );
  write_unlock( &j->state_lock );
  return ret;
}


///////////////////////////////////////////////////////////
// jnl_log_space_left
//
// Return the number of free blocks left in the journal
///////////////////////////////////////////////////////////
static int
jnl_log_space_left(
    IN jnl *j
    )
{
  int left = j->free;

  // assert_spin_locked( &j->state_lock );

  //
  // Be pessimistic here about the number of those free blocks which might be required for log blhdr control blocks
  //
#define MIN_LOG_RESERVED_BLOCKS 32 // Allow for rounding errors
  left -= MIN_LOG_RESERVED_BLOCKS;
  if ( left <= 0 )
    return 0;
  left -= ( left >> 3 );
  return left;
}


///////////////////////////////////////////////////////////
// jnl_update_hdr
//
//
///////////////////////////////////////////////////////////
static void
jnl_update_hdr(
    IN jnl *j,
    IN int wait
    )
{
  jnl_header  *hdr       = j->hdr;
  struct buffer_head *bh = j->hdr_buffer;

  //
  // As a special case, if the on-disk copy is already marked as needing no recovery and there are no outstanding transactions
  // in the filesystem, then we can safely defer the header update until the next commit by setting JNL_FLAGS_FLUSHED
  // This avoids attempting a write to a potential-readonly device.
  //
  if ( hdr->start == hdr->end && j->tail_sequence == j->tr_sequence ) {
    jnl_debug( JNL_TRACE_LEVEL_HDR, "jnl: Skipping header update\n" );
    goto out;
  }

  if ( buffer_write_io_error( bh ) ) {
    //
    // Oh, dear.  A previous attempt to write the journal header failed.  This could happen because the
    // USB device was yanked out.  Or it could happen to be a transient write error and maybe the block will
    // be remapped.  Nothing we can do but to retry the write and hope for the best.
    //
    printk( KERN_ERR "jnl: previous I/O error detected for journal header update for %s.\n", j->devname );
    clear_buffer_write_io_error( bh );
  }

  read_lock( &j->state_lock );
  hdr->sequence = j->tail_sequence;
//  hdr->start = hdr->end = ( j->tail - bh->b_blocknr ) << j->blocksize_bits;
  if ( 0 == j->tail ) {
    jnl_debug( JNL_TRACE_LEVEL_HDR, "JNL updating header: \"empty\", seq %x, errno %d\n", j->tail_sequence, j->errno );
    hdr->start  = hdr->end  = j->blocksize;
  } else {
    hdr->end    = ( j->head - bh->b_blocknr ) << j->blocksize_bits;
    hdr->start  = ( j->tail - bh->b_blocknr ) << j->blocksize_bits;
    jnl_debug( JNL_TRACE_LEVEL_HDR, "JNL updating header: [%llx - %llx), seq %x, errno %d\n", hdr->start, hdr->end, j->tail_sequence, j->errno );
  }
  read_unlock( &j->state_lock );

  hdr->checksum = 0;
  hdr->checksum = CalcCheckSum( hdr, JOURNAL_HEADER_CKSUM_SIZE );

  set_buffer_uptodate( bh );
  mark_buffer_dirty( bh );
  if ( !wait )
    write_dirty_buffer( bh, WRITE );
  else {
    sync_dirty_buffer( bh );
    if ( buffer_write_io_error( bh ) ) {
      printk( KERN_ERR "jnl: I/O error detected when updating journal header for %s.\n", j->devname );
      clear_buffer_write_io_error( bh );
      set_buffer_uptodate( bh );
    }
  }

out:
  //
  // If we have just flushed the log ( by marking start==end ), then any future commit will have to be careful to update the
  // header again to re-record the true start of the log
  //
  write_lock( &j->state_lock );
  if ( hdr->start == hdr->end )
    j->flags |= JNL_FLAGS_FLUSHED;
  else
    j->flags &= ~JNL_FLAGS_FLUSHED;
  write_unlock( &j->state_lock );
}


///////////////////////////////////////////////////////////
// jnl_cleanup_tail
//
// Check the list of checkpoint transactions for the journal to see if we have already got rid of any since the last update of the log tail
// in the journal header.  If so, we can instantly roll the header forward to remove those transactions from the log.
//
// Return <0 on error, 0 on success, 1 if there was nothing to clean up.
//
// Called with the journal lock held.
//
// This is the only part of the journaling code which really needs to be aware of transaction aborts
// Checkpointing involves writing to the main filesystem area rather than to the journal, so it can proceed even in abort state,
// but we must not update the super block if checkpointing may have failed
// Otherwise, we would lose some metadata buffers which should be written-back to the filesystem
///////////////////////////////////////////////////////////
static int
jnl_cleanup_tail(
    IN jnl *j
    )
{
  transaction *tr;
  unsigned int  first_tid;
  unsigned long blocknr, freed;
  TRACE_ONLY( const char *hint; )

  if ( jnl_is_aborted( j ) )
    return 1;

  //
  // OK, work out the oldest transaction remaining in the log, and the log block it starts at.
  //
  // If the log is now empty, we need to work out which is the next transaction ID we will write, and where it will start
  //
  write_lock( &j->state_lock );
  spin_lock( &j->list_lock );
  tr = j->checkpoint_tr;
  if ( NULL != tr ) {
    first_tid = tr->tid;
    blocknr   = tr->log_start;
    TRACE_ONLY( hint = "1"; )
  } else if ( NULL != ( tr = j->committing_tr ) ) {
    first_tid = tr->tid;
    blocknr   = tr->log_start;
    TRACE_ONLY( hint = "2"; )
  } else if ( NULL != ( tr = j->running_tr ) ) {
    first_tid = tr->tid;
    blocknr   = j->head;
    TRACE_ONLY( hint = "3"; )
  } else {
    first_tid = j->tr_sequence;
    blocknr   = j->head;
    TRACE_ONLY( hint = "4"; )
  }
  spin_unlock( &j->list_lock );

  jnl_debug( JNL_TRACE_LEVEL_CHECK, "jnl_cleanup_tail(%s): tr=[%x %x) block=%lx\n", hint, j->tail_sequence, first_tid, blocknr );

  J_ASSERT( blocknr != 0 );
  // If the oldest pinned transaction is at the tail of the log already then there's not much we can do right now
  if ( j->tail_sequence == first_tid ) {
    write_unlock( &j->state_lock );
    return 1;
  }

  // OK, update the header to recover the freed space
  // Physical blocks come first: have we wrapped beyond the end of the log?
  freed = blocknr - j->tail;
  if ( blocknr < j->tail )
    freed = freed + j->last - j->first;
//  trace_jnl_cleanup_tail( journal, first_tid, blocknr, freed );
  jnl_debug( JNL_TRACE_LEVEL_CHECK, "Cleaning journal tail tr=[%x %x) blocks=[%lx %lx) freeing %lx\n", j->tail_sequence, first_tid, blocknr, blocknr + freed, freed );

  j->free += freed;
  j->tail_sequence = first_tid;
  j->tail = blocknr;
  write_unlock( &j->state_lock );

  //
  // If there is an external journal, we need to make sure that any data blocks that were recently written out --- perhaps
  // by jnl_log_do_checkpoint() --- are flushed out before we drop the transactions from the external journal
  // It's unlikely this will be necessary, especially with a appropriately sized journal, but we need this to guarantee correctness
  // Fortunately jnl_cleanup_tail() doesn't get called all that often
  //
  if ( j->fs_dev != j->dev && ( j->flags & JNL_FLAGS_BARRIER ) )
    Blkdev_issue_flush( j->fs_dev );

  if ( !( j->flags & JNL_FLAGS_ABORT ) )
    jnl_update_hdr( j, 1 );

  return 0;
}


///////////////////////////////////////////////////////////
// jnl_sync_bh
//
// We were unable to perform jnl_trylock_bh_state() inside list_lock.
// The caller must restart a list walk.  Wait for someone else to run jnl_unlock_bh_state()
///////////////////////////////////////////////////////////
static void
jnl_sync_bh(
    IN jnl *j,
    IN struct buffer_head *bh
    ) __releases( j->list_lock )
{
  get_bh( bh );
  spin_unlock( &j->list_lock );
  jnl_lock_bh_state( bh );
  jnl_unlock_bh_state( bh );
  put_bh( bh );
}


///////////////////////////////////////////////////////////
// jnl_log_wait_commit
//
// Wait for a specified commit to complete. The caller may not hold the journal lock
///////////////////////////////////////////////////////////
static int
jnl_log_wait_commit(
    IN jnl *j,
    IN unsigned int tid
    )
{
  int err = 0;
  read_lock( &j->state_lock );
  while ( tid_gt( tid, j->commit_sequence ) ) {
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: wait [%x, %x)\n", j->commit_sequence, tid );
    wake_up( &j->wait_commit );
    read_unlock( &j->state_lock );
    wait_event( j->wait_done_commit, !tid_gt( tid, j->commit_sequence ) );
    read_lock( &j->state_lock );
  }
  read_unlock( &j->state_lock );

  if ( unlikely( jnl_is_aborted( j ) ) ) {
    printk( KERN_EMERG "journal commit I/O error\n" );
    err = -EIO;
  }

  return err;
}


///////////////////////////////////////////////////////////
// __buffer_unlink_first
//
// Unlink a buffer from a transaction checkpoint list
///////////////////////////////////////////////////////////
static inline void
__buffer_unlink_first(
    IN jnl_head *jh
    )
{
  transaction *tr = jh->cp_tr;
  jh->cpnext->cpprev = jh->cpprev;
  jh->cpprev->cpnext = jh->cpnext;
  if ( tr->checkpoint_list == jh ) {
    tr->checkpoint_list = jh->cpnext;
    if ( tr->checkpoint_list == jh )
      tr->checkpoint_list = NULL;
  }
}


///////////////////////////////////////////////////////////
// __buffer_unlink
//
// Unlink a buffer from a transaction checkpoint( io ) list
///////////////////////////////////////////////////////////
static inline void
__buffer_unlink(
    IN jnl_head *jh
    )
{
  transaction *tr = jh->cp_tr;
  __buffer_unlink_first( jh );
  if ( tr->checkpoint_io_list == jh ) {
    tr->checkpoint_io_list = jh->cpnext;
    if ( tr->checkpoint_io_list == jh )
      tr->checkpoint_io_list = NULL;
  }
}



// JNL memory management
//
// These functions are used to allocate block-sized chunks of memory used for making copies of buffer_head data
// Very often it will be page-sized chunks of data, but sometimes it will be in sub-page-size chunks
// ( For example, 16k pages on Power systems with a 4k block file system. )
// For blocks smaller than a page, we use a SLAB allocator
// There are slab caches for each block size, which are allocated at mount time, if necessary, and we only
// free ( all of ) the slab caches when/if the jnl_ module is unloaded
// For this reason we don't need to a mutex to protect access to jnl_slab[] allocating or releasing memory; only in jnl_create_slab().


///////////////////////////////////////////////////////////
// get_slab
//
//
///////////////////////////////////////////////////////////
static struct kmem_cache*
get_slab(
    IN size_t size
    )
{
  int i = order_base_2( size ) - 9;
  BUG_ON( i >= JNL_MAX_SLABS );
  if ( unlikely( i < 0 ) )
    i = 0;
  BUG_ON( NULL == jnl_slab[i] );
  return jnl_slab[i];
}


///////////////////////////////////////////////////////////
// jnl_alloc
//
//
///////////////////////////////////////////////////////////
static void*
jnl_alloc(
    IN size_t size,
    IN gfp_t  flags
    )
{
  void *ptr;

  BUG_ON( size & ( size-1 ) ); // Must be a power of 2

  flags |= __GFP_REPEAT;
  if ( PAGE_SIZE == size )
    ptr = (void*)__get_free_pages( flags, 0 );
  else if ( size > PAGE_SIZE ) {
    int order = get_order( size );
    if ( order < 3 )
      ptr = (void*)__get_free_pages( flags, order );
    else
      ptr = vmalloc( size );
  } else
    ptr = kmem_cache_alloc( get_slab( size ), flags );

  // Check alignment; SLUB has gotten this wrong in the past, and this can lead to user data corruption!
  BUG_ON( ( ( unsigned long ) ptr ) & ( size-1 ) );
  return ptr;
}


///////////////////////////////////////////////////////////
// jnl_free
//
//
///////////////////////////////////////////////////////////
static void
jnl_free(
    IN void *ptr,
    IN size_t size
    )
{
  if ( PAGE_SIZE == size )
    free_pages( ( unsigned long )ptr, 0 );
  else if ( size > PAGE_SIZE ) {
    int order = get_order( size );
    if ( order < 3 )
      free_pages( ( unsigned long )ptr, order );
    else
      vfree( ptr );
  } else {
    kmem_cache_free( get_slab( size ), ptr );
  }
}


///////////////////////////////////////////////////////////
// __jnl_drop_transaction
//
// We've finished with this transaction structure: adios...
//
// The transaction must have no links except for the checkpoint by this point.
//
// Called with the journal locked.
// Called with list_lock held.
///////////////////////////////////////////////////////////
static void
__jnl_drop_transaction(
    IN jnl *j,
    IN transaction *tr
    )
{
  assert_spin_locked( &j->list_lock );
  if ( tr->cpnext ) {
    tr->cpnext->cpprev = tr->cpprev;
    tr->cpprev->cpnext = tr->cpnext;
    if ( j->checkpoint_tr == tr )
      j->checkpoint_tr = tr->cpnext;
    if ( j->checkpoint_tr == tr )
      j->checkpoint_tr = NULL;
  }

  J_ASSERT( tr->state == T_FINISHED );
  J_ASSERT( tr->buffers == NULL );
  J_ASSERT( tr->forget == NULL );
  J_ASSERT( tr->iobuf_list == NULL );
  J_ASSERT( tr->shadow_list == NULL );
  J_ASSERT( tr->log_list == NULL );
  J_ASSERT( tr->checkpoint_list == NULL );
  J_ASSERT( tr->checkpoint_io_list == NULL );
  J_ASSERT( atomic_read( &tr->updates ) == 0 );
  J_ASSERT( j->committing_tr != tr );
  J_ASSERT( j->running_tr != tr );

  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "Dropping transaction %x [%lx %lx)\n", tr->tid, tr->log_start, tr->log_end );
}


//
// A jnl_head is attached to a buffer_head whenever JNL has an interest in the buffer.
//
// Whenever a buffer has an attached jnl_head, its ->state:BH_JNL bit is set
// This bit is tested in core kernel code where we need to take JNL-specific actions
// Testing the zeroness of ->b_private is not reliable there.
//
// When a buffer has its BH_JNL bit set, its ->count is elevated by one.
//
// When a buffer has its BH_JNL bit set it is immune from being released by core kernel code, mainly via ->b_count.
//
// A jnl_head is detached from its buffer_head when the jnl_head's jcount reaches zero
// Running transaction and checkpoint transaction hold their references to jcount.
//
// Various places in the kernel want to attach a jnl_head to a buffer_head _before_ attaching the jnl_head to a transaction
// To protect the jnl_head in this situation, jnl_add_hdr elevates the jnl_head's jcount refcount by one
// The caller must call jnl_put_hdr() to undo this
//
// So the typical usage would be:
//
//       ( Attach a jnl_head if needed.  Increments jcount )
//       jnl_head *jh = jnl_add_hdr( bh );
//       ...
//       ( Get another reference for transaction )
//       jnl_grab_hdr( bh );
//       jh->tr = xxx;
//       ( Put original reference )
//       jnl_put_hdr( jh );


///////////////////////////////////////////////////////////
// jnl_add_hdr
//
// Give a buffer_head a jnl_head.
///////////////////////////////////////////////////////////
static jnl_head*
jnl_add_hdr(
    IN struct buffer_head *bh
    )
{
  jnl_head *jh;
  jnl_head *new_jh = NULL;

repeat:
  if ( !buffer_jnl( bh ) ) {
    new_jh = kmem_cache_allocZ( jnl_head_cache );
    if ( NULL == new_jh ) {
      jnl_debug( 0, "out of memory for jnl_head\n" );
//      pr_notice_ratelimited( "ENOMEM in %s, retrying.\n", __func__ );
      while ( NULL == new_jh ) {
        yield();
        new_jh = kmem_cache_allocZ( jnl_head_cache );
      }
    }
  }

  jnl_lock_bh_hdr( bh );
  if ( buffer_jnl( bh ) ) {
    jh = bh2jh( bh );
  } else {
    J_ASSERT( ( atomic_read( &bh->b_count ) > 0 ) || ( bh->b_page && bh->b_page->mapping ) );
    if ( !new_jh ) {
      jnl_unlock_bh_hdr( bh );
      goto repeat;
    }

    jh      = new_jh;
    new_jh  = NULL;          // We consumed it
    set_buffer_jnl( bh );
    bh->b_private = jh;
    jh->bh      = bh;
    get_bh( bh );
    jnl_debug( JNL_TRACE_LEVEL_JHDR, "b=%"PSCT"x + jnl_head\n", bh->b_blocknr );
  }

  jh->jcount++;
  jnl_unlock_bh_hdr( bh );
  if ( new_jh )
    kmem_cache_free( jnl_head_cache, new_jh );
  return bh->b_private;
}


///////////////////////////////////////////////////////////
// jnl_put_hdr
//
// Drop a reference on the passed jnl_head
// If it fell to zero then release the jnl_head from the buffer_head
///////////////////////////////////////////////////////////
static void
jnl_put_hdr(
    IN jnl_head *jh
    )
{
  struct buffer_head *bh = jh2bh( jh );

  jnl_lock_bh_hdr( bh );
  J_ASSERT( jh->jcount > 0 );
  jh->jcount -= 1;
  if ( 0 != jh->jcount )
    jnl_unlock_bh_hdr( bh );
  else {
    J_ASSERT( NULL == jh->tr );
    J_ASSERT( NULL == jh->next_tr );
    J_ASSERT( NULL == jh->cp_tr );
    J_ASSERT( BJ_None == jh->jlist );
    J_ASSERT( buffer_jnl( bh ) );
    J_ASSERT( jh2bh( jh ) == bh );
    jnl_debug( JNL_TRACE_LEVEL_JHDR, "b=%"PSCT"x - jnl_hdr\n", bh->b_blocknr );
    if ( jh->frozen_data ) {
      printk( KERN_WARNING "freeing frozen_data\n" );
      jnl_free( jh->frozen_data, bh->b_size ); // TODO: May be oops if bh is a tail buffer
    }
    if ( jh->committed_data ) {
      printk( KERN_WARNING "freeing committed_data\n" );
      jnl_free( jh->committed_data, bh->b_size );
    }
    bh->b_private = NULL;
    jh->bh = NULL;        // debug, really
    clear_buffer_jnl( bh );
    kmem_cache_free( jnl_head_cache, jh );

    jnl_unlock_bh_hdr( bh );
    __brelse( bh );
  }
}


///////////////////////////////////////////////////////////
// jnl_grab_hdr
//
// Grab a ref against this buffer_head's jnl_head
// If it ended up not having a jnl_head, return NULL
///////////////////////////////////////////////////////////
static jnl_head*
jnl_grab_hdr(
    IN struct buffer_head *bh
    )
{
  jnl_head *jh = NULL;
  jnl_lock_bh_hdr( bh );
  if ( buffer_jnl( bh ) ) {
    jh = bh2jh( bh );
    jh->jcount++;
  }
  jnl_unlock_bh_hdr( bh );
  return jh;
}


///////////////////////////////////////////////////////////
// jnl_remove_checkpoint
//
// called after a buffer has been committed to disk ( either by being write-back flushed to disk, or being committed to the log )
//
// We cannot safely clean a transaction out of the log until all of the buffer updates committed in that transaction have safely been stored
// elsewhere on disk.  To achieve this, all of the buffers in a  transaction need to be maintained on the transaction's checkpoint
// lists until they have been rewritten, at which point this function is called to remove the buffer from the existing transaction's
// checkpoint lists.
//
// The function returns 1 if it frees the transaction, 0 otherwise.
//
// This function is called with list_lock held.
// This function is called with jnl_lock_bh_state( jh2bh( jh ) )
///////////////////////////////////////////////////////////
static int
jnl_remove_checkpoint(
    IN jnl_head *jh
    )
{
  transaction *tr = jh->cp_tr;
  jnl *j;

  if ( NULL == tr ) {
    jnl_debug( JNL_TRACE_LEVEL_CHECK, "remove_check_point: not on transaction ***\n" );
    return 0;
  }

  j = tr->journal;

  jnl_debug( JNL_TRACE_LEVEL_CHECK|JNL_TRACE_LEVEL_COMMIT, "remove_check_point: b=%"PSCT"x from tr=%x\n", jh2bh(jh)->b_blocknr, tr->tid );
  __buffer_unlink( jh );
  jh->cp_tr = NULL;
  jnl_put_hdr( jh );

  if ( NULL != tr->checkpoint_list || NULL != tr->checkpoint_io_list )
    return 0;

  //
  // There is one special case to worry about: if we have just pulled the buffer off a running or committing transaction's checkpoing list,
  // then even if the checkpoint list is empty, the transaction obviously cannot be dropped!
  //
  // The locking here around t_state is a bit sleazy. See the comment at the end of jnl_commit_transaction().
  //
  if ( T_FINISHED != tr->state )
    return 0;

#ifdef USE_JNL_STATS
  // OK, that was the last buffer for the transaction: we can now safely remove this transaction from the log
  if ( tr->chp_stats.chp_time )
    tr->chp_stats.chp_time = jnl_time_diff( tr->chp_stats.chp_time, jiffies );
#endif

  __jnl_drop_transaction( j, tr );
  kfree( tr );

  // Just in case anybody was waiting for more transactions to be checkpointed...
  wake_up( &j->wait_logspace );

  return 1;
}


///////////////////////////////////////////////////////////
// __buffer_relink_io
//
// Move a buffer from the checkpoint list to the checkpoint io list
//
// Called with list_lock held
///////////////////////////////////////////////////////////
static inline void
__buffer_relink_io(
    IN jnl_head *jh
    )
{
  transaction *tr = jh->cp_tr;

  __buffer_unlink_first( jh );
  if ( !tr->checkpoint_io_list ) {
    jh->cpnext = jh->cpprev = jh;
  } else {
    jh->cpnext = tr->checkpoint_io_list;
    jh->cpprev = tr->checkpoint_io_list->cpprev;
    jh->cpprev->cpnext = jh;
    jh->cpnext->cpprev = jh;
  }
  tr->checkpoint_io_list = jh;
}


///////////////////////////////////////////////////////////
// jnl_flush_batch
//
//
///////////////////////////////////////////////////////////
static void
jnl_flush_batch(
    IN jnl *j,
    IN OUT int *batch_count
    )
{
  int i, count = *batch_count;
  struct blk_plug plug;
  blk_start_plug( &plug );

  for ( i = 0; i < count; i++ )
    write_dirty_buffer( j->chkpt_bhs[i], WRITE_SYNC );

  blk_finish_plug( &plug );

  for ( i = 0; i < count; i++ ) {
    struct buffer_head *bh = j->chkpt_bhs[i];
    clear_buffer_jwrite( bh );
    jnl_debug( JNL_TRACE_LEVEL_IO, "brelse b=%"PSCT"x\n", bh->b_blocknr );
    __brelse( bh );
  }
  *batch_count = 0;
}


///////////////////////////////////////////////////////////
// jnl_process_buffer
//
// Try to flush one buffer from the checkpoint list to disk.
//
// Return 1 if something happened which requires us to abort the current scan of the checkpoint list
// Return <0 if the buffer has failed to be written out.
//
// Called with list_lock held and drops it if 1 is returned
// Called under jnl_lock_bh_state( jh2bh( jh ) ), and drops it
///////////////////////////////////////////////////////////
static int
jnl_process_buffer(
    IN jnl *j,
    IN jnl_head *jh,
    OUT int *batch_count,
    IN transaction *tr
    )
{
  struct buffer_head *bh = jh2bh( jh );
  int ret = 1;

  if ( buffer_locked( bh ) ) {
    get_bh( bh );
    spin_unlock( &j->list_lock );
    jnl_unlock_bh_state( bh );
    wait_on_buffer( bh );
    // the jnl_head may have gone by now
    jnl_debug( JNL_TRACE_LEVEL_IO, "brelse b=%"PSCT"x\n", bh->b_blocknr );
    __brelse( bh );
  } else if ( NULL != jh->tr ) {
    transaction *t    = jh->tr;
    unsigned int tid  = t->tid;

#ifdef USE_JNL_STATS
    tr->chp_stats.forced_to_close++;
#endif
    spin_unlock( &j->list_lock );
    jnl_unlock_bh_state( bh );
    if ( unlikely( j->flags & JNL_FLAGS_UNMOUNT ) ) {
      //
      // The journal thread is dead; so starting and waiting for a commit to finish will cause
      // us to wait for a _very_ long time
      //
      printk( KERN_ERR "jnl: %s: Waiting for Godot: b=%"PSCT"x\n", j->devname, bh->b_blocknr );
    }
    jnl_log_start_commit( j, tid );
    jnl_log_wait_commit( j, tid );
  } else if ( !buffer_dirty( bh ) ) {
    if ( unlikely( buffer_write_io_error( bh ) ) )
      ret = -EIO;
    get_bh( bh );
    J_ASSERT( !buffer_jnldirty( bh ) );
    jnl_debug( JNL_TRACE_LEVEL_IO, "remove from checkpoint\n" );
    jnl_remove_checkpoint( jh );
    spin_unlock( &j->list_lock );
    jnl_unlock_bh_state( bh );
    __brelse( bh );
  } else {
    //
    // Important: we are about to write the buffer, and possibly block, while still holding the journal lock.
    // We cannot afford to let the transaction logic start messing around with this buffer before we write it to
    // disk, as that would break recoverability.
    //
    jnl_debug( JNL_TRACE_LEVEL_IO, "queue\n" );
    get_bh( bh );
    J_ASSERT( !buffer_jwrite( bh ) );
    set_buffer_jwrite( bh );
    j->chkpt_bhs[*batch_count] = bh;
    __buffer_relink_io( jh );
    jnl_unlock_bh_state( bh );
#ifdef USE_JNL_STATS
    tr->chp_stats.written++;
#endif
    ( *batch_count )++;
    if ( JNL_NR_BATCH == *batch_count ) {
      spin_unlock( &j->list_lock );
      jnl_flush_batch( j, batch_count );
    } else {
      ret = 0;
    }
  }

  return ret;
}


///////////////////////////////////////////////////////////
// __jnl_abort_hard
//
// Quick version for internal journal use ( doesn't lock the journal ).
// Aborts hard --- we mark the abort as occurred, but do _nothing_ else,
// and don't attempt to make any other journal updates.
///////////////////////////////////////////////////////////
static void
__jnl_abort_hard(
    IN jnl *j
    )
{
  transaction *tr;

  if ( j->flags & JNL_FLAGS_ABORT )
    return;

  printk( KERN_ERR "Aborting journal on device %s.\n", j->devname );

  write_lock( &j->state_lock );
  j->flags |= JNL_FLAGS_ABORT;
  tr = j->running_tr;
  if ( NULL != tr )
    __jnl_log_start_commit( j, tr->tid );
  write_unlock( &j->state_lock );
}


///////////////////////////////////////////////////////////
// jnl_abort
//
//
///////////////////////////////////////////////////////////
static void
jnl_abort(
    IN jnl *j,
    IN int errno
    )
{
  if ( j->flags & JNL_FLAGS_ABORT )
    return;

  if ( !j->errno )
    j->errno = errno;

  __jnl_abort_hard( j );

  if ( errno )
    jnl_update_hdr( j, 1 );
}


///////////////////////////////////////////////////////////
// jnl_log_do_checkpoint
//
// We take the first transaction on the list of transactions to be checkpointed and send all its buffers to disk
// We submit larger chunks of data at once.
//
// The journal should be locked before calling this function
// Called with checkpoint_mutex held.
///////////////////////////////////////////////////////////
static int
jnl_log_do_checkpoint(
    IN jnl *j
    )
{
  transaction *tr;
  unsigned int this_tid;
  int result;

  jnl_debug( JNL_TRACE_LEVEL_CHECK, "Start checkpoint\n" );

  //
  // First thing: if there are any transactions in the log which don't need checkpointing, just eliminate them from the journal straight away.
  //
  result = jnl_cleanup_tail( j );
//  trace_jnl__checkpoint( journal, result );
//  jnl_debug( 1, "jnl_cleanup_tail returned %d\n", result );
  if ( result <= 0 )
    return result;

  //
  // OK, we need to start writing disk blocks.  Take one transaction and write it.
  //
  result = 0;
  spin_lock( &j->list_lock );

  tr = j->checkpoint_tr;

  if ( NULL == tr )
    goto out;

#ifdef USE_JNL_STATS
  if ( 0 == tr->chp_stats.chp_time )
    tr->chp_stats.chp_time = jiffies;
#endif
  this_tid = tr->tid;
restart:
  //
  // If someone cleaned up this transaction while we slept, we're done ( maybe it's a new transaction, but it fell at the same address ).
  //
  if ( j->checkpoint_tr == tr && tr->tid == this_tid ) {
    int batch_count = 0;
    int retry = 0, err;

    while ( !retry && tr->checkpoint_list ) {
      jnl_head *jh = tr->checkpoint_list;
      struct buffer_head *bh = jh2bh( jh );

      if ( !jnl_trylock_bh_state( bh ) ) {
        jnl_sync_bh( j, bh );
        retry = 1;
        break;
      }

      retry = jnl_process_buffer( j, jh, &batch_count, tr );
      if ( retry < 0 && !result )
        result = retry;
      if ( !retry && ( need_resched() || spin_needbreak( &j->list_lock ) ) ) {
        spin_unlock( &j->list_lock );
        retry = 1;
        break;
      }
    }

    if ( batch_count ) {
      if ( !retry ) {
        spin_unlock( &j->list_lock );
        retry = 1;
      }

      jnl_flush_batch( j, &batch_count );
    }

    if ( retry ) {
      spin_lock( &j->list_lock );
      goto restart;
    }

    //
    // Now we have cleaned up the first transaction's checkpoint list. Let's clean up the second one
    //
    // Clean up transaction's list of buffers submitted for io.
    // Wait for any pending IO to complete and remove any clean buffers.
    // Note that we take the buffers in the opposite ordering from the one in which they were submitted for IO.
    //
    err = 0;

restart_cp:
    // Did somebody clean up the transaction in the meanwhile?
    if ( j->checkpoint_tr == tr && tr->tid == this_tid ) {
      while ( tr->checkpoint_io_list ) {
        int released;
        jnl_head *jh = tr->checkpoint_io_list;
        struct buffer_head *bh = jh2bh( jh );
        if ( !jnl_trylock_bh_state( bh ) ) {
          jnl_sync_bh( j, bh );
          spin_lock( &j->list_lock );
          goto restart_cp;
        }

        get_bh( bh );
        if ( buffer_locked( bh ) ) {
          spin_unlock( &j->list_lock );
          jnl_unlock_bh_state( bh );
          wait_on_buffer( bh );
          // the jnl_head may have gone by now
          jnl_debug( JNL_TRACE_LEVEL_IO, "brelse b=%"PSCT"x\n", bh->b_blocknr );
          __brelse( bh );
          spin_lock( &j->list_lock );
          goto restart_cp;
        }

        if ( unlikely( buffer_write_io_error( bh ) ) )
          err = -EIO;

        //
        // Now in whatever state the buffer currently is, we know that it has been written out and so we can drop it from the list
        //
        released = jnl_remove_checkpoint( jh );
        jnl_unlock_bh_state( bh );
        jnl_debug( JNL_TRACE_LEVEL_IO, "brelse b=%"PSCT"x\n", bh->b_blocknr );
        __brelse( bh );
        if ( released )
          break;
      }
    }

    if ( !result )
      result = err;
  }
out:

  spin_unlock( &j->list_lock );
  if ( result < 0 )
    jnl_abort( j, result );
  else
    result = jnl_cleanup_tail( j );

  return result < 0 ? result : 0;
}

//static inline void update_t_max_wait( transaction *tr, unsigned long ts ){}

//
// Handle management.
//
// A handle_j is an object which represents a single atomic update to a filesystem, and which tracks
// all of the modifications which form part of that one update
//


///////////////////////////////////////////////////////////
// jnl_start_handle
//
// Given a handle, deal with any locking or stalling needed to make sure that there is enough journal space for the handle to begin
// Attach the handle to a transaction and set up the transaction's buffer credits.
///////////////////////////////////////////////////////////
static int
jnl_start_handle(
    IN jnl *j,
    IN handle_j *handle
    )
{
  transaction *tr, *new_tr = NULL;
  unsigned int  tid;
  int needed, need_to_start;
  int nblocks = handle->buffer_credits;
//  unsigned long ts = jiffies;
  TRACE_ONLY( const char *hint = ""; )
  int space_need, space_left;

  if ( nblocks > j->max_transaction_buffers ) {
    printk( KERN_ERR "jnl: %s wants too many credits ( %d > %d )\n", current->comm, nblocks, j->max_transaction_buffers );
    return -ENOSPC;
  }

alloc_transaction:
  if ( !j->running_tr ) {
    new_tr = kzalloc( sizeof(*new_tr), GFP_NOFS );
    if ( NULL == new_tr ) {
      //
      // Since __GFP_NOFAIL is going away, we will arrange to retry the allocation ourselves
      //
      congestion_wait( 0, HZ/50 );//BLK_RW_ASYNC, HZ/50 );
      goto alloc_transaction;
    }
  }

  //
  // We need to hold state_lock until t_updates has been incremented, for proper journal barrier handling
  //
repeat:
  read_lock( &j->state_lock );
  BUG_ON( j->flags & JNL_FLAGS_UNMOUNT );
  if ( jnl_is_aborted( j ) || ( 0 != j->errno && !( j->flags & JNL_FLAGS_ACK_ERR ) ) ) {
    read_unlock( &j->state_lock );
    kfree( new_tr );
    return -EROFS;
  }

  // Wait on the journal's transaction barrier if necessary
  if ( j->barrier_count ) {
    read_unlock( &j->state_lock );
    wait_event( j->wait_transaction_locked, j->barrier_count == 0 );
    goto repeat;
  }

  if ( !j->running_tr ) {
    read_unlock( &j->state_lock );
    if ( !new_tr )
      goto alloc_transaction;
    write_lock( &j->state_lock );
    if ( !j->running_tr && !j->barrier_count ) {
      // Initialize a new transaction
      // Create it in RUNNING state and add it to the current journal
      tr     = new_tr;
      new_tr = NULL;
      tr->journal     = j;
      tr->state       = T_RUNNING;
      tr->start_time  = ktime_get(); // GPL - ktime_get()
      tr->tid         = j->tr_sequence++;
      tr->expires     = jiffies + j->commit_interval;
      spin_lock_init( &tr->handle_lock );
      atomic_set( &tr->updates, 0 );
      atomic_set( &tr->outstanding_credits, 0 );
      atomic_set( &tr->handle_count, 0 );
      INIT_LIST_HEAD( &tr->private_list );

      // Set up the commit timer for the new transaction
#if defined HAVE_DECL_ROUND_JIFFIES_UP && HAVE_DECL_ROUND_JIFFIES_UP
      j->commit_timer.expires = round_jiffies_up( tr->expires ); // GPL
#else
      j->commit_timer.expires = HZ + round_jiffies( tr->expires ); // GPL
#endif
      add_timer( &j->commit_timer );

      J_ASSERT( j->running_tr == NULL );
      j->running_tr = tr;
      tr->max_wait  = 0;
      tr->start_jiffies = jiffies;
      TRACE_ONLY( hint = ", new tr"; )
    }
    write_unlock( &j->state_lock );
    goto repeat;
  }

  tr = j->running_tr;

  //
  // If the current transaction is locked down for commit, wait for the lock to be released.
  //
  if ( T_LOCKED == tr->state ) {
    DEFINE_WAIT( wait );
    prepare_to_wait( &j->wait_transaction_locked, &wait, TASK_UNINTERRUPTIBLE );
    read_unlock( &j->state_lock );
    schedule();
    finish_wait( &j->wait_transaction_locked, &wait );
    goto repeat;
  }

  //
  // If there is not enough space left in the log to write all potential buffers requested by this operation, we need to stall pending a log
  // checkpoint to free some more log space.
  //
  needed = atomic_add_return( nblocks, &tr->outstanding_credits );

  if ( needed > j->max_transaction_buffers ) {
    //
    // If the current transaction is already too large, then start to commit it: we can then go back and attach this handle to a new transaction.
    //
    DEFINE_WAIT( wait );
    jnl_debug( JNL_TRACE_LEVEL_HANDLE, "Handle %p starting new commit...\n", handle );
    atomic_sub( nblocks, &tr->outstanding_credits );
    prepare_to_wait( &j->wait_transaction_locked, &wait, TASK_UNINTERRUPTIBLE );
    tid = tr->tid;
    need_to_start = !tid_geq( j->commit_request, tid );
    read_unlock( &j->state_lock );
    if ( need_to_start )
      jnl_log_start_commit( j, tid );
    schedule();
    finish_wait( &j->wait_transaction_locked, &wait );
    goto repeat;
  }

  //
  // The commit code assumes that it can get enough log space without forcing a checkpoint.  This is *critical* for
  // correctness: a checkpoint of a buffer which is also associated with a committing transaction creates a deadlock,
  // so commit simply cannot force through checkpoints.
  //
  // We must therefore ensure the necessary space in the journal *before* starting to dirty potentially checkpointed buffers
  // in the new transaction.
  //
  // The worst part is, any transaction currently committing can reduce the free space arbitrarily.  Be careful to account for
  // those buffers when checkpointing.
  //

  // @@@ AKPM: This seems rather over-defensive.  We're giving commit a _lot_ of headroom: 1/4 of the journal plus the size of
  // the committing transaction.  Really, we only need to give it committing_tr->outstanding_credits plus "enough" for
  // the log control blocks.
  // Also, this test is inconsistent with the matching one in jnl_extend().
  //

  space_need = jnl_space_needed( j );
  space_left = jnl_log_space_left( j );

  if ( space_left < space_need ) {
    jnl_debug( JNL_TRACE_LEVEL_HANDLE, "h=%p waiting for checkpoint (%d < %d)...\n", handle, space_left, space_need );
    atomic_sub( nblocks, &tr->outstanding_credits );
    read_unlock( &j->state_lock );
    write_lock( &j->state_lock );
    space_need = jnl_space_needed( j );
    for ( ;; ) {
      space_left = jnl_log_space_left( j );
      if ( space_left >= space_need || (j->flags & JNL_FLAGS_ABORT) )
        break;

      write_unlock( &j->state_lock );
      mutex_lock( &j->checkpoint_mutex );

      //
      // Test again, another process may have checkpointed while we were waiting for the checkpoint lock
      // If there are no transactions ready to be checkpointed, try to recover journal space by calling jnl_cleanup_tail(),
      // and if that doesn't work, by waiting for the currently committing transaction to complete
      // If there is absolutely no way to make progress, this is either a BUG or corrupted filesystem,
      // so abort the journal and leave a stack trace for forensic evidence.
      //
      write_lock( &j->state_lock );
      spin_lock( &j->list_lock );
      space_need  = jnl_space_needed( j );
      space_left  = jnl_log_space_left( j );
      if ( space_left < space_need ) {
        int chkpt = NULL != j->checkpoint_tr;
        unsigned int tid = NULL != j->committing_tr? j->committing_tr->tid : 0;
        spin_unlock( &j->list_lock );
        write_unlock( &j->state_lock );
        if ( chkpt ) {
//          jnl_debug( JNL_TRACE_LEVEL_HANDLE, "waiting: chkpt\n" );
          jnl_log_do_checkpoint( j );
        } else if ( 0 == jnl_cleanup_tail( j ) ) {
//          jnl_debug( JNL_TRACE_LEVEL_HANDLE, "waiting: We were able to recover space; yay!\n" );
        } else if ( tid ) {
//          jnl_debug( JNL_TRACE_LEVEL_HANDLE, "waiting: jnl_log_wait_commit\n" );
          jnl_log_wait_commit( j, tid );
        } else {
          printk( KERN_ERR "needed %d blocks and only had %d space available\n", space_need, space_left );
          printk( KERN_ERR "no way to get more journal space in %s\n", j->devname );
          WARN_ON( 1 );
          jnl_abort( j, 0 );
        }
        write_lock( &j->state_lock );
      } else {
        spin_unlock( &j->list_lock );
      }
      mutex_unlock( &j->checkpoint_mutex );
    }

    write_unlock( &j->state_lock );
    goto repeat;
  }

  //
  // OK, account for the buffers that this operation expects to use and add the handle to the running transaction.
  //
//  update_t_max_wait( tr, ts );
  handle->tr = tr;
  atomic_inc( &tr->updates );
  atomic_inc( &tr->handle_count );
  jnl_debug( JNL_TRACE_LEVEL_HANDLE, "h=%p, id=%x%s given %d credits ( total %d, free %d )\n", handle, tr->tid, hint, nblocks,
                atomic_read( &tr->outstanding_credits ), jnl_log_space_left( j ) );
  read_unlock( &j->state_lock );

  lock_map_acquire( &handle->lockdep_map );
  kfree( new_tr );
  return 0;
}


///////////////////////////////////////////////////////////
// jnl_start
//
// Obtain a new handle.
// We make sure that the transaction can guarantee at least nblocks of modified buffers in the log
// We block until the log can guarantee that much space
//
// Return a pointer to a newly allocated handle, or an ERR_PTR() value on failure
///////////////////////////////////////////////////////////
static int
jnl_start(
    IN jnl *j,
    IN int nblocks,
    OUT handle_j  **handle
    )
{
  handle_j *h;
  int err;

  *handle = NULL;

  if ( !j )
    return -EROFS;

  if ( jnl_is_aborted( j ) )
    return -EROFS;

  h = kmem_cache_allocZ( jnl_handle_cache );
  if ( !h )
    return -ENOMEM;

  h->buffer_credits = nblocks;

  jnl_debug( JNL_TRACE_LEVEL_HANDLE, "jnl_start: h=%p, blocks=%d\n", h, nblocks );

#ifdef CONFIG_DEBUG_LOCK_ALLOC
  lockdep_init_map( &h->lockdep_map, "jnl_handle", &jnl_handle_key, 0 );
#endif

  err = jnl_start_handle( j, h );
  if ( err < 0 ) {
    kmem_cache_free( jnl_handle_cache, h );
    return err;
  }

  *handle = h;
  return 0;
}


///////////////////////////////////////////////////////////
// jnl_extend
//
// extend buffer credits
// Some transactions, such as large extends and truncates, can be done atomically all at once or in several stages
// The operation requests a credit for a number of buffer modifications in advance, but can extend its credit if it needs more.
//
// jnl_extend tries to give the running handle more buffer credits. It does not guarantee that allocation - this is a best-effort only.
// The calling process MUST be able to deal cleanly with a failure to extend here.
//
// Return 0 on success, non-zero on failure.
//
// return code < 0 implies an error
// return code > 0 implies normal transaction-full status.
///////////////////////////////////////////////////////////
static int
jnl_extend(
    IN handle_j *handle,
    IN int nblocks
    )
{
  transaction *tr = handle->tr;
  jnl *j = tr->journal;
  int result, wanted;

  if ( jnl_is_handle_aborted( handle ) )
    return -EIO;

  result = 1;
  read_lock( &j->state_lock );

  // Don't extend a locked-down transaction!
  if ( T_RUNNING != tr->state ) {
    jnl_debug( JNL_TRACE_LEVEL_HANDLE, "denied h=%p %d blocks: transaction not running\n", handle, nblocks );
  } else {
    spin_lock( &tr->handle_lock );
    wanted = atomic_read( &tr->outstanding_credits ) + nblocks;
    if ( wanted > j->max_transaction_buffers ) {
      jnl_debug( JNL_TRACE_LEVEL_HANDLE, "denied h=%p %d blocks: transaction too large\n", handle, nblocks );
    } else if ( wanted > jnl_log_space_left( j ) ) {
      jnl_debug( JNL_TRACE_LEVEL_HANDLE, "denied h=%p %d blocks: insufficient log space\n", handle, nblocks );
    } else {
      handle->buffer_credits += nblocks;
      atomic_add( nblocks, &tr->outstanding_credits );
      result = 0;
      jnl_debug( JNL_TRACE_LEVEL_HANDLE, "extended h=%p by %d\n", handle, nblocks );
    }
    spin_unlock( &tr->handle_lock );
  }

  read_unlock( &j->state_lock );

  return result;
}


///////////////////////////////////////////////////////////
// jnl_restart
//
// Restart a handle for a multi-transaction filesystem operation
//
// If the jnl_extend() call above fails to grant new buffer credits to a running handle, a call to jnl_restart will commit the
// handle's transaction so far and reattach the handle to a new transaction capabable of guaranteeing the requested number of credits
///////////////////////////////////////////////////////////
static int
jnl_restart(
    IN handle_j *h,
    IN int nblocks
    )
{
  unsigned int  tid;
  int need_to_start, ret;
  transaction *tr = h->tr;
  jnl *j = tr->journal;

  // If we've had an abort of any type, don't even think about actually doing the restart!
  if ( jnl_is_handle_aborted( h ) )
    return 0;

  //
  // First unlink the handle from its current transaction, and start the commit on that
  //
  J_ASSERT( atomic_read( &tr->updates ) > 0 );

  read_lock( &j->state_lock );
  spin_lock( &tr->handle_lock );
  atomic_sub( h->buffer_credits, &tr->outstanding_credits );
  if ( atomic_dec_and_test( &tr->updates ) )
    wake_up( &j->wait_updates );
  spin_unlock( &tr->handle_lock );

  jnl_debug( JNL_TRACE_LEVEL_HANDLE, "restarting handle %p\n", h );
  tid = tr->tid;
  need_to_start = !tid_geq( j->commit_request, tid );
  read_unlock( &j->state_lock );
  if ( need_to_start )
    jnl_log_start_commit( j, tid );
  lock_map_release( &h->lockdep_map );
  h->buffer_credits = nblocks;
  ret = jnl_start_handle( j, h );
  return ret;
}


#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_lock_updates
//
// establish a transaction barrier.
//
// This locks out any further updates from being started, and blocks until all existing updates have completed, returning only once the
// journal is in a quiescent state with no updates running.
//
// The journal lock should not be held on entry.
///////////////////////////////////////////////////////////
static void
jnl_lock_updates(
    IN jnl *j
    )
{
  DEFINE_WAIT( wait );

  write_lock( &j->state_lock );
  ++j->barrier_count;

  // Wait until there are no running updates
  while ( 1 ) {
    transaction *tr = j->running_tr;
    if ( NULL == tr )
      break;

    spin_lock( &tr->handle_lock );
    prepare_to_wait( &j->wait_updates, &wait, TASK_UNINTERRUPTIBLE );
    if ( !atomic_read( &tr->updates ) ) {
      spin_unlock( &tr->handle_lock );
      finish_wait( &j->wait_updates, &wait );
      break;
    }

    spin_unlock( &tr->handle_lock );
    write_unlock( &j->state_lock );
    schedule();
    finish_wait( &j->wait_updates, &wait );
    write_lock( &j->state_lock );
  }
  write_unlock( &j->state_lock );

  //
  // We have now established a barrier against other normal updates, but we also need to barrier against other jnl_lock_updates() calls
  // to make sure that we serialise special journal-locked operations too
  //
  mutex_lock( &j->barrier );
}


///////////////////////////////////////////////////////////
// jnl_unlock_updates
//
// Release a transaction barrier obtained with jnl_lock_updates()
///////////////////////////////////////////////////////////
static void
jnl_unlock_updates(
    IN jnl *j
    )
{
  J_ASSERT( j->barrier_count != 0 );

  mutex_unlock( &j->barrier );
  write_lock( &j->state_lock );
  --j->barrier_count;
  write_unlock( &j->state_lock );
  wake_up( &j->wait_transaction_locked );
}
#endif // #ifdef USE_JNL_EXTRA


///////////////////////////////////////////////////////////
// warn_dirty_buffer
//
//
///////////////////////////////////////////////////////////
static void
warn_dirty_buffer(
    IN struct buffer_head *bh
    )
{
  char b[BDEVNAME_SIZE];

  printk( KERN_WARNING
    "jnl: Spotted dirty metadata buffer ( dev = %s, blocknr = 0x%"PSCT"x ). "
    "There's a risk of filesystem corruption in case of system crash.\n",
    bdevname( bh->b_bdev, b ), bh->b_blocknr );
}


///////////////////////////////////////////////////////////
// __jnl_temp_unlink_buffer
//
// Remove a buffer from the appropriate transaction list.
//
// Note that this function can *change* the value of
// bh->tr->buffers, t_forget, t_iobuf_list, t_shadow_list, t_log_list or t_reserved_list
// If the caller is holding onto a copy of one of these pointers, it could go bad
// Generally the caller needs to re-read the pointer from the transaction.
//
// Called under list_lock.  The journal may not be locked.
///////////////////////////////////////////////////////////
static void
__jnl_temp_unlink_buffer(
    IN jnl_head *jh
    )
{
  jnl_head **list = NULL;
  transaction *tr = jh->tr;
  struct buffer_head *bh = jh2bh( jh );

  J_ASSERT( jnl_is_locked_bh_state( bh ) );

  if ( NULL != tr )
    assert_spin_locked( &tr->journal->list_lock );

  J_ASSERT( jh->jlist < BJ_Types );
  J_ASSERT( BJ_None == jh->jlist || NULL != tr );

  switch ( jh->jlist ) {
    case BJ_None:
      return;
    case BJ_Metadata:
      tr->nr_buffers--;
      J_ASSERT( tr->nr_buffers >= 0 );
      list = &tr->buffers;
      break;
    case BJ_Forget:
      list = &tr->forget;
      break;
    case BJ_IO:
      list = &tr->iobuf_list;
      break;
    case BJ_Shadow:
      list = &tr->shadow_list;
      break;
    case BJ_LogCtl:
      list = &tr->log_list;
      break;
    case BJ_Reserved:
      list = &tr->reserved_list;
      break;
  }

  if ( *list == jh ) {
    *list = jh->tnext;
    if ( *list == jh )
      *list = NULL;
  }
  jh->tprev->tnext = jh->tnext;
  jh->tnext->tprev = jh->tprev;

  jh->jlist = BJ_None;
  if ( test_clear_buffer_jnldirty( bh ) )
    mark_buffer_dirty( bh );  // Expose it to the VM
}


///////////////////////////////////////////////////////////
// __jnl_file_buffer
//
// File a buffer on the given transaction list
///////////////////////////////////////////////////////////
static void
__jnl_file_buffer(
    IN jnl_head *jh,
    IN transaction *tr,
    IN int jlist
    )
{
  jnl_head **list = NULL;
  int was_dirty = 0;
  struct buffer_head *bh = jh2bh( jh );

  J_ASSERT( jnl_is_locked_bh_state( bh ) );
  assert_spin_locked( &tr->journal->list_lock );

  J_ASSERT( jh->jlist < BJ_Types );
  J_ASSERT( jh->tr == tr || jh->tr == NULL );

  if ( jh->tr && jh->jlist == jlist )
    return;

  switch( jlist ) {
  case BJ_Metadata:
  case BJ_Reserved:
  case BJ_Shadow:
  case BJ_Forget:
    //
    // For metadata buffers, we track dirty bit in buffer_jnldirty instead of buffer_dirty
    // We should not see a dirty bit set here because we clear it in do_get_write_access but e.g.
    // tune2fs can modify the sb and set the dirty bit at any time so we try to gracefully handle that.
    //
    if ( buffer_dirty( bh ) )
      warn_dirty_buffer( bh );
    if ( test_clear_buffer_dirty( bh ) || test_clear_buffer_jnldirty( bh ) )
      was_dirty = 1;
  }

  if ( jh->tr )
    __jnl_temp_unlink_buffer( jh );
  else
    jnl_grab_hdr( bh );

  jh->tr = tr;

  switch ( jlist ) {
  case BJ_None:
    J_ASSERT( !jh->committed_data );
    J_ASSERT( !jh->frozen_data );
    return;
  case BJ_Metadata:
    tr->nr_buffers++;
    list = &tr->buffers;
    break;
  case BJ_Forget:
    list = &tr->forget;
    break;
  case BJ_IO:
    list = &tr->iobuf_list;
    break;
  case BJ_Shadow:
    list = &tr->shadow_list;
    break;
  case BJ_LogCtl:
    list = &tr->log_list;
    break;
  case BJ_Reserved:
    list = &tr->reserved_list;
    break;
  }

  if ( NULL == *list ) {
    jh->tnext = jh->tprev = jh;
    *list = jh;
  } else {
    // Insert at the tail of the list to preserve order
    jnl_head *first = *list;
    jnl_head *last  = first->tprev;
    jh->tprev = last;
    jh->tnext = first;
    last->tnext = first->tprev = jh;
  }

  jh->jlist = jlist;

  if ( was_dirty )
    set_buffer_jnldirty( bh );
}


///////////////////////////////////////////////////////////
// jnl_file_buffer
//
//
///////////////////////////////////////////////////////////
static void
jnl_file_buffer(
    IN jnl_head *jh,
    IN transaction *tr,
    IN int jlist
    )
{
  struct buffer_head *bh = jh2bh( jh );
  jnl_lock_bh_state( bh );
  spin_lock( &tr->journal->list_lock );
  __jnl_file_buffer( jh, tr, jlist );
  spin_unlock( &tr->journal->list_lock );
  jnl_unlock_bh_state( bh );
}



///////////////////////////////////////////////////////////
// hash
//
//
///////////////////////////////////////////////////////////
static inline int
hash(
    IN unsigned long block
    )
{
//  struct jnl_revoke_table *table = j->revoke;
//  int hash_shift = table->hash_shift;
  int hash_shift = JOURNAL_REVOKE_DEFAULT_HASH_SHIFT;
  int hash = ( int )block ^ ( int )( ( block >> 31 ) >> 1 );

  return ( ( hash << ( JOURNAL_REVOKE_DEFAULT_HASH_SHIFT - 6 ) ) ^
          ( hash >> 13 ) ^
          ( hash << ( hash_shift - 12 ) ) ) & ( JOURNAL_REVOKE_DEFAULT_HASH - 1 );
}


///////////////////////////////////////////////////////////
// find_revoke_record
//
// Find a revoke record in the journal's hash table
///////////////////////////////////////////////////////////
static jnl_revoke_record*
find_revoke_record(
    IN jnl *j,
    IN jnl_revoke_table *revoke,
    IN unsigned long blocknr
    )
{
  jnl_revoke_record *record;
  struct list_head *hash_list = &revoke[ hash( blocknr ) ];
//  struct list_head *hash_list = &j->revoke->hash_table[ hash( blocknr ) ];

  spin_lock( &j->revoke_lock );
  record = (jnl_revoke_record*)hash_list->next;
  while ( &( record->hash ) != hash_list ) {
    if ( record->blocknr == blocknr ) {
      spin_unlock( &j->revoke_lock );
      return record;
    }
    record = (jnl_revoke_record*)record->hash.next;
  }
  spin_unlock( &j->revoke_lock );
  return NULL;
}


///////////////////////////////////////////////////////////
// jnl_cancel_revoke
//
// For use only internally by the journaling code ( called from jnl_get_write_access ).
//
// We trust buffer_revoked() on the buffer if the buffer is already being journaled:
// if there is no revoke pending on the buffer, then we don't do anything here.
//
// This would break if it were possible for a buffer to be revoked and discarded, and then reallocated within the same transaction
// In such a case we would have lost the revoked bit, but when we arrived here the second time we would still
// have a pending revoke to cancel.  So, do not trust the Revoked bit on buffers unless RevokeValid is also set.

///////////////////////////////////////////////////////////
static int
jnl_cancel_revoke(
    IN handle_j *handle,
    IN jnl_head *jh
    )
{
  jnl_revoke_record *record;
  jnl *j = handle->tr->journal;
  int need_cancel;
  int did_revoke = 0;     // akpm: debug
  struct buffer_head *bh = jh2bh( jh );

  // Is the existing Revoke bit valid?
  // If so, we trust it, and only perform the full cancel if the revoke bit is set
  // If not, we can't trust the revoke bit, and we need to do the full search for a revoke record
  if ( test_set_buffer_revokevalid( bh ) ) {
    need_cancel = test_clear_buffer_revoked( bh );
  } else {
    need_cancel = 1;
    clear_buffer_revoked( bh );
  }

  if ( need_cancel ) {
//    jnl_debug( JNL_TRACE_LEVEL_REVOKE, "cancel_revoke: h=%p, b=%lx\n", handle, ( unsigned long )bh->b_blocknr );
    record = find_revoke_record( j, j->revoke, bh->b_blocknr );
    if ( record ) {
      jnl_debug( JNL_TRACE_LEVEL_REVOKE, "cancelled existing revoke on b=%"PSCT"x\n", bh->b_blocknr );
      spin_lock( &j->revoke_lock );
      list_del( &record->hash );
      spin_unlock( &j->revoke_lock );
      kmem_cache_free( jnl_revoke_record_cache, record );
      did_revoke = 1;
    }
  }

#ifdef JNL_EXPENSIVE_CHECKING
  // There better not be one left behind by now!
  record = find_revoke_record( j, j->revoke, bh->b_blocknr );
  J_ASSERT( record == NULL );
#endif

  //
  // Finally, have we just cleared revoke on an unhashed buffer_head?
  // If so, we'd better make sure we clear the revoked status on any hashed alias too,
  // otherwise the revoke state machine will get very upset later on
  //
  if ( need_cancel ) {
    struct buffer_head *bh2 = __find_get_block( bh->b_bdev, bh->b_blocknr, bh->b_size );
    if ( NULL != bh2 ) {
      if ( bh2 != bh )
        clear_buffer_revoked( bh2 );
      __brelse( bh2 );
    }
  }

  return did_revoke;
}


///////////////////////////////////////////////////////////
// jnl_clear_buffer_revoked_flags
//
// journal_clear_revoked_flag clears revoked flag of buffers in revoke table to reflect
// there is no revoked buffers in the next transaction which is going to be started.
///////////////////////////////////////////////////////////
static void
jnl_clear_buffer_revoked_flags(
    IN jnl *j
    )
{
  jnl_revoke_table *revoke = j->revoke;
  int i = 0;

  for ( i = 0; i < JOURNAL_REVOKE_DEFAULT_HASH; i++ ) {
    struct list_head *hash_list = &revoke[i];
    struct list_head *list_entry;

    list_for_each( list_entry, hash_list ) {
      jnl_revoke_record *r    = (jnl_revoke_record *)list_entry;
      struct buffer_head *bh  = __find_get_block( j->fs_dev, r->blocknr, j->blocksize );
      if ( NULL != bh ) {
        clear_buffer_revoked( bh );
        __brelse( bh );
      }
    }
  }
}


///////////////////////////////////////////////////////////
// do_get_write_access
//
// If the buffer is already part of the current transaction, then there is nothing we need to do
// If it is already part of a prior transaction which we are still committing to disk, then we need to make sure
// that we do not overwrite the old copy: we do copy-out to preserve the copy going to disk
// We also account the buffer against the handle's metadata buffer credits ( unless the buffer is already
// part of the transaction, that is ).
///////////////////////////////////////////////////////////
static int
do_get_write_access(
    IN handle_j *handle,
    IN jnl_head *jh,
    IN int force_copy
    )
{
  struct buffer_head *bh;
  transaction *tr;
  jnl *j;
  int err;
  unsigned blocksize;
  char *frozen_buffer = NULL;
  int need_copy = 0;
  TRACE_ONLY( const char *hint = force_copy?", force_copy" : ""; )
  TRACE_ONLY( sector_t blocknr; )

  if ( jnl_is_handle_aborted( handle ) )
    return -EROFS;

  tr  = handle->tr;
  j   = tr->journal;
  blocksize  = j->blocksize;

repeat:
  bh = jh2bh( jh );
  TRACE_ONLY( blocknr = bh->b_blocknr; )
  // @@@ Need to check for errors here at some point
  lock_buffer( bh );
  jnl_lock_bh_state( bh );

  //
  // We now hold the buffer lock so it is safe to query the buffer state.  Is the buffer dirty?
  //
  // If so, there are two possibilities.  The buffer may be non-journaled, and undergoing a quite legitimate writeback.
  // Otherwise, it is journaled, and we don't expect dirty buffers in that state ( the buffers should be marked JNL_Dirty instead. )
  // So either the IO is being done under our own control and this is a bug, or it's a third party IO such as
  // dump( 8 ) ( which may leave the buffer scheduled for read --- ie. locked but not dirty ) or tune2fs ( which may actually have
  // the buffer dirtied, ugh. )
  //
  if ( buffer_dirty( bh ) ) {
    //
    // First question: is this buffer already part of the current transaction or the existing committing transaction?
    //
    if ( jh->tr ) {
      J_ASSERT( jh->tr == tr || jh->tr == j->committing_tr );
      J_ASSERT( NULL == jh->next_tr || jh->next_tr == tr );
      warn_dirty_buffer( bh );
    }

    //
    // In any case we need to clean the dirty flag and we must do it under the buffer lock to be sure we don't race with running write-out
    //
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "get_write_access: Journalling dirty buffer: tr=%x, b=%"PSCT"x%s\n", tr->tid, blocknr, hint );
    clear_buffer_dirty( bh );
    set_buffer_jnldirty( bh );
  }

  unlock_buffer( bh );

  if ( jnl_is_handle_aborted( handle ) ) {
    jnl_unlock_bh_state( bh );
    err = -EROFS;
    goto out;
  }

  err = 0;

  //
  // The buffer is already part of this transaction if 'tr' or 'next_tr' points to it
  //
  if ( jh->tr == tr ) {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "get_write_access: already in this: tr=%x, b=%"PSCT"x%s\n", tr->tid, blocknr, hint );
    goto done;
  }

  if ( jh->next_tr == tr ) {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_COMMIT, "get_write_access: already in next: tr=%x, b=%"PSCT"x%s\n", tr->tid, blocknr, hint );
    goto done;
  }

  //
  // this is the first time this transaction is touching this buffer, reset the modified flag
  //
  jh->modified = 0;

  //
  // If there is already a copy-out version of this buffer, then we don't need to make another one
  //
  if ( NULL != jh->frozen_data ) {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_COMMIT, "get_write_access: has frozen data: tr=%x, b=%"PSCT"x%s\n", tr->tid, blocknr, hint );
    J_ASSERT( NULL == jh->next_tr );
    jh->next_tr = tr;
    goto done;
  }

  //
  // Is there data here we need to preserve?
  //
  if ( NULL != jh->tr && jh->tr != tr ) {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_COMMIT, "get_write_access: owned by older tr=%x: tr=%x, b=%"PSCT"x%s\n", jh->tr->tid, tr->tid, blocknr, hint );
    J_ASSERT( NULL == jh->next_tr );
    J_ASSERT( jh->tr == j->committing_tr );

    //
    // There is one case we have to be very careful about. If the committing transaction is currently writing
    // this buffer out to disk and has NOT made a copy-out, then we cannot modify the buffer contents at all right now
    // The essence of copy-out is that it is the extra copy, not the primary copy, which gets journaled
    // If the primary copy is already going to disk then we cannot do copy-out here
    //
    if ( BJ_Shadow == jh->jlist ) {
      DEFINE_WAIT_BIT( wait, &bh->b_state, BH_Unshadow );
      wait_queue_head_t *wqh = bit_waitqueue( &bh->b_state, BH_Unshadow );
      jnl_debug( JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_COMMIT, "b=%"PSCT"x on shadow: sleep\n", blocknr );
      jnl_unlock_bh_state( bh );
      // commit wakes up all shadow buffers after IO
      for ( ; ; ) {
        prepare_to_wait( wqh, &wait.wait, TASK_UNINTERRUPTIBLE );
        if ( BJ_Shadow != jh->jlist )
          break;
        schedule();
      }
      finish_wait( wqh, &wait.wait );
      jnl_debug( JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_COMMIT, "b=%"PSCT"x on shadow: wake\n", blocknr );
      goto repeat;
    }

    //
    // Only do the copy if the currently-owning transaction still needs it.  If it is on the Forget list, the
    // committing transaction is past that stage.  The buffer had better remain locked during the kmalloc,
    // but that should be true --- we hold the journal lock still and the buffer is already on the BUF_JOURNAL
    // list so won't be flushed.
    //
    // Subtle point, though: if this is a get_undo_access, then we will be relying on the frozen_data to contain
    // the new value of the committed_data record after the transaction, so we HAVE to force the frozen_data copy
    // in that case

    if ( BJ_Forget != jh->jlist || force_copy ) {
      if ( NULL == frozen_buffer ) {
        jnl_debug( JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_COMMIT, "allocate memory for buffer\n" );
        jnl_unlock_bh_state( bh );
        frozen_buffer = jnl_alloc( blocksize, GFP_NOFS );
        if ( !frozen_buffer ) {
          printk( KERN_EMERG"OOM for frozen_buffer\n" );
          jnl_debug( JNL_TRACE_LEVEL_ACCESS, "oom!\n" );
          err = -ENOMEM;
          jnl_lock_bh_state( bh );
          goto done;
        }
        goto repeat;
      }
      jnl_debug( JNL_TRACE_LEVEL_ACCESS|JNL_TRACE_LEVEL_COMMIT, "generate frozen data\n" );
      jh->frozen_data = frozen_buffer;
      frozen_buffer   = NULL;
      need_copy = 1;
    }

    jh->next_tr = tr;
  }

  //
  // Finally, if the buffer is not journaled right now, we need to make sure it doesn't get written to disk
  // before the caller actually commits the new data
  //
  if ( NULL == jh->tr ) {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "get_write_access: no transaction, file b=%"PSCT"x as BJ_Reserved\n", blocknr );
    J_ASSERT( !jh->next_tr );
    spin_lock( &j->list_lock );
    __jnl_file_buffer( jh, tr, BJ_Reserved );
    spin_unlock( &j->list_lock );
  } else {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "get_write_access: in tr=%x, b=%"PSCT"x\n", jh->tr->tid, blocknr );
  }

done:
  if ( need_copy ) {
    struct page *page;
    int offset;
    char *source;

    J_EXPECT( buffer_uptodate( bh ), "Possible IO failure.\n" );
    page    = bh->b_page;
    source  = atomic_kmap( page );
    if ( bh->b_size < blocksize ) {
      J_ASSERT( 512 == bh->b_size );
      offset  = 0;
    } else {
      offset  = offset_in_page( bh->b_data );
    }

    // Fire data frozen trigger just before we copy the data
//    jnl__buffer_frozen_trigger( jh, source + offset, jh->triggers );
    memcpy( jh->frozen_data, source+offset, blocksize );
    atomic_kunmap( source );

    // Now that the frozen data is saved off, we need to store any matching triggers
    //jh->frozen_triggers = jh->triggers;
  }

  jnl_unlock_bh_state( bh );

  //
  // If we are about to journal a buffer, then any revoke pending on it is no longer valid
  //
  jnl_cancel_revoke( handle, jh );

out:
  if ( unlikely( frozen_buffer ) ) // It's usually NULL
    jnl_free( frozen_buffer, blocksize );

  return err;
}


///////////////////////////////////////////////////////////
// jnl_get_write_access
//
// notify intent to modify a buffer for metadata ( not data ) update
//
// In full data journalling mode the buffer may be of type BJ_AsyncData,
// because we're write()ing a buffer which is also part of a shared mapping.
///////////////////////////////////////////////////////////
static int
jnl_get_write_access(
    IN handle_j *handle,
    IN struct buffer_head *bh
    )
{
  jnl_head *jh = jnl_add_hdr( bh );

  //
  // We do not want to get caught playing with fields which the log thread also manipulates
  // Make sure that the buffer completes any outstanding IO before proceeding
  //
  int rc = do_get_write_access( handle, jh, 0 );
  jnl_put_hdr( jh );
  return rc;
}


///////////////////////////////////////////////////////////
// jnl_get_create_access
//
// When the user wants to journal a newly created buffer_head ( ie. getblk() returned a new buffer and we are going to populate it
// manually rather than reading off disk ), then we need to keep the buffer_head locked until it has been completely filled with new data
// In this case, we should be able to make the assertion that the bh is not already part of an existing transaction.
//
// The buffer should already be locked by the caller by this point.
// There is no lock ranking violation: it was a newly created,
// unlocked buffer beforehand
//
//
// int jnl_get_create_access () - notify intent to use newly created bh
// @handle: transaction to new buffer to
// @bh: new buffer.
//
// Call this if you create a new bh.
///////////////////////////////////////////////////////////
static int
jnl_get_create_access(
    IN handle_j *handle,
    IN struct buffer_head *bh
    )
{
  transaction *tr = handle->tr;
  jnl *j = tr->journal;
  jnl_head *jh = jnl_add_hdr( bh );
  int err;

  if ( jnl_is_handle_aborted( handle ) ) {
    err = -EROFS;
    goto out;
  }

  err = 0;

  //
  // The buffer may already belong to this transaction due to pre-zeroing in the filesystem's new_block code
  // It may also be on the previous, committing transaction's lists, but it HAS to be in Forget state
  // in that case: the transaction must have deleted the buffer for it to be reused here
  //
  jnl_lock_bh_state( bh );
  spin_lock( &j->list_lock );
  J_ASSERT( ( jh->tr == tr || jh->tr == NULL || ( jh->tr == j->committing_tr && BJ_Forget == jh->jlist ) ) );
  J_ASSERT( jh->next_tr == NULL );
  J_ASSERT( buffer_locked( jh2bh( jh ) ) );

  if ( NULL == jh->tr ) {
    //
    // Previous jnl_forget() could have left the buffer with jnldirty bit set because it was being committed. When
    // the commit finished, we've filed the buffer for checkpointing and marked it dirty. Now we are reallocating
    // the buffer so the transaction freeing it must have committed and so it's safe to clear the dirty bit.
    //
    clear_buffer_dirty( bh  );
    // first access by this transaction
    jh->modified = 0;
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "get_create_access: file b=%"PSCT"x as BJ_Reserved\n", bh->b_blocknr );
    __jnl_file_buffer( jh, tr, BJ_Reserved );
  } else if ( jh->tr == j->committing_tr ) {
    // first access by this transaction
    jh->modified = 0;
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "get_create_access: b=%"PSCT"x, set next transaction\n", bh->b_blocknr );
    jh->next_tr = tr;
  } else {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "get_create_access: b=%"PSCT"x - ***\n", bh->b_blocknr );
  }

  spin_unlock( &j->list_lock );
  jnl_unlock_bh_state( bh );

  //
  // akpm: I added this.  jnl_alloc_branch can pick up new indirect blocks which contain freed but then revoked metadata
  // We need to cancel the revoke in case we end up freeing it yet again and the reallocating as data - this would cause a second revoke,
  // which hits an assertion error.
  //
  jnl_cancel_revoke( handle, jh );
out:
  jnl_put_hdr( jh );
  return err;
}


#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_get_undo_access
//
//
// Notify intent to modify metadata with non-rewindable consequences
//
// Sometimes there is a need to distinguish between metadata which has been committed to disk and that which has not
// The ext3fs code uses this for freeing and allocating space, we have to make sure that we do not reuse freed space
// until the deallocation has been committed, since if we overwrote that space we would make the delete
// un-rewindable in case of a crash.
//
// To deal with that, jnl_get_undo_access requests write access to a buffer for parts of non-rewindable operations such as delete
// operations on the bitmaps
// The journaling code must keep a copy of the buffer's contents prior to the undo_access call until such time
// as we know that the buffer has definitely been committed to disk.
//
// We never need to know which transaction the committed data is part of, buffers touched here are guaranteed to be dirtied later and so
// will be committed to a new transaction in due course, at which point we can discard the old committed data pointer.
//
// Returns error number or 0 on success.
///////////////////////////////////////////////////////////
static int
jnl_get_undo_access(
    IN handle_j *handle,
    IN struct buffer_head *bh
    )
{
  int err;
  jnl_head *jh = jnl_add_hdr( bh );
  char *committed_data = NULL;

  //
  // Do this first --- it can drop the journal lock, so we want to make sure that obtaining the committed_data is done
  // atomically wrt. completion of any outstanding commits.
  //
  err = do_get_write_access( handle, jh, 1 );
  if ( err )
    goto out;

repeat:
  if ( !jh->committed_data ) {
    committed_data = jnl_alloc( bh->b_size, GFP_NOFS );
    if ( !committed_data ) {
      printk( KERN_EMERG "No memory for committed data\n" );
      err = -ENOMEM;
      goto out;
    }
  }

  jnl_lock_bh_state( bh );
  if ( !jh->committed_data ) {
    // Copy out the current buffer contents into the preserved, committed copy
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "generate committed data\n" );
    if ( !committed_data ) {
      jnl_unlock_bh_state( bh );
      goto repeat;
    }

    memcpy( committed_data, bh->b_data, bh->b_size );
    jh->committed_data = committed_data;
    committed_data = NULL;
  }
  jnl_unlock_bh_state( bh );
out:
  jnl_put_hdr( jh );
  if ( unlikely( committed_data ) )
    jnl_free( committed_data, bh->b_size );
  return err;
}
#endif // #ifdef USE_JNL_EXTRA


///////////////////////////////////////////////////////////
// jnl_dirty_metadata
//
// mark a buffer as containing dirty metadata
//
// mark dirty metadata which needs to be journaled as part of the current transaction.
//
// The buffer must have previously had jnl_get_write_access()
// called so that it has a valid jnl_head attached to the buffer head.
//
// The buffer is placed on the transaction's metadata list and is marked as belonging to the transaction.
//
// Returns error number or 0 on success.
//
// Special care needs to be taken if the buffer already belongs to the current committing transaction
// ( in which case we should have frozen data present for that commit )
// In that case, we don't relink the buffer: that only gets done when the old transaction finally completes its commit.
///////////////////////////////////////////////////////////
static void
jnl_dirty_metadata(
    IN handle_j *h,
    IN struct buffer_head *bh,
    OUT int *credits
    )
{
  transaction *tr = h->tr;
  jnl *j = tr->journal;
  jnl_head *jh = bh2jh( bh );

  if ( jnl_is_handle_aborted( h ) )
    return;

  J_ASSERT( buffer_jnl( bh ) );
//  if ( !buffer_jnl( bh ) )
//    return; //ret = -EUCLEAN;

  jnl_lock_bh_state( bh );

  if ( 0 == jh->modified ) {
    //
    // This buffer's got modified and becoming part of the transaction. This needs to be done once a transaction -bzzz
    //
    jh->modified = 1;
    J_ASSERT( h->buffer_credits > 0 );
    h->buffer_credits--;
    TRACE_ONLY( h->credits_used += 1; )
  }

  if ( NULL != credits )
    *credits = h->buffer_credits;

  //
  // fastpath, to avoid expensive locking
  // If this buffer is already on the running transaction's metadata list there is nothing to do.
  // Nobody can take it off again because there is a handle open.
  // I _think_ we're OK here with SMP barriers - a mistaken decision will result in this test being false, so we go in and take the locks.
  //
  if ( jh->tr == tr && BJ_Metadata == jh->jlist ) {
    jnl_debug( JNL_TRACE_LEVEL_DIRTY_META, "dirty_metadata: b=%"PSCT"x already BJ_Metadata\n", bh->b_blocknr );
    J_ASSERT( jh->tr == j->running_tr );
  } else {
    set_buffer_jnldirty( bh );

    //
    // Metadata already on the current transaction list doesn't need to be filed.  Metadata on another transaction's list must
    // be committing, and will be refiled once the commit completes: leave it alone for now.
    //
    if ( jh->tr != tr ) {
      jnl_debug( JNL_TRACE_LEVEL_DIRTY_META, "dirty_metadata: b=%"PSCT"x already on other transaction\n", bh->b_blocknr );
      J_ASSERT( jh->tr == j->committing_tr );
      J_ASSERT( jh->next_tr == tr );
      // And this case is illegal: we can't reuse another transaction's data buffer, ever
    } else {
      // That test should have eliminated the following case:
      J_ASSERT( NULL == jh->frozen_data );
      jnl_debug( JNL_TRACE_LEVEL_DIRTY_META, "dirty_metadata: file b=%"PSCT"x as BJ_Metadata\n", bh->b_blocknr );

      spin_lock( &j->list_lock );
      __jnl_file_buffer( jh, h->tr, BJ_Metadata ) ;
      spin_unlock( &j->list_lock );
    }
  }
  jnl_unlock_bh_state( bh );
}


#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_release_buffer
//
// undo a get_write_access without any buffer updates, if the update decided in the end that it didn't need access
///////////////////////////////////////////////////////////
static void
jnl_release_buffer(
    IN handle_j *handle,
    IN struct buffer_head *bh
    )
{
//  jnl_debug( 6, "entry" );
}
#endif


///////////////////////////////////////////////////////////
// __jnl_unfile_buffer
//
// Remove buffer from all transactions
// jh and bh may be already freed when this function returns
///////////////////////////////////////////////////////////
static void
__jnl_unfile_buffer(
    IN jnl_head *jh
    )
{
  __jnl_temp_unlink_buffer( jh );
  jh->tr = NULL;
  jnl_put_hdr( jh );
}


///////////////////////////////////////////////////////////
// jnl_unfile_buffer
//
//
///////////////////////////////////////////////////////////
static void
jnl_unfile_buffer(
    IN jnl *j,
    IN jnl_head *jh
    )
{
  struct buffer_head *bh = jh2bh( jh );

  // Get reference so that buffer cannot be freed before we unlock it
  get_bh( bh );
  jnl_lock_bh_state( bh );
  spin_lock( &j->list_lock );
  __jnl_unfile_buffer( jh );
  spin_unlock( &j->list_lock );
  jnl_unlock_bh_state( bh );
  __brelse( bh );
}


#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_forget
//
// bforget() for potentially-journaled buffers.
//
// We can only do the bforget if there are no commits pending against the buffer
// If the buffer is dirty in the current running transaction we can safely unlink it.
//
// bh may not be a journalled buffer at all - it may be a non-JNL buffer which came off the hashtable
// Check for this.
//
// Decrements bh->b_count by one.
//
// Allow this call even if the handle has aborted --- it may be part of the caller's cleanup after an abort.
///////////////////////////////////////////////////////////
static int
jnl_forget(
    IN handle_j *h,
    IN struct buffer_head *bh
    )
{
  transaction *tr = h->tr;
  jnl *j = tr->journal;
  jnl_head *jh;
  int drop_reserve = 0;
  int err = 0;
  int was_modified = 0;

  jnl_debug( JNL_TRACE_LEVEL_ACCESS, "+forget b=%"PSCT"x\n", bh->b_blocknr );

  jnl_lock_bh_state( bh );
  spin_lock( &j->list_lock );

  if ( !buffer_jnl( bh ) )
    goto not_jnl;

  jh = bh2jh( bh );

  // Critical error: attempting to delete a bitmap buffer, maybe? Don't do any jnl operations, and return an error
  if ( !J_EXPECT( !jh->committed_data, "inconsistent data on disk" ) ) {
    err = -EIO;
    goto not_jnl;
  }

  // keep track of whether or not this transaction modified us
  was_modified = jh->modified;

  //
  // The buffer's going from the transaction, we must drop all references -bzzz
  //
  jh->modified = 0;

  if ( jh->tr == h->tr ) {
    J_ASSERT( NULL == jh->frozen_data );

    // If we are forgetting a buffer which is already part of this transaction, then we can just drop it from
    // the transaction immediately
    clear_buffer_dirty( bh );
    clear_buffer_jnldirty( bh );

    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "belongs to current transaction: unfile\n" );

    //
    // we only want to drop a reference if this transaction modified the buffer
    //
    if ( was_modified )
      drop_reserve = 1;

    //
    // We are no longer going to journal this buffer. However, the commit of this transaction is still
    // important to the buffer: the delete that we are now processing might obsolete an old log entry, so by
    // committing, we can satisfy the buffer's checkpoint.
    //
    // So, if we have a checkpoint on the buffer, we should now refile the buffer on our BJ_Forget list so that
    // we know to remove the checkpoint after we commit.
    //
    if ( jh->cp_tr ) {
      __jnl_temp_unlink_buffer( jh );
      __jnl_file_buffer( jh, tr, BJ_Forget );
    } else {
      __jnl_unfile_buffer( jh );
      if ( !buffer_jnl( bh ) ) {
        spin_unlock( &j->list_lock );
        jnl_unlock_bh_state( bh );
        __bforget( bh );
        goto drop;
      }
    }
  } else if ( jh->tr ) {
    J_ASSERT( ( jh->tr == j->committing_tr ) );
    // However, if the buffer is still owned by a prior ( committing ) transaction, we can't drop it yet...
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "belongs to older transaction\n" );
    // ... but we CAN drop it from the new transaction if we have also modified it since the original commit.
    if ( jh->next_tr ) {
      J_ASSERT( jh->next_tr == tr );
      jh->next_tr = NULL;

      //
      // only drop a reference if this transaction modified the buffer
      //
      if ( was_modified )
        drop_reserve = 1;
    }
  }

not_jnl:
  spin_unlock( &j->list_lock );
  jnl_unlock_bh_state( bh );
  jnl_debug( JNL_TRACE_LEVEL_IO, "brelse b=%"PSCT"x\n", bh->b_blocknr );
  __brelse( bh );
drop:
  if ( drop_reserve ) {
    // no need to reserve log space for this block -bzzz
    h->buffer_credits++;
    TRACE_ONLY( h->credits_used -= 1; )
  }
  return err;
}
#endif // #ifdef USE_JNL_EXTRA


///////////////////////////////////////////////////////////
// jnl_stop
//
// complete a transaction
//
// All done for a particular handle.
//
// There is not much action needed here.  We just return any remaining buffer credits to the transaction and remove the handle
// The only complication is that we need to start a commit operation if the
// filesystem is marked for synchronous update.
//
// jnl_stop itself will not usually return an error, but it may do so in unusual circumstances
// In particular, expect it to return -EIO if a jnl_abort has been executed since the transaction began
///////////////////////////////////////////////////////////
static int
jnl_stop(
    IN OUT handle_j **handle
    )
{
  handle_j *h = *handle;
  transaction *tr = h->tr;
  jnl *j = tr->journal;
  int err, wait_for_commit = 0;
  unsigned int tid;
  pid_t pid;

  if ( jnl_is_handle_aborted( h ) )
    err = -EIO;
  else {
    J_ASSERT( atomic_read( &tr->updates ) > 0 );
    err = 0;
  }

  *handle = NULL;

  jnl_debug( JNL_TRACE_LEVEL_HANDLE, "jnl_stop: h=%p, id=%x\n", h, tr->tid );

  //
  // Implement synchronous transaction batching.  If the handle was synchronous, don't force a commit immediately
  // Let's yield and let another thread piggyback onto this transaction
  // Keep doing that while new threads continue to arrive
  // It doesn't cost much - we're about to run a commit and sleep on IO anyway
  // Speeds up many-threaded, many-dir operations by 30x or more...
  //
  // We try and optimize the sleep time against what the underlying disk can do, instead of having a static sleep time
  // This is useful for the case where our storage is so fast that it is more optimal to go ahead and force a flush
  // and wait for the transaction to be committed than it is to wait for an arbitrary amount of time for new writers to
  // join the transaction
  // We achieve this by measuring how long it takes to commit a transaction, and compare it with how long this transaction
  // has been running, and if run time < commit time then we sleep for the delta and commit
  // This greatly helps super fast disks that would see slowdowns as more threads started doing fsyncs.
  //
  // But don't do this if this process was the most recent one to perform a synchronous write
  // We do this to detect the  case where a single process is doing a stream of sync writes
  // No point in waiting for joiners in that case.
  //

  pid = current->pid;
  if ( h->sync && j->last_sync_writer != pid ) {
#if defined HAVE_DECL_SCHEDULE_HRTIMEOUT && HAVE_DECL_SCHEDULE_HRTIMEOUT
    u64 commit_time, trans_time;

    j->last_sync_writer = pid;
    read_lock( &j->state_lock );
    commit_time = j->average_commit_time;
    read_unlock( &j->state_lock );

    trans_time  = ktime_to_ns( ktime_sub( ktime_get(), tr->start_time ) ); // GPL - ktime_get()
    commit_time = max_t( u64, commit_time, 1000*j->min_batch_time );
    commit_time = min_t( u64, commit_time, 1000*j->max_batch_time );
    if ( trans_time < commit_time ) {
      ktime_t expires = ktime_add_ns( ktime_get(), commit_time ); // GPL - ktime_get()
      set_current_state( TASK_UNINTERRUPTIBLE );
      schedule_hrtimeout( &expires, HRTIMER_MODE_ABS ); // GPL
    }
#else
    int old_handle_count = atomic_read( &tr->handle_count );
    j->last_sync_writer = pid;
    for ( ;; ){
      int new_handle_count;
      schedule_timeout_uninterruptible(1);
      new_handle_count = atomic_read( &tr->handle_count );
      if ( new_handle_count == old_handle_count )
        break;
      old_handle_count = new_handle_count;
    }
#endif
  }

  if ( h->sync )
    tr->synchronous_commit = 1;

  atomic_sub( h->buffer_credits, &tr->outstanding_credits );

#if 0 //def UFSD_DEBUG
//  if ( 0 != h->credits_used ) {
    if ( j->max_credits_used < h->credits_used ) {
      jnl_debug( 0, "credits used=%u\n", h->credits_used );
      j->max_credits_used = h->credits_used;
    }
//  }
#endif

  //
  // If the handle is marked SYNC, we need to set another commit going!
  // We also want to force a commit if the current transaction is occupying too much of the log, or if the transaction is too old now.
  //
  if ( h->sync
    || atomic_read( &tr->outstanding_credits ) > j->max_transaction_buffers
    || time_after_eq( jiffies, tr->expires ) ) {
    // Do this even for aborted journals: an abort still completes the commit thread, it just doesn't write anything to disk
    jnl_debug( JNL_TRACE_LEVEL_HANDLE, "transaction too old, requesting commit for handle %p, tr=%x\n", h, tr->tid );
    // This is non-blocking
    jnl_log_start_commit( j, tr->tid );
    //
    // Special case: JNL_SYNC synchronous updates require us to wait for the commit to complete.
    //
    if ( h->sync && !( current->flags & PF_MEMALLOC ) )
      wait_for_commit = 1;
  }

  //
  // Once we drop t_updates, if it goes to zero the transaction could start committing on us and eventually disappear
  // So once we do this, we must not dereference transaction pointer again
  //
  tid = tr->tid;
  if ( atomic_dec_and_test( &tr->updates ) ) {
    wake_up( &j->wait_updates );
    if ( j->barrier_count )
      wake_up( &j->wait_transaction_locked );
  }

  if ( wait_for_commit )
    err = jnl_log_wait_commit( j, tid );

  lock_map_release( &h->lockdep_map );
  kmem_cache_free( jnl_handle_cache, h );
  return err;
}


#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_force_commit
//
// force any uncommitted transactions
//
// For synchronous operations: force any uncommitted transactions to disk
// May seem kludgy, but it reuses all the handle batching code in a very simple manner
///////////////////////////////////////////////////////////
static int
jnl_force_commit(
    IN jnl *j
    )
{
  handle_j *handle;
  int err = jnl_start( j, 1, &handle );
  if ( err )
    return err;

  handle->sync = 1;
  return jnl_stop( &handle );
}


// int jnl_try_to_free_buffers() - try to free page buffers.
// @journal: journal for operation
// @page: to try and free
// @gfp_mask: we use the mask to detect how hard should we try to release
// buffers. If __GFP_WAIT and __GFP_FS is set, we wait for commit code to
// release the buffers.
//
//
// For all the buffers on this page,
// if they are fully written out ordered data, move them onto BUF_CLEAN
// so try_to_free_buffers() can reap them.
//
// This function returns non-zero if we wish try_to_free_buffers()
// to be called. We do this if the page is releasable by try_to_free_buffers().
// We also do it if the page has locked or dirty buffers and the caller wants
// us to perform sync or async writeout.
//
// This complicates JNL locking somewhat.  We aren't protected by the
// BKL here.  We wish to remove the buffer from its committing or
// running transaction's ->datalist via __jnl_unfile_buffer.
//
// This may *change* the value of tr->datalist, so anyone
// who looks at t_datalist needs to lock against this function.
//
// Even worse, someone may be doing a jnl_dirty_data on this
// buffer.  So we need to lock against that.  jnl_dirty_data()
// will come out of the lock with the buffer dirty, which makes it
// ineligible for release here.
//
// Who else is affected by this?  hmm...  Really the only contender
// is do_get_write_access() - it could be looking at the buffer while
// journal_jry_to_free_buffer() is changing its state.  But that
// cannot happen because we never reallocate freed data as metadata
// while the data is part of a transaction.  Yes?
//
// Return 0 on failure, 1 on success


///////////////////////////////////////////////////////////
// jnl_try_to_free_buffers
//
//
///////////////////////////////////////////////////////////
static int
jnl_try_to_free_buffers(
    IN jnl *j,
    IN struct page *page,
//    IN gfp_t gfp_mask
    IN unsigned gfp_mask
    )
{
  struct buffer_head *head;
  struct buffer_head *bh;
  int ret = 0;

  J_ASSERT( PageLocked( page ) );

  head  = page_buffers( page );
  bh    = head;
  do {
    jnl_head *jh;

    //
    // We take our own ref against the jnl_head here to avoid having to add tons of locking around each instance of jnl_put_hdr().
    //
    jh = jnl_grab_hdr( bh );
    if ( !jh )
      continue;

    jnl_lock_bh_state( bh );

    if ( !buffer_locked( bh ) && !buffer_dirty( bh ) && NULL == jh->next_tr ) {
      spin_lock( &j->list_lock );
      if ( jh->cp_tr != NULL && jh->tr == NULL ) {
        // written-back checkpointed metadata buffer
        if ( BJ_None == jh->jlist ) {
//          jnl_debug( JNL_TRACE_LEVEL_CHECK, "remove from checkpoint list\n" );
          jnl_remove_checkpoint( jh );
        }
      }
      spin_unlock( &j->list_lock );
    }

    jnl_put_hdr( jh );
    jnl_unlock_bh_state( bh );
    if ( buffer_jnl( bh ) )
      goto busy;
  } while ( ( bh = bh->b_this_page ) != head );

  ret = try_to_free_buffers( page );

busy:
  return ret;
}


///////////////////////////////////////////////////////////
// __dispose_buffer
//
// This buffer is no longer needed.  If it is on an older transaction's
// checkpoint list we need to record it on this transaction's forget list
// to pin this buffer ( and hence its checkpointing transaction ) down until
// this transaction commits.  If the buffer isn't on a checkpoint list, we
// release it.
// Returns non-zero if JNL no longer has an interest in the buffer.
//
// Called under list_lock.
//
// Called under jnl_lock_bh_state( bh ).
///////////////////////////////////////////////////////////
static int
__dispose_buffer(
    IN jnl_head *jh,
    IN transaction *tr
    )
{
  int may_free = 1;
  struct buffer_head *bh = jh2bh( jh );

  if ( jh->cp_tr ) {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "on running+cp transaction\n" );
    __jnl_temp_unlink_buffer( jh );

    //
    // We don't want to write the buffer anymore, clear the bit so that we don't confuse checks in __journal_file_buffer
    //
    clear_buffer_dirty( bh );
    __jnl_file_buffer( jh, tr, BJ_Forget );
    may_free = 0;
  } else {
    jnl_debug( JNL_TRACE_LEVEL_ACCESS, "on running transaction\n" );
    __jnl_unfile_buffer( jh );
  }
  return may_free;
}

/*
 * jnl_invalidatepage
 *
 * This code is tricky.  It has a number of cases to deal with.
 *
 * There are two invariants which this code relies on:
 *
 * i_size must be updated on disk before we start calling invalidatepage on the
 * data.
 *
 *  This is done in ext3 by defining an ext3_setattr method which
 *  updates i_size before truncate gets going.  By maintaining this
 *  invariant, we can be sure that it is safe to throw away any buffers
 *  attached to the current transaction: once the transaction commits,
 *  we know that the data will not be needed.
 *
 *  Note however that we can *not* throw away data belonging to the
 *  previous, committing transaction!
 *
 * Any disk blocks which *are* part of the previous, committing
 * transaction ( and which therefore cannot be discarded immediately ) are
 * not going to be reused in the new running transaction
 *
 *  The bitmap committed_data images guarantee this: any block which is
 *  allocated in one transaction and removed in the next will be marked
 *  as in-use in the committed_data bitmap, so cannot be reused until
 *  the next transaction to delete the block commits.  This means that
 *  leaving committing buffers dirty is quite safe: the disk blocks
 *  cannot be reallocated to a different file and so buffer aliasing is
 *  not possible.
 *
 *
 * The above applies mainly to ordered data mode.  In writeback mode we
 * don't make guarantees about the order in which data hits disk --- in
 * particular we don't guarantee that new dirty data is flushed before
 * transaction commit --- so it is always safe just to discard data
 * immediately in that mode.  --sct
 */

///////////////////////////////////////////////////////////
// journal_unmap_buffer
//
//
// The journal_unmap_buffer helper function returns zero if the buffer concerned remains pinned
// as an anonymous buffer belonging to an older transaction.
//
// We're outside-transaction here
// Either or both of running_tr and committing_tr may be NULL.
///////////////////////////////////////////////////////////
static int
journal_unmap_buffer(
    IN jnl *j,
    IN struct buffer_head *bh,
    IN int partial_page
    )
{
  transaction *tr;
  jnl_head *jh;
  int may_free = 1;
  int ret;

  jnl_debug( JNL_TRACE_LEVEL_IO, "+umap\n" );

  //
  // It is safe to proceed here without the list_lock because the buffers cannot be stolen by try_to_free_buffers as long as we are
  // holding the page lock. --sct
  //
  if ( !buffer_jnl( bh ) )
    goto zap_buffer_unlocked;

  // OK, we have data buffer in journaled mode
  write_lock( &j->state_lock );
  jnl_lock_bh_state( bh );
  spin_lock( &j->list_lock );

  jh = jnl_grab_hdr( bh );
  if ( !jh )
    goto zap_buffer_no_jh;

  //
  // We cannot remove the buffer from checkpoint lists until the transaction adding inode to orphan list ( let's call it T )
  // is committed.  Otherwise if the transaction changing the buffer would be cleaned from the journal before T is
  // committed, a crash will cause that the correct contents of the buffer will be lost.  On the other hand we have to
  // clear the buffer dirty bit at latest at the moment when the transaction marking the buffer as freed in the filesystem
  // structures is committed because from that moment on the block can be reallocated and used by a different page.
  // Since the block hasn't been freed yet but the inode has already been added to orphan list, it is safe for us to add
  // the buffer to BJ_Forget list of the newest transaction.
  //
	// Also we have to clear buffer_mapped flag of a truncated buffer because the buffer_head may be attached to the page straddling
  // i_size (can happen only when blocksize < pagesize) and thus the buffer_head can be reused when the file is extended again.
  // So we end up keeping around invalidated buffers attached to transactions' BJ_Forget list just to stop checkpointing code from cleaning up
  // the transaction this buffer was modified in.
  //
  tr = jh->tr;
  if ( NULL == tr ) {
    // First case: not on any transaction.  If it has no checkpoint link, then we can zap it:
    // it's a writeback-mode buffer so we don't care if it hits disk safely
    if ( !jh->cp_tr ) {
      jnl_debug( JNL_TRACE_LEVEL_IO, "not on any transaction: zap\n" );
      goto zap_buffer;
    }

    if ( !buffer_dirty( bh ) ) {
      // bdflush has written it.  We can drop it now
      goto zap_buffer;
    }

    // OK, it must be in the journal but still not written fully to disk: it's metadata or journaled data...
    if ( j->running_tr ) {
      // ... and once the current transaction has committed, the buffer won't be needed any longer
      jnl_debug( JNL_TRACE_LEVEL_IO, "checkpointed: add to BJ_Forget\n" );
      may_free = __dispose_buffer( jh, j->running_tr );
      goto zap_buffer;
    }

    // There is no currently-running transaction. So the orphan record which we wrote for this file must have
    // passed into commit.  We must attach this buffer to the committing transaction, if it exists
    if ( j->committing_tr ) {
      jnl_debug( JNL_TRACE_LEVEL_IO, "give to committing trans\n" );
      may_free = __dispose_buffer( jh, j->committing_tr );
      goto zap_buffer;
    }

    // The orphan record's transaction has committed.  We can cleanse this buffer
    clear_buffer_jnldirty( bh );
    goto zap_buffer;

  } else if ( tr == j->committing_tr ) {
    jnl_debug( JNL_TRACE_LEVEL_IO, "on committing transaction\n" );
		//
    // The buffer is committing, we simply cannot touch it.
    // If the page is straddling i_size we have to wait for commit and try again.
    //
		if ( partial_page ) {
			jnl_put_hdr( jh );
			spin_unlock( &j->list_lock );
			jnl_unlock_bh_state( bh );
			write_unlock( &j->state_lock );
			return -EBUSY;
		}

    //
    // OK, buffer won't be reachable after truncate. We just set j_next_transaction to the running transaction
    // (if there is one) and mark buffer as freed so that commit code knows it
    // should clear dirty bits when it is done with the buffer.
		//
    set_buffer_freed( bh );
    if ( j->running_tr && buffer_jnldirty( bh ) )
      jh->next_tr = j->running_tr;
    jnl_put_hdr( jh );
    spin_unlock( &j->list_lock );
    jnl_unlock_bh_state( bh );
    write_unlock( &j->state_lock );
    return 0;
  } else {
    // Good, the buffer belongs to the running transaction.
    // We are writing our own transaction's data, not any previous one's, so it is safe to throw it away
    // ( remember that we expect the filesystem to have set i_size already for this truncate so recovery will not
    // expose the disk blocks we are discarding here. )
    //
    J_ASSERT( tr == j->running_tr );
    jnl_debug( JNL_TRACE_LEVEL_IO, "on running transaction\n" );
    may_free = __dispose_buffer( jh, tr );
  }

zap_buffer:
  //
	// This is tricky. Although the buffer is truncated, it may be reused if blocksize < pagesize and it is attached to the page straddling EOF.
  // Since the buffer might have been added to BJ_Forget list of the running transaction, journal_get_write_access() won't clear
  // b_modified and credit accounting gets confused. So clear b_modified here.
	//
	jh->b_modified = 0;
  jnl_put_hdr( jh );
zap_buffer_no_jh:
  spin_unlock( &j->list_lock );
  jnl_unlock_bh_state( bh );
  write_unlock( &j->state_lock );
zap_buffer_unlocked:
  clear_buffer_dirty( bh );
  J_ASSERT( !buffer_jnldirty( bh ) );
  clear_buffer_mapped( bh );
  clear_buffer_req( bh );
  clear_buffer_new( bh );
  bh->b_bdev = NULL;
  return may_free;
}


///////////////////////////////////////////////////////////
// jnl_invalidatepage
//
// Reap page buffers containing data after offset in page
// Can return -EBUSY if buffers are part of the committing transaction and the page is straddling i_size.
// Caller then has to wait for current commit and try again
///////////////////////////////////////////////////////////
static int
jnl_invalidatepage(
    IN jnl *j,
    IN struct page *page,
    IN unsigned long offset
    )
{
  struct buffer_head *head, *bh, *next;
  unsigned int curr_off = 0;
  int may_free = 1;
  int ret;

  if ( !PageLocked( page ) )
    BUG();
  if ( !page_has_buffers( page ) )
    return 0;

  // We will potentially be playing with lists other than just the data lists ( especially for journaled data mode ), so be
  // cautious in our locking
  head = bh = page_buffers( page );
  do {
    unsigned int next_off = curr_off + bh->b_size;
    next = bh->b_this_page;
    if ( offset <= curr_off ) {
      // This block is wholly outside the truncation point
      lock_buffer( bh );
      ret = journal_unmap_buffer( j, bh );
      unlock_buffer( bh );
      if ( ret < 0 )
        return ret;
      may_free &= ret;
    }
    curr_off = next_off;
    bh = next;
  } while ( bh != head );

  if ( !offset && may_free && try_to_free_buffers( page ) ) {
    J_ASSERT( !page_has_buffers( page ) );
  }
  return 0;
}
#endif // #ifdef USE_JNL_EXTRA


///////////////////////////////////////////////////////////
// __jnl_refile_buffer
//
// Remove a buffer from its current buffer list in preparation for
// dropping it from its current transaction entirely.  If the buffer has
// already started to be used by a subsequent transaction, refile the
// buffer on that transaction's metadata list.
//
// Called under list_lock
// Called under jnl_lock_bh_state( jh2bh( jh ) )
//
// jh and bh may be already free when this function returns
///////////////////////////////////////////////////////////
static void
__jnl_refile_buffer(
    jnl_head *jh
    )
{
  int was_dirty, jlist;
  struct buffer_head *bh = jh2bh( jh );

  J_ASSERT( jnl_is_locked_bh_state( bh ) );
  if ( jh->tr )
    assert_spin_locked( &jh->tr->journal->list_lock );

  // If the buffer is now unused, just drop it
  if ( NULL == jh->next_tr ) {
    __jnl_unfile_buffer( jh );
    return;
  }

  //
  // It has been modified by a later transaction: add it to the new transaction's metadata list.
  //
  was_dirty = test_clear_buffer_jnldirty( bh );
  __jnl_temp_unlink_buffer( jh );

  //
  // We set transaction here because next_tr will inherit our jh reference and thus __jnl_file_buffer() must not
  // take a new one.
  //
  jh->tr = jh->next_tr;
  jh->next_tr = NULL;
  if ( buffer_freed( bh ) )
    jlist = BJ_Forget;
  else if ( jh->modified )
    jlist = BJ_Metadata;
  else
    jlist = BJ_Reserved;

  __jnl_file_buffer( jh, jh->tr, jlist );
  J_ASSERT( jh->tr->state == T_RUNNING );

  if ( was_dirty )
    set_buffer_jnldirty( bh );
}


///////////////////////////////////////////////////////////
// jnl_refile_buffer
//
// The jh and bh may be freed by this call.
///////////////////////////////////////////////////////////
static void
jnl_refile_buffer(
    IN jnl *j,
    IN jnl_head *jh
    )
{
  struct buffer_head *bh = jh2bh( jh );

  // Get reference so that buffer cannot be freed before we unlock it
  get_bh( bh );
  jnl_lock_bh_state( bh );
  spin_lock( &j->list_lock );
  __jnl_refile_buffer( jh );
  jnl_unlock_bh_state( bh );
  spin_unlock( &j->list_lock );
  __brelse( bh );
}


///////////////////////////////////////////////////////////
// jnl_init_revoke_table
//
//
///////////////////////////////////////////////////////////
static jnl_revoke_table*
jnl_init_revoke_table( void )
{
  int i = 0;

  jnl_revoke_table *t = kmalloc( JOURNAL_REVOKE_DEFAULT_HASH * sizeof( struct list_head ), GFP_KERNEL );
  if ( NULL == t )
    return NULL;

  for ( i = 0; i < JOURNAL_REVOKE_DEFAULT_HASH; i++ )
    INIT_LIST_HEAD( &t[i] );

  return t;
}


///////////////////////////////////////////////////////////
// jnl_destroy_revoke_table
//
//
///////////////////////////////////////////////////////////
static void
jnl_destroy_revoke_table(
    IN jnl_revoke_table *table
    )
{
  int i;

  for ( i = 0; i < JOURNAL_REVOKE_DEFAULT_HASH; i++ ) {
    struct list_head *hash_list = &table[i];
    J_ASSERT( list_empty( hash_list ) );
  }

  kfree( table );
}


///////////////////////////////////////////////////////////
// jnl_init_revoke
//
// Initialise the revoke table for a given journal to a given size
///////////////////////////////////////////////////////////
static int
jnl_init_revoke(
    IN jnl *j
    )
{
  J_ASSERT( NULL == j->revoke_table[0] );
  J_ASSERT( is_power_of_2( JOURNAL_REVOKE_DEFAULT_HASH ) );

  j->revoke_table[0] = jnl_init_revoke_table();
  if ( NULL == j->revoke_table[0] )
    return -ENOMEM;

  j->revoke_table[1] = jnl_init_revoke_table();
  if ( NULL == j->revoke_table[1] ){
    jnl_destroy_revoke_table( j->revoke_table[0] );
    return -ENOMEM;
  }

  j->revoke = j->revoke_table[1];

  spin_lock_init( &j->revoke_lock );
  return 0;
}


///////////////////////////////////////////////////////////
// jnl_destroy_revoke
//
// Destroy a journal's revoke table.  The table must already be empty!
///////////////////////////////////////////////////////////
static void
jnl_destroy_revoke(
    IN jnl *j
    )
{
  j->revoke = NULL;
  if ( NULL != j->revoke_table[0] )
    jnl_destroy_revoke_table( j->revoke_table[0] );
  if ( NULL != j->revoke_table[1] )
    jnl_destroy_revoke_table( j->revoke_table[1] );
}


#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_revoke
//
// revoke a given buffer_head from the journal.  This prevents the block from being replayed during recovery if we take a
// crash after this current transaction commits.  Any subsequent metadata writes of the buffer in this transaction cancel the revoke
//
// Note that this call may block --- it is up to the caller to make sure that there are no further calls to journal_write_metadata
// before the revoke is complete.  In ext3, this implies calling the revoke before clearing the block bitmap when we are deleting metadata
//
// Revoke performs a jnl_forget on any buffer_head passed in as a parameter, but does _not_ forget the buffer_head if the bh was only
// found implicitly.
//
// bh_in may not be a journalled buffer - it may have come off the hash tables without an attached jnl_head
//
// If bh_in is non-zero, jnl_revoke() will decrement its count by one
///////////////////////////////////////////////////////////
static int
jnl_revoke(
    IN handle_j *handle,
    IN unsigned long blocknr,
    IN struct buffer_head *bh_in
    )
{
  struct buffer_head *bh = NULL;
  jnl *j;
  struct block_device *bdev;

  might_sleep();
  if ( NULL != bh_in )
    jnl_debug( JNL_TRACE_LEVEL_REVOKE, "+revoke" );

  j     = handle->tr->journal;
  bdev  = j->fs_dev;
  bh    = bh_in;

  if ( NULL == bh ) {
    bh = __find_get_block( bdev, blocknr, j->blocksize );
    if ( bh )
      jnl_debug( JNL_TRACE_LEVEL_REVOKE, "found on hash\n" );
  }

#ifdef JNL_EXPENSIVE_CHECKING
  else {
    // If there is a different buffer_head lying around in memory anywhere...
    struct buffer_head *bh2 = __find_get_block( bdev, blocknr, j->blocksize );
    if ( NULL != bh2 ) {
      // ... and it has RevokeValid status...
      if ( bh2 != bh && buffer_revokevalid( bh2 ) ) {
        // ...then it better be revoked too, since it's illegal to create a revoke
        // record against a buffer_head which is not marked revoked --- that would
        // risk missing a subsequent revoke cancel
        J_ASSERT( buffer_revoked( bh2 ) );
      }
      put_bh( bh2 );
    }
  }
#endif

  // We really ought not ever to revoke twice in a row without first having the revoke cancelled: it's illegal to free a
  // block twice without allocating it in between!
  if ( NULL != bh ) {
    if ( !J_EXPECT( !buffer_revoked( bh ), "inconsistent data on disk" ) ) {
      if ( !bh_in )
        brelse( bh );
      return -EIO;
    }

    set_buffer_revoked( bh );
    set_buffer_revokevalid( bh );
    if ( NULL != bh_in ) {
      jnl_forget( handle, bh_in );
    } else {
      jnl_debug( JNL_TRACE_LEVEL_IO, "brelse b=%"PSCT"x\n", bh->b_blocknr );
      __brelse( bh );
    }
  }

  jnl_debug( JNL_TRACE_LEVEL_REVOKE, "insert revoke for block %lx, bh_in=%p\n", blocknr, bh_in );

  for( ;; ) {
    jnl_revoke_record *r = kmem_cache_alloc( jnl_revoke_record_cache, GFP_NOFS );
    if ( NULL != r ) {
      struct list_head *hash_list = &j->revoke[ hash( blocknr ) ];
//      r->sequence = seq; // handle->tr->tid
      r->blocknr = blocknr;
      spin_lock( &j->revoke_lock );
      list_add( &r->hash, hash_list );
      spin_unlock( &j->revoke_lock );
      return 0;
    }
    if ( !journal_oom_retry )
      return -ENOMEM;
    jnl_debug( 0, "ENOMEM, retrying\n" );
    yield();
  }
}
#endif // #ifdef USE_JNL_EXTRA


///////////////////////////////////////////////////////////
// jnl_next_log_block
//
//
///////////////////////////////////////////////////////////
static unsigned long
jnl_next_log_block(
    IN jnl *j
    )
{
  unsigned long blocknr;

  write_lock( &j->state_lock );
  J_ASSERT( j->free > 1 );
  blocknr = j->head;
  j->head++;
  j->free--;
  if ( j->head == j->last ) {
    j->head = j->first;
#ifdef UFSD_TRACE
    j->hdr->overlaps += 1;
    jnl_debug( 0, "overlaps=%x\n", j->hdr->overlaps );
#endif
  }
  write_unlock( &j->state_lock );

  return blocknr;
}


///////////////////////////////////////////////////////////
// jnl_get_blhdr_buffer
//
// We play buffer_head aliasing tricks to write data/metadata blocks to
// the journal without copying their contents, but for journal
// blhdr blocks we do need to generate bona fide buffers.
//
// After the caller of jnl_get_blhdr_buffer() has finished modifying
// the buffer's contents they really should run flush_dcache_page( bh->b_page ).
// But we don't bother doing that, so there will be coherency problems with
// mmaps of blockdevs which hold live JNL-controlled filesystems.
///////////////////////////////////////////////////////////
static jnl_head*
jnl_get_blhdr_buffer(
    IN jnl *j
    )
{
  struct buffer_head *bh;
  unsigned long blocknr = jnl_next_log_block( j );

  bh = __getblk( j->dev, blocknr, j->blocksize );
  if ( NULL == bh )
    return NULL;

  lock_buffer( bh );
  memset( bh->b_data, 0, j->blocksize );
  set_buffer_uptodate( bh );
  unlock_buffer( bh );
//  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "get_blhdr_buffer -> b=%"PSCT"x \n", bh->b_blocknr );
  return jnl_add_hdr( bh );
}


///////////////////////////////////////////////////////////
// jnl_end_buffer_io_sync
//
// Default IO end handler for temporary BJ_IO buffer_heads.
///////////////////////////////////////////////////////////
static void
jnl_end_buffer_io_sync(
    IN struct buffer_head *bh,
    IN int uptodate
    )
{
  jnl_debug( JNL_TRACE_LEVEL_IO, "end_io: b=%"PSCT"x\n", bh->b_blocknr );
  if ( uptodate )
    set_buffer_uptodate( bh );
  else
    clear_buffer_uptodate( bh );
  unlock_buffer( bh );
}


///////////////////////////////////////////////////////////
// release_buffer_page
//
// When an ext4 file is truncated, it is possible that some pages are not
// successfully freed, because they are attached to a committing transaction.
// After the transaction commits, these pages are left on the LRU, with no
// ->mapping, and with attached buffers.  These pages are trivially reclaimable
// by the VM, but their apparent absence upsets the VM accounting, and it makes
// the numbers in /proc/meminfo look odd.
//
// So here, we have a buffer which has just come off the forget list.  Look to
// see if we can strip all buffers from the backing page.
//
// Called under lock_journal(), and possibly under journal_datalist_lock.  The
// caller provided us with a ref against the buffer, and we drop that here.
///////////////////////////////////////////////////////////
static void
release_buffer_page(
    IN struct buffer_head *bh
    )
{
  struct page *page;
  if ( !buffer_dirty( bh )
    && 1 == atomic_read( &bh->b_count )
    && NULL != ( page = bh->b_page )
    && NULL != page->mapping
    // OK, it's a truncated page
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    && trylock_page( page )
#else
    && !test_and_set_bit( PG_locked, &page->flags )
#endif
    )
  {
    page_cache_get( page );
    __brelse( bh );
    try_to_free_buffers( page );
    unlock_page( page );
    page_cache_release( page );
  } else {
    __brelse( bh );
  }
}


///////////////////////////////////////////////////////////
// jnl_submit_blhdr
//
// We should have cleaned up our previous buffers by now, so if we are in abort mode we can now just skip the rest of the journal write entirely
//
// Returns 1 if the journal needs to be aborted or 0 on success
///////////////////////////////////////////////////////////
static int
jnl_submit_blhdr(
    IN jnl *j,
    IN transaction *tr,
    OUT struct buffer_head* *cbh
    )
{
  jnl_header *hdr = j->hdr;
  struct buffer_head *bh  = j->hdr_buffer;
  sector_t blocknr = bh->b_blocknr;
  jnl_head *jh;
  int ret;

//  *cbh = NULL; // Not neccessary 'cause *cbh is already NULL

  if ( jnl_is_aborted( j ) )
    return 0;

  jh  = jnl_add_hdr( bh );
  UNREFERENCED_PARAMETER( jh );
//  J_ASSERT( tr->log_start != tr->log_end );
//  hdr->start    = ( tr->log_start - bh->b_blocknr ) << j->blocksize_bits;
  hdr->end      = ( tr->log_end - blocknr ) << j->blocksize_bits;
  hdr->checksum = 0;
  hdr->checksum = CalcCheckSum( hdr, JOURNAL_HEADER_CKSUM_SIZE );

  jnl_debug( JNL_TRACE_LEVEL_HDR, "submit hdr b=%"PSCT"x : [%llx - %llx), [%llx - %llx)\n", blocknr, hdr->start, hdr->end,
             (hdr->start >> j->blocksize_bits) + blocknr, (hdr->end >> j->blocksize_bits) + blocknr );
  lock_buffer( bh );
  clear_buffer_dirty( bh );
  set_buffer_uptodate( bh );
  get_bh( bh );
  bh->b_end_io = jnl_end_buffer_io_sync;

#if !defined async_commit && defined WRITE_FLUSH_FUA
  if ( ( j->flags & JNL_FLAGS_BARRIER ) )
    ret = submit_bh( WRITE_SYNC | WRITE_FLUSH_FUA, bh );
  else
#endif
    ret = submit_bh( WRITE_SYNC, bh );

  *cbh = bh;

  return ret;
}


///////////////////////////////////////////////////////////
// jnl_clean_one_cp_list
//
// Find all the written-back checkpoint buffers in the given list and release them
// Returns number of bufers reaped ( for debug )
///////////////////////////////////////////////////////////
static int
jnl_clean_one_cp_list(
    IN jnl_head *jh,
    OUT int *released
    )
{
  int freed = 0;
  *released = 0;

  if ( NULL != jh ) {
    jnl_head *last_jh = jh->cpprev;
    jnl_head *next_jh = jh;

    do {
      jh      = next_jh;
      next_jh = jh->cpnext;
      // Use trylock because of the ranking
      if ( jnl_trylock_bh_state( jh2bh( jh ) ) ) {

        //
        // Try to release a checkpointed buffer from its transaction.
        // Returns 1 if we released it and 2 if we also released the whole transaction.
        //
        int ret = 0;
        struct buffer_head *bh = jh2bh( jh );
        if ( BJ_None == jh->jlist && !buffer_locked( bh )
          && !buffer_dirty( bh ) && !buffer_write_io_error( bh ) ) {
          //
          // Get our reference so that bh cannot be freed before we unlock it
          //
          get_bh( bh );
//          jnl_debug( JNL_TRACE_LEVEL_CHECK, "remove from checkpoint list\n" );
          ret = jnl_remove_checkpoint( jh ) + 1;
          jnl_unlock_bh_state( bh );
          jnl_debug( JNL_TRACE_LEVEL_IO, "brelse b=%"PSCT"x\n", bh->b_blocknr );
          __brelse( bh );
        } else {
          jnl_unlock_bh_state( bh );
        }

        if ( ret ) {
          freed++;
          if ( ret == 2 ) {
            *released = 1;
            return freed;
          }
        }
      }

      //
      // This function only frees up some memory if possible so we dont have an obligation
      // to finish processing. Bail out if preemption requested:
      //
      if ( need_resched() )
        return freed;
    } while ( jh != last_jh );
  }

  return freed;
}


///////////////////////////////////////////////////////////
// jnl_write_metadata_buffer
//
// Writes a metadata buffer to the journal
// The actual IO is not performed but a new buffer_head is constructed which labels the data to be written with the correct destination disk block.
//
// If the source buffer has already been modified by a new transaction since we took the last commit snapshot, we use the frozen copy of that data for IO
// If we end up using the existing buffer_head's data for the write, then we *have* to lock the buffer to prevent anyone
// else from using and possibly modifying it while the IO is in progress.
//
// The function returns a pointer to the buffer_heads to be used for IO.
//
// We assume that the journal has already been locked in this function.
///////////////////////////////////////////////////////////
static void
jnl_write_metadata_buffer(
    IN transaction *tr,
    IN jnl_head *jh_in,
    OUT jnl_head **jh_out,
    IN unsigned long blocknr
    )
{
  struct buffer_head *new_bh;
  jnl_head *new_jh;
  struct page *new_page;
  unsigned int new_offset;
  struct buffer_head *bh_in = jh2bh( jh_in );
  jnl *j = tr->journal;

  //
  // The buffer really shouldn't be locked: only the current committing transaction is allowed to write it,
  // so nobody else is allowed to do any IO.
  //
  // akpm: except if we're journalling data, and write() output is also part of a shared mapping, and another thread has
  // decided to launch a writepage() against this buffer.
  //
  J_ASSERT( buffer_jnldirty( bh_in ) );

  while( NULL == ( new_bh = alloc_buffer_head( GFP_NOFS ) ) ) {
    //
    // Failure is not an option, but __GFP_NOFAIL is going away; so we retry ourselves here.
    //
    congestion_wait( 0, HZ/50 );//BLK_RW_ASYNC, HZ/50 );
  }

  // keep subsequent assertions sane
  new_bh->b_state = 0;
  init_buffer( new_bh, NULL, NULL );
  atomic_set( &new_bh->b_count, 1 );
  new_jh = jnl_add_hdr( new_bh ); // This sleeps

  //
  // If a new transaction has already done a buffer copy-out, then we use that version of the data for the commit.
  //
  jnl_lock_bh_state( bh_in );
//repeat:
  if ( NULL != jh_in->frozen_data ) {
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "b=%lx uses frozen data of b=%"PSCT"x\n", blocknr, bh_in->b_blocknr );
    new_page   = virt_to_page( jh_in->frozen_data );
    new_offset = offset_in_page( jh_in->frozen_data );
  } else {
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "b=%lx uses original data of b=%"PSCT"x\n", blocknr, bh_in->b_blocknr );
    new_page   = bh_in->b_page;
    new_offset = bh_in->b_size < j->blocksize? 0 : offset_in_page( bh_in->b_data );
  }

  set_bh_page( new_bh, new_page, new_offset );
  new_jh->tr        = NULL;
  new_bh->b_size    = j->blocksize;//bh_in->b_size;
  new_bh->b_bdev    = tr->journal->dev;
  new_bh->b_blocknr = blocknr;
  set_buffer_mapped( new_bh );
  set_buffer_dirty( new_bh );

  *jh_out = new_jh;
  //
  // The to-be-written buffer needs to get moved to the io queue, and the original buffer whose contents we are shadowing or
  // copying is moved to the transaction's shadow queue.
  //
  spin_lock( &j->list_lock );
  __jnl_file_buffer( jh_in, tr, BJ_Shadow );
  spin_unlock( &j->list_lock );
  jnl_unlock_bh_state( bh_in );

  jnl_file_buffer( new_jh, tr, BJ_IO );

//  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "file b=%"PSCT"x as BJ_Shadow and b=%"PSCT"x as BJ_IO\n", bh_in->b_blocknr, new_bh->b_blocknr );
}


///////////////////////////////////////////////////////////
// jnl_commit_transaction
//
// The primary function for committing a transaction to the log
// This function is called by the journal thread to begin a complete commit.
///////////////////////////////////////////////////////////
static void
jnl_commit_transaction(
    IN jnl *j
    )
{
#ifdef USE_JNL_STATS
  transaction_stats stats;
#endif
  transaction *tr;
  jnl_head *jh, *new_jh, *blhdr_jh;
  struct buffer_head **wbuf = j->wbuf;
  struct buffer_head *bh, *new_bh;
  int bufs;
  int err;
  unsigned long blocknr;
  block_info *bi;
  block_list_header *blhdr;
  ktime_t start_time;
  u64 commit_time;
  int i, to_free = 0;
  struct buffer_head *cbh = NULL;
  jnl_revoke_table *revoke;
  struct blk_plug plug;
  unsigned int blocksize = j->blocksize;
//  int test_tail = 0;
//  TRACE_ONLY( sector_t jsb_block = j->hdr_buffer->b_blocknr; )

  //
  // First job: lock down the current transaction and wait for all outstanding updates to complete
  //
  if ( j->flags & JNL_FLAGS_FLUSHED )
    jnl_update_hdr( j, 1 );

  J_ASSERT( j->running_tr != NULL );
  J_ASSERT( j->committing_tr == NULL );
  tr = j->running_tr;
  J_ASSERT( tr->state == T_RUNNING );

//  trace_jnl__start_commit( journal, commit_transaction );
  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: starting commit of transaction %x\n", tr->tid );

  write_lock( &j->state_lock );
  tr->state = T_LOCKED;

#ifdef USE_JNL_STATS
  stats.run.wait    = tr->max_wait;
  stats.run.locked  = jiffies;
  stats.run.running = jnl_time_diff( tr->start_jiffies, stats.run.locked );
#endif

  spin_lock( &tr->handle_lock );

  while ( atomic_read( &tr->updates ) ) {
    DEFINE_WAIT( wait );
    prepare_to_wait( &j->wait_updates, &wait, TASK_UNINTERRUPTIBLE );
    if ( atomic_read( &tr->updates ) ) {
      spin_unlock( &tr->handle_lock );
      write_unlock( &j->state_lock );
      schedule();
      write_lock( &j->state_lock );
      spin_lock( &tr->handle_lock );
    }
    finish_wait( &j->wait_updates, &wait );
  }

  spin_unlock( &tr->handle_lock );

  J_ASSERT( atomic_read( &tr->outstanding_credits ) <= j->max_transaction_buffers );

  //
  // First thing we are allowed to do is to discard any remaining BJ_Reserved buffers
  // Note, it is _not_ permissible to assume that there are no such buffers: if a large filesystem operation
  // like a truncate needs to split itself over multiple transactions, then it may try to do a jnl_restart() while
  // there are still BJ_Reserved buffers outstanding
  // These must be released cleanly from the current transaction
  //
  // In this case, the filesystem must still reserve write access again before modifying the buffer in the new transaction, but
  // we do not require it to remember exactly which old buffers it has reserved
  // This is consistent with the existing behavior that multiple jnl_get_write_access() calls to the same
  // buffer are perfectly permissible
  //
  while ( tr->reserved_list ) {
    jh = tr->reserved_list;
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "reserved, unused: refile\n" );

    //
    // A jnl_get_undo_access()+jnl_release_buffer() may leave undo-committed data.
    //
    if ( jh->committed_data ) {
      bh = jh2bh( jh );
      jnl_lock_bh_state( bh );
      jnl_free( jh->committed_data, bh->b_size );
      jh->committed_data = NULL;
      jnl_unlock_bh_state( bh );
    }
    jnl_refile_buffer( j, jh );
  }

  //
  // Now try to drop any written-back buffers from the journal's checkpoint lists
  // We do this *before* commit because it potentially frees some memory
  //
  spin_lock( &j->list_lock );

  //
  // Find all the written-back checkpoint buffers in the journal and release them
  //
  if ( NULL != j->checkpoint_tr ) {
    transaction *tr   = j->checkpoint_tr;
    transaction *last = tr->cpprev;
    transaction *next = tr;
    int ret = 0;
    int released;

    do {
      tr   = next;
      next = tr->cpnext;
      ret += jnl_clean_one_cp_list( tr->checkpoint_list, &released );

      //
      // This function only frees up some memory if possible so we dont have an obligation to finish processing. Bail out if preemption requested:
      //
      if ( need_resched() )
        break;

      if ( released )
        continue;

      //
      // It is essential that we are as careful as in the case of t_checkpoint_list with removing the buffer from the list as
      // we can possibly see not yet submitted buffers on io_list
      ret += jnl_clean_one_cp_list( tr->checkpoint_io_list, &released );
      if ( need_resched() )
        break;
    } while ( tr != last );
  }

  spin_unlock( &j->list_lock );

//  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 1\n" );

  //
  // Clear revoked flag to reflect there is no revoked buffers
  // in the next transaction which is going to be started.
  //
  jnl_clear_buffer_revoked_flags( j );

  //
  // Switch to a new revoke table.
  // Select revoke for next transaction we do not want to suspend any processing until all revokes are written
  //
  revoke = j->revoke;
  if ( revoke == j->revoke_table[0] )
    j->revoke = j->revoke_table[1];
  else
    j->revoke = j->revoke_table[0];

  for ( i = 0; i < JOURNAL_REVOKE_DEFAULT_HASH; i++ )
    INIT_LIST_HEAD( &j->revoke[i] );

//  trace_jnl__commit_flushing( journal, commit_transaction );
#ifdef USE_JNL_STATS
  stats.run.flushing = jiffies;
  stats.run.locked   = jnl_time_diff( stats.run.locked, stats.run.flushing );
#endif

  tr->state         = T_FLUSH;
  j->committing_tr  = tr;
  j->running_tr     = NULL;
  start_time        = ktime_get(); // GPL - ktime_get()
  tr->log_start     = j->head;
  wake_up( &j->wait_transaction_locked );
  write_unlock( &j->state_lock );

//  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 2\n" );

  //
  // Now start flushing things to disk, in the order they appear on the transaction lists.  Data blocks go first.
  //
//      err = journal_submit_data_buffers( journal, commit_transaction );
//      if ( err )
//              jnl_abort( journal, err );

//  blk_start_plug( &plug );


//  blk_finish_plug( &plug );

//  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 2\n" );

  //
  // Loop over the transaction's entire buffer list:
  //
  write_lock( &j->state_lock );
  tr->state = T_COMMIT;
  write_unlock( &j->state_lock );

//  trace_jnl__commit_logging( journal, tr );
#ifdef USE_JNL_STATS
  stats.run.logging  = jiffies;
  stats.run.flushing = jnl_time_diff( stats.run.flushing, stats.run.logging );
  stats.run.blocks   = atomic_read( &tr->outstanding_credits );
  stats.run.blocks_logged = 0;
#endif

  J_ASSERT( tr->nr_buffers <= atomic_read( &tr->outstanding_credits ) );

  err       = 0;
  blhdr_jh  = NULL;
  bufs      = 0;
  bi        = NULL;
  blhdr     = NULL;
  blk_start_plug( &plug );

  for ( ;; ) {
    //
    // Find the next buffer to be journaled...
    //
    jh = tr->buffers;
    if ( NULL == jh ) {
//      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "commit_tr: no more buffers\n" );
      break;
    }

//    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "commit_tr: + b=%"PSCT"x\n", jh2bh( jh )->b_blocknr );
    // If we're in abort mode, we just un-journal the buffer and release it
    if ( jnl_is_aborted( j ) ) {
      clear_buffer_jnldirty( jh2bh( jh ) );
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "journal is aborting: refile\n" );
//      jnl__buffer_abort_trigger( jh, jh->frozen_data ? jh->frozen_triggers : jh->triggers );
      jnl_refile_buffer( j, jh );
      // If that was the last one, we need to clean up any blhdr buffers which may have been
      // already allocated, even if we are now aborting
      if ( !tr->buffers )
        goto start_journal_io;
      continue;
    }

    //
    // Make sure we have a 'blhdr_jh' block in which to record the metadata buffer
    //
    if ( NULL == blhdr_jh ) {
      J_ASSERT ( bufs == 0 );

      blhdr_jh = jnl_get_blhdr_buffer( j );
      if ( NULL == blhdr_jh ) {
        jnl_abort( j, -EIO );
        continue;
      }
      bh = jh2bh( blhdr_jh );
      J_ASSERT( blocksize == bh->b_size );

      blhdr = (block_list_header*)bh->b_data;
      if ( NULL == bi ) {
        J_ASSERT( blhdr_jh->bh->b_blocknr == tr->log_start );
        blhdr->flags = BLHDR_FLAGS_CHECK_CHECKSUMS | BLHDR_FLAGS_FIRST_HEADER;
      } else {
        blhdr->flags = BLHDR_FLAGS_CHECK_CHECKSUMS;
      }

      bi = blhdr->binfo;
      blhdr->bytes_used = blocksize;
      blhdr->num_blocks = 1;

      set_buffer_jwrite( bh );
      set_buffer_dirty( bh );
      wbuf[bufs++] = bh;

      // Record it so that we can wait for IO completion later
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph3: file b=%"PSCT"x as blhdr\n", bh->b_blocknr );
      jnl_file_buffer( blhdr_jh, tr, BJ_LogCtl );
    }

    bh  = jh2bh( jh );

    // Where is the buffer to be written?
    blocknr = jnl_next_log_block( j );

    //
    // jnl_start_handle() uses t_outstanding_credits to determine the free space in the log, but this counter is changed
    // by jnl_next_log_block() also.
    //
    atomic_dec( &tr->outstanding_credits );

    // Bump count to prevent truncate from stumbling over the shadowed buffer!
    // @@@ This can go if we ever get rid of the BJ_IO/BJ_Shadow pairing of buffers
    atomic_inc( &bh->b_count );

    // Make a temporary IO buffer with which to write it out
    // ( this will requeue both the metadata buffer and the temporary IO buffer ). new_bh goes on BJ_IO
    set_bit( BH_JWrite, &bh->b_state );

    //
    // akpm: jnl_write_metadata_buffer() sets new_bh->tr to commit_transaction.
    // We need to clean this up before we release new_bh ( which is of type BJ_IO )
    //
#ifdef UFSD_DEBUG
    if ( -1 == bi[-1].bnum ) {
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph3: %lx <=> sparse\n", blocknr );
    } else {
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph3: %lx <=> %"PSCT"x\n", blocknr, bh->b_blocknr );
//      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph3: %lx (BJ_IO) <=> %"PSCT"x (BJ_Shadow)\n", blocknr, bh->b_blocknr );
    }
#endif
    jnl_write_metadata_buffer( tr, jh, &new_jh, blocknr );

    new_bh = jh2bh( new_jh );
    set_bit( BH_JWrite, &new_bh->b_state );
    wbuf[bufs++] = new_bh;

    //
    // If block is revoked then mark it as '-1'
    //
    if ( NULL != find_revoke_record( j, revoke, bh->b_blocknr ) ) {
      bi->bnum = -1;
    } else {

      unsigned off;
      struct page *page = new_bh->b_page;
      char *addr = atomic_kmap( page );

      if ( unlikely( bh->b_size != blocksize ) ) {
        J_ASSERT( 512 == bh->b_size );
        bi->bnum  = bh->b_blocknr >> j->blocksize_bits_9;
        off       = 0;
//        test_tail = 1;
      } else {
        bi->bnum  = bh->b_blocknr;
        off       = offset_in_page( bh->b_data );
      }
      bi->cksum = CalcCheckSum( addr + off, blocksize );
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph3: [%x, %x) => %x\n", off, off + blocksize, bi->cksum );
      atomic_kunmap( addr );
    }

    bi->bsize = blocksize;
    blhdr->bytes_used += blocksize;
    blhdr->num_blocks += 1;
    bi  += 1;

    //
    // If there's no more to do, or if the 'blhdr' is full, let the IO rip!
    //
    if ( blhdr->num_blocks == blhdr->max_blocks || bufs >= j->wbufsize || NULL == tr->buffers ) {
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: Submit %d IOs\n", bufs );

      blhdr->max_blocks   = j->wbufsize;
      blhdr->sequence_num = tr->tid;
      J_ASSERT( 0 == blhdr->next );
      if ( NULL != tr->buffers )
        blhdr->next = -1; // Set non zero to indicate that this not the last block header in transaction

      J_ASSERT( 0 == blhdr->checksum );
      blhdr->checksum = CalcCheckSum( blhdr, BLHDR_CHECKSUM_SIZE );

start_journal_io:
      for ( i = 0; i < bufs; i++ ) {
        bh = wbuf[i];
//        jnl_debug( JNL_TRACE_LEVEL_IO, "To jnl %"PSCT"x%s\n", bh->b_blocknr, 0 == i? ", blhdr" : "" );
        lock_buffer( bh );
        clear_buffer_dirty( bh );
        set_buffer_uptodate( bh );
        bh->b_end_io = jnl_end_buffer_io_sync;
        submit_bh( WRITE_SYNC, bh );
      }

      cond_resched();
#ifdef USE_JNL_STATS
      stats.run.blocks_logged += bufs;
#endif

      // Force a new 'blhdr_jh' to be generated next time round the loop
      blhdr_jh = NULL;
      bufs = 0;
    }
  }

  tr->log_end = j->head;

  //
  // Free revoke memory
  //
  for ( i = 0; i < JOURNAL_REVOKE_DEFAULT_HASH; i++ ) {
    struct list_head *hash_list = &revoke[i];
    while ( !list_empty( hash_list ) ) {
      jnl_revoke_record *record = (jnl_revoke_record*)hash_list->next;
      list_del( &record->hash );
      kmem_cache_free( jnl_revoke_record_cache, record );
    }
  }

//  err = journal_finish_inode_data_buffers( j, tr );
//  if ( err ) {
//    printk( KERN_WARNING "jnl: Detected IO errors while flushing file data on %s\n", j->devname );
//    if ( j->flags & JNL_FLAGS_ABORT_ON_SYNCDATA_ERR )
//      jnl_abort( journal, err );
//    err = 0;
//  }

  write_lock( &j->state_lock );
  J_ASSERT( tr->state == T_COMMIT );
  tr->state = T_COMMIT_DFLUSH;
  write_unlock( &j->state_lock );

  //
  // If the journal is not located on the file system device, then we must flush the file system device before we issue the commit record
  //
  if ( tr->need_data_flush && j->fs_dev != j->dev && ( j->flags & JNL_FLAGS_BARRIER ) )
    Blkdev_issue_flush( j->fs_dev );

#if 0 //def async_commit
  // Done it all: now write the commit record asynchronously
  err = jnl_submit_blhdr( j, tr, &cbh );
  if ( err )
    __jnl_abort_hard( j );
#endif

  blk_finish_plug( &plug );

  // Lo and behold: we have just managed to send a transaction to the log.  Before we can commit it, wait for the IO so far to complete
  // Control buffers being written are on the transaction's t_log_list queue, and metadata buffers are on the t_iobuf_list queue.
  //
  // Wait for the buffers in reverse order. That way we are less likely to be woken up until all IOs have completed, and
  // so we incur less scheduling load.
  //
  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 3, tr=%x\n", tr->tid );

  //
  // akpm: these are BJ_IO, and list_lock is not needed. See __journal_jry_to_free_buffer.
  //
wait_for_iobuf:
  while ( NULL != tr->iobuf_list ) {
    TRACE_ONLY( sector_t jblock; )
    jh = tr->iobuf_list->tprev;
    bh = jh2bh( jh );
    if ( buffer_locked( bh ) ) {
      wait_on_buffer( bh );
      goto wait_for_iobuf;
    }

    if ( cond_resched() )
      goto wait_for_iobuf;

    if ( unlikely( !buffer_uptodate( bh ) ) )
      err = -EIO;

    TRACE_ONLY( jblock = bh->b_blocknr; )
    clear_buffer_jwrite( bh );

//    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph4: free b=%"PSCT"x\n", bh->b_blocknr );
    jnl_unfile_buffer( j, jh );

    //
    // ->iobuf_list should contain only dummy buffer_heads which were created by jnl_write_metadata_buffer().
    //
//    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "dumping temporary bh\n" );
    jnl_put_hdr( jh );
    __brelse( bh );
    J_ASSERT( atomic_read( &bh->b_count ) == 0 );
    free_buffer_head( bh );

    //
    // We also have to unlock and free the corresponding shadowed buffer
    //
    jh = tr->shadow_list->tprev;
    bh = jh2bh( jh );
    clear_bit( BH_JWrite, &bh->b_state );
    J_ASSERT( buffer_jnldirty( bh ) );

    // The metadata is now released for reuse, but we need to remember it against this transaction so that when
    // we finally commit, we can do any checkpointing required.
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph4: free b=%"PSCT"x and file b=%"PSCT"x (%d) as BJ_Forget\n", jblock, bh->b_blocknr, atomic_read( &bh->b_count ) - 1 );
    jnl_file_buffer( jh, tr, BJ_Forget );

    //
    // Wake up any transactions which were waiting for this IO to complete. The barrier must be here so that changes by
    // jnl_file_buffer() take effect before wake_up_bit() does the waitqueue check.
    //
    smp_mb();
    wake_up_bit( &bh->b_state, BH_Unshadow );
//    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "brelse shadowed buffer b=%"PSCT"x\n", bh->b_blocknr );
    __brelse( bh );
  }

  J_ASSERT( NULL == tr->shadow_list );

  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 4, tr=%x\n", tr->tid );

  // Here we wait for the blhdr_jh record buffers
wait_for_ctlbuf:
  while ( NULL != tr->log_list ) {
    jh = tr->log_list->tprev;
    bh = jh2bh( jh );
    if ( buffer_locked( bh ) ) {
      wait_on_buffer( bh );
      goto wait_for_ctlbuf;
    }
    if ( cond_resched() )
      goto wait_for_ctlbuf;

    if ( unlikely( !buffer_uptodate( bh ) ) )
      err = -EIO;

    clear_buffer_jwrite( bh );
    jnl_unfile_buffer( j, jh );
    jnl_put_hdr( jh );
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "ph5: control buffer b=%"PSCT"x (%d) writeout done: unfile\n", bh->b_blocknr, atomic_read( &bh->b_count ) - 1 );
    __brelse( bh );           // One for getblk
    // AKPM: bforget here
  }

  if ( err )
    jnl_abort( j, err );

  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 5, tr=%x\n", tr->tid );
  write_lock( &j->state_lock );
  J_ASSERT( tr->state == T_COMMIT_DFLUSH );
  tr->state = T_COMMIT_JFLUSH;
  write_unlock( &j->state_lock );

#if 1 //ndef async_commit
  err = jnl_submit_blhdr( j, tr, &cbh );
  if ( err )
    __jnl_abort_hard( j );
#endif

  if ( NULL != cbh ) {
    get_bh( cbh );
    clear_buffer_dirty( cbh );
    wait_on_buffer( cbh );

    if ( unlikely( !buffer_uptodate( cbh ) ) )
      err = -EIO;
    put_bh( cbh );            // One for getblk()
    jnl_put_hdr( bh2jh( cbh ) );
  }

#ifdef async_commit
  if ( j->flags & JNL_FLAGS_BARRIER )
    Blkdev_issue_flush( j->dev );
#endif

  if ( err )
    jnl_abort( j, err );

  //
  // End of a transaction!  Finally, we can do checkpoint processing: any buffers committed as a result of this
  // transaction can be removed from any checkpoint list it was on before
  //
  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 6, tr=%x\n", tr->tid );

  J_ASSERT( tr->buffers == NULL );
  J_ASSERT( tr->checkpoint_list == NULL );
  J_ASSERT( tr->iobuf_list == NULL );
  J_ASSERT( tr->shadow_list == NULL );
  J_ASSERT( tr->log_list == NULL );

restart_loop:
  //
  // As there are other places ( journal_unmap_buffer() ) adding buffers to this list we have to be careful and hold the list_lock.
  //
  spin_lock( &j->list_lock );
  while ( NULL != tr->forget ) {
    transaction *cp_transaction;
    int try_to_free = 0;

    jh = tr->forget;
    spin_unlock( &j->list_lock );
    bh = jh2bh( jh );

    // Get a reference so that bh cannot be freed before we are done with it
    get_bh( bh );
    jnl_lock_bh_state( bh );
    J_ASSERT( jh->tr == tr );

    //
    // If there is undo-protected committed data against this buffer, then we can remove it now.  If it is a
    // buffer needing such protection, the old frozen_data field now points to a committed version of the
    // buffer, so rotate that field to the new committed data.
    //
    // Otherwise, we can just throw away the frozen data now.
    //
    // We also know that the frozen data has already fired its triggers if they exist, so we can clear that too.
    //
    if ( NULL != jh->committed_data ) {
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "remove committed_data from b=%"PSCT"x\n", bh->b_blocknr );
      jnl_free( jh->committed_data, bh->b_size );
      jh->committed_data = NULL;
      if ( NULL != jh->frozen_data ) {
        jh->committed_data = jh->frozen_data;
        jh->frozen_data = NULL;
//        jh->frozen_triggers = NULL;
      }
    } else if ( NULL != jh->frozen_data ) {
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "remove frozen_data from b=%"PSCT"x\n", bh->b_blocknr );
      jnl_free( jh->frozen_data, blocksize );
      jh->frozen_data = NULL;
//      jh->frozen_triggers = NULL;
    }

    spin_lock( &j->list_lock );
    cp_transaction = jh->cp_tr;
    if ( NULL != cp_transaction ) {
//      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "remove b=%"PSCT"x from old checkpoint tr=%x\n", bh->b_blocknr, cp_transaction->tid );
#ifdef USE_JNL_STATS
      cp_transaction->chp_stats.dropped++;
#endif
      jnl_remove_checkpoint( jh );
    }

    // Only re-checkpoint the buffer_head if it is marked dirty.  If the buffer was added to the BJ_Forget list
    // by jnl_forget, it may no longer be dirty and there's no point in keeping a checkpoint record for it.
    //
    // A buffer which has been freed while still being journaled by a previous transaction.
    //
    if ( buffer_freed( bh ) ) {
      //
      // If the running transaction is the one containing "add to orphan" operation (b_next_transaction != NULL),
      // we have to wait for that transaction to commit before we can really get rid of the buffer.
      // So just clear b_modified to not confuse transaction credit accounting and refile the buffer to
      // BJ_Forget of the running transaction. If the just committed transaction contains "add to orphan" operation,
      // we can completely invalidate the buffer now.
      // We are rather through in that since the buffer may be still accessible when blocksize < pagesize
      // and it is attached to the last partial page.
      //
      jh->modified = 0;
      if ( !jh->next_tr ) {
        clear_buffer_freed( bh );
        clear_buffer_jnldirty( bh );
        clear_buffer_mapped( bh );
        clear_buffer_new( bh );
        clear_buffer_req( bh );
        bh->b_bdev = NULL;
      }
    }

    if ( buffer_jnldirty( bh ) ) {
      // put a committed buffer onto a checkpoint list so that we know when it is safe to clean the transaction out of the log
      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "add b=%"PSCT"x to new checkpoint tr=%x\n", bh->b_blocknr, tr->tid );
      J_ASSERT( jh->cp_tr == NULL );

      // Get reference for checkpointing transaction
      jnl_grab_hdr( bh );
      jh->cp_tr = tr;

      if ( !tr->checkpoint_list ) {
        jh->cpnext = jh->cpprev = jh;
      } else {
        jh->cpnext = tr->checkpoint_list;
        jh->cpprev = tr->checkpoint_list->cpprev;
        jh->cpprev->cpnext = jh;
        jh->cpnext->cpprev = jh;
      }
      tr->checkpoint_list = jh;

      if ( jnl_is_aborted( j ) )
        clear_buffer_jnldirty( bh );
    } else {
      J_ASSERT( !buffer_dirty( bh ) );

      jnl_debug( JNL_TRACE_LEVEL_COMMIT, "b=%"PSCT"x freed\n", bh->b_blocknr );

      //
      // The buffer on BJ_Forget list and not jnldirty means it has been freed by this transaction and hence it
      // could not have been reallocated until this transaction has committed. *BUT* it could be
      // reallocated once we have written all the data to disk and before we process the buffer on BJ_Forget list
      //
      if ( !jh->next_tr )
        try_to_free = 1;
    }

//    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "refile or unfile buffer b=%"PSCT"x\n", bh->b_blocknr );
    __jnl_refile_buffer( jh );
    jnl_unlock_bh_state( bh );
    if ( try_to_free )
      release_buffer_page( bh );        // Drops bh reference
    else
      __brelse( bh );
    cond_resched_lock( &j->list_lock );
  }

  spin_unlock( &j->list_lock );

  //
  // This is a bit sleazy.  We use list_lock to protect transition of a transaction into T_FINISHED state and calling
  // __jnl_drop_transaction(). Otherwise we could race with other checkpointing code processing the transaction...
  //
  write_lock( &j->state_lock );
  spin_lock( &j->list_lock );

  //
  // Now recheck if some buffers did not get attached to the transaction while the lock was dropped...
  //
  if ( tr->forget ) {
    spin_unlock( &j->list_lock );
    write_unlock( &j->state_lock );
    goto restart_loop;
  }

  // Done with this transaction!
  jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: commit phase 7, tr=%x\n", tr->tid );

  J_ASSERT( tr->state == T_COMMIT_JFLUSH );

  tr->start_jiffies = jiffies;
#ifdef USE_JNL_STATS
  stats.run.logging = jnl_time_diff( stats.run.logging, tr->start_jiffies );

  //
  // File the transaction statistics
  //
  stats.tid = tr->tid;
  stats.run.handle_count = atomic_read( &tr->handle_count );

  //
  // Calculate overall stats
  //
  spin_lock( &j->history_lock );
  j->stats.tid++;
  j->stats.run.wait += stats.run.wait;
  j->stats.run.running += stats.run.running;
  j->stats.run.locked += stats.run.locked;
  j->stats.run.flushing += stats.run.flushing;
  j->stats.run.logging += stats.run.logging;
  j->stats.run.handle_count += stats.run.handle_count;
  j->stats.run.blocks += stats.run.blocks;
  j->stats.run.blocks_logged += stats.run.blocks_logged;
  spin_unlock( &j->history_lock );
#endif

  tr->state = T_FINISHED;
  J_ASSERT( tr == j->committing_tr );
  j->commit_sequence = tr->tid;
  j->committing_tr = NULL;
  commit_time = ktime_to_ns( ktime_sub( ktime_get(), start_time ) ); // GPL - ktime_get()

  //
  // weight the commit time higher than the average time so we don't react too strongly to vast changes in the commit time
  //
  if ( likely( j->average_commit_time ) )
    j->average_commit_time = ( commit_time + j->average_commit_time*3 ) / 4;
  else
    j->average_commit_time = commit_time;
  write_unlock( &j->state_lock );

  if ( NULL == tr->checkpoint_list && NULL == tr->checkpoint_io_list ) {
    __jnl_drop_transaction( j, tr );
    to_free = 1;
  } else {
    if ( NULL == j->checkpoint_tr ) {
      j->checkpoint_tr = tr;
      tr->cpnext = tr;
      tr->cpprev = tr;
    } else {
      tr->cpnext = j->checkpoint_tr;
      tr->cpprev = tr->cpnext->cpprev;
      tr->cpnext->cpprev = tr;
      tr->cpprev->cpnext = tr;
    }
  }
  spin_unlock( &j->list_lock );

  if ( j->commit_callback ) {
    (*j->commit_callback)( j->private, &tr->private_list );
//    j->commit_callback( j, tr );
  }

//  jnl_debug( 0, "jnl: [%x, %x], %x\n", j->tail_sequence, j->commit_sequence, j->hdr->overlaps );
  if ( to_free )
    kfree( tr );

  wake_up( &j->wait_done_commit );

//  BUG_ON( test_tail );
}


///////////////////////////////////////////////////////////
// commit_timeout
//
// Helper function used to manage commit timeouts
///////////////////////////////////////////////////////////
static void commit_timeout( unsigned long data )
{
  struct task_struct *p = (struct task_struct*)data;
  wake_up_process( p );
}


///////////////////////////////////////////////////////////
// jnl_thread
//
// The main thread function used to manage a logging device journal.
//
// This kernel thread is responsible for two things:
//
// 1 ) COMMIT:  Every so often we need to commit the current state of the
//    filesystem to disk.  The journal thread is responsible for writing
//    all of the metadata buffers to disk.
//
// 2 ) CHECKPOINT: We cannot reuse a used section of the log file until all
//    of the data in that part of the log has been rewritten elsewhere on
//    the disk.  Flushing these old buffers to reclaim space in the log is
//    known as checkpointing, and this thread is responsible for that job.
///////////////////////////////////////////////////////////
static int
jnl_thread(
    IN void *arg
    )
{
  jnl *j = arg;
  transaction *tr;

  //
  // Set up an interval timer which can be used to trigger a commit wakeup after the commit interval expires
  //
  setup_timer( &j->commit_timer, commit_timeout, (unsigned long)current );

#if defined HAVE_DECL_SET_FREEZABLE && HAVE_DECL_SET_FREEZABLE
  set_freezable();
#endif

  // Record that the journal thread is running
  j->task = current;
  wake_up( &j->wait_done_commit );

  //
  // And now, wait forever for commit wakeup events
  //
  write_lock( &j->state_lock );

  for ( ;; ) {
    if ( j->flags & JNL_FLAGS_UNMOUNT ) {
      write_unlock( &j->state_lock );
      del_timer_sync( &j->commit_timer );
      j->task = NULL;
      wake_up( &j->wait_done_commit );
      jnl_debug( JNL_TRACE_LEVEL_THREAD, "Journal thread exiting.\n" );
      return 0;
    }

    jnl_debug( JNL_TRACE_LEVEL_THREAD, "commit: sequence=%x, request=%x\n", j->commit_sequence, j->commit_request );

    if ( j->commit_sequence != j->commit_request ) {
//      jnl_debug( JNL_TRACE_LEVEL_THREAD, "OK, requests differ: %x != %x\n", j->commit_sequence, j->commit_request );
      write_unlock( &j->state_lock );
      del_timer_sync( &j->commit_timer );
      jnl_commit_transaction( j );
      write_lock( &j->state_lock );
      continue;
    }

    wake_up( &j->wait_done_commit );

    if ( freezing( current ) ) {
      //
      // The simpler the better. Flushing journal isn't a good idea, because that depends on threads that may be already stopped.
      //
      jnl_debug( JNL_TRACE_LEVEL_THREAD, "now suspending jnl_thread\n" );
      write_unlock( &j->state_lock );
#if defined HAVE_DECL_REFRIGERATOR && HAVE_DECL_REFRIGERATOR
      refrigerator();
#else
      try_to_freeze();
#endif
      write_lock( &j->state_lock );
    } else {
      //
      // We assume on resume that commits are already there, so we don't sleep
      //
      DEFINE_WAIT( wait );

      prepare_to_wait( &j->wait_commit, &wait, TASK_INTERRUPTIBLE );

      if ( j->commit_sequence == j->commit_request
        && ( NULL == (tr=j->running_tr) || !time_after_eq( jiffies, tr->expires ) )
        && !(j->flags & JNL_FLAGS_UNMOUNT) ) {
        write_unlock( &j->state_lock );
        schedule();
        write_lock( &j->state_lock );
      }

      finish_wait( &j->wait_commit, &wait );
    }

    //
    // Were we woken up by a commit wakeup event?
    //
    tr = j->running_tr;
    if ( NULL != tr && time_after_eq( jiffies, tr->expires ) ) {
      j->commit_request = tr->tid;
      jnl_debug( JNL_TRACE_LEVEL_THREAD, "jnl_thread woke because of timeout. set commit_tr=%x\n", j->commit_request );
    } else {
      jnl_debug( JNL_TRACE_LEVEL_THREAD, "jnl_thread wakes\n" );
    }
  }
}

#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_force_commit_nested
//
// Force and wait upon a commit if the calling process is not within transaction
// This is used for forcing out undo-protected data which contains bitmaps, when the fs is running out of space.
//
// We can only force the running transaction if we don't have an active handle;
// otherwise, we will deadlock.
//
// Returns true if a transaction was started.
///////////////////////////////////////////////////////////
static int
jnl_force_commit_nested(
    IN jnl *j
    )
{
  transaction *tr = NULL;
  unsigned int tid;
  int need_to_start = 0;

  read_lock( &j->state_lock );
  if ( j->running_tr && !current->journal_info ) {
    tr = j->running_tr;
    if ( !tid_geq( j->commit_request, tr->tid ) )
      need_to_start = 1;
  } else if ( j->committing_tr )
    tr = j->committing_tr;

  if ( !tr ) {
    read_unlock( &j->state_lock );
    return 0;       // Nothing to retry
  }

  tid = tr->tid;
  read_unlock( &j->state_lock );
  if ( need_to_start )
    jnl_log_start_commit( j, tid );
  jnl_log_wait_commit( j, tid );
  return 1;
}
#endif


///////////////////////////////////////////////////////////
// jnl_start_commit
//
// Start a commit of the current running transaction ( if any )
// Returns true if a transaction is going to be committed ( or is currently already committing )
///////////////////////////////////////////////////////////
static int
jnl_start_commit(
    IN jnl *j,
    IN int  wait
    )
{
  int ret;
  unsigned int tid;

  write_lock( &j->state_lock );
  if ( j->running_tr ) {
    // There's a running transaction and we've just made sure it's commit has been scheduled
    tid = j->running_tr->tid;
    __jnl_log_start_commit( j, tid );
    ret = 1;
  } else if ( j->committing_tr ) {
    //
    // If jnl recently started a commit, then we have to wait for completion of that transaction
    //
    tid = j->committing_tr->tid;
    ret = 2;
  } else {
    tid = 0; // to suppress warning
    ret = 0; // no commits
  }
  write_unlock( &j->state_lock );

  if ( 0 != ret ) {
    jnl_debug( JNL_TRACE_LEVEL_COMMIT, "jnl: start_commit %x%s\n", tid, 1 == ret? ",r":",c" );
    if ( wait )
      jnl_log_wait_commit( j, tid );
  }

  return ret;
}

#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_trans_will_send_data_barrier
//
// Return 1 if a given transaction has not yet sent barrier request connected with a transaction commit
// If 0 is returned, transaction may or may not have sent the barrier. Used to avoid sending barrier twice in common cases
///////////////////////////////////////////////////////////
static int
jnl_trans_will_send_data_barrier(
    IN jnl *j,
    IN unsigned int tid
    )
{
  int ret = 0;
  transaction *commit_trans;

  if ( !( j->flags & JNL_FLAGS_BARRIER ) )
    return 0;
  read_lock( &j->state_lock );
  // Transaction already committed?
  if ( tid_geq( j->commit_sequence, tid ) )
    goto out;
  commit_trans = j->committing_tr;
  if ( !commit_trans || commit_trans->tid != tid ) {
    ret = 1;
    goto out;
  }

  //
  // Transaction is being committed and we already proceeded to submitting a flush to fs partition?
  //
  if ( j->fs_dev != j->dev ) {
    if ( !commit_trans->need_data_flush || commit_trans->state >= T_COMMIT_DFLUSH )
      goto out;
  } else {
    if ( commit_trans->state >= T_COMMIT_JFLUSH )
      goto out;
  }
  ret = 1;
out:
  read_unlock( &j->state_lock );
  return ret;
}
#endif // #ifdef USE_JNL_EXTRA


///////////////////////////////////////////////////////////
// jnl_create_slab
//
//
///////////////////////////////////////////////////////////
static int
jnl_create_slab(
    IN size_t size
    )
{
  static DEFINE_MUTEX( jnl_slab_create_mutex );
  int i, err = 0;

  if ( PAGE_SIZE == size )
    return 0;

  i = order_base_2( size ) - 9;
  if ( i >= JNL_MAX_SLABS )
    return -EINVAL;

  if ( unlikely( i < 0 ) )
    i = 0;

  mutex_lock( &jnl_slab_create_mutex );
  if ( NULL == jnl_slab[i] ) {
    unsigned int slab_size = 512 << i;

    jnl_slab[i] = Kmem_cache_create( jnl_slab_names[i], slab_size, slab_size, 0 );
    if ( NULL == jnl_slab[i] ){
      printk( KERN_EMERG "jnl: no memory for jnl_slab cache\n" );
      err = -ENOMEM;
    }
  }
  mutex_unlock( &jnl_slab_create_mutex );

  return err;
}


#if defined CONFIG_PROC_FS

static struct proc_dir_entry *proc_info_root = NULL;
static const char proc_info_root_name[] = "fs/ufsd_jnl";

///////////////////////////////////////////////////////////
// jnl_proc_version
//
//
///////////////////////////////////////////////////////////
static int jnl_proc_version_show(struct seq_file *m, void *o)
{
  seq_printf( m, "%s\ndriver (%s) loaded at %p\n", s_FileVer, s_DriverVer, __this_module.module_core );
  return 0;
}

static int jnl_proc_version_open(struct inode *inode, struct file *file)
{
  return single_open( file, jnl_proc_version_show, NULL );
}

static const struct file_operations jnl_proc_version_fops = {
  .owner    = THIS_MODULE,
  .read		  = seq_read,
  .llseek   = seq_lseek,
  .release  = single_release,
	.open		  = jnl_proc_version_open,
};

#endif // #if defined CONFIG_PROC_FS


///////////////////////////////////////////////////////////
// jnl_load
//
// Read journal from disk
///////////////////////////////////////////////////////////
static int
jnl_load(
    IN struct super_block *sb,
    IN unsigned long long off,
    OUT jnl **journal
    )
{
  unsigned int mask;
  int err;
  jnl_header *hdr;
  struct buffer_head *bh = NULL;
  u64 jsb_block;
  jnl *j = kzalloc( sizeof( jnl ), GFP_KERNEL );

  // Set default value
  *journal = NULL;

  if ( NULL == j )
    return -ENOMEM;

  init_waitqueue_head( &j->wait_transaction_locked );
  init_waitqueue_head( &j->wait_logspace );
  init_waitqueue_head( &j->wait_done_commit );
  init_waitqueue_head( &j->wait_checkpoint );
  init_waitqueue_head( &j->wait_commit );
  init_waitqueue_head( &j->wait_updates );
  mutex_init( &j->barrier );
  mutex_init( &j->checkpoint_mutex );
  spin_lock_init( &j->revoke_lock );
  spin_lock_init( &j->list_lock );
  rwlock_init( &j->state_lock );

  j->commit_interval  = ( HZ * JNL_DEFAULT_MAX_COMMIT_AGE );
//  j->min_batch_time   = 0;
  j->max_batch_time   = 15000; // 15ms

  // Set up a default-sized revoke table for the new mount
  if ( 0 != jnl_init_revoke( j ) ) {
    kfree( j );
    return -ENOMEM;
  }

  spin_lock_init( &j->history_lock );

  j->dev  = j->fs_dev = sb->s_bdev;
  bdevname( j->dev, j->devname );

  //
  // Read journal info
  //
  jsb_block = off >> sb->s_blocksize_bits;
  bh = __bread( j->dev, jsb_block, sb->s_blocksize );
  if ( NULL == bh ) {
    printk( KERN_ERR "Cannot get buffer for journal header\n" );
    err = -EIO;
    goto out_err;
  }

  hdr   = (jnl_header*)bh->b_data;

  if ( JOURNAL_HEADER_MAGIC != hdr->magic
    || 0x12345678 != hdr->endian
    || hdr->start != hdr->end
    || hdr->size < 1024*1024
//    || hdr->blhdr_size != sb->s_blocksize
    || hdr->jhdr_size < 512
    || hdr->jhdr_size > sb->s_blocksize ) {

    printk( KERN_ERR "Invalid jnl\n" );
    err = -EINVAL;
    goto out_err;
  }

  mask = sb->s_blocksize - 1;

  if ( hdr->blhdr_size != sb->s_blocksize || 0 != (hdr->start & mask) || 0 != (hdr->size & mask) ) {
    // Align down
    u64 mask64 = ~(u64)mask;

    jnl_debug( 0, "jnl: %s: update %x,[%llx:%llx]\n", sb->s_id, hdr->blhdr_size, hdr->start, hdr->size );

    hdr->blhdr_size = sb->s_blocksize;
    hdr->start &= mask64;
    hdr->size  &= mask64;
    mark_buffer_dirty( bh );
    sync_dirty_buffer( bh );
  }

  jnl_debug( 0, "+jnl: %s: b=%llxx%x,[%llx:%llx],seq=%x\n",
                sb->s_id, jsb_block, hdr->blhdr_size, hdr->start, hdr->size, hdr->sequence );

  hdr->jhdr_size = hdr->blhdr_size;

  j->hdr_buffer  = bh;
  j->hdr         = hdr;

  J_ASSERT( hdr->blhdr_size == sb->s_blocksize );

  j->blocksize      = sb->s_blocksize;
  j->blocksize_bits = sb->s_blocksize_bits;
  j->blocksize_bits_9 = j->blocksize_bits - 9;
  j->maxlen         = ( hdr->size >> sb->s_blocksize_bits ) - 1;

//  jnl_debug( 0, "jnl: %s, 0x%llx, bits %d, blksize %lx\n", sb->s_id, off, sb->s_blocksize_bits, sb->s_blocksize );

  j->wbufsize = j->blocksize / sizeof( block_info ) - 1;
  j->wbuf     = kmalloc( ( j->wbufsize + 1 ) * sizeof( struct buffer_head* ), GFP_KERNEL );
  if ( NULL == j->wbuf ) {
    printk( KERN_ERR "Can't allocate bhs for commit thread\n" );
    err = -ENOMEM;
    goto out_err;
  }

  j->tail_sequence = hdr->sequence;
  j->first  = jsb_block + 1;
  j->last   = jsb_block + j->maxlen + 1;
  j->free   = j->maxlen;
//  j->errno  = sb->s_errno;
  j->head   = jsb_block + (hdr->start >> j->blocksize_bits);
  if ( j->head >= j->last )
    j->head = j->first;
  j->tail = j->head;

  j->tr_sequence = j->tail_sequence;
  j->commit_sequence  = j->tr_sequence - 1;
  j->commit_request   = j->commit_sequence;
  j->max_transaction_buffers = j->maxlen / 4;

  TRACE_ONLY( hdr->overlaps = 0; )

  //
  // Create a slab for this blocksize
  //
  if ( jnl_create_slab( sb->s_blocksize ) ) {
    printk( KERN_ERR "Can't allocate slab\n" );
    err = -ENOMEM;
    goto out_err;
  }

  // Add the dynamic fields and write it to disk
//  jnl_update_hdr( j, 1 );

  //
  // Start thread
  //
  {
    void *p = kthread_run( jnl_thread, j, "jnl_%s", j->devname );
    if ( IS_ERR( p ) ) {
      err = PTR_ERR( p );
      goto out_err;
    }
  }

  wait_event( j->wait_done_commit, j->task != NULL );

  j->flags &= ~JNL_FLAGS_ABORT;
//  j->flags |= JNL_FLAGS_LOADED;

  *journal = j;

  return 0;

out_err:
  if ( NULL != bh )
    __brelse( bh );
  kfree( j->wbuf );
  kfree( j );
  return err;
}


///////////////////////////////////////////////////////////
// jnl_destroy
//
// Release a jnl structure
///////////////////////////////////////////////////////////
static int
jnl_destroy(
    IN jnl *j
    )
{
  int err = 0;

  //
  // Stop commit thread
  //
  write_lock( &j->state_lock );
  j->flags |= JNL_FLAGS_UNMOUNT;

  while ( j->task ) {
    wake_up( &j->wait_commit );
    write_unlock( &j->state_lock );
    wait_event( j->wait_done_commit, j->task == NULL );
    write_lock( &j->state_lock );
  }
  write_unlock( &j->state_lock );

  // Force a final log commit
  if ( j->running_tr )
    jnl_commit_transaction( j );

  // Force any old transactions to disk
  // Totally anal locking here...
  spin_lock( &j->list_lock );
  while ( j->checkpoint_tr != NULL ) {
    spin_unlock( &j->list_lock );
    mutex_lock( &j->checkpoint_mutex );
    jnl_log_do_checkpoint( j );
    mutex_unlock( &j->checkpoint_mutex );
    spin_lock( &j->list_lock );
  }

  J_ASSERT( j->running_tr == NULL );
  J_ASSERT( j->committing_tr == NULL );
  J_ASSERT( j->checkpoint_tr == NULL );
  spin_unlock( &j->list_lock );

  if ( NULL != j->hdr_buffer ) {
    TRACE_ONLY( jnl_header *hdr = (jnl_header*)j->hdr_buffer->b_data; )
    if ( !jnl_is_aborted( j ) ) {
      // We can now mark the journal as empty
      j->tail = 0; // magic value ( empty journal )
      j->tail_sequence = ++j->tr_sequence;
      jnl_update_hdr( j, 1 );
    } else {
      err = -EIO;
    }
    jnl_debug( 0, "-jnl: %s: %llx,seq=%x,m=%x\n",
                    j->devname, hdr->start, hdr->sequence, j->max_credits_used );

    brelse( j->hdr_buffer );
  }

  if ( j->revoke )
    jnl_destroy_revoke( j );
  kfree( j->wbuf );
  kfree( j );

  return err;
}

#ifdef USE_JNL_EXTRA
///////////////////////////////////////////////////////////
// jnl_flush
//
// Flush all data for a given journal to disk and empty the journal.
// Filesystems can use this when remounting readonly to ensure that
// recovery does not need to happen on remount.
///////////////////////////////////////////////////////////
static int
jnl_flush(
    IN jnl *j
    )
{
  int err = 0;
  transaction *tr = NULL;
  unsigned long old_tail;

  write_lock( &j->state_lock );

  // Force everything buffered to the log...
  if ( j->running_tr ) {
    tr = j->running_tr;
    __jnl_log_start_commit( j, tr->tid );
  } else if ( j->committing_tr )
    tr = j->committing_tr;

  // Wait for the log commit to complete...
  if ( tr ) {
    unsigned int tid = tr->tid;
    write_unlock( &j->state_lock );
    jnl_log_wait_commit( j, tid );
  } else {
    write_unlock( &j->state_lock );
  }

  // ...and flush everything in the log out to disk.
  spin_lock( &j->list_lock );
  while ( !err && j->checkpoint_tr != NULL ) {
    spin_unlock( &j->list_lock );
    mutex_lock( &j->checkpoint_mutex );
    err = jnl_log_do_checkpoint( j );
    mutex_unlock( &j->checkpoint_mutex );
    spin_lock( &j->list_lock );
  }
  spin_unlock( &j->list_lock );

  if ( jnl_is_aborted( j ) )
    return -EIO;

  jnl_cleanup_tail( j );

  //
  // Finally, mark the journal as really needing no recovery
  // This sets s_start==0 in the underlying header, which is the magic code for a fully-recovered header
  // Any future commits of data to the journal will restore the current s_start value
  write_lock( &j->state_lock );
  old_tail = j->tail;
  j->tail = 0;  // magic value ( empty journal )
  write_unlock( &j->state_lock );
  jnl_update_hdr( j, 1 );
  write_lock( &j->state_lock );
  j->tail = old_tail;

  J_ASSERT( !j->running_tr );
  J_ASSERT( !j->committing_tr );
  J_ASSERT( !j->checkpoint_tr );
  J_ASSERT( j->head == j->tail );
  J_ASSERT( j->tail_sequence == j->tr_sequence );
  write_unlock( &j->state_lock );
  return 0;
}
#endif


/**
 * void jnl_abort () - Shutdown the journal immediately.
 * @journal: the journal to shutdown.
 * @errno:   an error number to record in the journal indicating
 *           the reason for the shutdown.
 *
 * Perform a complete, immediate shutdown of the ENTIRE
 * journal ( not of a single transaction ).  This operation cannot be
 * undone without closing and reopening the journal.
 *
 * The jnl_abort function is intended to support higher level error
 * recovery mechanisms such as the ufsd remount-readonly error
 * mode.
 *
 * Journal abort has very specific semantics.  Any existing dirty,
 * unjournaled buffers in the main filesystem will still be written to
 * disk by bdflush, but the journaling mechanism will be suspended
 * immediately and no further transaction commits will be honoured.
 *
 * Any dirty, journaled buffers will be written back to disk without
 * hitting the journal.  Atomicity cannot be guaranteed on an aborted
 * filesystem, but we _do_ attempt to leave as much data as possible
 * behind for fsck to use for cleanup.
 *
 * Any attempt to get a new transaction handle on a journal which is in
 * ABORT state will just result in an -EROFS error return.  A
 * jnl_stop on an existing handle will return -EIO if we have
 * entered abort state during the update.
 *
 * Recursive transactions are not disturbed by journal abort until the
 * final jnl_stop, which will receive the -EIO error.
 *
 * Finally, the jnl_abort call allows the caller to supply an errno
 * which will be recorded ( if possible ) in the journal header.  This
 * allows a client to record failure conditions in the middle of a
 * transaction without having to complete the transaction to record the
 * failure to disk.  jnl_error, for example, now uses this
 * functionality.
 *
 * Errors which originate from within the journaling layer will NOT
 * supply an errno; a null errno implies that absolutely no further
 * writes are done to the journal ( unless there are any already in
 * progress ).
 *
 */


///////////////////////////////////////////////////////////
// jnl_errno
//
// returns the journal's error state of -EROFS if the journal has been aborted on this mount time
///////////////////////////////////////////////////////////
static int
jnl_errno(
    IN jnl *j
    )
{
  int err;
  read_lock( &j->state_lock );
  if ( j->flags & JNL_FLAGS_ABORT )
    err = -EROFS;
  else
    err = j->errno;
  read_unlock( &j->state_lock );
  return err;
}

#ifdef USE_JNL_EXTRA

///////////////////////////////////////////////////////////
// jnl_clear_err
//
// clears the journal's error state
///////////////////////////////////////////////////////////
static int
jnl_clear_err(
    IN jnl *j
    )
{
  int err = 0;

  write_lock( &j->state_lock );
  if ( j->flags & JNL_FLAGS_ABORT )
    err = -EROFS;
  else
    j->errno = 0;
  write_unlock( &j->state_lock );
  return err;
}


///////////////////////////////////////////////////////////
// jnl_ack_err
//
// Ack journal err
///////////////////////////////////////////////////////////
static void
jnl_ack_err(
    IN jnl *j
    )
{
  write_lock( &j->state_lock );
  if ( j->errno )
    j->flags |= JNL_FLAGS_ACK_ERR;
  write_unlock( &j->state_lock );
}


///////////////////////////////////////////////////////////
// jnl_blocks_per_page
//
//
///////////////////////////////////////////////////////////
static int
jnl_blocks_per_page(
    IN struct inode *i
    )
{
  return 1 << ( PAGE_CACHE_SHIFT - i->i_sb->s_blocksize_bits );
}
#endif // #ifdef USE_JNL_EXTRA

const struct jnl_operations jnl_op = {
  .start                = jnl_start,
  .restart              = jnl_restart,
  .extend               = jnl_extend,
  .get_write_access     = jnl_get_write_access,
  .get_create_access    = jnl_get_create_access,
  .dirty_metadata       = jnl_dirty_metadata,
  .stop                 = jnl_stop,
  .load                 = jnl_load,
  .destroy              = jnl_destroy,
  .abort                = jnl_abort,
  .errno                = jnl_errno,
  .start_commit         = jnl_start_commit,
  .is_aborted           = jnl_is_aborted,
  .is_handle_aborted    = jnl_is_handle_aborted,
  .abort_handle         = jnl_abort_handle,

#ifdef USE_JNL_EXTRA
  .flush                = jnl_flush,
  .lock_updates         = jnl_lock_updates,
  .unlock_updates       = jnl_unlock_updates,
  .force_commit_nested  = jnl_force_commit_nested,
  .force_commit         = jnl_force_commit,
  .log_start_commit     = jnl_log_start_commit,
  .revoke               = jnl_revoke,
  .clear_err            = jnl_clear_err,
  .try_to_free_buffers  = jnl_try_to_free_buffers,
  .invalidatepage       = jnl_invalidatepage,
  .forget               = jnl_forget,
  .release_buffer       = jnl_release_buffer,
  .get_undo_access      = jnl_get_undo_access,
  .blocks_per_page      = jnl_blocks_per_page,
  .trans_will_send_data_barrier = jnl_trans_will_send_data_barrier,
#endif
};

///////////////////////////////////////////////////////////
// Proc_create_data
//
// Helper function/macros to reduce chaos
///////////////////////////////////////////////////////////
#if defined HAVE_DECL_PROC_CREATE_DATA && HAVE_DECL_PROC_CREATE_DATA
  #define Proc_create_data  proc_create_data
#else
  // 2.6.22 -
static struct proc_dir_entry*
Proc_create_data(
    IN const char *name,
    IN umode_t mode,
    IN struct proc_dir_entry *parent,
    IN const struct file_operations *fops,
    IN void *data
    )
{
	struct proc_dir_entry *e = create_proc_entry( name, mode, parent );
  if ( NULL != e ) {
    e->data = data;
    e->proc_fops = fops;
  }
  return e;
}
#endif


///////////////////////////////////////////////////////////
// jnl_init
//
// Module startup
///////////////////////////////////////////////////////////
static int __init
jnl_init( void )
{
#ifndef UFSD_TRACE_SILENT
  printk( KERN_NOTICE "jnl: driver (%s) loaded at %p\n", s_DriverVer, __this_module.module_core );
#endif

#if defined CONFIG_PROC_FS
  if ( NULL == proc_info_root ) {
    proc_info_root = proc_mkdir( proc_info_root_name, NULL );
    if ( NULL != proc_info_root ) {
#if defined HAVE_STRUCT_PROC_DIR_ENTRY_OWNER && HAVE_STRUCT_PROC_DIR_ENTRY_OWNER
      proc_info_root->owner = THIS_MODULE;
#endif
      Proc_create_data( "version", 0, proc_info_root, &jnl_proc_version_fops, NULL );
    } else {
      printk( KERN_NOTICE "jnl: cannot create /proc/%s", proc_info_root_name );
    }
  }
#endif

  jnl_revoke_record_cache = Kmem_cache_create( "jnl_revoke_record", sizeof( jnl_revoke_record ), 0, SLAB_HWCACHE_ALIGN|SLAB_TEMPORARY );
  if ( NULL == jnl_revoke_record_cache )
    return -ENOMEM;

  jnl_head_cache = Kmem_cache_create( "jnl_head", sizeof( jnl_head ), 0, SLAB_TEMPORARY );
  if ( NULL == jnl_head_cache )
    goto Exit1;

  jnl_handle_cache = Kmem_cache_create( "jnl_handle", sizeof( struct jnl_handle ), 0, SLAB_TEMPORARY );
  if ( NULL == jnl_handle_cache )
    goto Exit2;

  // Ok
  return 0;

Exit2:
  kmem_cache_destroy( jnl_head_cache );
  jnl_head_cache = NULL;

Exit1:
  kmem_cache_destroy( jnl_revoke_record_cache );
  jnl_revoke_record_cache = NULL;

#if defined CONFIG_PROC_FS
  if ( NULL != proc_info_root ) {
    remove_proc_entry( "version", proc_info_root );
    proc_info_root = NULL;
    remove_proc_entry( proc_info_root_name, NULL );
  }
#endif

  return -ENOMEM;
}


///////////////////////////////////////////////////////////
// jnl_exit
//
// Module shutdown
///////////////////////////////////////////////////////////
static void __exit
jnl_exit( void )
{
  int i;

#if defined CONFIG_PROC_FS
  if ( NULL != proc_info_root ) {
    remove_proc_entry( "version", proc_info_root );
    proc_info_root = NULL;
    remove_proc_entry( proc_info_root_name, NULL );
  }
#endif

  if ( NULL != jnl_handle_cache )
    kmem_cache_destroy( jnl_handle_cache );

  if ( NULL != jnl_head_cache )
    kmem_cache_destroy( jnl_head_cache );

  if ( NULL != jnl_revoke_record_cache )
    kmem_cache_destroy( jnl_revoke_record_cache );

  for ( i = 0; i < JNL_MAX_SLABS; i++ ) {
    if ( NULL != jnl_slab[i] )
      kmem_cache_destroy( jnl_slab[i] );
  }

#ifndef UFSD_TRACE_SILENT
  printk( KERN_NOTICE "jnl: driver unloaded\n" );
#endif
}

#endif // #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)

EXPORT_SYMBOL( jnl_op );

MODULE_DESCRIPTION("Paragon jnl driver");
MODULE_LICENSE( "GPL" );
module_init( jnl_init );
module_exit( jnl_exit );
