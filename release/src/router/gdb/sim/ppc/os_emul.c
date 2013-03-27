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


#ifndef _OS_EMUL_C_
#define _OS_EMUL_C_

#include "cpu.h"
#include "idecode.h"
#include "os_emul.h"

#include "emul_generic.h"
#include "emul_netbsd.h"
#include "emul_unix.h"
#include "emul_chirp.h"
#include "emul_bugapi.h"

static const os_emul *(os_emulations[]) = {
  &emul_chirp,
  &emul_bugapi,
  &emul_netbsd,
  &emul_solaris,
  &emul_linux,
  0
};


INLINE_OS_EMUL\
(os_emul *)
os_emul_create(const char *file_name,
	       device *root)
{
  const char *emulation_name = NULL;
  bfd *image;
  os_emul *chosen_emulation = NULL;

  bfd_init(); /* would never hurt */

  /* open the file */
  image = bfd_openr(file_name, NULL);
  if (image == NULL) {
    bfd_perror(file_name);
    error("nothing loaded\n");
  }

  /* check it is an executable */
  if (!bfd_check_format(image, bfd_object)) {
    TRACE(trace_tbd,
	  ("FIXME - should check more than just bfd_check_format\n"));
    TRACE(trace_os_emul,
	  ("%s not an executable, assumeing a device file\n", file_name));
    bfd_close(image);
    image = NULL;
  }

  /* if a device file, load that before trying the emulations on */
  if (image == NULL) {
    psim_merge_device_file(root, file_name);
  }

  /* see if the device tree already specifies the required emulation */
  if (tree_find_property(root, "/openprom/options/os-emul") != NULL)
    emulation_name =
      tree_find_string_property(root, "/openprom/options/os-emul");
  else
    emulation_name = NULL;

  /* go through each emulation to see if they reconize it. FIXME -
     should have some sort of imported table from a separate file */
  {
    os_emul_data *emul_data;
    const os_emul **possible_emulation;
    chosen_emulation = NULL;
    for (possible_emulation = os_emulations, emul_data = NULL;
	 *possible_emulation != NULL && emul_data == NULL;
	 possible_emulation++) {
      emul_data = (*possible_emulation)->create(root,
						image,
						emulation_name);
      if (emul_data != NULL) {
	chosen_emulation = ZALLOC(os_emul);
	*chosen_emulation = **possible_emulation;
	chosen_emulation->data = emul_data;
      }
    }
  }

  /* clean up */
  if (image != NULL)
    bfd_close(image);
  return chosen_emulation;
}

INLINE_OS_EMUL\
(void)
os_emul_init(os_emul *emulation,
	     int nr_cpus)
{
  if (emulation != (os_emul*)0)
    emulation->init(emulation->data, nr_cpus);
}

INLINE_OS_EMUL\
(void)
os_emul_system_call(cpu *processor,
		    unsigned_word cia)
{
  os_emul *emulation = cpu_os_emulation(processor);
  if (emulation != (os_emul*)0 && emulation->system_call != 0)
    emulation->system_call(processor, cia, emulation->data);
  else
    error("System call emulation not available\n");
}

INLINE_OS_EMUL\
(int)
os_emul_instruction_call(cpu *processor,
			 unsigned_word cia,
			 unsigned_word ra)
{
  os_emul *emulation = cpu_os_emulation(processor);
  if (emulation != (os_emul*)0 && emulation->instruction_call != 0)
    return emulation->instruction_call(processor, cia, ra, emulation->data);
  else
    return 0;
}


#endif /* _OS_EMUL_C_ */
