/* *INDENT-OFF* */ /* THIS FILE IS GENERATED */

/* Dynamic architecture support for GDB, the GNU debugger.

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

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

/* This file was created with the aid of ``gdbarch.sh''.

   The Bourne shell script ``gdbarch.sh'' creates the files
   ``new-gdbarch.c'' and ``new-gdbarch.h and then compares them
   against the existing ``gdbarch.[hc]''.  Any differences found
   being reported.

   If editing this file, please also run gdbarch.sh and merge any
   changes into that script. Conversely, when making sweeping changes
   to this file, modifying gdbarch.sh and using its output may prove
   easier. */

#ifndef GDBARCH_H
#define GDBARCH_H

struct floatformat;
struct ui_file;
struct frame_info;
struct value;
struct objfile;
struct obj_section;
struct minimal_symbol;
struct regcache;
struct reggroup;
struct regset;
struct disassemble_info;
struct target_ops;
struct obstack;
struct bp_target_info;
struct target_desc;

extern struct gdbarch *current_gdbarch;


/* The following are pre-initialized by GDBARCH. */

extern const struct bfd_arch_info * gdbarch_bfd_arch_info (struct gdbarch *gdbarch);
/* set_gdbarch_bfd_arch_info() - not applicable - pre-initialized. */

extern int gdbarch_byte_order (struct gdbarch *gdbarch);
/* set_gdbarch_byte_order() - not applicable - pre-initialized. */

extern enum gdb_osabi gdbarch_osabi (struct gdbarch *gdbarch);
/* set_gdbarch_osabi() - not applicable - pre-initialized. */

extern const struct target_desc * gdbarch_target_desc (struct gdbarch *gdbarch);
/* set_gdbarch_target_desc() - not applicable - pre-initialized. */


/* The following are initialized by the target dependent code. */

/* Number of bits in a char or unsigned char for the target machine.
   Just like CHAR_BIT in <limits.h> but describes the target machine.
   v:TARGET_CHAR_BIT:int:char_bit::::8 * sizeof (char):8::0:
  
   Number of bits in a short or unsigned short for the target machine. */

extern int gdbarch_short_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_short_bit (struct gdbarch *gdbarch, int short_bit);

/* Number of bits in an int or unsigned int for the target machine. */

extern int gdbarch_int_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_int_bit (struct gdbarch *gdbarch, int int_bit);

/* Number of bits in a long or unsigned long for the target machine. */

extern int gdbarch_long_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_long_bit (struct gdbarch *gdbarch, int long_bit);

/* Number of bits in a long long or unsigned long long for the target
   machine. */

extern int gdbarch_long_long_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_long_long_bit (struct gdbarch *gdbarch, int long_long_bit);

/* The ABI default bit-size and format for "float", "double", and "long
   double".  These bit/format pairs should eventually be combined into
   a single object.  For the moment, just initialize them as a pair.
   Each format describes both the big and little endian layouts (if
   useful). */

extern int gdbarch_float_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_float_bit (struct gdbarch *gdbarch, int float_bit);

extern const struct floatformat ** gdbarch_float_format (struct gdbarch *gdbarch);
extern void set_gdbarch_float_format (struct gdbarch *gdbarch, const struct floatformat ** float_format);

extern int gdbarch_double_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_double_bit (struct gdbarch *gdbarch, int double_bit);

extern const struct floatformat ** gdbarch_double_format (struct gdbarch *gdbarch);
extern void set_gdbarch_double_format (struct gdbarch *gdbarch, const struct floatformat ** double_format);

extern int gdbarch_long_double_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_long_double_bit (struct gdbarch *gdbarch, int long_double_bit);

extern const struct floatformat ** gdbarch_long_double_format (struct gdbarch *gdbarch);
extern void set_gdbarch_long_double_format (struct gdbarch *gdbarch, const struct floatformat ** long_double_format);

/* For most targets, a pointer on the target and its representation as an
   address in GDB have the same size and "look the same".  For such a
   target, you need only set gdbarch_ptr_bit and gdbarch_addr_bit
   / addr_bit will be set from it.
  
   If gdbarch_ptr_bit and gdbarch_addr_bit are different, you'll probably
   also need to set gdbarch_pointer_to_address and gdbarch_address_to_pointer
   as well.
  
   ptr_bit is the size of a pointer on the target */

extern int gdbarch_ptr_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_ptr_bit (struct gdbarch *gdbarch, int ptr_bit);

/* addr_bit is the size of a target address as represented in gdb */

extern int gdbarch_addr_bit (struct gdbarch *gdbarch);
extern void set_gdbarch_addr_bit (struct gdbarch *gdbarch, int addr_bit);

/* One if `char' acts like `signed char', zero if `unsigned char'. */

extern int gdbarch_char_signed (struct gdbarch *gdbarch);
extern void set_gdbarch_char_signed (struct gdbarch *gdbarch, int char_signed);

extern int gdbarch_read_pc_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_read_pc_ftype) (struct regcache *regcache);
extern CORE_ADDR gdbarch_read_pc (struct gdbarch *gdbarch, struct regcache *regcache);
extern void set_gdbarch_read_pc (struct gdbarch *gdbarch, gdbarch_read_pc_ftype *read_pc);

extern int gdbarch_write_pc_p (struct gdbarch *gdbarch);

typedef void (gdbarch_write_pc_ftype) (struct regcache *regcache, CORE_ADDR val);
extern void gdbarch_write_pc (struct gdbarch *gdbarch, struct regcache *regcache, CORE_ADDR val);
extern void set_gdbarch_write_pc (struct gdbarch *gdbarch, gdbarch_write_pc_ftype *write_pc);

/* Function for getting target's idea of a frame pointer.  FIXME: GDB's
   whole scheme for dealing with "frames" and "frame pointers" needs a
   serious shakedown. */

typedef void (gdbarch_virtual_frame_pointer_ftype) (CORE_ADDR pc, int *frame_regnum, LONGEST *frame_offset);
extern void gdbarch_virtual_frame_pointer (struct gdbarch *gdbarch, CORE_ADDR pc, int *frame_regnum, LONGEST *frame_offset);
extern void set_gdbarch_virtual_frame_pointer (struct gdbarch *gdbarch, gdbarch_virtual_frame_pointer_ftype *virtual_frame_pointer);

extern int gdbarch_pseudo_register_read_p (struct gdbarch *gdbarch);

typedef void (gdbarch_pseudo_register_read_ftype) (struct gdbarch *gdbarch, struct regcache *regcache, int cookednum, gdb_byte *buf);
extern void gdbarch_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache, int cookednum, gdb_byte *buf);
extern void set_gdbarch_pseudo_register_read (struct gdbarch *gdbarch, gdbarch_pseudo_register_read_ftype *pseudo_register_read);

extern int gdbarch_pseudo_register_write_p (struct gdbarch *gdbarch);

typedef void (gdbarch_pseudo_register_write_ftype) (struct gdbarch *gdbarch, struct regcache *regcache, int cookednum, const gdb_byte *buf);
extern void gdbarch_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache, int cookednum, const gdb_byte *buf);
extern void set_gdbarch_pseudo_register_write (struct gdbarch *gdbarch, gdbarch_pseudo_register_write_ftype *pseudo_register_write);

extern int gdbarch_num_regs (struct gdbarch *gdbarch);
extern void set_gdbarch_num_regs (struct gdbarch *gdbarch, int num_regs);

/* This macro gives the number of pseudo-registers that live in the
   register namespace but do not get fetched or stored on the target.
   These pseudo-registers may be aliases for other registers,
   combinations of other registers, or they may be computed by GDB. */

extern int gdbarch_num_pseudo_regs (struct gdbarch *gdbarch);
extern void set_gdbarch_num_pseudo_regs (struct gdbarch *gdbarch, int num_pseudo_regs);

/* GDB's standard (or well known) register numbers.  These can map onto
   a real register or a pseudo (computed) register or not be defined at
   all (-1).
   gdbarch_sp_regnum will hopefully be replaced by UNWIND_SP. */

extern int gdbarch_sp_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_sp_regnum (struct gdbarch *gdbarch, int sp_regnum);

extern int gdbarch_pc_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_pc_regnum (struct gdbarch *gdbarch, int pc_regnum);

extern int gdbarch_ps_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_ps_regnum (struct gdbarch *gdbarch, int ps_regnum);

extern int gdbarch_fp0_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_fp0_regnum (struct gdbarch *gdbarch, int fp0_regnum);

/* Convert stab register number (from `r' declaration) to a gdb REGNUM. */

typedef int (gdbarch_stab_reg_to_regnum_ftype) (int stab_regnr);
extern int gdbarch_stab_reg_to_regnum (struct gdbarch *gdbarch, int stab_regnr);
extern void set_gdbarch_stab_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_stab_reg_to_regnum_ftype *stab_reg_to_regnum);

/* Provide a default mapping from a ecoff register number to a gdb REGNUM. */

typedef int (gdbarch_ecoff_reg_to_regnum_ftype) (int ecoff_regnr);
extern int gdbarch_ecoff_reg_to_regnum (struct gdbarch *gdbarch, int ecoff_regnr);
extern void set_gdbarch_ecoff_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_ecoff_reg_to_regnum_ftype *ecoff_reg_to_regnum);

/* Provide a default mapping from a DWARF register number to a gdb REGNUM. */

typedef int (gdbarch_dwarf_reg_to_regnum_ftype) (int dwarf_regnr);
extern int gdbarch_dwarf_reg_to_regnum (struct gdbarch *gdbarch, int dwarf_regnr);
extern void set_gdbarch_dwarf_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_dwarf_reg_to_regnum_ftype *dwarf_reg_to_regnum);

/* Convert from an sdb register number to an internal gdb register number. */

typedef int (gdbarch_sdb_reg_to_regnum_ftype) (int sdb_regnr);
extern int gdbarch_sdb_reg_to_regnum (struct gdbarch *gdbarch, int sdb_regnr);
extern void set_gdbarch_sdb_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_sdb_reg_to_regnum_ftype *sdb_reg_to_regnum);

typedef int (gdbarch_dwarf2_reg_to_regnum_ftype) (int dwarf2_regnr);
extern int gdbarch_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, int dwarf2_regnr);
extern void set_gdbarch_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, gdbarch_dwarf2_reg_to_regnum_ftype *dwarf2_reg_to_regnum);

typedef const char * (gdbarch_register_name_ftype) (int regnr);
extern const char * gdbarch_register_name (struct gdbarch *gdbarch, int regnr);
extern void set_gdbarch_register_name (struct gdbarch *gdbarch, gdbarch_register_name_ftype *register_name);

/* Return the type of a register specified by the architecture.  Only
   the register cache should call this function directly; others should
   use "register_type". */

extern int gdbarch_register_type_p (struct gdbarch *gdbarch);

typedef struct type * (gdbarch_register_type_ftype) (struct gdbarch *gdbarch, int reg_nr);
extern struct type * gdbarch_register_type (struct gdbarch *gdbarch, int reg_nr);
extern void set_gdbarch_register_type (struct gdbarch *gdbarch, gdbarch_register_type_ftype *register_type);

/* See gdbint.texinfo, and PUSH_DUMMY_CALL. */

extern int gdbarch_unwind_dummy_id_p (struct gdbarch *gdbarch);

typedef struct frame_id (gdbarch_unwind_dummy_id_ftype) (struct gdbarch *gdbarch, struct frame_info *info);
extern struct frame_id gdbarch_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *info);
extern void set_gdbarch_unwind_dummy_id (struct gdbarch *gdbarch, gdbarch_unwind_dummy_id_ftype *unwind_dummy_id);

/* Implement UNWIND_DUMMY_ID and PUSH_DUMMY_CALL, then delete
   deprecated_fp_regnum. */

extern int gdbarch_deprecated_fp_regnum (struct gdbarch *gdbarch);
extern void set_gdbarch_deprecated_fp_regnum (struct gdbarch *gdbarch, int deprecated_fp_regnum);

/* See gdbint.texinfo.  See infcall.c. */

extern int gdbarch_push_dummy_call_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_push_dummy_call_ftype) (struct gdbarch *gdbarch, struct value *function, struct regcache *regcache, CORE_ADDR bp_addr, int nargs, struct value **args, CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr);
extern CORE_ADDR gdbarch_push_dummy_call (struct gdbarch *gdbarch, struct value *function, struct regcache *regcache, CORE_ADDR bp_addr, int nargs, struct value **args, CORE_ADDR sp, int struct_return, CORE_ADDR struct_addr);
extern void set_gdbarch_push_dummy_call (struct gdbarch *gdbarch, gdbarch_push_dummy_call_ftype *push_dummy_call);

extern int gdbarch_call_dummy_location (struct gdbarch *gdbarch);
extern void set_gdbarch_call_dummy_location (struct gdbarch *gdbarch, int call_dummy_location);

extern int gdbarch_push_dummy_code_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_push_dummy_code_ftype) (struct gdbarch *gdbarch, CORE_ADDR sp, CORE_ADDR funaddr, int using_gcc, struct value **args, int nargs, struct type *value_type, CORE_ADDR *real_pc, CORE_ADDR *bp_addr, struct regcache *regcache);
extern CORE_ADDR gdbarch_push_dummy_code (struct gdbarch *gdbarch, CORE_ADDR sp, CORE_ADDR funaddr, int using_gcc, struct value **args, int nargs, struct type *value_type, CORE_ADDR *real_pc, CORE_ADDR *bp_addr, struct regcache *regcache);
extern void set_gdbarch_push_dummy_code (struct gdbarch *gdbarch, gdbarch_push_dummy_code_ftype *push_dummy_code);

typedef void (gdbarch_print_registers_info_ftype) (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, int regnum, int all);
extern void gdbarch_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, int regnum, int all);
extern void set_gdbarch_print_registers_info (struct gdbarch *gdbarch, gdbarch_print_registers_info_ftype *print_registers_info);

extern int gdbarch_print_float_info_p (struct gdbarch *gdbarch);

typedef void (gdbarch_print_float_info_ftype) (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, const char *args);
extern void gdbarch_print_float_info (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, const char *args);
extern void set_gdbarch_print_float_info (struct gdbarch *gdbarch, gdbarch_print_float_info_ftype *print_float_info);

extern int gdbarch_print_vector_info_p (struct gdbarch *gdbarch);

typedef void (gdbarch_print_vector_info_ftype) (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, const char *args);
extern void gdbarch_print_vector_info (struct gdbarch *gdbarch, struct ui_file *file, struct frame_info *frame, const char *args);
extern void set_gdbarch_print_vector_info (struct gdbarch *gdbarch, gdbarch_print_vector_info_ftype *print_vector_info);

/* MAP a GDB RAW register number onto a simulator register number.  See
   also include/...-sim.h. */

typedef int (gdbarch_register_sim_regno_ftype) (int reg_nr);
extern int gdbarch_register_sim_regno (struct gdbarch *gdbarch, int reg_nr);
extern void set_gdbarch_register_sim_regno (struct gdbarch *gdbarch, gdbarch_register_sim_regno_ftype *register_sim_regno);

typedef int (gdbarch_cannot_fetch_register_ftype) (int regnum);
extern int gdbarch_cannot_fetch_register (struct gdbarch *gdbarch, int regnum);
extern void set_gdbarch_cannot_fetch_register (struct gdbarch *gdbarch, gdbarch_cannot_fetch_register_ftype *cannot_fetch_register);

typedef int (gdbarch_cannot_store_register_ftype) (int regnum);
extern int gdbarch_cannot_store_register (struct gdbarch *gdbarch, int regnum);
extern void set_gdbarch_cannot_store_register (struct gdbarch *gdbarch, gdbarch_cannot_store_register_ftype *cannot_store_register);

/* setjmp/longjmp support. */

extern int gdbarch_get_longjmp_target_p (struct gdbarch *gdbarch);

typedef int (gdbarch_get_longjmp_target_ftype) (struct frame_info *frame, CORE_ADDR *pc);
extern int gdbarch_get_longjmp_target (struct gdbarch *gdbarch, struct frame_info *frame, CORE_ADDR *pc);
extern void set_gdbarch_get_longjmp_target (struct gdbarch *gdbarch, gdbarch_get_longjmp_target_ftype *get_longjmp_target);

extern int gdbarch_believe_pcc_promotion (struct gdbarch *gdbarch);
extern void set_gdbarch_believe_pcc_promotion (struct gdbarch *gdbarch, int believe_pcc_promotion);

typedef int (gdbarch_convert_register_p_ftype) (int regnum, struct type *type);
extern int gdbarch_convert_register_p (struct gdbarch *gdbarch, int regnum, struct type *type);
extern void set_gdbarch_convert_register_p (struct gdbarch *gdbarch, gdbarch_convert_register_p_ftype *convert_register_p);

typedef void (gdbarch_register_to_value_ftype) (struct frame_info *frame, int regnum, struct type *type, gdb_byte *buf);
extern void gdbarch_register_to_value (struct gdbarch *gdbarch, struct frame_info *frame, int regnum, struct type *type, gdb_byte *buf);
extern void set_gdbarch_register_to_value (struct gdbarch *gdbarch, gdbarch_register_to_value_ftype *register_to_value);

typedef void (gdbarch_value_to_register_ftype) (struct frame_info *frame, int regnum, struct type *type, const gdb_byte *buf);
extern void gdbarch_value_to_register (struct gdbarch *gdbarch, struct frame_info *frame, int regnum, struct type *type, const gdb_byte *buf);
extern void set_gdbarch_value_to_register (struct gdbarch *gdbarch, gdbarch_value_to_register_ftype *value_to_register);

/* Construct a value representing the contents of register REGNUM in
   frame FRAME, interpreted as type TYPE.  The routine needs to
   allocate and return a struct value with all value attributes
   (but not the value contents) filled in. */

typedef struct value * (gdbarch_value_from_register_ftype) (struct type *type, int regnum, struct frame_info *frame);
extern struct value * gdbarch_value_from_register (struct gdbarch *gdbarch, struct type *type, int regnum, struct frame_info *frame);
extern void set_gdbarch_value_from_register (struct gdbarch *gdbarch, gdbarch_value_from_register_ftype *value_from_register);

typedef CORE_ADDR (gdbarch_pointer_to_address_ftype) (struct type *type, const gdb_byte *buf);
extern CORE_ADDR gdbarch_pointer_to_address (struct gdbarch *gdbarch, struct type *type, const gdb_byte *buf);
extern void set_gdbarch_pointer_to_address (struct gdbarch *gdbarch, gdbarch_pointer_to_address_ftype *pointer_to_address);

typedef void (gdbarch_address_to_pointer_ftype) (struct type *type, gdb_byte *buf, CORE_ADDR addr);
extern void gdbarch_address_to_pointer (struct gdbarch *gdbarch, struct type *type, gdb_byte *buf, CORE_ADDR addr);
extern void set_gdbarch_address_to_pointer (struct gdbarch *gdbarch, gdbarch_address_to_pointer_ftype *address_to_pointer);

extern int gdbarch_integer_to_address_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_integer_to_address_ftype) (struct gdbarch *gdbarch, struct type *type, const gdb_byte *buf);
extern CORE_ADDR gdbarch_integer_to_address (struct gdbarch *gdbarch, struct type *type, const gdb_byte *buf);
extern void set_gdbarch_integer_to_address (struct gdbarch *gdbarch, gdbarch_integer_to_address_ftype *integer_to_address);

/* It has been suggested that this, well actually its predecessor,
   should take the type/value of the function to be called and not the
   return type.  This is left as an exercise for the reader.
   NOTE: cagney/2004-06-13: The function stack.c:return_command uses
   the predicate with default hack to avoid calling store_return_value
   (via legacy_return_value), when a small struct is involved. */

extern int gdbarch_return_value_p (struct gdbarch *gdbarch);

typedef enum return_value_convention (gdbarch_return_value_ftype) (struct gdbarch *gdbarch, struct type *valtype, struct regcache *regcache, gdb_byte *readbuf, const gdb_byte *writebuf);
extern enum return_value_convention gdbarch_return_value (struct gdbarch *gdbarch, struct type *valtype, struct regcache *regcache, gdb_byte *readbuf, const gdb_byte *writebuf);
extern void set_gdbarch_return_value (struct gdbarch *gdbarch, gdbarch_return_value_ftype *return_value);

/* The deprecated methods extract_return_value, store_return_value,
   DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS and
   deprecated_use_struct_convention have all been folded into
   RETURN_VALUE. */

typedef void (gdbarch_extract_return_value_ftype) (struct type *type, struct regcache *regcache, gdb_byte *valbuf);
extern void gdbarch_extract_return_value (struct gdbarch *gdbarch, struct type *type, struct regcache *regcache, gdb_byte *valbuf);
extern void set_gdbarch_extract_return_value (struct gdbarch *gdbarch, gdbarch_extract_return_value_ftype *extract_return_value);

typedef void (gdbarch_store_return_value_ftype) (struct type *type, struct regcache *regcache, const gdb_byte *valbuf);
extern void gdbarch_store_return_value (struct gdbarch *gdbarch, struct type *type, struct regcache *regcache, const gdb_byte *valbuf);
extern void set_gdbarch_store_return_value (struct gdbarch *gdbarch, gdbarch_store_return_value_ftype *store_return_value);

typedef int (gdbarch_deprecated_use_struct_convention_ftype) (int gcc_p, struct type *value_type);
extern int gdbarch_deprecated_use_struct_convention (struct gdbarch *gdbarch, int gcc_p, struct type *value_type);
extern void set_gdbarch_deprecated_use_struct_convention (struct gdbarch *gdbarch, gdbarch_deprecated_use_struct_convention_ftype *deprecated_use_struct_convention);

typedef CORE_ADDR (gdbarch_skip_prologue_ftype) (CORE_ADDR ip);
extern CORE_ADDR gdbarch_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR ip);
extern void set_gdbarch_skip_prologue (struct gdbarch *gdbarch, gdbarch_skip_prologue_ftype *skip_prologue);

typedef int (gdbarch_inner_than_ftype) (CORE_ADDR lhs, CORE_ADDR rhs);
extern int gdbarch_inner_than (struct gdbarch *gdbarch, CORE_ADDR lhs, CORE_ADDR rhs);
extern void set_gdbarch_inner_than (struct gdbarch *gdbarch, gdbarch_inner_than_ftype *inner_than);

typedef const gdb_byte * (gdbarch_breakpoint_from_pc_ftype) (CORE_ADDR *pcptr, int *lenptr);
extern const gdb_byte * gdbarch_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr, int *lenptr);
extern void set_gdbarch_breakpoint_from_pc (struct gdbarch *gdbarch, gdbarch_breakpoint_from_pc_ftype *breakpoint_from_pc);

extern int gdbarch_adjust_breakpoint_address_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_adjust_breakpoint_address_ftype) (struct gdbarch *gdbarch, CORE_ADDR bpaddr);
extern CORE_ADDR gdbarch_adjust_breakpoint_address (struct gdbarch *gdbarch, CORE_ADDR bpaddr);
extern void set_gdbarch_adjust_breakpoint_address (struct gdbarch *gdbarch, gdbarch_adjust_breakpoint_address_ftype *adjust_breakpoint_address);

typedef int (gdbarch_memory_insert_breakpoint_ftype) (struct bp_target_info *bp_tgt);
extern int gdbarch_memory_insert_breakpoint (struct gdbarch *gdbarch, struct bp_target_info *bp_tgt);
extern void set_gdbarch_memory_insert_breakpoint (struct gdbarch *gdbarch, gdbarch_memory_insert_breakpoint_ftype *memory_insert_breakpoint);

typedef int (gdbarch_memory_remove_breakpoint_ftype) (struct bp_target_info *bp_tgt);
extern int gdbarch_memory_remove_breakpoint (struct gdbarch *gdbarch, struct bp_target_info *bp_tgt);
extern void set_gdbarch_memory_remove_breakpoint (struct gdbarch *gdbarch, gdbarch_memory_remove_breakpoint_ftype *memory_remove_breakpoint);

extern CORE_ADDR gdbarch_decr_pc_after_break (struct gdbarch *gdbarch);
extern void set_gdbarch_decr_pc_after_break (struct gdbarch *gdbarch, CORE_ADDR decr_pc_after_break);

/* A function can be addressed by either it's "pointer" (possibly a
   descriptor address) or "entry point" (first executable instruction).
   The method "convert_from_func_ptr_addr" converting the former to the
   latter.  gdbarch_deprecated_function_start_offset is being used to implement
   a simplified subset of that functionality - the function's address
   corresponds to the "function pointer" and the function's start
   corresponds to the "function entry point" - and hence is redundant. */

extern CORE_ADDR gdbarch_deprecated_function_start_offset (struct gdbarch *gdbarch);
extern void set_gdbarch_deprecated_function_start_offset (struct gdbarch *gdbarch, CORE_ADDR deprecated_function_start_offset);

/* Return the remote protocol register number associated with this
   register.  Normally the identity mapping. */

typedef int (gdbarch_remote_register_number_ftype) (struct gdbarch *gdbarch, int regno);
extern int gdbarch_remote_register_number (struct gdbarch *gdbarch, int regno);
extern void set_gdbarch_remote_register_number (struct gdbarch *gdbarch, gdbarch_remote_register_number_ftype *remote_register_number);

/* Fetch the target specific address used to represent a load module. */

extern int gdbarch_fetch_tls_load_module_address_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_fetch_tls_load_module_address_ftype) (struct objfile *objfile);
extern CORE_ADDR gdbarch_fetch_tls_load_module_address (struct gdbarch *gdbarch, struct objfile *objfile);
extern void set_gdbarch_fetch_tls_load_module_address (struct gdbarch *gdbarch, gdbarch_fetch_tls_load_module_address_ftype *fetch_tls_load_module_address);

extern CORE_ADDR gdbarch_frame_args_skip (struct gdbarch *gdbarch);
extern void set_gdbarch_frame_args_skip (struct gdbarch *gdbarch, CORE_ADDR frame_args_skip);

extern int gdbarch_unwind_pc_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_unwind_pc_ftype) (struct gdbarch *gdbarch, struct frame_info *next_frame);
extern CORE_ADDR gdbarch_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame);
extern void set_gdbarch_unwind_pc (struct gdbarch *gdbarch, gdbarch_unwind_pc_ftype *unwind_pc);

extern int gdbarch_unwind_sp_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_unwind_sp_ftype) (struct gdbarch *gdbarch, struct frame_info *next_frame);
extern CORE_ADDR gdbarch_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame);
extern void set_gdbarch_unwind_sp (struct gdbarch *gdbarch, gdbarch_unwind_sp_ftype *unwind_sp);

/* DEPRECATED_FRAME_LOCALS_ADDRESS as been replaced by the per-frame
   frame-base.  Enable frame-base before frame-unwind. */

extern int gdbarch_frame_num_args_p (struct gdbarch *gdbarch);

typedef int (gdbarch_frame_num_args_ftype) (struct frame_info *frame);
extern int gdbarch_frame_num_args (struct gdbarch *gdbarch, struct frame_info *frame);
extern void set_gdbarch_frame_num_args (struct gdbarch *gdbarch, gdbarch_frame_num_args_ftype *frame_num_args);

extern int gdbarch_frame_align_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_frame_align_ftype) (struct gdbarch *gdbarch, CORE_ADDR address);
extern CORE_ADDR gdbarch_frame_align (struct gdbarch *gdbarch, CORE_ADDR address);
extern void set_gdbarch_frame_align (struct gdbarch *gdbarch, gdbarch_frame_align_ftype *frame_align);

/* deprecated_reg_struct_has_addr has been replaced by
   stabs_argument_has_addr. */

extern int gdbarch_deprecated_reg_struct_has_addr_p (struct gdbarch *gdbarch);

typedef int (gdbarch_deprecated_reg_struct_has_addr_ftype) (int gcc_p, struct type *type);
extern int gdbarch_deprecated_reg_struct_has_addr (struct gdbarch *gdbarch, int gcc_p, struct type *type);
extern void set_gdbarch_deprecated_reg_struct_has_addr (struct gdbarch *gdbarch, gdbarch_deprecated_reg_struct_has_addr_ftype *deprecated_reg_struct_has_addr);

typedef int (gdbarch_stabs_argument_has_addr_ftype) (struct gdbarch *gdbarch, struct type *type);
extern int gdbarch_stabs_argument_has_addr (struct gdbarch *gdbarch, struct type *type);
extern void set_gdbarch_stabs_argument_has_addr (struct gdbarch *gdbarch, gdbarch_stabs_argument_has_addr_ftype *stabs_argument_has_addr);

extern int gdbarch_frame_red_zone_size (struct gdbarch *gdbarch);
extern void set_gdbarch_frame_red_zone_size (struct gdbarch *gdbarch, int frame_red_zone_size);

typedef CORE_ADDR (gdbarch_convert_from_func_ptr_addr_ftype) (struct gdbarch *gdbarch, CORE_ADDR addr, struct target_ops *targ);
extern CORE_ADDR gdbarch_convert_from_func_ptr_addr (struct gdbarch *gdbarch, CORE_ADDR addr, struct target_ops *targ);
extern void set_gdbarch_convert_from_func_ptr_addr (struct gdbarch *gdbarch, gdbarch_convert_from_func_ptr_addr_ftype *convert_from_func_ptr_addr);

/* On some machines there are bits in addresses which are not really
   part of the address, but are used by the kernel, the hardware, etc.
   for special purposes.  gdbarch_addr_bits_remove takes out any such bits so
   we get a "real" address such as one would find in a symbol table.
   This is used only for addresses of instructions, and even then I'm
   not sure it's used in all contexts.  It exists to deal with there
   being a few stray bits in the PC which would mislead us, not as some
   sort of generic thing to handle alignment or segmentation (it's
   possible it should be in TARGET_READ_PC instead). */

typedef CORE_ADDR (gdbarch_addr_bits_remove_ftype) (CORE_ADDR addr);
extern CORE_ADDR gdbarch_addr_bits_remove (struct gdbarch *gdbarch, CORE_ADDR addr);
extern void set_gdbarch_addr_bits_remove (struct gdbarch *gdbarch, gdbarch_addr_bits_remove_ftype *addr_bits_remove);

/* It is not at all clear why gdbarch_smash_text_address is not folded into
   gdbarch_addr_bits_remove. */

typedef CORE_ADDR (gdbarch_smash_text_address_ftype) (CORE_ADDR addr);
extern CORE_ADDR gdbarch_smash_text_address (struct gdbarch *gdbarch, CORE_ADDR addr);
extern void set_gdbarch_smash_text_address (struct gdbarch *gdbarch, gdbarch_smash_text_address_ftype *smash_text_address);

/* FIXME/cagney/2001-01-18: This should be split in two.  A target method that
   indicates if the target needs software single step.  An ISA method to
   implement it.
  
   FIXME/cagney/2001-01-18: This should be replaced with something that inserts
   breakpoints using the breakpoint system instead of blatting memory directly
   (as with rs6000).
  
   FIXME/cagney/2001-01-18: The logic is backwards.  It should be asking if the
   target can single step.  If not, then implement single step using breakpoints.
  
   A return value of 1 means that the software_single_step breakpoints
   were inserted; 0 means they were not. */

extern int gdbarch_software_single_step_p (struct gdbarch *gdbarch);

typedef int (gdbarch_software_single_step_ftype) (struct frame_info *frame);
extern int gdbarch_software_single_step (struct gdbarch *gdbarch, struct frame_info *frame);
extern void set_gdbarch_software_single_step (struct gdbarch *gdbarch, gdbarch_software_single_step_ftype *software_single_step);

/* Return non-zero if the processor is executing a delay slot and a
   further single-step is needed before the instruction finishes. */

extern int gdbarch_single_step_through_delay_p (struct gdbarch *gdbarch);

typedef int (gdbarch_single_step_through_delay_ftype) (struct gdbarch *gdbarch, struct frame_info *frame);
extern int gdbarch_single_step_through_delay (struct gdbarch *gdbarch, struct frame_info *frame);
extern void set_gdbarch_single_step_through_delay (struct gdbarch *gdbarch, gdbarch_single_step_through_delay_ftype *single_step_through_delay);

/* FIXME: cagney/2003-08-28: Need to find a better way of selecting the
   disassembler.  Perhaps objdump can handle it? */

typedef int (gdbarch_print_insn_ftype) (bfd_vma vma, struct disassemble_info *info);
extern int gdbarch_print_insn (struct gdbarch *gdbarch, bfd_vma vma, struct disassemble_info *info);
extern void set_gdbarch_print_insn (struct gdbarch *gdbarch, gdbarch_print_insn_ftype *print_insn);

typedef CORE_ADDR (gdbarch_skip_trampoline_code_ftype) (struct frame_info *frame, CORE_ADDR pc);
extern CORE_ADDR gdbarch_skip_trampoline_code (struct gdbarch *gdbarch, struct frame_info *frame, CORE_ADDR pc);
extern void set_gdbarch_skip_trampoline_code (struct gdbarch *gdbarch, gdbarch_skip_trampoline_code_ftype *skip_trampoline_code);

/* If IN_SOLIB_DYNSYM_RESOLVE_CODE returns true, and SKIP_SOLIB_RESOLVER
   evaluates non-zero, this is the address where the debugger will place
   a step-resume breakpoint to get us past the dynamic linker. */

typedef CORE_ADDR (gdbarch_skip_solib_resolver_ftype) (struct gdbarch *gdbarch, CORE_ADDR pc);
extern CORE_ADDR gdbarch_skip_solib_resolver (struct gdbarch *gdbarch, CORE_ADDR pc);
extern void set_gdbarch_skip_solib_resolver (struct gdbarch *gdbarch, gdbarch_skip_solib_resolver_ftype *skip_solib_resolver);

/* Some systems also have trampoline code for returning from shared libs. */

typedef int (gdbarch_in_solib_return_trampoline_ftype) (CORE_ADDR pc, char *name);
extern int gdbarch_in_solib_return_trampoline (struct gdbarch *gdbarch, CORE_ADDR pc, char *name);
extern void set_gdbarch_in_solib_return_trampoline (struct gdbarch *gdbarch, gdbarch_in_solib_return_trampoline_ftype *in_solib_return_trampoline);

/* A target might have problems with watchpoints as soon as the stack
   frame of the current function has been destroyed.  This mostly happens
   as the first action in a funtion's epilogue.  in_function_epilogue_p()
   is defined to return a non-zero value if either the given addr is one
   instruction after the stack destroying instruction up to the trailing
   return instruction or if we can figure out that the stack frame has
   already been invalidated regardless of the value of addr.  Targets
   which don't suffer from that problem could just let this functionality
   untouched. */

typedef int (gdbarch_in_function_epilogue_p_ftype) (struct gdbarch *gdbarch, CORE_ADDR addr);
extern int gdbarch_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR addr);
extern void set_gdbarch_in_function_epilogue_p (struct gdbarch *gdbarch, gdbarch_in_function_epilogue_p_ftype *in_function_epilogue_p);

/* Given a vector of command-line arguments, return a newly allocated
   string which, when passed to the create_inferior function, will be
   parsed (on Unix systems, by the shell) to yield the same vector.
   This function should call error() if the argument vector is not
   representable for this target or if this target does not support
   command-line arguments.
   ARGC is the number of elements in the vector.
   ARGV is an array of strings, one per argument. */

typedef char * (gdbarch_construct_inferior_arguments_ftype) (struct gdbarch *gdbarch, int argc, char **argv);
extern char * gdbarch_construct_inferior_arguments (struct gdbarch *gdbarch, int argc, char **argv);
extern void set_gdbarch_construct_inferior_arguments (struct gdbarch *gdbarch, gdbarch_construct_inferior_arguments_ftype *construct_inferior_arguments);

typedef void (gdbarch_elf_make_msymbol_special_ftype) (asymbol *sym, struct minimal_symbol *msym);
extern void gdbarch_elf_make_msymbol_special (struct gdbarch *gdbarch, asymbol *sym, struct minimal_symbol *msym);
extern void set_gdbarch_elf_make_msymbol_special (struct gdbarch *gdbarch, gdbarch_elf_make_msymbol_special_ftype *elf_make_msymbol_special);

typedef void (gdbarch_coff_make_msymbol_special_ftype) (int val, struct minimal_symbol *msym);
extern void gdbarch_coff_make_msymbol_special (struct gdbarch *gdbarch, int val, struct minimal_symbol *msym);
extern void set_gdbarch_coff_make_msymbol_special (struct gdbarch *gdbarch, gdbarch_coff_make_msymbol_special_ftype *coff_make_msymbol_special);

extern const char * gdbarch_name_of_malloc (struct gdbarch *gdbarch);
extern void set_gdbarch_name_of_malloc (struct gdbarch *gdbarch, const char * name_of_malloc);

extern int gdbarch_cannot_step_breakpoint (struct gdbarch *gdbarch);
extern void set_gdbarch_cannot_step_breakpoint (struct gdbarch *gdbarch, int cannot_step_breakpoint);

extern int gdbarch_have_nonsteppable_watchpoint (struct gdbarch *gdbarch);
extern void set_gdbarch_have_nonsteppable_watchpoint (struct gdbarch *gdbarch, int have_nonsteppable_watchpoint);

extern int gdbarch_address_class_type_flags_p (struct gdbarch *gdbarch);

typedef int (gdbarch_address_class_type_flags_ftype) (int byte_size, int dwarf2_addr_class);
extern int gdbarch_address_class_type_flags (struct gdbarch *gdbarch, int byte_size, int dwarf2_addr_class);
extern void set_gdbarch_address_class_type_flags (struct gdbarch *gdbarch, gdbarch_address_class_type_flags_ftype *address_class_type_flags);

extern int gdbarch_address_class_type_flags_to_name_p (struct gdbarch *gdbarch);

typedef const char * (gdbarch_address_class_type_flags_to_name_ftype) (struct gdbarch *gdbarch, int type_flags);
extern const char * gdbarch_address_class_type_flags_to_name (struct gdbarch *gdbarch, int type_flags);
extern void set_gdbarch_address_class_type_flags_to_name (struct gdbarch *gdbarch, gdbarch_address_class_type_flags_to_name_ftype *address_class_type_flags_to_name);

extern int gdbarch_address_class_name_to_type_flags_p (struct gdbarch *gdbarch);

typedef int (gdbarch_address_class_name_to_type_flags_ftype) (struct gdbarch *gdbarch, const char *name, int *type_flags_ptr);
extern int gdbarch_address_class_name_to_type_flags (struct gdbarch *gdbarch, const char *name, int *type_flags_ptr);
extern void set_gdbarch_address_class_name_to_type_flags (struct gdbarch *gdbarch, gdbarch_address_class_name_to_type_flags_ftype *address_class_name_to_type_flags);

/* Is a register in a group */

typedef int (gdbarch_register_reggroup_p_ftype) (struct gdbarch *gdbarch, int regnum, struct reggroup *reggroup);
extern int gdbarch_register_reggroup_p (struct gdbarch *gdbarch, int regnum, struct reggroup *reggroup);
extern void set_gdbarch_register_reggroup_p (struct gdbarch *gdbarch, gdbarch_register_reggroup_p_ftype *register_reggroup_p);

/* Fetch the pointer to the ith function argument. */

extern int gdbarch_fetch_pointer_argument_p (struct gdbarch *gdbarch);

typedef CORE_ADDR (gdbarch_fetch_pointer_argument_ftype) (struct frame_info *frame, int argi, struct type *type);
extern CORE_ADDR gdbarch_fetch_pointer_argument (struct gdbarch *gdbarch, struct frame_info *frame, int argi, struct type *type);
extern void set_gdbarch_fetch_pointer_argument (struct gdbarch *gdbarch, gdbarch_fetch_pointer_argument_ftype *fetch_pointer_argument);

/* Return the appropriate register set for a core file section with
   name SECT_NAME and size SECT_SIZE. */

extern int gdbarch_regset_from_core_section_p (struct gdbarch *gdbarch);

typedef const struct regset * (gdbarch_regset_from_core_section_ftype) (struct gdbarch *gdbarch, const char *sect_name, size_t sect_size);
extern const struct regset * gdbarch_regset_from_core_section (struct gdbarch *gdbarch, const char *sect_name, size_t sect_size);
extern void set_gdbarch_regset_from_core_section (struct gdbarch *gdbarch, gdbarch_regset_from_core_section_ftype *regset_from_core_section);

/* Read offset OFFSET of TARGET_OBJECT_LIBRARIES formatted shared libraries list from
   core file into buffer READBUF with length LEN. */

extern int gdbarch_core_xfer_shared_libraries_p (struct gdbarch *gdbarch);

typedef LONGEST (gdbarch_core_xfer_shared_libraries_ftype) (struct gdbarch *gdbarch, gdb_byte *readbuf, ULONGEST offset, LONGEST len);
extern LONGEST gdbarch_core_xfer_shared_libraries (struct gdbarch *gdbarch, gdb_byte *readbuf, ULONGEST offset, LONGEST len);
extern void set_gdbarch_core_xfer_shared_libraries (struct gdbarch *gdbarch, gdbarch_core_xfer_shared_libraries_ftype *core_xfer_shared_libraries);

/* If the elements of C++ vtables are in-place function descriptors rather
   than normal function pointers (which may point to code or a descriptor),
   set this to one. */

extern int gdbarch_vtable_function_descriptors (struct gdbarch *gdbarch);
extern void set_gdbarch_vtable_function_descriptors (struct gdbarch *gdbarch, int vtable_function_descriptors);

/* Set if the least significant bit of the delta is used instead of the least
   significant bit of the pfn for pointers to virtual member functions. */

extern int gdbarch_vbit_in_delta (struct gdbarch *gdbarch);
extern void set_gdbarch_vbit_in_delta (struct gdbarch *gdbarch, int vbit_in_delta);

/* Advance PC to next instruction in order to skip a permanent breakpoint. */

extern int gdbarch_skip_permanent_breakpoint_p (struct gdbarch *gdbarch);

typedef void (gdbarch_skip_permanent_breakpoint_ftype) (struct regcache *regcache);
extern void gdbarch_skip_permanent_breakpoint (struct gdbarch *gdbarch, struct regcache *regcache);
extern void set_gdbarch_skip_permanent_breakpoint (struct gdbarch *gdbarch, gdbarch_skip_permanent_breakpoint_ftype *skip_permanent_breakpoint);

/* Refresh overlay mapped state for section OSECT. */

extern int gdbarch_overlay_update_p (struct gdbarch *gdbarch);

typedef void (gdbarch_overlay_update_ftype) (struct obj_section *osect);
extern void gdbarch_overlay_update (struct gdbarch *gdbarch, struct obj_section *osect);
extern void set_gdbarch_overlay_update (struct gdbarch *gdbarch, gdbarch_overlay_update_ftype *overlay_update);

extern struct gdbarch_tdep *gdbarch_tdep (struct gdbarch *gdbarch);


/* Mechanism for co-ordinating the selection of a specific
   architecture.

   GDB targets (*-tdep.c) can register an interest in a specific
   architecture.  Other GDB components can register a need to maintain
   per-architecture data.

   The mechanisms below ensures that there is only a loose connection
   between the set-architecture command and the various GDB
   components.  Each component can independently register their need
   to maintain architecture specific data with gdbarch.

   Pragmatics:

   Previously, a single TARGET_ARCHITECTURE_HOOK was provided.  It
   didn't scale.

   The more traditional mega-struct containing architecture specific
   data for all the various GDB components was also considered.  Since
   GDB is built from a variable number of (fairly independent)
   components it was determined that the global aproach was not
   applicable. */


/* Register a new architectural family with GDB.

   Register support for the specified ARCHITECTURE with GDB.  When
   gdbarch determines that the specified architecture has been
   selected, the corresponding INIT function is called.

   --

   The INIT function takes two parameters: INFO which contains the
   information available to gdbarch about the (possibly new)
   architecture; ARCHES which is a list of the previously created
   ``struct gdbarch'' for this architecture.

   The INFO parameter is, as far as possible, be pre-initialized with
   information obtained from INFO.ABFD or the global defaults.

   The ARCHES parameter is a linked list (sorted most recently used)
   of all the previously created architures for this architecture
   family.  The (possibly NULL) ARCHES->gdbarch can used to access
   values from the previously selected architecture for this
   architecture family.  The global ``current_gdbarch'' shall not be
   used.

   The INIT function shall return any of: NULL - indicating that it
   doesn't recognize the selected architecture; an existing ``struct
   gdbarch'' from the ARCHES list - indicating that the new
   architecture is just a synonym for an earlier architecture (see
   gdbarch_list_lookup_by_info()); a newly created ``struct gdbarch''
   - that describes the selected architecture (see gdbarch_alloc()).

   The DUMP_TDEP function shall print out all target specific values.
   Care should be taken to ensure that the function works in both the
   multi-arch and non- multi-arch cases. */

struct gdbarch_list
{
  struct gdbarch *gdbarch;
  struct gdbarch_list *next;
};

struct gdbarch_info
{
  /* Use default: NULL (ZERO). */
  const struct bfd_arch_info *bfd_arch_info;

  /* Use default: BFD_ENDIAN_UNKNOWN (NB: is not ZERO).  */
  int byte_order;

  /* Use default: NULL (ZERO). */
  bfd *abfd;

  /* Use default: NULL (ZERO). */
  struct gdbarch_tdep_info *tdep_info;

  /* Use default: GDB_OSABI_UNINITIALIZED (-1).  */
  enum gdb_osabi osabi;

  /* Use default: NULL (ZERO).  */
  const struct target_desc *target_desc;
};

typedef struct gdbarch *(gdbarch_init_ftype) (struct gdbarch_info info, struct gdbarch_list *arches);
typedef void (gdbarch_dump_tdep_ftype) (struct gdbarch *gdbarch, struct ui_file *file);

/* DEPRECATED - use gdbarch_register() */
extern void register_gdbarch_init (enum bfd_architecture architecture, gdbarch_init_ftype *);

extern void gdbarch_register (enum bfd_architecture architecture,
                              gdbarch_init_ftype *,
                              gdbarch_dump_tdep_ftype *);


/* Return a freshly allocated, NULL terminated, array of the valid
   architecture names.  Since architectures are registered during the
   _initialize phase this function only returns useful information
   once initialization has been completed. */

extern const char **gdbarch_printable_names (void);


/* Helper function.  Search the list of ARCHES for a GDBARCH that
   matches the information provided by INFO. */

extern struct gdbarch_list *gdbarch_list_lookup_by_info (struct gdbarch_list *arches, const struct gdbarch_info *info);


/* Helper function.  Create a preliminary ``struct gdbarch''.  Perform
   basic initialization using values obtained from the INFO and TDEP
   parameters.  set_gdbarch_*() functions are called to complete the
   initialization of the object. */

extern struct gdbarch *gdbarch_alloc (const struct gdbarch_info *info, struct gdbarch_tdep *tdep);


/* Helper function.  Free a partially-constructed ``struct gdbarch''.
   It is assumed that the caller freeds the ``struct
   gdbarch_tdep''. */

extern void gdbarch_free (struct gdbarch *);


/* Helper function.  Allocate memory from the ``struct gdbarch''
   obstack.  The memory is freed when the corresponding architecture
   is also freed.  */

extern void *gdbarch_obstack_zalloc (struct gdbarch *gdbarch, long size);
#define GDBARCH_OBSTACK_CALLOC(GDBARCH, NR, TYPE) ((TYPE *) gdbarch_obstack_zalloc ((GDBARCH), (NR) * sizeof (TYPE)))
#define GDBARCH_OBSTACK_ZALLOC(GDBARCH, TYPE) ((TYPE *) gdbarch_obstack_zalloc ((GDBARCH), sizeof (TYPE)))


/* Helper function. Force an update of the current architecture.

   The actual architecture selected is determined by INFO, ``(gdb) set
   architecture'' et.al., the existing architecture and BFD's default
   architecture.  INFO should be initialized to zero and then selected
   fields should be updated.

   Returns non-zero if the update succeeds */

extern int gdbarch_update_p (struct gdbarch_info info);


/* Helper function.  Find an architecture matching info.

   INFO should be initialized using gdbarch_info_init, relevant fields
   set, and then finished using gdbarch_info_fill.

   Returns the corresponding architecture, or NULL if no matching
   architecture was found.  "current_gdbarch" is not updated.  */

extern struct gdbarch *gdbarch_find_by_info (struct gdbarch_info info);


/* Helper function.  Set the global "current_gdbarch" to "gdbarch".

   FIXME: kettenis/20031124: Of the functions that follow, only
   gdbarch_from_bfd is supposed to survive.  The others will
   dissappear since in the future GDB will (hopefully) be truly
   multi-arch.  However, for now we're still stuck with the concept of
   a single active architecture.  */

extern void deprecated_current_gdbarch_select_hack (struct gdbarch *gdbarch);


/* Register per-architecture data-pointer.

   Reserve space for a per-architecture data-pointer.  An identifier
   for the reserved data-pointer is returned.  That identifer should
   be saved in a local static variable.

   Memory for the per-architecture data shall be allocated using
   gdbarch_obstack_zalloc.  That memory will be deleted when the
   corresponding architecture object is deleted.

   When a previously created architecture is re-selected, the
   per-architecture data-pointer for that previous architecture is
   restored.  INIT() is not re-called.

   Multiple registrarants for any architecture are allowed (and
   strongly encouraged).  */

struct gdbarch_data;

typedef void *(gdbarch_data_pre_init_ftype) (struct obstack *obstack);
extern struct gdbarch_data *gdbarch_data_register_pre_init (gdbarch_data_pre_init_ftype *init);
typedef void *(gdbarch_data_post_init_ftype) (struct gdbarch *gdbarch);
extern struct gdbarch_data *gdbarch_data_register_post_init (gdbarch_data_post_init_ftype *init);
extern void deprecated_set_gdbarch_data (struct gdbarch *gdbarch,
                                         struct gdbarch_data *data,
			                 void *pointer);

extern void *gdbarch_data (struct gdbarch *gdbarch, struct gdbarch_data *);


/* Set the dynamic target-system-dependent parameters (architecture,
   byte-order, ...) using information found in the BFD */

extern void set_gdbarch_from_file (bfd *);


/* Initialize the current architecture to the "first" one we find on
   our list.  */

extern void initialize_current_architecture (void);

/* gdbarch trace variable */
extern int gdbarch_debug;

extern void gdbarch_dump (struct gdbarch *gdbarch, struct ui_file *file);

#endif
