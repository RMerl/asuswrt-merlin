/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 2003, 2004 Andrew Cagney

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _HW_HTAB_C_
#define _HW_HTAB_C_

#include "device_table.h"

#include "bfd.h"


/* DEVICE


   htab - pseudo-device describing a PowerPC hash table

   
   DESCRIPTION
   
   
   During the initialization of the device tree, the pseudo-device
   <<htab>>, in conjunction with any child <<pte>> pseudo-devices,
   will create a PowerPC hash table in memory.  The hash table values
   are written using dma transfers.
   
   The size and address of the hash table are determined by properties
   of the htab node.
    
   By convention, the htab device is made a child of the
   <</openprom/init>> node.
   
   By convention, the real address of the htab is used as the htab
   nodes unit address.
   
   
   PROPERTIES
   

   real-address = <address> (required)

   The physical address of the hash table.  The PowerPC architecture
   places limitations on what is a valid hash table real-address.

   
   nr-bytes = <size> (required)

   The size of the hash table (in bytes) that is to be created at
   <<real-address>>.  The PowerPC architecture places limitations on
   what is a valid hash table size.

   
   claim = <anything> (optional)

   If this property is present, the memory used to construct the hash
   table will be claimed from the memory device.  The memory device
   being specified by the <</chosen/memory>> ihandle property.

   
   EXAMPLES
   
   Enable tracing.
   
   |  $  psim -t htab-device \
   
   
   Create a htab specifying the base address and minimum size.
   
   |    -o '/openprom/init/htab@0x10000/real-address 0x10000' \
   |    -o '/openprom/init/htab@0x10000/claim 0' \
   |    -o '/openprom/init/htab@0x10000/nr-bytes 65536' \
   
   
   BUGS
   
   
   See the <<pte>> device.
   
   
   */

   
/* DEVICE


   pte - pseudo-device describing a htab entry


   DESCRIPTION

   
   The <<pte>> pseudo-device, which must be a child of a <<htabl>>
   node, describes a virtual to physical mapping that is to be entered
   into the parents hash table.
   
   Two alternative specifications of the mapping are allowed.  Either
   a section of physical memory can be mapped to a virtual address, or
   the header of an executible image can be used to define the
   mapping.
   
   By convention, the real address of the map is specified as the pte
   devices unit address.
   
   
   PROPERTIES

   
   real-address = <address> (required)

   The starting physical address that is to be mapped by the hash
   table.

   
   wimg = <int> (required)
   pp = <int> (required)

   The value of hash table protection bits that are to be used when
   creating the virtual to physical address map.

   
   claim = <anything> (optional)

   If this property is present, the real memory that is being mapped by the 
   hash table will be claimed from the memory node (specified by the 
   ihandle <</chosen/memory>>).
   

   virtual-address = <integer> [ <integer> ]  (option A)
   nr-bytes = <size>  (option A)

   Option A - Virtual virtual address (and size) at which the physical
   address is to be mapped.  If multiple values are specified for the
   virtual address then they are concatenated to gether to form a
   longer virtual address.

   
   file-name = <string>  (option B)

   Option B - An executable image that is to be loaded (starting at
   the physical address specified above) and then mapped in using
   informatioin taken from the executables header.  information found
   in the files header.

   
   EXAMPLES
   
   
   Enable tracing (note that both the <<htab>> and <<pte>> device use the 
   same trace option).
   
   |   -t htab-device \
   
   
   Map a block of physical memory into a specified virtual address:
 	
   |  -o '/openprom/init/htab/pte@0x0/real-address 0' \
   |  -o '/openprom/init/htab/pte@0x0/nr-bytes 4096' \
   |  -o '/openprom/init/htab/pte@0x0/virtual-address 0x1000000' \
   |  -o '/openprom/init/htab/pte@0x0/claim 0' \
   |  -o '/openprom/init/htab/pte@0x0/wimg 0x7' \
   |  -o '/openprom/init/htab/pte@0x0/pp 0x2' \
   
   
   Map a file into memory.
   
   |  -o '/openprom/init/htab/pte@0x10000/real-address 0x10000' \
   |  -o '/openprom/init/htab/pte@0x10000/file-name "netbsd.elf' \
   |  -o '/openprom/init/htab/pte@0x10000/wimg 0x7' \
   |  -o '/openprom/init/htab/pte@0x10000/pp 0x2' \
   
   
   BUGS
   
   
   For an ELF executable, the header defines both the virtual and real 
   address at which each file section should be loaded.  At present, the 
   real addresses that are specified in the header are ignored, the file 
   instead being loaded in to physical memory in a linear fashion.
   
   When claiming memory, this device assumes that the #address-cells
   and #size-cells is one.  For future implementations, this may not
   be the case.
   
   */



static void
htab_decode_hash_table(device *me,
		       unsigned32 *htaborg,
		       unsigned32 *htabmask)
{
  unsigned_word htab_ra;
  unsigned htab_nr_bytes;
  unsigned n;
  device *parent = device_parent(me);
  /* determine the location/size of the hash table */
  if (parent == NULL
      || strcmp(device_name(parent), "htab") != 0)
    device_error(parent, "must be a htab device");
  htab_ra = device_find_integer_property(parent, "real-address");
  htab_nr_bytes = device_find_integer_property(parent, "nr-bytes");
  if (htab_nr_bytes < 0x10000) {
    device_error(parent, "htab size 0x%x less than 0x1000",
		 htab_nr_bytes);
  }
  for (n = htab_nr_bytes; n > 1; n = n / 2) {
    if (n % 2 != 0)
      device_error(parent, "htab size 0x%x not a power of two",
		   htab_nr_bytes);
  }
  *htaborg = htab_ra;
  /* Position the HTABMASK ready for use against a hashed address and
     not ready for insertion into SDR1.HTABMASK.  */
  *htabmask = MASKED32(htab_nr_bytes - 1, 7, 31-6);
  /* Check that the MASK and ADDRESS do not overlap.  */
  if ((htab_ra & (*htabmask)) != 0) {
    device_error(parent, "htaborg 0x%lx not aligned to htabmask 0x%lx",
		 (unsigned long)*htaborg, (unsigned long)*htabmask);
  }
  DTRACE(htab, ("htab - htaborg=0x%lx htabmask=0x%lx\n",
		(unsigned long)*htaborg, (unsigned long)*htabmask));
}

static void
htab_map_page(device *me,
	      unsigned_word ra,
	      unsigned64 va,
	      unsigned wimg,
	      unsigned pp,
	      unsigned32 htaborg,
	      unsigned32 htabmask)
{
  /* keep everything left shifted so that the numbering is easier */
  unsigned64 vpn = va << 12;
  unsigned32 vsid = INSERTED32(EXTRACTED64(vpn, 0, 23), 0, 23);
  unsigned32 vpage = INSERTED32(EXTRACTED64(vpn, 24, 39), 0, 15);
  unsigned32 hash = INSERTED32(EXTRACTED32(vsid, 5, 23)
			       ^ EXTRACTED32(vpage, 0, 15),
			       7, 31-6);
  int h;
  for (h = 0; h < 2; h++) {
    unsigned32 pteg = (htaborg | (hash & htabmask));
    int pti;
    for (pti = 0; pti < 8; pti++) {
      unsigned32 pte = pteg + 8 * pti;
      unsigned32 current_target_pte0;
      unsigned32 current_pte0;
      if (device_dma_read_buffer(device_parent(me),
				 &current_target_pte0,
				 0, /*space*/
				 pte,
				 sizeof(current_target_pte0)) != 4)
	device_error(me, "failed to read a pte at 0x%lx", (unsigned long)pte);
      current_pte0 = T2H_4(current_target_pte0);
      if (MASKED32(current_pte0, 0, 0)) {
	/* full pte, check it isn't already mapping the same virtual
           address */
	unsigned32 curr_vsid = INSERTED32(EXTRACTED32(current_pte0, 1, 24), 0, 23);
	unsigned32 curr_api = INSERTED32(EXTRACTED32(current_pte0, 26, 31), 0, 5);
	unsigned32 curr_h = EXTRACTED32(current_pte0, 25, 25);
	if (curr_h == h
	    && curr_vsid == vsid
	    && curr_api == MASKED32(vpage, 0, 5))
	  device_error(me, "duplicate map - va=0x%08lx ra=0x%lx vsid=0x%lx h=%d vpage=0x%lx hash=0x%lx pteg=0x%lx+%2d pte0=0x%lx",
		       (unsigned long)va,
		       (unsigned long)ra,
		       (unsigned long)vsid,
		       h,
		       (unsigned long)vpage,
		       (unsigned long)hash,
		       (unsigned long)pteg,
		       pti * 8,
		       (unsigned long)current_pte0);
      }
      else {
	/* empty pte fill it */
	unsigned32 pte0 = (MASK32(0, 0)
			   | INSERTED32(EXTRACTED32(vsid, 0, 23), 1, 24)
			   | INSERTED32(h, 25, 25)
			   | INSERTED32(EXTRACTED32(vpage, 0, 5), 26, 31));
	unsigned32 target_pte0 = H2T_4(pte0);
	unsigned32 pte1 = (INSERTED32(EXTRACTED32(ra, 0, 19), 0, 19)
			   | INSERTED32(wimg, 25, 28)
			   | INSERTED32(pp, 30, 31));
	unsigned32 target_pte1 = H2T_4(pte1);
	if (device_dma_write_buffer(device_parent(me),
				    &target_pte0,
				    0, /*space*/
				    pte,
				    sizeof(target_pte0),
				    1/*ro?*/) != 4
	    || device_dma_write_buffer(device_parent(me),
				       &target_pte1,
				       0, /*space*/
				       pte + 4,
				       sizeof(target_pte1),
				       1/*ro?*/) != 4)
	  device_error(me, "failed to write a pte a 0x%lx", (unsigned long)pte);
	DTRACE(htab, ("map - va=0x%08lx ra=0x%lx vsid=0x%lx h=%d vpage=0x%lx hash=0x%lx pteg=0x%lx+%2d pte0=0x%lx pte1=0x%lx\n",
		      (unsigned long)va,
		      (unsigned long)ra,
		      (unsigned long)vsid,
		      h,
		      (unsigned long)vpage,
		      (unsigned long)hash,
		      (unsigned long)pteg,
		      pti * 8,
		      (unsigned long)pte0,
		      (unsigned long)pte1));
	return;
      }
    }
    /* re-hash */
    hash = MASKED32(~hash, 0, 18);
  }
}

static unsigned_word
claim_memory(device *me,
	     device_instance *memory,
	     unsigned_word ra,
	     unsigned_word size)
{
  unsigned32 args[3];
  unsigned32 results[1];
  int status;
  args[0] = 0; /* alignment */
  args[1] = size;
  args[2] = ra;
  status = device_instance_call_method(memory, "claim", 3, args, 1, results);
  if (status != 0)
    device_error(me, "failed to claim memory");
  return results[0];
}

static void
htab_map_region(device *me,
		device_instance *memory,
		unsigned_word pte_ra,
		unsigned64 pte_va,
		unsigned nr_bytes,
		unsigned wimg,
		unsigned pp,
		unsigned32 htaborg,
		unsigned32 htabmask)
{
  unsigned_word ra;
  unsigned64 va;
  /* claim the memory */
  if (memory != NULL)
    claim_memory(me, memory, pte_ra, nr_bytes);
  /* go through all pages and create a pte for each */
  for (ra = pte_ra, va = pte_va;
       ra < pte_ra + nr_bytes;
       ra += 0x1000, va += 0x1000) {
    htab_map_page(me, ra, va, wimg, pp, htaborg, htabmask);
  }
}
  
typedef struct _htab_binary_sizes {
  unsigned_word text_ra;
  unsigned_word text_base;
  unsigned_word text_bound;
  unsigned_word data_ra;
  unsigned_word data_base;
  unsigned data_bound;
  device *me;
} htab_binary_sizes;

static void
htab_sum_binary(bfd *abfd,
		sec_ptr sec,
		PTR data)
{
  htab_binary_sizes *sizes = (htab_binary_sizes*)data;
  unsigned_word size = bfd_get_section_size (sec);
  unsigned_word vma = bfd_get_section_vma (abfd, sec);
  unsigned_word ra = bfd_get_section_lma (abfd, sec);

  /* skip the section if no memory to allocate */
  if (! (bfd_get_section_flags(abfd, sec) & SEC_ALLOC))
    return;

  if ((bfd_get_section_flags (abfd, sec) & SEC_CODE)
      || (bfd_get_section_flags (abfd, sec) & SEC_READONLY)) {
    if (sizes->text_bound < vma + size)
      sizes->text_bound = ALIGN_PAGE(vma + size);
    if (sizes->text_base > vma)
      sizes->text_base = FLOOR_PAGE(vma);
    if (sizes->text_ra > ra)
      sizes->text_ra = FLOOR_PAGE(ra);
  }
  else if ((bfd_get_section_flags (abfd, sec) & SEC_DATA)
	   || (bfd_get_section_flags (abfd, sec) & SEC_ALLOC)) {
    if (sizes->data_bound < vma + size)
      sizes->data_bound = ALIGN_PAGE(vma + size);
    if (sizes->data_base > vma)
      sizes->data_base = FLOOR_PAGE(vma);
    if (sizes->data_ra > ra)
      sizes->data_ra = FLOOR_PAGE(ra);
  }
}

static void
htab_dma_binary(bfd *abfd,
		sec_ptr sec,
		PTR data)
{
  htab_binary_sizes *sizes = (htab_binary_sizes*)data;
  void *section_init;
  unsigned_word section_vma;
  unsigned_word section_size;
  unsigned_word section_ra;
  device *me = sizes->me;

  /* skip the section if no memory to allocate */
  if (! (bfd_get_section_flags(abfd, sec) & SEC_ALLOC))
    return;

  /* check/ignore any sections of size zero */
  section_size = bfd_get_section_size (sec);
  if (section_size == 0)
    return;

  /* if nothing to load, ignore this one */
  if (! (bfd_get_section_flags(abfd, sec) & SEC_LOAD))
    return;

  /* find where it is to go */
  section_vma = bfd_get_section_vma(abfd, sec);
  section_ra = 0;
  if ((bfd_get_section_flags (abfd, sec) & SEC_CODE)
      || (bfd_get_section_flags (abfd, sec) & SEC_READONLY))
    section_ra = (section_vma - sizes->text_base + sizes->text_ra);
  else if ((bfd_get_section_flags (abfd, sec) & SEC_DATA))
    section_ra = (section_vma - sizes->data_base + sizes->data_ra);
  else 
    return; /* just ignore it */

  DTRACE(htab,
	 ("load - name=%-7s vma=0x%.8lx size=%6ld ra=0x%.8lx flags=%3lx(%s%s%s%s%s )\n",
	  bfd_get_section_name(abfd, sec),
	  (long)section_vma,
	  (long)section_size,
	  (long)section_ra,
	  (long)bfd_get_section_flags(abfd, sec),
	  bfd_get_section_flags(abfd, sec) & SEC_LOAD ? " LOAD" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_CODE ? " CODE" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_DATA ? " DATA" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_ALLOC ? " ALLOC" : "",
	  bfd_get_section_flags(abfd, sec) & SEC_READONLY ? " READONLY" : ""
	  ));

  /* dma in the sections data */
  section_init = zalloc(section_size);
  if (!bfd_get_section_contents(abfd,
				sec,
				section_init, 0,
				section_size)) {
    bfd_perror("devices/pte");
    device_error(me, "no data loaded");
  }
  if (device_dma_write_buffer(device_parent(me),
			      section_init,
			      0 /*space*/,
			      section_ra,
			      section_size,
			      1 /*violate_read_only*/)
      != section_size)
    device_error(me, "broken dma transfer");
  zfree(section_init); /* only free if load */
}

/* create a memory map from a binaries virtual addresses to a copy of
   the binary laid out linearly in memory */

static void
htab_map_binary(device *me,
		device_instance *memory,
		unsigned_word ra,
		unsigned wimg,
		unsigned pp,
		const char *file_name,
		unsigned32 htaborg,
		unsigned32 htabmask)
{
  htab_binary_sizes sizes;
  bfd *image;
  sizes.text_ra = -1;
  sizes.data_ra = -1;
  sizes.text_base = -1;
  sizes.data_base = -1;
  sizes.text_bound = 0;
  sizes.data_bound = 0;
  sizes.me = me;

  /* open the file */
  image = bfd_openr(file_name, NULL);
  if (image == NULL) {
    bfd_perror("devices/pte");
    device_error(me, "the file %s not loaded", file_name);
  }

  /* check it is valid */
  if (!bfd_check_format(image, bfd_object)) {
    bfd_close(image);
    device_error(me, "the file %s has an invalid binary format", file_name);
  }

  /* determine the size of each of the files regions */
  bfd_map_over_sections (image, htab_sum_binary, (PTR) &sizes);

  /* if needed, determine the real addresses of the sections */
  if (ra != -1) {
    sizes.text_ra = ra;
    sizes.data_ra = ALIGN_PAGE(sizes.text_ra + 
			       (sizes.text_bound - sizes.text_base));
  }

  DTRACE(htab, ("text map - base=0x%lx bound=0x%lx-1 ra=0x%lx\n",
		(unsigned long)sizes.text_base,
		(unsigned long)sizes.text_bound,
		(unsigned long)sizes.text_ra));
  DTRACE(htab, ("data map - base=0x%lx bound=0x%lx-1 ra=0x%lx\n",
		(unsigned long)sizes.data_base,
		(unsigned long)sizes.data_bound,
		(unsigned long)sizes.data_ra));

  /* check for and fix a botched image (text and data segments
     overlap) */
  if ((sizes.text_base <= sizes.data_base
       && sizes.text_bound >= sizes.data_bound)
      || (sizes.data_base <= sizes.text_base
	  && sizes.data_bound >= sizes.data_bound)
      || (sizes.text_bound > sizes.data_base
	  && sizes.text_bound <= sizes.data_bound)
      || (sizes.text_base >= sizes.data_base
	  && sizes.text_base < sizes.data_bound)) {
    DTRACE(htab, ("text and data segment overlaped - using just data segment\n"));
    /* check va->ra linear */
    if ((sizes.text_base - sizes.text_ra)
	!= (sizes.data_base - sizes.data_ra))
      device_error(me, "overlapping but missaligned text and data segments");
    /* enlarge the data segment */
    if (sizes.text_base < sizes.data_base)
      sizes.data_base = sizes.text_base;
    if (sizes.text_bound > sizes.data_bound)
      sizes.data_bound = sizes.text_bound;
    if (sizes.text_ra < sizes.data_ra)
      sizes.data_ra = sizes.text_ra;
    /* zap the text segment */
    sizes.text_base = 0;
    sizes.text_bound = 0;
    sizes.text_ra = 0;
    DTRACE(htab, ("common map - base=0x%lx bound=0x%lx-1 ra=0x%lx\n",
		  (unsigned long)sizes.data_base,
		  (unsigned long)sizes.data_bound,
		  (unsigned long)sizes.data_ra));
  }

  /* set up virtual memory maps for each of the regions */
  if (sizes.text_bound - sizes.text_base > 0) {
    htab_map_region(me, memory, sizes.text_ra, sizes.text_base,
		    sizes.text_bound - sizes.text_base,
		    wimg, pp,
		    htaborg, htabmask);
  }

  htab_map_region(me, memory, sizes.data_ra, sizes.data_base,
		  sizes.data_bound - sizes.data_base,
		  wimg, pp,
		  htaborg, htabmask);

  /* dma the sections into physical memory */
  bfd_map_over_sections (image, htab_dma_binary, (PTR) &sizes);
}

static void
htab_init_data_callback(device *me)
{
  device_instance *memory = NULL;
  if (WITH_TARGET_WORD_BITSIZE != 32)
    device_error(me, "only 32bit targets currently suported");

  /* find memory device */
  if (device_find_property(me, "claim") != NULL)
    memory = tree_find_ihandle_property(me, "/chosen/memory");

  /* for the htab, just allocate space for it */
  if (strcmp(device_name(me), "htab") == 0) {
    unsigned_word address = device_find_integer_property(me, "real-address");
    unsigned_word length = device_find_integer_property(me, "nr-bytes");
    unsigned_word base = claim_memory(me, memory, address, length);
    if (base == -1 || base != address)
      device_error(me, "cannot allocate hash table");
  }

  /* for the pte, do all the real work */
  if (strcmp(device_name(me), "pte") == 0) {
    unsigned32 htaborg;
    unsigned32 htabmask;

    htab_decode_hash_table(me, &htaborg, &htabmask);

    if (device_find_property(me, "file-name") != NULL) {
      /* map in a binary */
      unsigned pte_wimg = device_find_integer_property(me, "wimg");
      unsigned pte_pp = device_find_integer_property(me, "pp");
      const char *file_name = device_find_string_property(me, "file-name");
      if (device_find_property(me, "real-address") != NULL) {
	unsigned32 pte_ra = device_find_integer_property(me, "real-address");
	DTRACE(htab, ("pte - ra=0x%lx, wimg=%ld, pp=%ld, file-name=%s\n",
		      (unsigned long)pte_ra,
		      (unsigned long)pte_wimg,
		      (long)pte_pp,
		      file_name));
	htab_map_binary(me, memory, pte_ra, pte_wimg, pte_pp, file_name,
			htaborg, htabmask);
      }
      else {
	DTRACE(htab, ("pte - wimg=%ld, pp=%ld, file-name=%s\n",
		      (unsigned long)pte_wimg,
		      (long)pte_pp,
		      file_name));
	htab_map_binary(me, memory, -1, pte_wimg, pte_pp, file_name,
			htaborg, htabmask);
      }
    }
    else {
      /* handle a normal mapping definition */
      unsigned64 pte_va = 0;
      unsigned32 pte_ra = device_find_integer_property(me, "real-address");
      unsigned pte_nr_bytes = device_find_integer_property(me, "nr-bytes");
      unsigned pte_wimg = device_find_integer_property(me, "wimg");
      unsigned pte_pp = device_find_integer_property(me, "pp");
      signed_cell partial_va;
      int i;
      for (i = 0;
	   device_find_integer_array_property(me, "virtual-address", i, &partial_va);
	   i++) {
	pte_va = (pte_va << WITH_TARGET_WORD_BITSIZE) | (unsigned_cell)partial_va;
      }
      DTRACE(htab, ("pte - ra=0x%lx, wimg=%ld, pp=%ld, va=0x%lx, nr_bytes=%ld\n",
		    (unsigned long)pte_ra,
		    (long)pte_wimg,
		    (long)pte_pp,
		    (unsigned long)pte_va,
		    (long)pte_nr_bytes));
      htab_map_region(me, memory, pte_ra, pte_va, pte_nr_bytes, pte_wimg, pte_pp,
		      htaborg, htabmask);
    }
  }
}


static device_callbacks const htab_callbacks = {
  { NULL, htab_init_data_callback, },
  { NULL, }, /* address */
  { NULL, }, /* IO */
  { passthrough_device_dma_read_buffer,
    passthrough_device_dma_write_buffer, },
  { NULL, }, /* interrupt */
  { generic_device_unit_decode,
    generic_device_unit_encode, },
};

const device_descriptor hw_htab_device_descriptor[] = {
  { "htab", NULL, &htab_callbacks },
  { "pte", NULL, &htab_callbacks }, /* yep - uses htab's table */
  { NULL },
};

#endif /* _HW_HTAB_C_ */
