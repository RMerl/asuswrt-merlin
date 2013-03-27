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


#ifndef _EMUL_GENERIC_H_
#define _EMUL_GENERIC_H_

#include "cpu.h"
#include "idecode.h"
#include "os_emul.h"

#include "tree.h"

#include "bfd.h"

#ifndef INLINE_EMUL_GENERIC
#define INLINE_EMUL_GENERIC
#endif

/* various PowerPC instructions for writing into memory */
enum {
  emul_call_instruction = 0x1,
  emul_loop_instruction = 0x48000000, /* branch to . */
  emul_rfi_instruction = 0x4c000064,
  emul_blr_instruction = 0x4e800020,
};


/* emulation specific data */

typedef struct _os_emul_data os_emul_data;

typedef os_emul_data *(os_emul_create_handler)
     (device *tree,
      bfd *image,
      const char *emul_name);
typedef void (os_emul_init_handler)
     (os_emul_data *emul_data,
      int nr_cpus);
typedef void (os_emul_system_call_handler)
     (cpu *processor,
      unsigned_word cia,
      os_emul_data *emul_data);
typedef int (os_emul_instruction_call_handler)
     (cpu *processor,
      unsigned_word cia,
      unsigned_word ra,
      os_emul_data *emul_data);

struct _os_emul {
  const char *name;
  os_emul_create_handler *create;
  os_emul_init_handler *init;
  os_emul_system_call_handler *system_call;
  os_emul_instruction_call_handler *instruction_call;
  os_emul_data *data;
};


/* One class of emulation - system call is pretty general, provide a
   common template for implementing this */

typedef struct _emul_syscall emul_syscall;
typedef struct _emul_syscall_descriptor emul_syscall_descriptor;

typedef void (emul_syscall_handler)
     (os_emul_data *emul_data,
      unsigned call,
      const int arg0,
      cpu *processor,
      unsigned_word cia);

struct _emul_syscall_descriptor {
  emul_syscall_handler *handler;
  const char *name;
};

struct _emul_syscall {
  emul_syscall_descriptor *syscall_descriptor;
  int nr_system_calls;
  char **error_names;
  int nr_error_names;
  char **signal_names;
  int nr_signal_names;
};


INLINE_EMUL_GENERIC void emul_do_system_call
(os_emul_data *emul_data,
 emul_syscall *syscall,
 unsigned call,
 const int arg0,
 cpu *processor,
 unsigned_word cia);


INLINE_EMUL_GENERIC unsigned64 emul_read_gpr64
(cpu *processor,
 int g);

INLINE_EMUL_GENERIC void emul_write_gpr64
(cpu *processor,
 int g,
 unsigned64 val);

INLINE_EMUL_GENERIC void emul_write_status
(cpu *processor,
 int status,
 int errno);

INLINE_EMUL_GENERIC void emul_write2_status
(cpu *processor,
 int status1,
 int status2,
 int errno);

INLINE_EMUL_GENERIC char *emul_read_string
(char *dest,
 unsigned_word addr,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);

INLINE_EMUL_GENERIC unsigned_word emul_read_word
(unsigned_word addr,
 cpu *processor,
 unsigned_word cia);
 
INLINE_EMUL_GENERIC void emul_write_word
(unsigned_word addr,
 unsigned_word buf,
 cpu *processor,
 unsigned_word cia);
 
INLINE_EMUL_GENERIC void emul_read_buffer
(void *dest,
 unsigned_word addr,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);

INLINE_EMUL_GENERIC void emul_write_buffer
(const void *source,
 unsigned_word addr,
 unsigned nr_bytes,
 cpu *processor,
 unsigned_word cia);

/* Simplify the construction of device trees */

INLINE_EMUL_GENERIC void emul_add_tree_options
(device *tree,
 bfd *image,
 const char *emul,
 const char *env,
 int oea_interrupt_prefix);

INLINE_EMUL_GENERIC void emul_add_tree_hardware
(device *tree);

#endif /* _EMUL_GENERIC_H_ */
