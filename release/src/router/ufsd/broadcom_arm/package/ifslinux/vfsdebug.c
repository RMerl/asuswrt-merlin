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

static const unsigned short szTstEnd[3] __attribute__ ((used)) = {0x694C,0x4274,0x6769};

#if defined UFSD_DEBUG

//
// Activate this define to build driver with predefined trace and log
//
// #define UFSD_DEFAULT_LOGTO  "/ufsd/ufsd.log"

#ifdef UFSD_DEFAULT_LOGTO
  static const char s_LogTo[] = UFSD_DEFAULT_LOGTO;
  long UFSD_DebugTraceLevel   = ~(DEBUG_TRACE_VFS_WBWE|DEBUG_TRACE_MEMMNGR|DEBUG_TRACE_IO|DEBUG_TRACE_UFSDAPI);
  static int s_CycleBytes     = 25*1024*1024;
#else
  static char* s_LogTo        = NULL;
  long UFSD_DebugTraceLevel   = DEBUG_TRACE_DEFAULT;
  static int s_CycleBytes     = 0;
#endif

static atomic_t UFSD_DebugTraceIndent;
static struct file* log_file;
static int log_file_opened;
static int log_file_error;

static void ufsd_vlog( const char*  fmt, va_list ap );

///////////////////////////////////////////////////////////
// UFSD_DebugInc
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
UFSD_DebugInc(
    IN int Indent
    )
{
  atomic_add( Indent, &UFSD_DebugTraceIndent );
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

  if ( !log_file_opened
#ifndef UFSD_DEFAULT_LOGTO
    && NULL != s_LogTo
#endif
     ) {

    log_file_opened = 1;

    log_file = filp_open( s_LogTo, O_WRONLY | O_CREAT | O_TRUNC, S_IRUGO | S_IWUGO );
    if ( IS_ERR(log_file) ) {
      long error = PTR_ERR(log_file);
      log_file = NULL;
      printk(KERN_NOTICE  QUOTED_UFSD_DEVICE": failed to start log to '%s' (errno=%ld), using system log\n", s_LogTo, -error);
    }
    else {
      ASSERT(NULL != log_file);
    }
  }

  if ( NULL != log_file && NULL != log_file->f_op && NULL != log_file->f_op->write && !log_file_error ) {

    mm_segment_t old_limit = get_fs();
    long error = 0;

    set_fs(KERNEL_DS);

    if ( 0 != s_CycleBytes ) {
      int to_write = log_file->f_pos + len > s_CycleBytes? (s_CycleBytes - log_file->f_pos) : len;
      ASSERT( to_write >= 0 );
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
    if ( 0 == log_file_error ) {
      struct address_space* m = log_file->f_dentry->d_inode->i_mapping;
      long hint = log_file->f_pos - 4*PAGE_SIZE;
      if ( m->nrpages > 32 && hint > 0 ) {
        unsigned end = hint & ~(PAGE_SIZE - 1);
//        unsigned long nrpages = m->nrpages;
        int err = filemap_fdatawrite_range( m, 0, end - 1 );
        if ( 0 == err )
          truncate_inode_pages_range( m, 0, end - 1 );
//        printk("truncate_inode_pages_range %x, %lu -> %lu, %d\n", end, nrpages, m->nrpages, err );
      }
    }
#endif

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
// InitTrace
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
InitTrace( void )
{
}


///////////////////////////////////////////////////////////
// CloseTrace
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
CloseTrace( void )
{
  if ( NULL != log_file ){
    filp_close(log_file, NULL);
    log_file = NULL;
    log_file_error = 0;
  }

#ifndef UFSD_DEFAULT_LOGTO
  if ( NULL != s_LogTo ){
    kfree( s_LogTo );
    s_LogTo = NULL;
  }
#endif
}


///////////////////////////////////////////////////////////
// SetTrace
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
SetTrace(
    IN const char* TraceFile
    )
{
#ifdef UFSD_DEFAULT_LOGTO
  UNREFERENCED_PARAMETER( TraceFile );
#else
  int len;
  if ( NULL != log_file )
    return;

  len = strlen( TraceFile );
  s_LogTo = kmalloc( len + 1, GFP_NOFS);
  if ( NULL != s_LogTo )
    memcpy( s_LogTo, TraceFile, len + 1 );
#endif
}


///////////////////////////////////////////////////////////
// SetCycle
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
SetCycle(
    IN int  CycleBytes
    )
{
  // Align on 256KB
  s_CycleBytes = ((CycleBytes + 256*1024 - 1) >> 18) << 18;
}


///////////////////////////////////////////////////////////
// UFSD_DebugPrintf
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALLv void
UFSD_DebugPrintf( const char *fmt, ... )
{
  va_list ap;
  va_start( ap,fmt );
  ufsd_vlog( fmt, ap );

  // bug on asserts
//  BUG_ON( '*' == fmt[0] && '*' == fmt[1] && '*' == fmt[2] && '*' == fmt[3] );
#if defined HAVE_DECL_VPRINTK && HAVE_DECL_VPRINTK
  if ( '*' == fmt[0] && '*' == fmt[1] && '*' == fmt[2] && '*' == fmt[3] )
    vprintk( fmt, ap );
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
  long Level = UFSD_DebugTraceLevel;
  const char* Name = strrchr( FileName, '/' );
  if ( NULL == Name )
    Name = FileName - 1;

  UFSD_DebugTraceLevel |= DEBUG_TRACE_ERROR | DEBUG_TRACE_UFSD;
  DebugTrace(0, DEBUG_TRACE_ERROR, ("\"%s\": UFSD error 0x%x, %s, %d\n", current->comm, Err, Name + 1, Line));
  UFSD_DebugTraceLevel = Level;
//  BUG_ON( 1 );
}


#if defined UFSD_TRACE

///////////////////////////////////////////////////////////
// _UFSDTrace
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALLv void
_UFSDTrace( const char* fmt, ... )
{
  if ( (UFSD_DebugTraceLevel & DEBUG_TRACE_UFSD)
    || '\5' == fmt[0] ){
    va_list ap;
    va_start(ap,fmt);
    ufsd_vlog( fmt, ap );
    va_end(ap);
  }
}

#endif // #if defined UFSD_TRACE

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
  int len = atomic_read( &UFSD_DebugTraceIndent );

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
  else
    ufsd_log( buf, len );
}


///////////////////////////////////////////////////////////
// UFSD_DumpStack
//
// Sometimes it is usefull to call this function from library
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
UFSD_DumpStack( void )
{
  dump_stack();
}

#else

///////////////////////////////////////////////////////////
// UFSDError
//
//
///////////////////////////////////////////////////////////
UFSDAPI_CALL void
UFSDError( int Err, const char* FileName, int Line )
{
  const char* Name = strrchr( FileName, '/' );
  if ( NULL == Name )
    Name = FileName - 1;
  printk( KERN_NOTICE  QUOTED_UFSD_DEVICE "\"%s\": UFSD error 0x%x, %s, %d\n", current->comm, Err, Name + 1, Line );
}

#endif // UFSD_DEBUG


///////////////////////////////////////////////////////////
// UFSD_printk
//
// Used to show errors while converting strings in library
///////////////////////////////////////////////////////////
UFSDAPI_CALLv int
UFSD_printk(
    const char *fmt, ...
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
  UNREFERENCED_PARAMETER( fmt );
  return 0;
#endif
}
