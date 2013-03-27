/* Architecture-dependent code for the Fujitsu FR-V, for GDB, the GNU Debugger.
   Copyright (C) 2004, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Enumerate the possible ABIs for FR-V.  */
enum frv_abi
  {
    FRV_ABI_EABI,
    FRV_ABI_FDPIC
  };

/* Register numbers.  The order in which these appear define the
   remote protocol, so take care in changing them.  */
enum {
  /* Register numbers 0 -- 63 are always reserved for general-purpose
     registers.  The chip at hand may have less.  */
  first_gpr_regnum = 0,
  sp_regnum = 1,
  fp_regnum = 2,
  struct_return_regnum = 3,
  last_gpr_regnum = 63,

  /* Register numbers 64 -- 127 are always reserved for floating-point
     registers.  The chip at hand may have less.  */
  first_fpr_regnum = 64,
  last_fpr_regnum = 127,

  /* The PC register.  */
  pc_regnum = 128,

  /* Register numbers 129 on up are always reserved for special-purpose
     registers.  */
  first_spr_regnum = 129,
  psr_regnum = 129,
  ccr_regnum = 130,
  cccr_regnum = 131,
  fdpic_loadmap_exec_regnum = 132,
  fdpic_loadmap_interp_regnum = 133,
  tbr_regnum = 135,
  brr_regnum = 136,
  dbar0_regnum = 137,
  dbar1_regnum = 138,
  dbar2_regnum = 139,
  dbar3_regnum = 140,
  scr0_regnum = 141,
  scr1_regnum = 142,
  scr2_regnum = 143,
  scr3_regnum = 144,
  lr_regnum = 145,
  lcr_regnum = 146,
  iacc0h_regnum = 147,
  iacc0l_regnum = 148,
  fsr0_regnum = 149,
  acc0_regnum = 150,
  acc7_regnum = 157,
  accg0123_regnum = 158,
  accg4567_regnum = 159,
  msr0_regnum = 160,
  msr1_regnum = 161,
  gner0_regnum = 162,
  gner1_regnum = 163,
  fner0_regnum = 164,
  fner1_regnum = 165,
  last_spr_regnum = 165,

  /* The total number of registers we know exist.  */
  frv_num_regs = last_spr_regnum + 1,

  /* Pseudo registers */
  first_pseudo_regnum = frv_num_regs,

  /* iacc0 - the 64-bit concatenation of iacc0h and iacc0l.  */
  iacc0_regnum = first_pseudo_regnum + 0,
  accg0_regnum = first_pseudo_regnum + 1,
  accg7_regnum = accg0_regnum + 7,

  last_pseudo_regnum = accg7_regnum,
  frv_num_pseudo_regs = last_pseudo_regnum - first_pseudo_regnum + 1,
};

/* Return the FR-V ABI associated with GDBARCH.  */
enum frv_abi frv_abi (struct gdbarch *gdbarch);

/* Fetch the interpreter and executable loadmap addresses (for shared
   library support) for the FDPIC ABI.  Return 0 if successful, -1 if
   not.  (E.g, -1 will be returned if the ABI isn't the FDPIC ABI.)  */
int frv_fdpic_loadmap_addresses (struct gdbarch *gdbarch,
                                 CORE_ADDR *interp_addr, CORE_ADDR *exec_addr);

/* Given a function entry point, find and return the GOT address for the
   containing load module.  */
CORE_ADDR frv_fdpic_find_global_pointer (CORE_ADDR addr);

/* Given a function entry point, find and return the canonical descriptor
   for that function, if one exists.  If no canonical descriptor could
   be found, return 0.  */
CORE_ADDR frv_fdpic_find_canonical_descriptor (CORE_ADDR entry_point);


/* Given an objfile, return the address of its link map.  This value is
   needed for TLS support.  */
CORE_ADDR frv_fetch_objfile_link_map (struct objfile *objfile);
