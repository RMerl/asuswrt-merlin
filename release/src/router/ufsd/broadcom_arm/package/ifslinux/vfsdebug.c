/*++


Module Name:

    vfsdebug.c

Abstract:

    This module implements UFSD debug subsystem

Author:

    Ahdrey Shedel

Revision History:

    18/09/2000 - Andrey Shedel - Created
    Since 29/07/2005 - Alexander Mamaev

--*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/sched.h>

#include "config.h"
#include "ufsdapi.h"

//
// Endianess test
//
static const unsigned short szTstEnd[3] __attribute__ ((used)) = {0x694C,0x4274,0x6769};

#if defined UFSD_TRACE

char ufsd_trace_level[16] = "";

//
// Activate this define to build driver with predefined trace and log
//
// #define UFSD_DEFAULT_LOGTO  "/ufsd/ufsd.log"

#ifdef UFSD_DEFAULT_LOGTO
  char ufsd_trace_file[128]     = UFSD_DEFAULT_LOGTO;
  unsigned long UFSD_TraceLevel = ~(UFSD_LEVEL_VFS_WBWE|UFSD_LEVEL_MEMMNGR|UFSD_LEVEL_IO|UFSD_LEVEL_UFSDAPI);
  unsigned long UFSD_CycleMB    = 25;
#else
  char ufsd_trace_file[128]     = "";
  unsigned long UFSD_TraceLevel = UFSD_LEVEL_DEFAULT;
  unsigned long UFSD_CycleMB    = 0;
#endif

static atomic_t UFSD_TraceIndent;
static struct file* log_file;
static int log_file_opened;
static int log_file_error;

static void ufsd_vlog( const char*  fmt, va_list ap );

///////////////////////////////////////////////////////////
// UFSD_TraceInc
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
UFSD_TraceInc(
    IN int Indent
    )
{
  atomic_add( Indent, &UFSD_TraceIndent );
}


///////////////////////////////////////////////////////////
// ufsd_log
//
//
///////////////////////////////////////////////////////////
static void
ufsd_log(
    IN const char * fmt,
    IN int len
    )
{
  if ( len <= 0 || 0 == fmt[0] )
    return;

  if ( !log_file_opened && 0 != ufsd_trace_file[0] ) {
    log_file_opened = 1;
    log_file = filp_open( ufsd_trace_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUGO | S_IWUGO );
    if ( IS_ERR(log_file) ) {
      long error = PTR_ERR(log_file);
      log_file = NULL;
      printk(KERN_NOTICE  QUOTED_UFSD_DEVICE": failed to start log to '%s' (errno=%ld), using system log\n", ufsd_trace_file, -error);
    }
    else {
      assert(NULL != log_file);
    }
  }

  if ( NULL != log_file && NULL != log_file->f_op && NULL != log_file->f_op->write && !log_file_error ) {

    mm_segment_t old_limit = get_fs();
    long error = 0;

    set_fs( KERNEL_DS );

    if ( 0 != UFSD_CycleMB ) {
      size_t bytes = UFSD_CycleMB << 20;
      int to_write = log_file->f_pos + len > bytes? (bytes - log_file->f_pos) : len;
      assert( to_write >= 0 );
      if ( to_write <= 0 )
        to_write = 0;
      else {
        error = log_file->f_op->write(log_file, fmt, to_write, &log_file->f_pos);
        if ( error < 0 )
          log_file_error = error;
        fmt += to_write;
        len -= to_write;
      }

      if ( 0 != len )
        log_file->f_pos = 0;
    }

    if ( 0 != len ) {
      error = log_file->f_op->write(log_file, fmt, len, &log_file->f_pos );
      if ( error < 0 )
        log_file_error = error;
    }

    set_fs(old_limit);

    if ( error < 0 )
      printk("log write failed: %ld\n", -error);
  }
  // Comment out this 'else' to duplicate the output to klog.
  else {
    printk( KERN_NOTICE  QUOTED_UFSD_DEVICE":%s", fmt );
  }
}


///////////////////////////////////////////////////////////
// CloseTrace
//
//
///////////////////////////////////////////////////////////
void
CloseTrace( void )
{
  if ( NULL != log_file ){
    filp_close( log_file, NULL );
    log_file = NULL;
    log_file_error = 0;
    log_file_opened = 0;
  }
}


///////////////////////////////////////////////////////////
// _UFSDTrace
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALLv void
_UFSDTrace( const char* fmt, ... )
{
  va_list ap;
  va_start( ap,fmt );
  ufsd_vlog( fmt, ap );

  // bug on asserts
//  BUG_ON( '*' == fmt[0] && '*' == fmt[1] && '*' == fmt[2] && '*' == fmt[3] );
#if defined HAVE_DECL_VPRINTK && HAVE_DECL_VPRINTK
  if ( '*' == fmt[0] && '*' == fmt[1] && '*' == fmt[2] && '*' == fmt[3] ) {
    printk( "%s: ", current->comm );
    vprintk( fmt, ap );
  }
#endif
  va_end( ap );
}


///////////////////////////////////////////////////////////
// UFSDError
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
UFSDError( int Err, const char* FileName, int Line )
{
  long Level = UFSD_TraceLevel;
  const char* Name = strrchr( FileName, '/' );
  if ( NULL == Name )
    Name = FileName - 1;

  UFSD_TraceLevel |= UFSD_LEVEL_ERROR | UFSD_LEVEL_UFSD;
  DebugTrace(0, UFSD_LEVEL_ERROR, ("\"%s\": UFSD error 0x%x, %s, %d\n", current->comm, Err, Name + 1, Line));
  UFSD_TraceLevel = Level;
//  BUG_ON( 1 );
}


///////////////////////////////////////////////////////////
// ufsd_vlog
//
//
///////////////////////////////////////////////////////////
static void
ufsd_vlog(
    IN const char*  fmt,
    IN va_list      ap
    )
{
  char buf[160];
  int len = atomic_read( &UFSD_TraceIndent );

  if ( len > 0 && len < 20 )
    memset( buf, ' ', len );
  else
    len = 0;

  len += vsnprintf( buf + len, sizeof(buf) - len, fmt, ap );

  if ( len > sizeof(buf) ) {
    len = sizeof(buf);
    buf[sizeof(buf)-3] = '.';
    buf[sizeof(buf)-2] = '.';
    buf[sizeof(buf)-1] = '\n';
  }

  if ( '\5' == fmt[0] )
    printk( KERN_NOTICE  QUOTED_UFSD_DEVICE":%s", fmt + 1 );
  else {

    if ( '*' == fmt[0] && '*' == fmt[1] && '*' == fmt[2] && '*' == fmt[3] && len < sizeof(buf) ) {
      int ln = strlen( current->comm );
      if ( ln + len >= sizeof(buf) )
        ln = sizeof(buf) - len - 1;
      if ( ln > 0 ) {
        memmove( buf + ln, buf, len + 1 );
        memcpy( buf, current->comm, ln );
        len += ln;
      }
    }

    ufsd_log( buf, len );
  }
}

#endif // #if defined UFSD_TRACE


#ifdef UFSD_DEBUG

///////////////////////////////////////////////////////////
// UFSD_DumpStack
//
// Sometimes it is usefull to call this function from library
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
_UFSD_DumpStack( void )
{
  dump_stack();
}


static long UFSD_TraceLevel_Old;
///////////////////////////////////////////////////////////
// _UFSD_TurnOnTraceLevel
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
_UFSD_TurnOnTraceLevel( void )
{
  UFSD_TraceLevel_Old = UFSD_TraceLevel;
  UFSD_TraceLevel = -1;
}


///////////////////////////////////////////////////////////
// _UFSD_RevertTraceLevel
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
_UFSD_RevertTraceLevel( void )
{
  UFSD_TraceLevel  = UFSD_TraceLevel_Old;
}


///////////////////////////////////////////////////////////
// IsZero
//
//
///////////////////////////////////////////////////////////
int
IsZero(
    IN const char*  data,
    IN size_t       bytes
    )
{
  if ( 0 == (((size_t)data)%sizeof(int)) ) {
    while( bytes >= sizeof(int) ) {
      if ( 0 != *(int*)data )
        return 0;
      bytes -= sizeof(int);
      data  += sizeof(int);
    }
  }

  while( 0 != bytes-- ) {
    if ( 0 != *data++ )
      return 0;
  }
  return 1;
}

#if 0
#include <linux/buffer_head.h>
///////////////////////////////////////////////////////////
// TracePageBuffers
//
//
///////////////////////////////////////////////////////////
void
TracePageBuffers(
    IN struct page* page,
    IN int hdr
    )
{
  if ( hdr ) {
    DebugTrace(+1, UFSD_LEVEL_PAGE_BH, ("p=%p f=%lx:\n", page, page->flags ));
  } else if ( UFSD_TraceLevel & UFSD_LEVEL_PAGE_BH ) {
    UFSD_TraceInc( +1 );
  }

  if ( page_has_buffers( page ) ) {
    struct buffer_head* head  = page_buffers(page);
    struct buffer_head* bh    = head;
    do {
      if ( (sector_t)-1 == bh->b_blocknr ) {
        DebugTrace( 0, UFSD_LEVEL_PAGE_BH, ("bh=%p,%lx\n", bh, bh->b_state) );
      } else {
        DebugTrace( 0, UFSD_LEVEL_PAGE_BH, ("bh=%p,%lx,%"PSCT"x\n", bh, bh->b_state, bh->b_blocknr ) );
      }
      bh = bh->b_this_page;
    } while( bh != head );
  } else {
    DebugTrace(0, UFSD_LEVEL_PAGE_BH, ("no buffers\n" ));
  }

  if ( UFSD_TraceLevel & UFSD_LEVEL_PAGE_BH )
    UFSD_TraceInc( -1 );
}

#include <linux/pagevec.h>
///////////////////////////////////////////////////////////
// trace_pages
//
//
///////////////////////////////////////////////////////////
unsigned
trace_pages(
    IN struct address_space* mapping
    )
{
  struct pagevec pvec;
  pgoff_t next = 0;
  unsigned Ret = 0;
  int i;

  pagevec_init( &pvec, 0 );

  while ( pagevec_lookup( &pvec, mapping, next, PAGEVEC_SIZE ) ) {
    for ( i = 0; i < pvec.nr; i++ ) {
      struct page *page = pvec.pages[i];
      void* d = kmap_atomic( page, KM_USER0 );
      DebugTrace( 0, UFSD_LEVEL_VFS, ("p=%p o=%llx f=%lx%s\n", page, (UINT64)page->index << PAGE_CACHE_SHIFT, page->flags, IsZero( d, PAGE_CACHE_SIZE )?", zero" : "" ));
      TracePageBuffers( page, 0 );
      kunmap_atomic( d, KM_USER0 );
      if ( page->index > next )
        next = page->index;
      Ret += 1;
      next += 1;
    }
    pagevec_release(&pvec);
  }
  if ( 0 == next )
    DebugTrace( 0, UFSD_LEVEL_VFS, ("no pages\n"));
  return Ret;
}


///////////////////////////////////////////////////////////
// DropPages
//
//
///////////////////////////////////////////////////////////
void
DropPages(
    IN struct address_space* m
    )
{
  filemap_fdatawrite( m );
  unmap_mapping_range( m, 0, 0, 1 );
  truncate_inode_pages( m, 0 );
  unmap_mapping_range( m, 0, 0, 1 );
}
#endif

#endif // #ifdef UFSD_DEBUG

#ifndef UFSD_NO_PRINTK
///////////////////////////////////////////////////////////
// UFSD_printk
//
// Used to show errors from library
///////////////////////////////////////////////////////////
UFSDAPI_CALLv int
UFSD_printk(
    IN const char *fmt, ...
    )
{
#if defined HAVE_DECL_VPRINTK && HAVE_DECL_VPRINTK
  va_list ap;
  int r;

  va_start( ap, fmt );
  r = vprintk( fmt, ap );
//  ufsd_vlog( fmt, ap );
  va_end( ap );

  return r;
#else
  return 0;
#endif
}
#endif
