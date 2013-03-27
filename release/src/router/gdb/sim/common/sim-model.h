/* Architecture, machine, and model support.
   Copyright (C) 1997, 1998, 1999, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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

/* Nomenclature:
   architecture = one of sparc, mips, sh, etc.
   in the sparc architecture, mach = one of v6, v7, v8, sparclite, etc.
   in the v8 mach, model = one of supersparc, etc.
*/

/* This file is intended to be included by sim-basics.h.  */

#ifndef SIM_MODEL_H
#define SIM_MODEL_H

/* Function unit and instruction timing support.
   ??? This is obviously insufficiently general.
   It's useful but it needs elaborating upon.  */

typedef struct {
  unsigned char name; /* actually a UNIT_TYPE enum */
  unsigned char issue;
  unsigned char done;
} UNIT;

#ifndef MAX_UNITS
#define MAX_UNITS 1
#endif

typedef int (MODEL_FN) (sim_cpu *, void *);

typedef struct {
  /* This is an integer that identifies this insn.
     How this works is up to the target.  */
  int num;

  /* Function to handle insn-specific profiling.  */
  MODEL_FN *model_fn;

  /* Array of function units used by this insn.  */
  UNIT units[MAX_UNITS];
} INSN_TIMING;

/* Struct to describe various implementation properties of a cpu.
   When multiple cpu variants are supported, the sizes of some structs
   can vary.  */

typedef struct {
  /* The size of the SIM_CPU struct.  */
  int sim_cpu_size;
#define IMP_PROPS_SIM_CPU_SIZE(cpu_props) ((cpu_props)->sim_cpu_size)
  /* An SCACHE element can vary in size, depending on the selected cpu.
     This is zero if the SCACHE isn't in use for this variant.  */
  int scache_elm_size;
#define IMP_PROPS_SCACHE_ELM_SIZE(cpu_props) ((cpu_props)->scache_elm_size)
} MACH_IMP_PROPERTIES;

/* A machine variant.  */

typedef struct {
  const char *name;
#define MACH_NAME(m) ((m)->name)
  /* This is the argument to bfd_scan_arch.  */
  const char *bfd_name;
#define MACH_BFD_NAME(m) ((m)->bfd_name)
  enum mach_attr num;
#define MACH_NUM(m) ((m)->num)

  int word_bitsize;
#define MACH_WORD_BITSIZE(m) ((m)->word_bitsize)
  int addr_bitsize;
#define MACH_ADDR_BITSIZE(m) ((m)->addr_bitsize)

  /* Pointer to null-entry terminated table of models of this mach.
     The default is the first one.  */
  const struct model *models;
#define MACH_MODELS(m) ((m)->models)

  /* Pointer to the implementation properties of this mach.  */
  const MACH_IMP_PROPERTIES *imp_props;
#define MACH_IMP_PROPS(m) ((m)->imp_props)

  /* Called by sim_model_set when the model of a cpu is set.  */
  void (* init_cpu) (sim_cpu *);
#define MACH_INIT_CPU(m) ((m)->init_cpu)

  /* Initialize the simulator engine for this cpu.
     Used by cgen simulators to initialize the insn descriptor table.  */
  void (* prepare_run) (sim_cpu *);
#define MACH_PREPARE_RUN(m) ((m)->prepare_run)
} MACH;

/* A model (implementation) of a machine.  */

typedef struct model {
  const char *name;
#define MODEL_NAME(m) ((m)->name)
  const MACH *mach;
#define MODEL_MACH(m) ((m)->mach)
  /* An enum that distinguished the model.  */
  int num;
#define MODEL_NUM(m) ((m)->num)
  /* Pointer to timing table for this model.  */
  const INSN_TIMING *timing;
#define MODEL_TIMING(m) ((m)->timing)
  void (* init) (sim_cpu *);
#define MODEL_INIT(m) ((m)->init)
} MODEL;

/* Tables of supported machines.  */
/* ??? In a simulator of multiple architectures, will need multiple copies of
   this.  Have an `archs' array that contains a pointer to the machs array
   for each (which in turn has a pointer to the models array for each).  */
extern const MACH *sim_machs[];

/* Model module handlers.  */
extern MODULE_INSTALL_FN sim_model_install;

/* Support routines.  */
extern void sim_model_set (SIM_DESC sd_, sim_cpu *cpu_, const MODEL *model_);
extern const MODEL * sim_model_lookup (const char *name_);
extern const MACH * sim_mach_lookup (const char *name_);
extern const MACH * sim_mach_lookup_bfd_name (const char *bfd_name_);

#endif /* SIM_MODEL_H */
