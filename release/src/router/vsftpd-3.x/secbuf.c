/*
 * Part of Very Secure FTPd
 * Licence: GPL v2
 * Author: Chris Evans
 * secbuf.c
 *
 * Here are some routines providing the (possibly silly) concept of a secure
 * buffer. A secure buffer may not be overflowed. A single byte overflow
 * will cause the program to safely terminate.
 */

#include "secbuf.h"
#include "utility.h"
#include "sysutil.h"
#include "sysdeputil.h"

void
vsf_secbuf_alloc(char** p_ptr, unsigned int size)
{
  unsigned int page_offset;
  unsigned int round_up;
  char* p_mmap;
  char* p_no_access_page;
  unsigned int page_size = vsf_sysutil_getpagesize();

  /* Free any previous buffer */
  vsf_secbuf_free(p_ptr);
  /* Round up to next page size */
  page_offset = size % page_size;
  if (page_offset)
  {
    unsigned int num_pages = size / page_size;
    num_pages++;
    round_up = num_pages * page_size;
  }
  else
  {
    /* Allocation is on a page-size boundary */
    round_up = size;
  }
  /* Add on another two pages to make inaccessible */
  round_up += page_size * 2;

  p_mmap = vsf_sysutil_map_anon_pages(round_up);
  /* Map the first and last page inaccessible */
  p_no_access_page = p_mmap + round_up - page_size;
  vsf_sysutil_memprotect(p_no_access_page, page_size, kVSFSysUtilMapProtNone);
  /* Before we make the "before" page inaccessible, store the size in it.
   * A little hack so that we don't need to explicitly be passed the size
   * when freeing an existing secure buffer
   */
  *((unsigned int*)p_mmap) = round_up;
  p_no_access_page = p_mmap;
  vsf_sysutil_memprotect(p_no_access_page, page_size, kVSFSysUtilMapProtNone);

  p_mmap += page_size;
  if (page_offset)
  {
    p_mmap += (page_size - page_offset);
  }
  *p_ptr = p_mmap;
}

void
vsf_secbuf_free(char** p_ptr)
{
  unsigned int map_size;
  unsigned long page_offset;
  char* p_mmap = *p_ptr;
  unsigned int page_size = vsf_sysutil_getpagesize();
  if (p_mmap == 0)
  {
    return;
  }
  /* Calculate the actual start of the mmap region */
  page_offset = (unsigned long) p_mmap % page_size;
  if (page_offset)
  {
    p_mmap -= page_offset;
  }
  p_mmap -= page_size;
  /* First make the first page readable so we can get the size */
  vsf_sysutil_memprotect(p_mmap, page_size, kVSFSysUtilMapProtReadOnly);
  /* Extract the mapping size */
  map_size = *((unsigned int*)p_mmap);
  /* Lose the mapping */
  vsf_sysutil_memunmap(p_mmap, map_size);
}

