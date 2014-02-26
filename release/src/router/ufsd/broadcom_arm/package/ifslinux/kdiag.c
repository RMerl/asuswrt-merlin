#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>


int mem = 1024;

struct RoMemory{
  void* Addr;
  size_t nPages;
  struct page* Pages[1];
};

struct RoMemory* g_Mem;


///////////////////////////////////////////////////////////
// kdiag_init
//
// Module startup
///////////////////////////////////////////////////////////
static int __init
kdiag_init( void )
{
  size_t k;
  size_t nPages = ((size_t)mem * 1024 + PAGE_SIZE - 1) >> PAGE_SHIFT;
  size_t BytesPerStruct = sizeof( struct RoMemory ) + (nPages - 1) * sizeof( struct page* );

  g_Mem = (struct RoMemory*)kmalloc( BytesPerStruct, GFP_KERNEL );
  if ( NULL == g_Mem )
    return -ENOMEM;
  memset( g_Mem, 0, BytesPerStruct );

  g_Mem->nPages = nPages;

  for ( k = 0; k < nPages; k++ ) {
    g_Mem->Pages[k] = alloc_page( GFP_KERNEL );
    if ( NULL == g_Mem->Pages[k] ) {
      goto Exit;
    }
  }

  g_Mem->Addr = vmap( g_Mem->Pages, nPages, VM_MAP, PAGE_READONLY );
  if ( NULL == g_Mem->Addr )
    goto Exit;

  printk( KERN_NOTICE "kdiag: Allocated 0x%zx read-only memory pages\n", nPages );

  // Ok
  return 0;

Exit:

  while( k ) {
    __free_page( g_Mem->Pages[--k] );
  }

  kfree( g_Mem );
  g_Mem = NULL;

  printk( KERN_NOTICE "kdiag: Failed to allocate %d KB read-only memory\n", mem );

  return -ENOMEM;
}


///////////////////////////////////////////////////////////
// kdiag_exit
//
// Module shutdown
///////////////////////////////////////////////////////////
static void __exit
kdiag_exit( void )
{
  printk( KERN_NOTICE "kdiag: driver unloaded\n" );

  if ( NULL != g_Mem ) {
    size_t k;
    if ( NULL != g_Mem->Addr )
      vunmap( g_Mem->Addr );

    for ( k = 0; k < g_Mem->nPages; k++ )
      __free_page( g_Mem->Pages[k] );

    kfree( g_Mem );
  }
}


MODULE_DESCRIPTION("Test kdiag driver");
MODULE_LICENSE( "GPL" );
module_init( kdiag_init );
module_exit( kdiag_exit );

module_param( mem, int, S_IRUGO);
MODULE_PARM_DESC( mem, " Size in Kb for ro hole" );
