/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _VM_C_
#define _VM_C_

#if 0
#include "basics.h"
#include "registers.h"
#include "device.h"
#include "corefile.h"
#include "vm.h"
#include "interrupts.h"
#include "mon.h"
#endif

#include "cpu.h"

/* OEA vs VEA

   For the VEA model, the VM layer is almost transparent.  It's only
   purpose is to maintain separate core_map's for the instruction
   and data address spaces.  This being so that writes to instruction
   space or execution of a data space is prevented.

   For the OEA model things are more complex.  The reason for separate
   instruction and data models becomes crucial.  The OEA model is
   built out of three parts.  An instruction map, a data map and an
   underlying structure that provides access to the VM data kept in
   main memory. */


/* OEA data structures:

   The OEA model maintains internal data structures that shadow the
   semantics of the various OEA VM registers (BAT, SR, etc).  This
   allows a simple efficient model of the VM to be implemented.

   Consistency between OEA registers and this model's internal data
   structures is maintained by updating the structures at
   `synchronization' points.  Of particular note is that (at the time
   of writing) the memory data types for BAT registers are rebuilt
   when ever the processor moves between problem and system states.

   Unpacked values are stored in the OEA so that they correctly align
   to where they will be needed by the PTE address. */


/* Protection table:

   Matrix of processor state, type of access and validity */

typedef enum {
  om_supervisor_state,
  om_problem_state,
  nr_om_modes
} om_processor_modes;

typedef enum {
  om_data_read, om_data_write,
  om_instruction_read, om_access_any,
  nr_om_access_types
} om_access_types;

static int om_valid_access[2][4][nr_om_access_types] = {
  /* read, write, instruction, any */
  /* K bit == 0 */
  { /*r  w  i  a       pp */
    { 1, 1, 1, 1 }, /* 00 */
    { 1, 1, 1, 1 }, /* 01 */
    { 1, 1, 1, 1 }, /* 10 */
    { 1, 0, 1, 1 }, /* 11 */
  },
  /* K bit == 1  or P bit valid */
  { /*r  w  i  a       pp */
    { 0, 0, 0, 0 }, /* 00 */
    { 1, 0, 1, 1 }, /* 01 */
    { 1, 1, 1, 1 }, /* 10 */
    { 1, 0, 1, 1 }, /* 11 */
  }
};


/* Bat translation:

   The bat data structure only contains information on valid BAT
   translations for the current processor mode and type of access. */

typedef struct _om_bat {
  unsigned_word block_effective_page_index;
  unsigned_word block_effective_page_index_mask;
  unsigned_word block_length_mask;
  unsigned_word block_real_page_number;
  int protection_bits;
} om_bat;

enum _nr_om_bat_registers {
  nr_om_bat_registers = 4
};

typedef struct _om_bats {
  int nr_valid_bat_registers;
  om_bat bat[nr_om_bat_registers];
} om_bats;


/* Segment TLB:

   In this model the 32 and 64 bit segment tables are treated in very
   similar ways.  The 32bit segment registers are treated as a
   simplification of the 64bit segment tlb */

enum _om_segment_tlb_constants {
#if (WITH_TARGET_WORD_BITSIZE == 64)
  sizeof_segment_table_entry_group = 128,
  sizeof_segment_table_entry = 16,
#endif
  om_segment_tlb_index_start_bit = 32,
  om_segment_tlb_index_stop_bit = 35,
  nr_om_segment_tlb_entries = 16,
  nr_om_segment_tlb_constants
};

typedef struct _om_segment_tlb_entry {
  int key[nr_om_modes];
  om_access_types invalid_access; /* set to instruction if no_execute bit */
  unsigned_word masked_virtual_segment_id; /* aligned ready for pte group addr */
#if (WITH_TARGET_WORD_BITSIZE == 64)
  int is_valid;
  unsigned_word masked_effective_segment_id;
#endif
} om_segment_tlb_entry;

typedef struct _om_segment_tlb {
  om_segment_tlb_entry entry[nr_om_segment_tlb_entries];
} om_segment_tlb;


/* Page TLB:

   This OEA model includes a small direct map Page TLB.  The tlb is to
   cut down on the need for the OEA to perform walks of the page hash
   table. */

enum _om_page_tlb_constants {
  om_page_tlb_index_start_bit = 46,
  om_page_tlb_index_stop_bit = 51,
  nr_om_page_tlb_entries = 64,
#if (WITH_TARGET_WORD_BITSIZE == 64)
  sizeof_pte_group = 128,
  sizeof_pte = 16,
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
  sizeof_pte_group = 64,
  sizeof_pte = 8,
#endif
  nr_om_page_tlb_constants
};

typedef struct _om_page_tlb_entry {
  int protection;
  int changed;
  unsigned_word real_address_of_pte_1;
  unsigned_word masked_virtual_segment_id;
  unsigned_word masked_page;
  unsigned_word masked_real_page_number;
} om_page_tlb_entry;

typedef struct _om_page_tlb {
  om_page_tlb_entry entry[nr_om_page_tlb_entries];
} om_page_tlb;


/* memory translation:

   OEA memory translation possibly involves BAT, SR, TLB and HTAB
   information*/

typedef struct _om_map {

  /* local cache of register values */
  int is_relocate;
  int is_problem_state;

  /* block address translation */
  om_bats *bat_registers;

  /* failing that, translate ea to va using segment tlb */
#if (WITH_TARGET_WORD_BITSIZE == 64)
  unsigned_word real_address_of_segment_table;
#endif
  om_segment_tlb *segment_tlb;

  /* then va to ra using hashed page table and tlb */
  unsigned_word real_address_of_page_table;
  unsigned_word page_table_hash_mask;
  om_page_tlb *page_tlb;

  /* physical memory for fetching page table entries */
  core_map *physical;

  /* address xor for PPC endian */
  unsigned xor[WITH_XOR_ENDIAN];

} om_map;


/* VM objects:

   External objects defined by vm.h */

struct _vm_instruction_map {
  /* real memory for last part */
  core_map *code;
  /* translate effective to real */
  om_map translation;
};

struct _vm_data_map {
  /* translate effective to real */
  om_map translation;
  /* real memory for translated address */
  core_map *read;
  core_map *write;
};


/* VM:

   Underlying memory object.  For the VEA this is just the
   core_map. For OEA it is the instruction and data memory
   translation's */

struct _vm {

  /* OEA: base address registers */
  om_bats ibats;
  om_bats dbats;

  /* OEA: segment registers */
  om_segment_tlb segment_tlb;

  /* OEA: translation lookaside buffers */
  om_page_tlb instruction_tlb;
  om_page_tlb data_tlb;

  /* real memory */
  core *physical;

  /* memory maps */
  vm_instruction_map instruction_map;
  vm_data_map data_map;

};


/* OEA Support procedures */


STATIC_INLINE_VM\
(unsigned_word)
om_segment_tlb_index(unsigned_word ea)
{
  unsigned_word index = EXTRACTED(ea,
				  om_segment_tlb_index_start_bit,
				  om_segment_tlb_index_stop_bit);
  return index;
}

STATIC_INLINE_VM\
(unsigned_word)
om_page_tlb_index(unsigned_word ea)
{
  unsigned_word index = EXTRACTED(ea,
				  om_page_tlb_index_start_bit,
				  om_page_tlb_index_stop_bit);
  return index;
}

STATIC_INLINE_VM\
(unsigned_word)
om_hash_page(unsigned_word masked_vsid,
	     unsigned_word ea)
{
  unsigned_word extracted_ea = EXTRACTED(ea, 36, 51);
#if (WITH_TARGET_WORD_BITSIZE == 32)
  unsigned_word masked_ea = INSERTED32(extracted_ea, 7, 31-6);
  unsigned_word hash = masked_vsid ^ masked_ea;
#endif
#if (WITH_TARGET_WORD_BITSIZE == 64)
  unsigned_word masked_ea = INSERTED64(extracted_ea, 18, 63-7);
  unsigned_word hash = masked_vsid ^ masked_ea;
#endif
  TRACE(trace_vm, ("ea=0x%lx - masked-vsid=0x%lx masked-ea=0x%lx hash=0x%lx\n",
		   (unsigned long)ea,
		   (unsigned long)masked_vsid,
		   (unsigned long)masked_ea,
		   (unsigned long)hash));
  return hash;
}

STATIC_INLINE_VM\
(unsigned_word)
om_pte_0_api(unsigned_word pte_0)
{
#if (WITH_TARGET_WORD_BITSIZE == 32)
  return EXTRACTED32(pte_0, 26, 31);
#endif
#if (WITH_TARGET_WORD_BITSIZE == 64)
  return EXTRACTED64(pte_0, 52, 56);
#endif
}

STATIC_INLINE_VM\
(unsigned_word)
om_pte_0_hash(unsigned_word pte_0)
{
#if (WITH_TARGET_WORD_BITSIZE == 32)
  return EXTRACTED32(pte_0, 25, 25);
#endif
#if (WITH_TARGET_WORD_BITSIZE == 64)
  return EXTRACTED64(pte_0, 62, 62);
#endif
}

STATIC_INLINE_VM\
(int)
om_pte_0_valid(unsigned_word pte_0)
{
#if (WITH_TARGET_WORD_BITSIZE == 32)
  return MASKED32(pte_0, 0, 0) != 0;
#endif
#if (WITH_TARGET_WORD_BITSIZE == 64)
  return MASKED64(pte_0, 63, 63) != 0;
#endif
}

STATIC_INLINE_VM\
(unsigned_word)
om_ea_masked_page(unsigned_word ea)
{
  return MASKED(ea, 36, 51);
}

STATIC_INLINE_VM\
(unsigned_word)
om_ea_masked_byte(unsigned_word ea)
{
  return MASKED(ea, 52, 63);
}

/* return the VSID aligned for pte group addr */
STATIC_INLINE_VM\
(unsigned_word)
om_pte_0_masked_vsid(unsigned_word pte_0)
{
#if (WITH_TARGET_WORD_BITSIZE == 32)
  return INSERTED32(EXTRACTED32(pte_0, 1, 24), 31-6-24+1, 31-6);
#endif
#if (WITH_TARGET_WORD_BITSIZE == 64)
  return INSERTED64(EXTRACTED64(pte_0, 0, 51), 63-7-52+1, 63-7);
#endif
}

STATIC_INLINE_VM\
(unsigned_word)
om_pte_1_pp(unsigned_word pte_1)
{
  return MASKED(pte_1, 62, 63); /*PP*/
}

STATIC_INLINE_VM\
(int)
om_pte_1_referenced(unsigned_word pte_1)
{
  return EXTRACTED(pte_1, 55, 55);
}

STATIC_INLINE_VM\
(int)
om_pte_1_changed(unsigned_word pte_1)
{
  return EXTRACTED(pte_1, 56, 56);
}

STATIC_INLINE_VM\
(int)
om_pte_1_masked_rpn(unsigned_word pte_1)
{
  return MASKED(pte_1, 0, 51); /*RPN*/
}

STATIC_INLINE_VM\
(unsigned_word)
om_ea_api(unsigned_word ea)
{
  return EXTRACTED(ea, 36, 41);
}


/* Page and Segment table read/write operators, these need to still
   account for the PPC's XOR operation */

STATIC_INLINE_VM\
(unsigned_word)
om_read_word(om_map *map,
	     unsigned_word ra,
	     cpu *processor,
	     unsigned_word cia)
{
  if (WITH_XOR_ENDIAN)
    ra ^= map->xor[sizeof(instruction_word) - 1];
  return core_map_read_word(map->physical, ra, processor, cia);
}

STATIC_INLINE_VM\
(void)
om_write_word(om_map *map,
	      unsigned_word ra,
	      unsigned_word val,
	      cpu *processor,
	      unsigned_word cia)
{
  if (WITH_XOR_ENDIAN)
    ra ^= map->xor[sizeof(instruction_word) - 1];
  core_map_write_word(map->physical, ra, val, processor, cia);
}


/* Bring things into existance */

INLINE_VM\
(vm *)
vm_create(core *physical)
{
  vm *virtual;

  /* internal checks */
  if (nr_om_segment_tlb_entries
      != (1 << (om_segment_tlb_index_stop_bit
		- om_segment_tlb_index_start_bit + 1)))
    error("internal error - vm_create - problem with om_segment constants\n");
  if (nr_om_page_tlb_entries
      != (1 << (om_page_tlb_index_stop_bit
		- om_page_tlb_index_start_bit + 1)))
    error("internal error - vm_create - problem with om_page constants\n");

  /* create the new vm register file */
  virtual = ZALLOC(vm);

  /* set up core */
  virtual->physical = physical;

  /* set up the address decoders */
  virtual->instruction_map.translation.bat_registers = &virtual->ibats;
  virtual->instruction_map.translation.segment_tlb = &virtual->segment_tlb;
  virtual->instruction_map.translation.page_tlb = &virtual->instruction_tlb;
  virtual->instruction_map.translation.is_relocate = 0;
  virtual->instruction_map.translation.is_problem_state = 0;
  virtual->instruction_map.translation.physical = core_readable(physical);
  virtual->instruction_map.code = core_readable(physical);

  virtual->data_map.translation.bat_registers = &virtual->dbats;
  virtual->data_map.translation.segment_tlb = &virtual->segment_tlb;
  virtual->data_map.translation.page_tlb = &virtual->data_tlb;
  virtual->data_map.translation.is_relocate = 0;
  virtual->data_map.translation.is_problem_state = 0;
  virtual->data_map.translation.physical = core_readable(physical);
  virtual->data_map.read = core_readable(physical);
  virtual->data_map.write = core_writeable(physical);

  return virtual;
}


STATIC_INLINE_VM\
(om_bat *)
om_effective_to_bat(om_map *map,
		    unsigned_word ea)
{
  int curr_bat = 0;
  om_bats *bats = map->bat_registers;
  int nr_bats = bats->nr_valid_bat_registers;

  for (curr_bat = 0; curr_bat < nr_bats; curr_bat++) {
    om_bat *bat = bats->bat + curr_bat;
    if ((ea & bat->block_effective_page_index_mask)
	!= bat->block_effective_page_index)
      continue;
    return bat;
  }

  return NULL;
}


STATIC_INLINE_VM\
(om_segment_tlb_entry *)
om_effective_to_virtual(om_map *map, 
			unsigned_word ea,
			cpu *processor,
			unsigned_word cia)
{
  /* first try the segment tlb */
  om_segment_tlb_entry *segment_tlb_entry = (map->segment_tlb->entry
					     + om_segment_tlb_index(ea));

#if (WITH_TARGET_WORD_BITSIZE == 32)
  TRACE(trace_vm, ("ea=0x%lx - sr[%ld] - masked-vsid=0x%lx va=0x%lx%07lx\n",
		   (unsigned long)ea,
		   (long)om_segment_tlb_index(ea),
		   (unsigned long)segment_tlb_entry->masked_virtual_segment_id, 
		   (unsigned long)EXTRACTED32(segment_tlb_entry->masked_virtual_segment_id, 31-6-24+1, 31-6),
		   (unsigned long)EXTRACTED32(ea, 4, 31)));
  return segment_tlb_entry;
#endif

#if (WITH_TARGET_WORD_BITSIZE == 64)
  if (segment_tlb_entry->is_valid
      && (segment_tlb_entry->masked_effective_segment_id == MASKED(ea, 0, 35))) {
    error("fixme - is there a need to update any bits\n");
    return segment_tlb_entry;
  }

  /* drats, segment tlb missed */
  {
    unsigned_word segment_id_hash = ea;
    int current_hash = 0;
    for (current_hash = 0; current_hash < 2; current_hash += 1) {
      unsigned_word segment_table_entry_group =
	(map->real_address_of_segment_table
	 | (MASKED64(segment_id_hash, 31, 35) >> (56-35)));
      unsigned_word segment_table_entry;
      for (segment_table_entry = segment_table_entry_group;
	   segment_table_entry < (segment_table_entry_group
				  + sizeof_segment_table_entry_group);
	   segment_table_entry += sizeof_segment_table_entry) {
	/* byte order? */
	unsigned_word segment_table_entry_dword_0 =
	  om_read_word(map->physical, segment_table_entry, processor, cia);
	unsigned_word segment_table_entry_dword_1 =
	  om_read_word(map->physical, segment_table_entry + 8,
		       processor, cia);
	int is_valid = MASKED64(segment_table_entry_dword_0, 56, 56) != 0;
	unsigned_word masked_effective_segment_id =
	  MASKED64(segment_table_entry_dword_0, 0, 35);
	if (is_valid && masked_effective_segment_id == MASKED64(ea, 0, 35)) {
	  /* don't permit some things */
	  if (MASKED64(segment_table_entry_dword_0, 57, 57))
	    error("om_effective_to_virtual() - T=1 in STE not supported\n");
	  /* update segment tlb */
	  segment_tlb_entry->is_valid = is_valid;
	  segment_tlb_entry->masked_effective_segment_id =
	    masked_effective_segment_id;
	  segment_tlb_entry->key[om_supervisor_state] =
	    EXTRACTED64(segment_table_entry_dword_0, 58, 58);
	  segment_tlb_entry->key[om_problem_state] =
	    EXTRACTED64(segment_table_entry_dword_0, 59, 59);
	  segment_tlb_entry->invalid_access =
	    (MASKED64(segment_table_entry_dword_0, 60, 60)
	     ? om_instruction_read
	     : om_access_any);
	  segment_tlb_entry->masked_virtual_segment_id =
	    INSERTED64(EXTRACTED64(segment_table_entry_dword_1, 0, 51),
		       18-13, 63-7); /* aligned ready for pte group addr */
	  return segment_tlb_entry;
	}
      }
      segment_id_hash = ~segment_id_hash;
    }
  }
  return NULL;
#endif
}



STATIC_INLINE_VM\
(om_page_tlb_entry *)
om_virtual_to_real(om_map *map, 
		   unsigned_word ea,
		   om_segment_tlb_entry *segment_tlb_entry,
		   om_access_types access,
		   cpu *processor,
		   unsigned_word cia)
{
  om_page_tlb_entry *page_tlb_entry = (map->page_tlb->entry
				       + om_page_tlb_index(ea));

  /* is it a tlb hit? */
  if ((page_tlb_entry->masked_virtual_segment_id
       == segment_tlb_entry->masked_virtual_segment_id)
      && (page_tlb_entry->masked_page
	  == om_ea_masked_page(ea))) {
    TRACE(trace_vm, ("ea=0x%lx - tlb hit - tlb=0x%lx\n",
	       (long)ea, (long)page_tlb_entry));
    return page_tlb_entry;
  }
      
  /* drats, it is a tlb miss */
  {
    unsigned_word page_hash =
      om_hash_page(segment_tlb_entry->masked_virtual_segment_id, ea);
    int current_hash;
    for (current_hash = 0; current_hash < 2; current_hash += 1) {
      unsigned_word real_address_of_pte_group =
	(map->real_address_of_page_table
	 | (page_hash & map->page_table_hash_mask));
      unsigned_word real_address_of_pte_0;
      TRACE(trace_vm,
	    ("ea=0x%lx - htab search %d - htab=0x%lx hash=0x%lx mask=0x%lx pteg=0x%lx\n",
	     (long)ea, current_hash,
	     map->real_address_of_page_table,
	     page_hash,
	     map->page_table_hash_mask,
	     (long)real_address_of_pte_group));
      for (real_address_of_pte_0 = real_address_of_pte_group;
	   real_address_of_pte_0 < (real_address_of_pte_group
				    + sizeof_pte_group);
	   real_address_of_pte_0 += sizeof_pte) {
	unsigned_word pte_0 = om_read_word(map,
					   real_address_of_pte_0,
					   processor, cia);
	/* did we hit? */
	if (om_pte_0_valid(pte_0)
	    && (current_hash == om_pte_0_hash(pte_0))
	    && (segment_tlb_entry->masked_virtual_segment_id
		== om_pte_0_masked_vsid(pte_0))
	    && (om_ea_api(ea) == om_pte_0_api(pte_0))) {
	  unsigned_word real_address_of_pte_1 = (real_address_of_pte_0
						 + sizeof_pte / 2);
	  unsigned_word pte_1 = om_read_word(map,
					     real_address_of_pte_1,
					     processor, cia);
	  page_tlb_entry->protection = om_pte_1_pp(pte_1);
	  page_tlb_entry->changed = om_pte_1_changed(pte_1);
	  page_tlb_entry->masked_virtual_segment_id = segment_tlb_entry->masked_virtual_segment_id;
	  page_tlb_entry->masked_page = om_ea_masked_page(ea);
	  page_tlb_entry->masked_real_page_number = om_pte_1_masked_rpn(pte_1);
	  page_tlb_entry->real_address_of_pte_1 = real_address_of_pte_1;
	  if (!om_pte_1_referenced(pte_1)) {
	    om_write_word(map,
			  real_address_of_pte_1,
			  pte_1 | BIT(55),
			  processor, cia);
	    TRACE(trace_vm,
		  ("ea=0x%lx - htab hit - set ref - tlb=0x%lx &pte1=0x%lx\n",
		   (long)ea, (long)page_tlb_entry, (long)real_address_of_pte_1));
	  }
	  else {
	    TRACE(trace_vm,
		  ("ea=0x%lx - htab hit - tlb=0x%lx &pte1=0x%lx\n",
		   (long)ea, (long)page_tlb_entry, (long)real_address_of_pte_1));
	  }
	  return page_tlb_entry;
	}
      }
      page_hash = ~page_hash; /*???*/
    }
  }
  return NULL;
}


STATIC_INLINE_VM\
(void)
om_interrupt(cpu *processor,
	     unsigned_word cia,
	     unsigned_word ea,
	     om_access_types access,
	     storage_interrupt_reasons reason)
{
  switch (access) {
  case om_data_read:
    data_storage_interrupt(processor, cia, ea, reason, 0/*!is_store*/);
    break;
  case om_data_write:
    data_storage_interrupt(processor, cia, ea, reason, 1/*is_store*/);
    break;
  case om_instruction_read:
    instruction_storage_interrupt(processor, cia, reason);
    break;
  default:
    error("internal error - om_interrupt - unexpected access type %d", access);
  }
}


STATIC_INLINE_VM\
(unsigned_word)
om_translate_effective_to_real(om_map *map,
			       unsigned_word ea,
			       om_access_types access,
			       cpu *processor,
			       unsigned_word cia,
			       int abort)
{
  om_bat *bat = NULL;
  om_segment_tlb_entry *segment_tlb_entry = NULL;
  om_page_tlb_entry *page_tlb_entry = NULL;
  unsigned_word ra;

  if (!map->is_relocate) {
    ra = ea;
    TRACE(trace_vm, ("ea=0x%lx - direct map - ra=0x%lx\n",
		     (long)ea, (long)ra));
    return ra;
  }

  /* match with BAT? */
  bat = om_effective_to_bat(map, ea);
  if (bat != NULL) {
    if (!om_valid_access[1][bat->protection_bits][access]) {
      TRACE(trace_vm, ("ea=0x%lx - bat access violation\n", (long)ea));
      if (abort)
	om_interrupt(processor, cia, ea, access,
		     protection_violation_storage_interrupt);
      else
	return MASK(0, 63);
    }

    ra = ((ea & bat->block_length_mask) | bat->block_real_page_number);
    TRACE(trace_vm, ("ea=0x%lx - bat translation - ra=0x%lx\n",
		     (long)ea, (long)ra));
    return ra;
  }

  /* translate ea to va using segment map */
  segment_tlb_entry = om_effective_to_virtual(map, ea, processor, cia);
#if (WITH_TARGET_WORD_BITSIZE == 64)
  if (segment_tlb_entry == NULL) {
    TRACE(trace_vm, ("ea=0x%lx - segment tlb miss\n", (long)ea));
    if (abort)
      om_interrupt(processor, cia, ea, access,
		   segment_table_miss_storage_interrupt);
    else
      return MASK(0, 63);
  }
#endif
  /* check for invalid segment access type */
  if (segment_tlb_entry->invalid_access == access) {
    TRACE(trace_vm, ("ea=0x%lx - segment access invalid\n", (long)ea));
    if (abort)
      om_interrupt(processor, cia, ea, access,
		   protection_violation_storage_interrupt);
    else
      return MASK(0, 63);
  }

  /* lookup in PTE */
  page_tlb_entry = om_virtual_to_real(map, ea, segment_tlb_entry,
				      access,
				      processor, cia);
  if (page_tlb_entry == NULL) {
    TRACE(trace_vm, ("ea=0x%lx - page tlb miss\n", (long)ea));
    if (abort)
      om_interrupt(processor, cia, ea, access,
		   hash_table_miss_storage_interrupt);
    else
      return MASK(0, 63);
  }
  if (!(om_valid_access
	[segment_tlb_entry->key[map->is_problem_state]]
	[page_tlb_entry->protection]
	[access])) {
    TRACE(trace_vm, ("ea=0x%lx - page tlb access violation\n", (long)ea));
    if (abort)
      om_interrupt(processor, cia, ea, access,
		   protection_violation_storage_interrupt);
    else
      return MASK(0, 63);
  }

  /* update change bit as needed */
  if (access == om_data_write &&!page_tlb_entry->changed) {
    unsigned_word pte_1 = om_read_word(map,
				       page_tlb_entry->real_address_of_pte_1,
				       processor, cia);
    om_write_word(map,
		  page_tlb_entry->real_address_of_pte_1,
		  pte_1 | BIT(56),
		  processor, cia);
    TRACE(trace_vm, ("ea=0x%lx - set change bit - tlb=0x%lx &pte1=0x%lx\n",
		     (long)ea, (long)page_tlb_entry,
		     (long)page_tlb_entry->real_address_of_pte_1));
  }

  ra = (page_tlb_entry->masked_real_page_number | om_ea_masked_byte(ea));
  TRACE(trace_vm, ("ea=0x%lx - page translation - ra=0x%lx\n",
		   (long)ea, (long)ra));
  return ra;
}


/*
 * Definition of operations for memory management
 */


/* rebuild all the relevant bat information */
STATIC_INLINE_VM\
(void)
om_unpack_bat(om_bat *bat,
	      spreg ubat,
	      spreg lbat)
{
  /* for extracting out the offset within a page */
  bat->block_length_mask = ((MASKED(ubat, 51, 61) << (17-2))
			    | MASK(63-17+1, 63));

  /* for checking the effective page index */
  bat->block_effective_page_index = MASKED(ubat, 0, 46);
  bat->block_effective_page_index_mask = ~bat->block_length_mask;

  /* protection information */
  bat->protection_bits = EXTRACTED(lbat, 62, 63);
  bat->block_real_page_number = MASKED(lbat, 0, 46);
}


/* rebuild the given bat table */
STATIC_INLINE_VM\
(void)
om_unpack_bats(om_bats *bats,
	       spreg *raw_bats,
	       msreg msr)
{
  int i;
  bats->nr_valid_bat_registers = 0;
  for (i = 0; i < nr_om_bat_registers*2; i += 2) {
    spreg ubat = raw_bats[i];
    spreg lbat = raw_bats[i+1];
    if ((msr & msr_problem_state)
	? EXTRACTED(ubat, 63, 63)
	: EXTRACTED(ubat, 62, 62)) {
      om_unpack_bat(&bats->bat[bats->nr_valid_bat_registers],
		    ubat, lbat);
      bats->nr_valid_bat_registers += 1;
    }
  }
}


#if (WITH_TARGET_WORD_BITSIZE == 32)
STATIC_INLINE_VM\
(void)
om_unpack_sr(vm *virtual,
	     sreg *srs,
	     int which_sr,
	     cpu *processor,
	     unsigned_word cia)
{
  om_segment_tlb_entry *segment_tlb_entry = 0;
  sreg new_sr_value = 0;

  /* check register in range */
  ASSERT(which_sr >= 0 && which_sr < nr_om_segment_tlb_entries);

  /* get the working values */
  segment_tlb_entry = &virtual->segment_tlb.entry[which_sr];  
  new_sr_value = srs[which_sr];
  
  /* do we support this */
  if (MASKED32(new_sr_value, 0, 0))
    cpu_error(processor, cia, "unsupported value of T in segment register %d",
	      which_sr);

  /* update info */
  segment_tlb_entry->key[om_supervisor_state] = EXTRACTED32(new_sr_value, 1, 1);
  segment_tlb_entry->key[om_problem_state] = EXTRACTED32(new_sr_value, 2, 2);
  segment_tlb_entry->invalid_access = (MASKED32(new_sr_value, 3, 3)
				       ? om_instruction_read
				       : om_access_any);
  segment_tlb_entry->masked_virtual_segment_id =
    INSERTED32(EXTRACTED32(new_sr_value, 8, 31),
	       31-6-24+1, 31-6); /* aligned ready for pte group addr */
}
#endif


#if (WITH_TARGET_WORD_BITSIZE == 32)
STATIC_INLINE_VM\
(void)
om_unpack_srs(vm *virtual,
	      sreg *srs,
	      cpu *processor,
	      unsigned_word cia)
{
  int which_sr;
  for (which_sr = 0; which_sr < nr_om_segment_tlb_entries; which_sr++) {
    om_unpack_sr(virtual, srs, which_sr,
		 processor, cia);
  }
}
#endif


/* Rebuild all the data structures for the new context as specifed by
   the passed registers */
INLINE_VM\
(void)
vm_synchronize_context(vm *virtual,
		       spreg *sprs,
		       sreg *srs,
		       msreg msr,
		       /**/
		       cpu *processor,
		       unsigned_word cia)
{

  /* enable/disable translation */
  int problem_state = (msr & msr_problem_state) != 0;
  int data_relocate = (msr & msr_data_relocate) != 0;
  int instruction_relocate = (msr & msr_instruction_relocate) != 0;
  int little_endian = (msr & msr_little_endian_mode) != 0;

  unsigned_word page_table_hash_mask;
  unsigned_word real_address_of_page_table;
 
  /* update current processor mode */
  virtual->instruction_map.translation.is_relocate = instruction_relocate;
  virtual->instruction_map.translation.is_problem_state = problem_state;
  virtual->data_map.translation.is_relocate = data_relocate;
  virtual->data_map.translation.is_problem_state = problem_state;

  /* update bat registers for the new context */
  om_unpack_bats(&virtual->ibats, &sprs[spr_ibat0u], msr);
  om_unpack_bats(&virtual->dbats, &sprs[spr_dbat0u], msr);

  /* unpack SDR1 - the storage description register 1 */
#if (WITH_TARGET_WORD_BITSIZE == 64)
  real_address_of_page_table = MASKED64(sprs[spr_sdr1], 0, 45);
  page_table_hash_mask = MASK64(18+28-EXTRACTED64(sprs[spr_sdr1], 59, 63),
				63-7);
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
  real_address_of_page_table = MASKED32(sprs[spr_sdr1], 0, 15);
  page_table_hash_mask = (INSERTED32(EXTRACTED32(sprs[spr_sdr1], 23, 31),
				     7, 7+9-1)
			  | MASK32(7+9, 31-6));
#endif
  virtual->instruction_map.translation.real_address_of_page_table = real_address_of_page_table;
  virtual->instruction_map.translation.page_table_hash_mask = page_table_hash_mask;
  virtual->data_map.translation.real_address_of_page_table = real_address_of_page_table;
  virtual->data_map.translation.page_table_hash_mask = page_table_hash_mask;


  /* unpack the segment tlb registers */
#if (WITH_TARGET_WORD_BITSIZE == 32)
  om_unpack_srs(virtual, srs,
		processor, cia);
#endif
 
  /* set up the XOR registers if the current endian mode conflicts
     with what is in the MSR */
  if (WITH_XOR_ENDIAN) {
    int i = 1;
    unsigned mask;
    if ((little_endian && CURRENT_TARGET_BYTE_ORDER == LITTLE_ENDIAN)
	|| (!little_endian && CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN))
      mask = 0;
    else
      mask = WITH_XOR_ENDIAN - 1;
    while (i - 1 < WITH_XOR_ENDIAN) {
      virtual->instruction_map.translation.xor[i-1] = mask;
      virtual->data_map.translation.xor[i-1] =  mask;
      mask = (mask << 1) & (WITH_XOR_ENDIAN - 1);
      i = i * 2;
    }
  }
  else {
    /* don't allow the processor to change endian modes */
    if ((little_endian && CURRENT_TARGET_BYTE_ORDER != LITTLE_ENDIAN)
	|| (!little_endian && CURRENT_TARGET_BYTE_ORDER != BIG_ENDIAN))
      cpu_error(processor, cia, "attempt to change hardwired byte order");
  }
}

/* update vm data structures due to a TLB operation */

INLINE_VM\
(void)
vm_page_tlb_invalidate_entry(vm *memory,
			     unsigned_word ea)
{
  int i = om_page_tlb_index(ea);
  memory->instruction_tlb.entry[i].masked_virtual_segment_id = MASK(0, 63);
  memory->data_tlb.entry[i].masked_virtual_segment_id = MASK(0, 63);
  TRACE(trace_vm, ("ea=0x%lx - tlb invalidate entry\n", (long)ea));
}

INLINE_VM\
(void)
vm_page_tlb_invalidate_all(vm *memory)
{
  int i;
  for (i = 0; i < nr_om_page_tlb_entries; i++) {
    memory->instruction_tlb.entry[i].masked_virtual_segment_id = MASK(0, 63);
    memory->data_tlb.entry[i].masked_virtual_segment_id = MASK(0, 63);
  }
  TRACE(trace_vm, ("tlb invalidate all\n"));
}



INLINE_VM\
(vm_data_map *)
vm_create_data_map(vm *memory)
{
  return &memory->data_map;
}


INLINE_VM\
(vm_instruction_map *)
vm_create_instruction_map(vm *memory)
{
  return &memory->instruction_map;
}


STATIC_INLINE_VM\
(unsigned_word)
vm_translate(om_map *map,
	     unsigned_word ea,
	     om_access_types access,
	     cpu *processor,
	     unsigned_word cia,
	     int abort)
{
  switch (CURRENT_ENVIRONMENT) {
  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    return ea;
  case OPERATING_ENVIRONMENT:
    return om_translate_effective_to_real(map, ea, access,
					  processor, cia,
					  abort);
  default:
    error("internal error - vm_translate - bad switch");
    return 0;
  }
}


INLINE_VM\
(unsigned_word)
vm_real_data_addr(vm_data_map *map,
		  unsigned_word ea,
		  int is_read,
		  cpu *processor,
		  unsigned_word cia)
{
  return vm_translate(&map->translation,
		      ea,
		      is_read ? om_data_read : om_data_write,
		      processor,
		      cia,
		      1); /*abort*/
}


INLINE_VM\
(unsigned_word)
vm_real_instruction_addr(vm_instruction_map *map,
			 cpu *processor,
			 unsigned_word cia)
{
  return vm_translate(&map->translation,
		      cia,
		      om_instruction_read,
		      processor,
		      cia,
		      1); /*abort*/
}

INLINE_VM\
(instruction_word)
vm_instruction_map_read(vm_instruction_map *map,
			cpu *processor,
			unsigned_word cia)
{
  unsigned_word ra = vm_real_instruction_addr(map, processor, cia);
  ASSERT((cia & 0x3) == 0); /* always aligned */
  if (WITH_XOR_ENDIAN)
    ra ^= map->translation.xor[sizeof(instruction_word) - 1];
  return core_map_read_4(map->code, ra, processor, cia);
}


INLINE_VM\
(int)
vm_data_map_read_buffer(vm_data_map *map,
			void *target,
			unsigned_word addr,
			unsigned nr_bytes,
			cpu *processor,
			unsigned_word cia)
{
  unsigned count;
  for (count = 0; count < nr_bytes; count++) {
    unsigned_1 byte;
    unsigned_word ea = addr + count;
    unsigned_word ra = vm_translate(&map->translation,
				    ea, om_data_read,
				    processor, /*processor*/
				    cia, /*cia*/
				    processor != NULL); /*abort?*/
    if (ra == MASK(0, 63))
      break;
    if (WITH_XOR_ENDIAN)
      ra ^= map->translation.xor[0];
    if (core_map_read_buffer(map->read, &byte, ra, sizeof(byte))
	!= sizeof(byte))
      break;
    ((unsigned_1*)target)[count] = T2H_1(byte);
  }
  return count;
}


INLINE_VM\
(int)
vm_data_map_write_buffer(vm_data_map *map,
			 const void *source,
			 unsigned_word addr,
			 unsigned nr_bytes,
			 int violate_read_only_section,
			 cpu *processor,
			 unsigned_word cia)
{
  unsigned count;
  unsigned_1 byte;
  for (count = 0; count < nr_bytes; count++) {
    unsigned_word ea = addr + count;
    unsigned_word ra = vm_translate(&map->translation,
				    ea, om_data_write,
				    processor,
				    cia,
				    processor != NULL); /*abort?*/
    if (ra == MASK(0, 63))
      break;
    if (WITH_XOR_ENDIAN)
      ra ^= map->translation.xor[0];
    byte = T2H_1(((unsigned_1*)source)[count]);
    if (core_map_write_buffer((violate_read_only_section
			       ? map->read
			       : map->write),
			      &byte, ra, sizeof(byte)) != sizeof(byte))
      break;
  }
  return count;
}


/* define the read/write 1/2/4/8/word functions */

#define N 1
#include "vm_n.h"
#undef N

#define N 2
#include "vm_n.h"
#undef N

#define N 4
#include "vm_n.h"
#undef N

#define N 8
#include "vm_n.h"
#undef N

#define N word
#include "vm_n.h"
#undef N



#endif /* _VM_C_ */
