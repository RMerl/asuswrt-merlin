/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

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

/* NOTE: see end of file for #undef of these macros */
#define unsigned_N XCONCAT2(unsigned_,N)
#define T2H_N XCONCAT2(T2H_,N)
#define H2T_N XCONCAT2(H2T_,N)

#define core_map_read_N XCONCAT2(core_map_read_,N)
#define core_map_write_N XCONCAT2(core_map_write_,N)

INLINE_CORE(unsigned_N)
core_map_read_N(core_map *map,
		unsigned_word addr,
		cpu *processor,
		unsigned_word cia)
{
  core_mapping *mapping = core_map_find_mapping(map,
						addr,
						sizeof(unsigned_N),
						processor,
						cia,
						1); /*abort*/
  if (WITH_CALLBACK_MEMORY && mapping->device != NULL) {
    unsigned_N data;
    if (device_io_read_buffer(mapping->device,
			      &data,
			      mapping->space,
			      addr,
			      sizeof(unsigned_N), /* nr_bytes */
			      processor,
			      cia) != sizeof(unsigned_N))
      device_error(mapping->device, "internal error - core_read_N() - io_read_buffer should not fail");
    return T2H_N(data);
  }
  else
    return T2H_N(*(unsigned_N*)core_translate(mapping, addr));
}



INLINE_CORE(void)
core_map_write_N(core_map *map,
		 unsigned_word addr,
		 unsigned_N val,
		 cpu *processor,
		 unsigned_word cia)
{
  core_mapping *mapping = core_map_find_mapping(map,
						addr,
						sizeof(unsigned_N),
						processor,
						cia,
						1); /*abort*/
  if (WITH_CALLBACK_MEMORY && mapping->device != NULL) {
    unsigned_N data = H2T_N(val);
    if (device_io_write_buffer(mapping->device,
			       &data,
			       mapping->space,
			       addr,
			       sizeof(unsigned_N), /* nr_bytes */
			       processor,
			       cia) != sizeof(unsigned_N))
      device_error(mapping->device, "internal error - core_write_N() - io_write_buffer should not fail");
  }
  else
    *(unsigned_N*)core_translate(mapping, addr) = H2T_N(val);
}

/* NOTE: see start of file for #define of these macros */
#undef unsigned_N
#undef T2H_N
#undef H2T_N
#undef core_map_read_N
#undef core_map_write_N
