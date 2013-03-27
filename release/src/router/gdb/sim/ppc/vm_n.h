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


#ifndef N
#error "N must be #defined"
#endif

/* NOTE: See end of file for #undef */
#define unsigned_N XCONCAT2(unsigned_,N)
#define T2H_N XCONCAT2(T2H_,N)
#define H2T_N XCONCAT2(H2T_,N)
#define vm_data_map_read_N XCONCAT2(vm_data_map_read_,N)
#define vm_data_map_write_N XCONCAT2(vm_data_map_write_,N)


INLINE_VM\
(unsigned_N)
vm_data_map_read_N(vm_data_map *map,
		   unsigned_word ea,
		   cpu *processor,
		   unsigned_word cia)
{
  if ((ea & (sizeof(unsigned_N)-1)) == 0) {
    unsigned ra = vm_real_data_addr(map, ea, 1/*is-read*/, processor, cia);
    unsigned_N val;
    if (WITH_XOR_ENDIAN)
      ra ^= map->translation.xor[sizeof(unsigned_N) - 1];
    val = XCONCAT2(core_map_read_,N)(map->read, ra, processor, cia);
    if (WITH_MON & MONITOR_LOAD_STORE_UNIT)
      mon_read(ea, ra, sizeof(unsigned_N), processor, cia);
    TRACE(trace_load_store, ("load cia=0x%lx ea=0x%lx N=%ld val=0x%lx\n",
			     (long)cia, (long)ea, (long)sizeof(unsigned_N), (long)val));
    return val;
  }
  else {
    switch (CURRENT_ALIGNMENT) {
    case STRICT_ALIGNMENT:
      alignment_interrupt(processor, cia, ea);
      return 0;
    case NONSTRICT_ALIGNMENT:
      {
	unsigned_N val;
	if (vm_data_map_read_buffer(map, &val, ea, sizeof(unsigned_N), processor, cia)
	    != sizeof(unsigned_N)) {
	  cpu_error(processor, cia, "misaligned %d byte read to 0x%lx failed",
		    sizeof(unsigned_N), (unsigned long)ea);
	}
	val = T2H_N(val);
	if (WITH_MON & MONITOR_LOAD_STORE_UNIT) {
	  /* YUCK */
	  unsigned ra = vm_real_data_addr(map, ea, 1, processor, cia);
	  mon_read(ea, ra, sizeof(unsigned_N), processor, cia);
	}
	TRACE(trace_load_store, ("load cia=0x%lx ea=0x%lx N=%ld data=0x%lx\n",
				 (long)cia, (long)ea, (long)sizeof(unsigned_N), (long)val));
	return val;
      }
    default:
      error("internal error - vm_data_map_read_N - bad switch");
      return 0;
    }
  }
}

INLINE_VM\
(void)
vm_data_map_write_N(vm_data_map *map,
		    unsigned_word ea,
		    unsigned_N val,
		    cpu *processor,
		    unsigned_word cia)
{
  if ((ea & (sizeof(unsigned_N)-1)) == 0) {
    unsigned ra = vm_real_data_addr(map, ea, 0/*is-read?*/, processor, cia);
    if (WITH_XOR_ENDIAN)
      ra ^= map->translation.xor[sizeof(unsigned_N) - 1];
    XCONCAT2(core_map_write_,N)(map->write, ra, val, processor, cia);
    if (WITH_MON & MONITOR_LOAD_STORE_UNIT)
      mon_write(ea, ra, sizeof(unsigned_N), processor, cia);
    TRACE(trace_load_store, ("store cia=0x%lx ea=0x%lx N=%ld val=0x%lx\n",
			     (long)cia, (long)ea, (long)sizeof(unsigned_N), (long)val));
  }
  else {
    switch (CURRENT_ALIGNMENT) {
    case STRICT_ALIGNMENT:
      alignment_interrupt(processor, cia, ea);
      break;
    case NONSTRICT_ALIGNMENT:
      {
	unsigned_N data = H2T_N(val);
	if (vm_data_map_write_buffer(map, &data, ea, sizeof(unsigned_N), 0, processor, cia)
	    != sizeof(unsigned_N)) {
	  cpu_error(processor, cia, "misaligned %d byte write to 0x%lx failed",
		    sizeof(unsigned_N), (unsigned long)ea);
	}
	if (WITH_MON & MONITOR_LOAD_STORE_UNIT) {
	  /* YUCK */
	  unsigned ra = vm_real_data_addr(map, ea, 1, processor, cia);
	  mon_write(ea, ra, sizeof(unsigned_N), processor, cia);
	}
	TRACE(trace_load_store, ("store cia=0x%lx ea=0x%lx N=%ld val=0x%lx\n",
				 (long)cia, (long)ea, (long)sizeof(unsigned_N), (long)val));
      }
      break;
    default:
      error("internal error - vm_data_map_write_N - bad switch");
    }
  }
}

/* NOTE: see start of file for #define */
#undef unsigned_N
#undef T2H_N
#undef H2T_N
#undef vm_data_map_read_N
#undef vm_data_map_write_N
