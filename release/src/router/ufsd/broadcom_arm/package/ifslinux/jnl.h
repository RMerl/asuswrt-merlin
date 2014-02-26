//////////////////////////////////////////////////////////////////////////////
// This file is under the terms of the GNU General Public License, version 2.
// http://www.gnu.org/licenses/gpl-2.0.html
// Filesystem journal-writing code for HFS+ (based on jbd2 implementation in Kernel)..
//////////////////////////////////////////////////////////////////////////////

//#define USE_JNL_EXTRA

typedef struct jnl_handle   handle_j;   // Atomic operation type
typedef struct jnl_control  jnl;            // Journal control structure 

#if defined _MSC_VER
  typedef unsigned __int64    _UINT64;
#elif defined __GNUC__
  typedef unsigned long long  _UINT64;
#else
  #error "Does your compiler support build-in 64bit values?"
#endif


//=========================================================
//        The journaling code user interface:
//
// Create and destroy handles
// Register buffer modifications against the current transaction.
//=========================================================

struct jnl_operations {
  int   (*start)(jnl*, int nblocks, handle_j**);
  int   (*restart)(handle_j*, int nblocks);
  int   (*extend)(handle_j*, int nblocks);
  int   (*get_write_access)(handle_j*, struct buffer_head* );
  int   (*get_create_access)(handle_j*, struct buffer_head* );
  void  (*dirty_metadata)(handle_j*, struct buffer_head*, int* credits );
  int   (*stop)(handle_j**);

  int   (*load)( struct super_block*, _UINT64 off, jnl** );
  int   (*destroy)(jnl*);
  void  (*abort)(jnl*, int);
  int   (*errno)(jnl*);

  int   (*start_commit)(jnl*, int wait);
  int   (*is_aborted)(jnl*);
  int   (*is_handle_aborted)(handle_j*);
  void  (*abort_handle)(handle_j*);

#ifdef USE_JNL_EXTRA
  int   (*flush)(jnl*);
  void  (*lock_updates)(jnl*);
  void  (*unlock_updates)(jnl*);
  int   (*force_commit_nested)(jnl*);
  int   (*force_commit)(jnl*);
  int   (*log_start_commit)(jnl*, unsigned int tid);
  int   (*revoke)(handle_j*, unsigned long, struct buffer_head *);
  int   (*clear_err)(jnl*);
  int   (*try_to_free_buffers)(jnl*, struct page*, unsigned gfp_t );
  void  (*invalidatepage)(jnl*, struct page*, unsigned long );
  int   (*forget)(handle_j*, struct buffer_head* );
  void  (*release_buffer)(handle_j*, struct buffer_head* );
  int   (*get_undo_access)(handle_j*, struct buffer_head* );
  int   (*blocks_per_page)(struct inode*);
  int   (*trans_will_send_data_barrier)(jnl*, unsigned int tid);
#endif
};


extern const struct jnl_operations jnl_op;
