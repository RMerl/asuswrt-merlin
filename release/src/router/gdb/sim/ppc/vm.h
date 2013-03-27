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


#ifndef _VM_H_
#define _VM_H_

typedef struct _vm vm;
typedef struct _vm_data_map vm_data_map;
typedef struct _vm_instruction_map vm_instruction_map;


/* each PowerPC requires two virtual memory maps */

INLINE_VM\
(vm *) vm_create
(core *memory);

INLINE_VM\
(vm_data_map *) vm_create_data_map
(vm *memory);

INLINE_VM\
(vm_instruction_map *) vm_create_instruction_map
(vm *memory);


/* address translation, if the translation is invalid
   these will not return */

INLINE_VM\
(unsigned_word) vm_real_data_addr
(vm_data_map *data_map,
 unsigned_word ea,
 int is_read,
 cpu *processor,
 unsigned_word cia);

INLINE_VM\
(unsigned_word) vm_real_instruction_addr
(vm_instruction_map *instruction_map,
 cpu *processor,
 unsigned_word cia);


/* generic block transfers.  Dependant on the presence of the
   PROCESSOR arg, either returns the number of bytes transfered or (if
   PROCESSOR is non NULL) aborts the simulation */

INLINE_VM\
(int) vm_data_map_read_buffer
(vm_data_map *map,
 void *target,
 unsigned_word addr,
 unsigned len,
 cpu *processor,
 unsigned_word cia);

INLINE_VM\
(int) vm_data_map_write_buffer
(vm_data_map *map,
 const void *source,
 unsigned_word addr,
 unsigned len,
 int violate_read_only_section,
 cpu *processor,
 unsigned_word cia);


/* fetch the next instruction from memory */

INLINE_VM\
(instruction_word) vm_instruction_map_read
(vm_instruction_map *instruction_map,
 cpu *processor,
 unsigned_word cia);


/* read data from memory */

#define DECLARE_VM_DATA_MAP_READ_N(N) \
INLINE_VM\
(unsigned_##N) vm_data_map_read_##N \
(vm_data_map *map, \
 unsigned_word ea, \
 cpu *processor, \
 unsigned_word cia);

DECLARE_VM_DATA_MAP_READ_N(1)
DECLARE_VM_DATA_MAP_READ_N(2)
DECLARE_VM_DATA_MAP_READ_N(4)
DECLARE_VM_DATA_MAP_READ_N(8)
DECLARE_VM_DATA_MAP_READ_N(word)


/* write data to memory */

#define DECLARE_VM_DATA_MAP_WRITE_N(N) \
INLINE_VM\
(void) vm_data_map_write_##N \
(vm_data_map *map, \
 unsigned_word addr, \
 unsigned_##N val, \
 cpu *processor, \
 unsigned_word cia);

DECLARE_VM_DATA_MAP_WRITE_N(1)
DECLARE_VM_DATA_MAP_WRITE_N(2)
DECLARE_VM_DATA_MAP_WRITE_N(4)
DECLARE_VM_DATA_MAP_WRITE_N(8)
DECLARE_VM_DATA_MAP_WRITE_N(word)


/* update vm data structures due to a synchronization point */

INLINE_VM\
(void) vm_synchronize_context
(vm *memory,
 spreg *sprs,
 sreg *srs,
 msreg msr,
 /**/
 cpu *processor,
 unsigned_word cia);


/* update vm data structures due to a TLB operation */

INLINE_VM\
(void) vm_page_tlb_invalidate_entry
(vm *memory,
 unsigned_word ea);

INLINE_VM\
(void) vm_page_tlb_invalidate_all
(vm *memory);

#endif
