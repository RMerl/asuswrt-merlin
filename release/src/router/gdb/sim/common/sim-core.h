/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

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


#ifndef SIM_CORE_H
#define SIM_CORE_H


/* core signals (error conditions)
   Define SIM_CORE_SIGNAL to catch these signals - see sim-core.c for
   details.  */

typedef enum {
  sim_core_unmapped_signal,
  sim_core_unaligned_signal,
  nr_sim_core_signals,
} sim_core_signals;

/* Type of SIM_CORE_SIGNAL handler.  */
typedef void (SIM_CORE_SIGNAL_FN)
     (SIM_DESC sd, sim_cpu *cpu, sim_cia cia, unsigned map, int nr_bytes,
      address_word addr, transfer_type transfer, sim_core_signals sig);

extern SIM_CORE_SIGNAL_FN sim_core_signal;


/* basic types */

typedef struct _sim_core_mapping sim_core_mapping;
struct _sim_core_mapping {
  /* common */
  int level;
  int space;
  unsigned_word base;
  unsigned_word bound;
  unsigned_word nr_bytes;
  unsigned mask;
  /* memory map */
  void *free_buffer;
  void *buffer;
  /* callback map */
#if (WITH_HW)
  struct hw *device;
#else
  device *device;
#endif
  /* tracing */
  int trace;
  /* growth */
  sim_core_mapping *next;
};

typedef struct _sim_core_map sim_core_map;
struct _sim_core_map {
  sim_core_mapping *first;
};


typedef struct _sim_core_common {
  sim_core_map map[nr_maps];
} sim_core_common;


/* Main core structure */

typedef struct _sim_core sim_core;
struct _sim_core {
  sim_core_common common;
  address_word byte_xor; /* apply xor universally */
};


/* Per CPU distributed component of the core.  At present this is
   mostly a clone of the global core data structure. */

typedef struct _sim_cpu_core {
  sim_core_common common;
  address_word xor[WITH_XOR_ENDIAN + 1]; /* +1 to avoid zero-sized array */
} sim_cpu_core;


/* Install the "core" module.  */

extern SIM_RC sim_core_install (SIM_DESC sd);



/* Create a memory region within the core.

   CPU - when non NULL, specifes the single processor that the memory
   space is to be attached to. (INIMPLEMENTED).

   LEVEL - specifies the ordering of the memory region.  Lower regions
   are searched first.  Within a level, memory regions can not
   overlap.

   MAPMASK - Bitmask specifying the memory maps that the region is to
   be attached to.  Typically the enums sim-basics.h:access_* are used.

   ADDRESS_SPACE - For device regions, a MAP:ADDRESS pair is
   translated into ADDRESS_SPACE:OFFSET before being passed to the
   client device.

   MODULO - when the simulator has been configured WITH_MODULO support
   and is greater than zero, specifies that accesses to the region
   [ADDR .. ADDR+NR_BYTES) should be mapped onto the sub region [ADDR
   .. ADDR+MODULO).  The modulo value must be a power of two.

   DEVICE - When non NULL, indicates that this is a callback memory
   space and specified device's memory callback handler should be
   called.

   OPTIONAL_BUFFER - when non NULL, specifies the buffer to use for
   data read & written to the region.  Normally a more efficient
   internal structure is used.  It is assumed that buffer is allocated
   such that the byte alignmed of OPTIONAL_BUFFER matches ADDR vis
   (OPTIONAL_BUFFER % 8) == (ADDR % 8)).  It is defined to be a sub-optimal
   hook that allows clients to do nasty things that the interface doesn't
   accomodate. */

extern void sim_core_attach
(SIM_DESC sd,
 sim_cpu *cpu,
 int level,
 unsigned mapmask,
 int address_space,
 address_word addr,
 address_word nr_bytes,
 unsigned modulo,
#if (WITH_HW)
 struct hw *client,
#else
 device *client,
#endif
 void *optional_buffer);


/* Delete a memory section within the core.

 */

extern void sim_core_detach
(SIM_DESC sd,
 sim_cpu *cpu,
 int level,
 int address_space,
 address_word addr);


/* Variable sized read/write

   Transfer a variable sized block of raw data between the host and
   target.  Should any problems occur, the number of bytes
   successfully transfered is returned.

   No host/target byte endian conversion is performed.  No xor-endian
   conversion is performed.

   If CPU argument, when non NULL, specifies the processor specific
   address map that is to be used in the transfer. */


extern unsigned sim_core_read_buffer
(SIM_DESC sd,
 sim_cpu *cpu,
 unsigned map,
 void *buffer,
 address_word addr,
 unsigned nr_bytes);

extern unsigned sim_core_write_buffer
(SIM_DESC sd,
 sim_cpu *cpu,
 unsigned map,
 const void *buffer,
 address_word addr,
 unsigned nr_bytes);



/* Configure the core's XOR endian transfer mode.  Only applicable
   when WITH_XOR_ENDIAN is enabled.

   Targets suporting XOR endian, shall notify the core of any changes
   in state via this call.

   The CPU argument, when non NULL, specifes the single processor that
   the xor-endian configuration is to be applied to. */

extern void sim_core_set_xor
(SIM_DESC sd,
 sim_cpu *cpu,
 int is_xor);


/* XOR version of variable sized read/write.

   Transfer a variable sized block of raw data between the host and
   target.  Should any problems occur, the number of bytes
   successfully transfered is returned.

   No host/target byte endian conversion is performed.  If applicable
   (WITH_XOR_ENDIAN and xor-endian set), xor-endian conversion *is*
   performed.

   If CPU argument, when non NULL, specifies the processor specific
   address map that is to be used in the transfer. */

extern unsigned sim_core_xor_read_buffer
(SIM_DESC sd,
 sim_cpu *cpu,
 unsigned map,
 void *buffer,
 address_word addr,
 unsigned nr_bytes);

extern unsigned sim_core_xor_write_buffer
(SIM_DESC sd,
 sim_cpu *cpu,
 unsigned map,
 const void *buffer,
 address_word addr,
 unsigned nr_bytes);



/* Fixed sized, processor oriented, read/write.

   Transfer a fixed amout of memory between the host and target.  The
   data transfered is translated from/to host to/from target byte
   order (including xor endian).  Should the transfer fail, the
   operation shall abort (no return).

   ALIGNED assumes yhat the specified ADDRESS is correctly alligned
   for an N byte transfer (no alignment checks are made).  Passing an
   incorrectly aligned ADDRESS is erroneous.

   UNALIGNED checks/modifies the ADDRESS according to the requirements
   of an N byte transfer. Action, as defined by WITH_ALIGNMENT, being
   taken should the check fail.

   MISSALIGNED transfers the data regardless.

   Misaligned xor-endian accesses are broken into a sequence of
   transfers each <= WITH_XOR_ENDIAN bytes */


#define DECLARE_SIM_CORE_WRITE_N(ALIGNMENT,N,M) \
INLINE_SIM_CORE\
(void) sim_core_write_##ALIGNMENT##_##N \
(sim_cpu *cpu, \
 sim_cia cia, \
 unsigned map, \
 address_word addr, \
 unsigned_##M val);

DECLARE_SIM_CORE_WRITE_N(aligned,1,1)
DECLARE_SIM_CORE_WRITE_N(aligned,2,2)
DECLARE_SIM_CORE_WRITE_N(aligned,4,4)
DECLARE_SIM_CORE_WRITE_N(aligned,8,8)
DECLARE_SIM_CORE_WRITE_N(aligned,16,16)

#define sim_core_write_unaligned_1 sim_core_write_aligned_1 
DECLARE_SIM_CORE_WRITE_N(unaligned,2,2)
DECLARE_SIM_CORE_WRITE_N(unaligned,4,4)
DECLARE_SIM_CORE_WRITE_N(unaligned,8,8)
DECLARE_SIM_CORE_WRITE_N(unaligned,16,16)

DECLARE_SIM_CORE_WRITE_N(misaligned,3,4)
DECLARE_SIM_CORE_WRITE_N(misaligned,5,8)
DECLARE_SIM_CORE_WRITE_N(misaligned,6,8)
DECLARE_SIM_CORE_WRITE_N(misaligned,7,8)

#define sim_core_write_1 sim_core_write_aligned_1
#define sim_core_write_2 sim_core_write_aligned_2
#define sim_core_write_4 sim_core_write_aligned_4
#define sim_core_write_8 sim_core_write_aligned_8
#define sim_core_write_16 sim_core_write_aligned_16

#define sim_core_write_unaligned_word XCONCAT2(sim_core_write_unaligned_,WITH_TARGET_WORD_BITSIZE)
#define sim_core_write_aligned_word XCONCAT2(sim_core_write_aligned_,WITH_TARGET_WORD_BITSIZE)
#define sim_core_write_word XCONCAT2(sim_core_write_,WITH_TARGET_WORD_BITSIZE)

#undef DECLARE_SIM_CORE_WRITE_N


#define DECLARE_SIM_CORE_READ_N(ALIGNMENT,N,M) \
INLINE_SIM_CORE\
(unsigned_##M) sim_core_read_##ALIGNMENT##_##N \
(sim_cpu *cpu, \
 sim_cia cia, \
 unsigned map, \
 address_word addr);

DECLARE_SIM_CORE_READ_N(aligned,1,1)
DECLARE_SIM_CORE_READ_N(aligned,2,2)
DECLARE_SIM_CORE_READ_N(aligned,4,4)
DECLARE_SIM_CORE_READ_N(aligned,8,8)
DECLARE_SIM_CORE_READ_N(aligned,16,16)

#define sim_core_read_unaligned_1 sim_core_read_aligned_1
DECLARE_SIM_CORE_READ_N(unaligned,2,2)
DECLARE_SIM_CORE_READ_N(unaligned,4,4)
DECLARE_SIM_CORE_READ_N(unaligned,8,8)
DECLARE_SIM_CORE_READ_N(unaligned,16,16)

DECLARE_SIM_CORE_READ_N(misaligned,3,4)
DECLARE_SIM_CORE_READ_N(misaligned,5,8)
DECLARE_SIM_CORE_READ_N(misaligned,6,8)
DECLARE_SIM_CORE_READ_N(misaligned,7,8)


#define sim_core_read_1 sim_core_read_aligned_1
#define sim_core_read_2 sim_core_read_aligned_2
#define sim_core_read_4 sim_core_read_aligned_4
#define sim_core_read_8 sim_core_read_aligned_8
#define sim_core_read_16 sim_core_read_aligned_16

#define sim_core_read_unaligned_word XCONCAT2(sim_core_read_unaligned_,WITH_TARGET_WORD_BITSIZE)
#define sim_core_read_aligned_word XCONCAT2(sim_core_read_aligned_,WITH_TARGET_WORD_BITSIZE)
#define sim_core_read_word XCONCAT2(sim_core_read_,WITH_TARGET_WORD_BITSIZE)

#undef DECLARE_SIM_CORE_READ_N


#if (WITH_DEVICES)
/* TODO: create sim/common/device.h */
/* These are defined with each particular cpu.  */
void device_error (device *me, char* message, ...);
int device_io_read_buffer(device *me, void *dest, int space, address_word addr, unsigned nr_bytes, SIM_DESC sd, sim_cpu *processor, sim_cia cia);
int device_io_write_buffer(device *me, const void *source, int space, address_word addr, unsigned nr_bytes, SIM_DESC sd, sim_cpu *processor, sim_cia cia);
#endif


#endif
