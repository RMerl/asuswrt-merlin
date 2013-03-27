/* Renesas M32C target-dependent code for GDB, the GNU debugger.

   Copyright 2004, 2005, 2007 Free Software Foundation, Inc.

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

#include "defs.h"

#include <stdarg.h>

#if defined (HAVE_STRING_H)
#include <string.h>
#endif

#include "gdb_assert.h"
#include "elf-bfd.h"
#include "elf/m32c.h"
#include "gdb/sim-m32c.h"
#include "dis-asm.h"
#include "gdbtypes.h"
#include "regcache.h"
#include "arch-utils.h"
#include "frame.h"
#include "frame-unwind.h"
#include "dwarf2-frame.h"
#include "dwarf2expr.h"
#include "symtab.h"
#include "gdbcore.h"
#include "value.h"
#include "reggroups.h"
#include "prologue-value.h"
#include "target.h"


/* The m32c tdep structure.  */

static struct reggroup *m32c_dma_reggroup;

struct m32c_reg;

/* The type of a function that moves the value of REG between CACHE or
   BUF --- in either direction.  */
typedef void (m32c_move_reg_t) (struct m32c_reg *reg,
				struct regcache *cache,
				void *buf);

struct m32c_reg
{
  /* The name of this register.  */
  const char *name;

  /* Its type.  */
  struct type *type;

  /* The architecture this register belongs to.  */
  struct gdbarch *arch;

  /* Its GDB register number.  */
  int num;

  /* Its sim register number.  */
  int sim_num;

  /* Its DWARF register number, or -1 if it doesn't have one.  */
  int dwarf_num;

  /* Register group memberships.  */
  unsigned int general_p : 1;
  unsigned int dma_p : 1;
  unsigned int system_p : 1;
  unsigned int save_restore_p : 1;

  /* Functions to read its value from a regcache, and write its value
     to a regcache.  */
  m32c_move_reg_t *read, *write;

  /* Data for READ and WRITE functions.  The exact meaning depends on
     the specific functions selected; see the comments for those
     functions.  */
  struct m32c_reg *rx, *ry;
  int n;
};


/* An overestimate of the number of raw and pseudoregisters we will
   have.  The exact answer depends on the variant of the architecture
   at hand, but we can use this to declare statically allocated
   arrays, and bump it up when needed.  */
#define M32C_MAX_NUM_REGS (75)

/* The largest assigned DWARF register number.  */
#define M32C_MAX_DWARF_REGNUM (40)


struct gdbarch_tdep
{
  /* All the registers for this variant, indexed by GDB register
     number, and the number of registers present.  */
  struct m32c_reg regs[M32C_MAX_NUM_REGS];

  /* The number of valid registers.  */
  int num_regs;

  /* Interesting registers.  These are pointers into REGS.  */
  struct m32c_reg *pc, *flg;
  struct m32c_reg *r0, *r1, *r2, *r3, *a0, *a1;
  struct m32c_reg *r2r0, *r3r2r1r0, *r3r1r2r0;
  struct m32c_reg *sb, *fb, *sp;

  /* A table indexed by DWARF register numbers, pointing into
     REGS.  */
  struct m32c_reg *dwarf_regs[M32C_MAX_DWARF_REGNUM + 1];

  /* Types for this architecture.  We can't use the builtin_type_foo
     types, because they're not initialized when building a gdbarch
     structure.  */
  struct type *voyd, *ptr_voyd, *func_voyd;
  struct type *uint8, *uint16;
  struct type *int8, *int16, *int32, *int64;

  /* The types for data address and code address registers.  */
  struct type *data_addr_reg_type, *code_addr_reg_type;

  /* The number of bytes a return address pushed by a 'jsr' instruction
     occupies on the stack.  */
  int ret_addr_bytes;

  /* The number of bytes an address register occupies on the stack
     when saved by an 'enter' or 'pushm' instruction.  */
  int push_addr_bytes;
};


/* Types.  */

static void
make_types (struct gdbarch *arch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  unsigned long mach = gdbarch_bfd_arch_info (arch)->mach;
  int data_addr_reg_bits, code_addr_reg_bits;
  char type_name[50];

#if 0
  /* This is used to clip CORE_ADDR values, so this value is
     appropriate both on the m32c, where pointers are 32 bits long,
     and on the m16c, where pointers are sixteen bits long, but there
     may be code above the 64k boundary.  */
  set_gdbarch_addr_bit (arch, 24);
#else
  /* GCC uses 32 bits for addrs in the dwarf info, even though
     only 16/24 bits are used.  Setting addr_bit to 24 causes
     errors in reading the dwarf addresses.  */
  set_gdbarch_addr_bit (arch, 32);
#endif

  set_gdbarch_int_bit (arch, 16);
  switch (mach)
    {
    case bfd_mach_m16c:
      data_addr_reg_bits = 16;
      code_addr_reg_bits = 24;
      set_gdbarch_ptr_bit (arch, 16);
      tdep->ret_addr_bytes = 3;
      tdep->push_addr_bytes = 2;
      break;

    case bfd_mach_m32c:
      data_addr_reg_bits = 24;
      code_addr_reg_bits = 24;
      set_gdbarch_ptr_bit (arch, 32);
      tdep->ret_addr_bytes = 4;
      tdep->push_addr_bytes = 4;
      break;

    default:
      gdb_assert (0);
    }

  /* The builtin_type_mumble variables are sometimes uninitialized when
     this is called, so we avoid using them.  */
  tdep->voyd = init_type (TYPE_CODE_VOID, 1, 0, "void", NULL);
  tdep->ptr_voyd = init_type (TYPE_CODE_PTR, gdbarch_ptr_bit (arch) / 8,
			      TYPE_FLAG_UNSIGNED, NULL, NULL);
  TYPE_TARGET_TYPE (tdep->ptr_voyd) = tdep->voyd;
  tdep->func_voyd = lookup_function_type (tdep->voyd);

  sprintf (type_name, "%s_data_addr_t",
	   gdbarch_bfd_arch_info (arch)->printable_name);
  tdep->data_addr_reg_type
    = init_type (TYPE_CODE_PTR, data_addr_reg_bits / 8,
		 TYPE_FLAG_UNSIGNED, xstrdup (type_name), NULL);
  TYPE_TARGET_TYPE (tdep->data_addr_reg_type) = tdep->voyd;

  sprintf (type_name, "%s_code_addr_t",
	   gdbarch_bfd_arch_info (arch)->printable_name);
  tdep->code_addr_reg_type
    = init_type (TYPE_CODE_PTR, code_addr_reg_bits / 8,
		 TYPE_FLAG_UNSIGNED, xstrdup (type_name), NULL);
  TYPE_TARGET_TYPE (tdep->code_addr_reg_type) = tdep->func_voyd;

  tdep->uint8  = init_type (TYPE_CODE_INT, 1, TYPE_FLAG_UNSIGNED,
			    "uint8_t", NULL);
  tdep->uint16 = init_type (TYPE_CODE_INT, 2, TYPE_FLAG_UNSIGNED,
			    "uint16_t", NULL);
  tdep->int8   = init_type (TYPE_CODE_INT, 1, 0, "int8_t", NULL);
  tdep->int16  = init_type (TYPE_CODE_INT, 2, 0, "int16_t", NULL);
  tdep->int32  = init_type (TYPE_CODE_INT, 4, 0, "int32_t", NULL);
  tdep->int64  = init_type (TYPE_CODE_INT, 8, 0, "int64_t", NULL);
}



/* Register set.  */

static const char *
m32c_register_name (int num)
{
  return gdbarch_tdep (current_gdbarch)->regs[num].name;
}


static struct type *
m32c_register_type (struct gdbarch *arch, int reg_nr)
{
  return gdbarch_tdep (arch)->regs[reg_nr].type;
}


static int
m32c_register_sim_regno (int reg_nr)
{
  return gdbarch_tdep (current_gdbarch)->regs[reg_nr].sim_num;
}


static int
m32c_debug_info_reg_to_regnum (int reg_nr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  if (0 <= reg_nr && reg_nr <= M32C_MAX_DWARF_REGNUM
      && tdep->dwarf_regs[reg_nr])
    return tdep->dwarf_regs[reg_nr]->num;
  else
    /* The DWARF CFI code expects to see -1 for invalid register
       numbers.  */
    return -1;
}


int
m32c_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			  struct reggroup *group)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct m32c_reg *reg = &tdep->regs[regnum];

  /* The anonymous raw registers aren't in any groups.  */
  if (! reg->name)
    return 0;

  if (group == all_reggroup)
    return 1;

  if (group == general_reggroup
      && reg->general_p)
    return 1;

  if (group == m32c_dma_reggroup
      && reg->dma_p)
    return 1;

  if (group == system_reggroup
      && reg->system_p)
    return 1;

  /* Since the m32c DWARF register numbers refer to cooked registers, not
     raw registers, and frame_pop depends on the save and restore groups
     containing registers the DWARF CFI will actually mention, our save
     and restore groups are cooked registers, not raw registers.  (This is
     why we can't use the default reggroup function.)  */
  if ((group == save_reggroup
       || group == restore_reggroup)
      && reg->save_restore_p)
    return 1;

  return 0;
}


/* Register move functions.  We declare them here using
   m32c_move_reg_t to check the types.  */
static m32c_move_reg_t m32c_raw_read,      m32c_raw_write;
static m32c_move_reg_t m32c_banked_read,   m32c_banked_write;
static m32c_move_reg_t m32c_sb_read, 	   m32c_sb_write;
static m32c_move_reg_t m32c_part_read,     m32c_part_write;
static m32c_move_reg_t m32c_cat_read,      m32c_cat_write;
static m32c_move_reg_t m32c_r3r2r1r0_read, m32c_r3r2r1r0_write;


/* Copy the value of the raw register REG from CACHE to BUF.  */
static void
m32c_raw_read (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  regcache_raw_read (cache, reg->num, buf);
}


/* Copy the value of the raw register REG from BUF to CACHE.  */
static void
m32c_raw_write (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  regcache_raw_write (cache, reg->num, (const void *) buf);
}


/* Return the value of the 'flg' register in CACHE.  */
static int
m32c_read_flg (struct regcache *cache)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_regcache_arch (cache));
  ULONGEST flg;
  regcache_raw_read_unsigned (cache, tdep->flg->num, &flg);
  return flg & 0xffff;
}


/* Evaluate the real register number of a banked register.  */
static struct m32c_reg *
m32c_banked_register (struct m32c_reg *reg, struct regcache *cache)
{
  return ((m32c_read_flg (cache) & reg->n) ? reg->ry : reg->rx);
}


/* Move the value of a banked register from CACHE to BUF.
   If the value of the 'flg' register in CACHE has any of the bits
   masked in REG->n set, then read REG->ry.  Otherwise, read
   REG->rx.  */
static void
m32c_banked_read (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  struct m32c_reg *bank_reg = m32c_banked_register (reg, cache);
  regcache_raw_read (cache, bank_reg->num, buf);
}


/* Move the value of a banked register from BUF to CACHE.
   If the value of the 'flg' register in CACHE has any of the bits
   masked in REG->n set, then write REG->ry.  Otherwise, write
   REG->rx.  */
static void
m32c_banked_write (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  struct m32c_reg *bank_reg = m32c_banked_register (reg, cache);
  regcache_raw_write (cache, bank_reg->num, (const void *) buf);
}


/* Move the value of SB from CACHE to BUF.  On bfd_mach_m32c, SB is a
   banked register; on bfd_mach_m16c, it's not.  */
static void
m32c_sb_read (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  if (gdbarch_bfd_arch_info (reg->arch)->mach == bfd_mach_m16c)
    m32c_raw_read (reg->rx, cache, buf);
  else
    m32c_banked_read (reg, cache, buf);
}


/* Move the value of SB from BUF to CACHE.  On bfd_mach_m32c, SB is a
   banked register; on bfd_mach_m16c, it's not.  */
static void
m32c_sb_write (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  if (gdbarch_bfd_arch_info (reg->arch)->mach == bfd_mach_m16c)
    m32c_raw_write (reg->rx, cache, buf);
  else
    m32c_banked_write (reg, cache, buf);
}


/* Assuming REG uses m32c_part_read and m32c_part_write, set *OFFSET_P
   and *LEN_P to the offset and length, in bytes, of the part REG
   occupies in its underlying register.  The offset is from the
   lower-addressed end, regardless of the architecture's endianness.
   (The M32C family is always little-endian, but let's keep those
   assumptions out of here.)  */
static void
m32c_find_part (struct m32c_reg *reg, int *offset_p, int *len_p)
{
  /* The length of the containing register, of which REG is one part.  */
  int containing_len = TYPE_LENGTH (reg->rx->type);

  /* The length of one "element" in our imaginary array.  */
  int elt_len = TYPE_LENGTH (reg->type);

  /* The offset of REG's "element" from the least significant end of
     the containing register.  */
  int elt_offset = reg->n * elt_len;

  /* If we extend off the end, trim the length of the element.  */
  if (elt_offset + elt_len > containing_len)
    {
      elt_len = containing_len - elt_offset;
      /* We shouldn't be declaring partial registers that go off the
	 end of their containing registers.  */
      gdb_assert (elt_len > 0);
    }

  /* Flip the offset around if we're big-endian.  */
  if (gdbarch_byte_order (reg->arch) == BFD_ENDIAN_BIG)
    elt_offset = TYPE_LENGTH (reg->rx->type) - elt_offset - elt_len;

  *offset_p = elt_offset;
  *len_p = elt_len;
}


/* Move the value of a partial register (r0h, intbl, etc.) from CACHE
   to BUF.  Treating the value of the register REG->rx as an array of
   REG->type values, where higher indices refer to more significant
   bits, read the value of the REG->n'th element.  */
static void
m32c_part_read (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  int offset, len;
  memset (buf, 0, TYPE_LENGTH (reg->type));
  m32c_find_part (reg, &offset, &len);
  regcache_cooked_read_part (cache, reg->rx->num, offset, len, buf);
}


/* Move the value of a banked register from BUF to CACHE.
   Treating the value of the register REG->rx as an array of REG->type
   values, where higher indices refer to more significant bits, write
   the value of the REG->n'th element.  */
static void
m32c_part_write (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  int offset, len;
  m32c_find_part (reg, &offset, &len);
  regcache_cooked_write_part (cache, reg->rx->num, offset, len, buf);
}


/* Move the value of REG from CACHE to BUF.  REG's value is the
   concatenation of the values of the registers REG->rx and REG->ry,
   with REG->rx contributing the more significant bits.  */
static void
m32c_cat_read (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  int high_bytes = TYPE_LENGTH (reg->rx->type);
  int low_bytes  = TYPE_LENGTH (reg->ry->type);
  /* For address arithmetic.  */
  unsigned char *cbuf = buf;

  gdb_assert (TYPE_LENGTH (reg->type) == high_bytes + low_bytes);

  if (gdbarch_byte_order (reg->arch) == BFD_ENDIAN_BIG)
    {
      regcache_cooked_read (cache, reg->rx->num, cbuf);
      regcache_cooked_read (cache, reg->ry->num, cbuf + high_bytes);
    }
  else
    {
      regcache_cooked_read (cache, reg->rx->num, cbuf + low_bytes);
      regcache_cooked_read (cache, reg->ry->num, cbuf);
    }
}


/* Move the value of REG from CACHE to BUF.  REG's value is the
   concatenation of the values of the registers REG->rx and REG->ry,
   with REG->rx contributing the more significant bits.  */
static void
m32c_cat_write (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  int high_bytes = TYPE_LENGTH (reg->rx->type);
  int low_bytes  = TYPE_LENGTH (reg->ry->type);
  /* For address arithmetic.  */
  unsigned char *cbuf = buf;

  gdb_assert (TYPE_LENGTH (reg->type) == high_bytes + low_bytes);

  if (gdbarch_byte_order (reg->arch) == BFD_ENDIAN_BIG)
    {
      regcache_cooked_write (cache, reg->rx->num, cbuf);
      regcache_cooked_write (cache, reg->ry->num, cbuf + high_bytes);
    }
  else
    {
      regcache_cooked_write (cache, reg->rx->num, cbuf + low_bytes);
      regcache_cooked_write (cache, reg->ry->num, cbuf);
    }
}


/* Copy the value of the raw register REG from CACHE to BUF.  REG is
   the concatenation (from most significant to least) of r3, r2, r1,
   and r0.  */
static void
m32c_r3r2r1r0_read (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (reg->arch);
  int len = TYPE_LENGTH (tdep->r0->type);

  /* For address arithmetic.  */
  unsigned char *cbuf = buf;

  if (gdbarch_byte_order (reg->arch) == BFD_ENDIAN_BIG)
    {
      regcache_cooked_read (cache, tdep->r0->num, cbuf + len * 3);
      regcache_cooked_read (cache, tdep->r1->num, cbuf + len * 2);
      regcache_cooked_read (cache, tdep->r2->num, cbuf + len * 1);
      regcache_cooked_read (cache, tdep->r3->num, cbuf);
    }
  else
    {
      regcache_cooked_read (cache, tdep->r0->num, cbuf);
      regcache_cooked_read (cache, tdep->r1->num, cbuf + len * 1);
      regcache_cooked_read (cache, tdep->r2->num, cbuf + len * 2);
      regcache_cooked_read (cache, tdep->r3->num, cbuf + len * 3);
    }
}


/* Copy the value of the raw register REG from BUF to CACHE.  REG is
   the concatenation (from most significant to least) of r3, r2, r1,
   and r0.  */
static void
m32c_r3r2r1r0_write (struct m32c_reg *reg, struct regcache *cache, void *buf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (reg->arch);
  int len = TYPE_LENGTH (tdep->r0->type);

  /* For address arithmetic.  */
  unsigned char *cbuf = buf;

  if (gdbarch_byte_order (reg->arch) == BFD_ENDIAN_BIG)
    {
      regcache_cooked_write (cache, tdep->r0->num, cbuf + len * 3);
      regcache_cooked_write (cache, tdep->r1->num, cbuf + len * 2);
      regcache_cooked_write (cache, tdep->r2->num, cbuf + len * 1);
      regcache_cooked_write (cache, tdep->r3->num, cbuf);
    }
  else
    {
      regcache_cooked_write (cache, tdep->r0->num, cbuf);
      regcache_cooked_write (cache, tdep->r1->num, cbuf + len * 1);
      regcache_cooked_write (cache, tdep->r2->num, cbuf + len * 2);
      regcache_cooked_write (cache, tdep->r3->num, cbuf + len * 3);
    }
}


static void
m32c_pseudo_register_read (struct gdbarch *arch,
			   struct regcache *cache,
			   int cookednum,
			   gdb_byte *buf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  struct m32c_reg *reg;

  gdb_assert (0 <= cookednum && cookednum < tdep->num_regs);
  gdb_assert (arch == get_regcache_arch (cache));
  gdb_assert (arch == tdep->regs[cookednum].arch);
  reg = &tdep->regs[cookednum];

  reg->read (reg, cache, buf);
}


static void
m32c_pseudo_register_write (struct gdbarch *arch,
			    struct regcache *cache,
			    int cookednum,
			    const gdb_byte *buf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  struct m32c_reg *reg;

  gdb_assert (0 <= cookednum && cookednum < tdep->num_regs);
  gdb_assert (arch == get_regcache_arch (cache));
  gdb_assert (arch == tdep->regs[cookednum].arch);
  reg = &tdep->regs[cookednum];

  reg->write (reg, cache, (void *) buf);
}


/* Add a register with the given fields to the end of ARCH's table.
   Return a pointer to the newly added register.  */
static struct m32c_reg *
add_reg (struct gdbarch *arch,
	 const char *name,
	 struct type *type,
	 int sim_num,
	 m32c_move_reg_t *read,
	 m32c_move_reg_t *write,
	 struct m32c_reg *rx,
	 struct m32c_reg *ry,
	 int n)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  struct m32c_reg *r = &tdep->regs[tdep->num_regs];

  gdb_assert (tdep->num_regs < M32C_MAX_NUM_REGS);

  r->name           = name;
  r->type           = type;
  r->arch           = arch;
  r->num            = tdep->num_regs;
  r->sim_num        = sim_num;
  r->dwarf_num      = -1;
  r->general_p      = 0;
  r->dma_p          = 0;
  r->system_p       = 0;
  r->save_restore_p = 0;
  r->read           = read;
  r->write          = write;
  r->rx             = rx;
  r->ry             = ry;
  r->n              = n;

  tdep->num_regs++;

  return r;
}


/* Record NUM as REG's DWARF register number.  */
static void
set_dwarf_regnum (struct m32c_reg *reg, int num)
{
  gdb_assert (num < M32C_MAX_NUM_REGS);

  /* Update the reg->DWARF mapping.  Only count the first number
     assigned to this register.  */
  if (reg->dwarf_num == -1)
    reg->dwarf_num = num;

  /* Update the DWARF->reg mapping.  */
  gdbarch_tdep (reg->arch)->dwarf_regs[num] = reg;
}


/* Mark REG as a general-purpose register, and return it.  */
static struct m32c_reg *
mark_general (struct m32c_reg *reg)
{
  reg->general_p = 1;
  return reg;
}


/* Mark REG as a DMA register, and return it.  */
static struct m32c_reg *
mark_dma (struct m32c_reg *reg)
{
  reg->dma_p = 1;
  return reg;
}


/* Mark REG as a SYSTEM register, and return it.  */
static struct m32c_reg *
mark_system (struct m32c_reg *reg)
{
  reg->system_p = 1;
  return reg;
}


/* Mark REG as a save-restore register, and return it.  */
static struct m32c_reg *
mark_save_restore (struct m32c_reg *reg)
{
  reg->save_restore_p = 1;
  return reg;
}


#define FLAGBIT_B	0x0010
#define FLAGBIT_U	0x0080

/* Handy macros for declaring registers.  These all evaluate to
   pointers to the register declared.  Macros that define two
   registers evaluate to a pointer to the first.  */

/* A raw register named NAME, with type TYPE and sim number SIM_NUM.  */
#define R(name, type, sim_num)					\
  (add_reg (arch, (name), (type), (sim_num),			\
	    m32c_raw_read, m32c_raw_write, NULL, NULL, 0))

/* The simulator register number for a raw register named NAME.  */
#define SIM(name) (m32c_sim_reg_ ## name)

/* A raw unsigned 16-bit data register named NAME.
   NAME should be an identifier, not a string.  */
#define R16U(name)						\
  (R(#name, tdep->uint16, SIM (name)))

/* A raw data address register named NAME.
   NAME should be an identifier, not a string.  */
#define RA(name)						\
  (R(#name, tdep->data_addr_reg_type, SIM (name)))

/* A raw code address register named NAME.  NAME should
   be an identifier, not a string.  */
#define RC(name)						\
  (R(#name, tdep->code_addr_reg_type, SIM (name)))

/* A pair of raw registers named NAME0 and NAME1, with type TYPE.
   NAME should be an identifier, not a string.  */
#define RP(name, type)				\
  (R(#name "0", (type), SIM (name ## 0)),	\
   R(#name "1", (type), SIM (name ## 1)) - 1)

/* A raw banked general-purpose data register named NAME.
   NAME should be an identifier, not a string.  */
#define RBD(name)						\
  (R(NULL, tdep->int16, SIM (name ## _bank0)),		\
   R(NULL, tdep->int16, SIM (name ## _bank1)) - 1)

/* A raw banked data address register named NAME.
   NAME should be an identifier, not a string.  */
#define RBA(name)						\
  (R(NULL, tdep->data_addr_reg_type, SIM (name ## _bank0)),	\
   R(NULL, tdep->data_addr_reg_type, SIM (name ## _bank1)) - 1)

/* A cooked register named NAME referring to a raw banked register
   from the bank selected by the current value of FLG.  RAW_PAIR
   should be a pointer to the first register in the banked pair.
   NAME must be an identifier, not a string.  */
#define CB(name, raw_pair)				\
  (add_reg (arch, #name, (raw_pair)->type, 0,		\
	    m32c_banked_read, m32c_banked_write,	\
            (raw_pair), (raw_pair + 1), FLAGBIT_B))

/* A pair of registers named NAMEH and NAMEL, of type TYPE, that
   access the top and bottom halves of the register pointed to by
   NAME.  NAME should be an identifier.  */
#define CHL(name, type)							\
  (add_reg (arch, #name "h", (type), 0,					\
	    m32c_part_read, m32c_part_write, name, NULL, 1),		\
   add_reg (arch, #name "l", (type), 0,					\
	    m32c_part_read, m32c_part_write, name, NULL, 0) - 1)

/* A register constructed by concatenating the two registers HIGH and
   LOW, whose name is HIGHLOW and whose type is TYPE.  */
#define CCAT(high, low, type)					\
  (add_reg (arch, #high #low, (type), 0,			\
	    m32c_cat_read, m32c_cat_write, (high), (low), 0))

/* Abbreviations for marking register group membership.  */
#define G(reg)   (mark_general (reg))
#define S(reg)   (mark_system  (reg))
#define DMA(reg) (mark_dma     (reg))


/* Construct the register set for ARCH.  */
static void
make_regs (struct gdbarch *arch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  int mach = gdbarch_bfd_arch_info (arch)->mach;
  int num_raw_regs;
  int num_cooked_regs;

  struct m32c_reg *r0;
  struct m32c_reg *r1;
  struct m32c_reg *r2;
  struct m32c_reg *r3;
  struct m32c_reg *a0;
  struct m32c_reg *a1;
  struct m32c_reg *fb;
  struct m32c_reg *sb;
  struct m32c_reg *sp;
  struct m32c_reg *r0hl;
  struct m32c_reg *r1hl;
  struct m32c_reg *r2hl;
  struct m32c_reg *r3hl;
  struct m32c_reg *intbhl;
  struct m32c_reg *r2r0;
  struct m32c_reg *r3r1;
  struct m32c_reg *r3r1r2r0;
  struct m32c_reg *r3r2r1r0;
  struct m32c_reg *a1a0;

  struct m32c_reg *raw_r0_pair = RBD (r0);
  struct m32c_reg *raw_r1_pair = RBD (r1);
  struct m32c_reg *raw_r2_pair = RBD (r2);
  struct m32c_reg *raw_r3_pair = RBD (r3);
  struct m32c_reg *raw_a0_pair = RBA (a0);
  struct m32c_reg *raw_a1_pair = RBA (a1);
  struct m32c_reg *raw_fb_pair = RBA (fb);

  /* sb is banked on the bfd_mach_m32c, but not on bfd_mach_m16c.
     We always declare both raw registers, and deal with the distinction
     in the pseudoregister.  */
  struct m32c_reg *raw_sb_pair = RBA (sb);

  struct m32c_reg *usp         = S (RA (usp));
  struct m32c_reg *isp         = S (RA (isp));
  struct m32c_reg *intb        = S (RC (intb));
  struct m32c_reg *pc          = G (RC (pc));
  struct m32c_reg *flg         = G (R16U (flg));

  if (mach == bfd_mach_m32c)
    {
      struct m32c_reg *svf     = S (R16U (svf));
      struct m32c_reg *svp     = S (RC (svp));
      struct m32c_reg *vct     = S (RC (vct));

      struct m32c_reg *dmd01   = DMA (RP (dmd, tdep->uint8));
      struct m32c_reg *dct01   = DMA (RP (dct, tdep->uint16));
      struct m32c_reg *drc01   = DMA (RP (drc, tdep->uint16));
      struct m32c_reg *dma01   = DMA (RP (dma, tdep->data_addr_reg_type));
      struct m32c_reg *dsa01   = DMA (RP (dsa, tdep->data_addr_reg_type));
      struct m32c_reg *dra01   = DMA (RP (dra, tdep->data_addr_reg_type));
    }

  num_raw_regs = tdep->num_regs;

  r0 	      = G (CB (r0, raw_r0_pair));
  r1 	      = G (CB (r1, raw_r1_pair));
  r2          = G (CB (r2, raw_r2_pair));
  r3          = G (CB (r3, raw_r3_pair));
  a0          = G (CB (a0, raw_a0_pair));
  a1          = G (CB (a1, raw_a1_pair));
  fb          = G (CB (fb, raw_fb_pair));

  /* sb is banked on the bfd_mach_m32c, but not on bfd_mach_m16c.
     Specify custom read/write functions that do the right thing.  */
  sb          = G (add_reg (arch, "sb", raw_sb_pair->type, 0,
			    m32c_sb_read, m32c_sb_write,
			    raw_sb_pair, raw_sb_pair + 1, 0));

  /* The current sp is either usp or isp, depending on the value of
     the FLG register's U bit.  */
  sp          = G (add_reg (arch, "sp", usp->type, 0,
			    m32c_banked_read, m32c_banked_write,
			    isp, usp, FLAGBIT_U));

  r0hl        = CHL (r0, tdep->int8);
  r1hl        = CHL (r1, tdep->int8);
  r2hl        = CHL (r2, tdep->int8);
  r3hl        = CHL (r3, tdep->int8);
  intbhl      = CHL (intb, tdep->int16);

  r2r0        = CCAT (r2,   r0,   tdep->int32);
  r3r1        = CCAT (r3,   r1,   tdep->int32);
  r3r1r2r0    = CCAT (r3r1, r2r0, tdep->int64);

  r3r2r1r0
    = add_reg (arch, "r3r2r1r0", tdep->int64, 0,
	       m32c_r3r2r1r0_read, m32c_r3r2r1r0_write, NULL, NULL, 0);

  if (mach == bfd_mach_m16c)
    a1a0 = CCAT (a1, a0, tdep->int32);
  else
    a1a0 = NULL;

  num_cooked_regs = tdep->num_regs - num_raw_regs;

  tdep->pc   	 = pc;
  tdep->flg  	 = flg;
  tdep->r0   	 = r0;
  tdep->r1   	 = r1;
  tdep->r2   	 = r2;
  tdep->r3   	 = r3;
  tdep->r2r0 	 = r2r0;
  tdep->r3r2r1r0 = r3r2r1r0;
  tdep->r3r1r2r0 = r3r1r2r0;
  tdep->a0       = a0;
  tdep->a1       = a1;
  tdep->sb       = sb;
  tdep->fb   	 = fb;
  tdep->sp   	 = sp;

  /* Set up the DWARF register table.  */
  memset (tdep->dwarf_regs, 0, sizeof (tdep->dwarf_regs));
  set_dwarf_regnum (r0hl + 1, 0x01);
  set_dwarf_regnum (r0hl + 0, 0x02);
  set_dwarf_regnum (r1hl + 1, 0x03);
  set_dwarf_regnum (r1hl + 0, 0x04);
  set_dwarf_regnum (r0,       0x05);
  set_dwarf_regnum (r1,       0x06);
  set_dwarf_regnum (r2,       0x07);
  set_dwarf_regnum (r3,       0x08);
  set_dwarf_regnum (a0,       0x09);
  set_dwarf_regnum (a1,       0x0a);
  set_dwarf_regnum (fb,       0x0b);
  set_dwarf_regnum (sp,       0x0c);
  set_dwarf_regnum (pc,       0x0d); /* GCC's invention */
  set_dwarf_regnum (sb,       0x13);
  set_dwarf_regnum (r2r0,     0x15);
  set_dwarf_regnum (r3r1,     0x16);
  if (a1a0)
    set_dwarf_regnum (a1a0,   0x17);

  /* Enumerate the save/restore register group.

     The regcache_save and regcache_restore functions apply their read
     function to each register in this group.

     Since frame_pop supplies frame_unwind_register as its read
     function, the registers meaningful to the Dwarf unwinder need to
     be in this group.

     On the other hand, when we make inferior calls, save_inferior_status
     and restore_inferior_status use them to preserve the current register
     values across the inferior call.  For this, you'd kind of like to
     preserve all the raw registers, to protect the interrupted code from
     any sort of bank switching the callee might have done.  But we handle
     those cases so badly anyway --- for example, it matters whether we
     restore FLG before or after we restore the general-purpose registers,
     but there's no way to express that --- that it isn't worth worrying
     about.

     We omit control registers like inthl: if you call a function that
     changes those, it's probably because you wanted that change to be
     visible to the interrupted code.  */
  mark_save_restore (r0);
  mark_save_restore (r1);
  mark_save_restore (r2);
  mark_save_restore (r3);
  mark_save_restore (a0);
  mark_save_restore (a1);
  mark_save_restore (sb);
  mark_save_restore (fb);
  mark_save_restore (sp);
  mark_save_restore (pc);
  mark_save_restore (flg);

  set_gdbarch_num_regs (arch, num_raw_regs);
  set_gdbarch_num_pseudo_regs (arch, num_cooked_regs);
  set_gdbarch_pc_regnum (arch, pc->num);
  set_gdbarch_sp_regnum (arch, sp->num);
  set_gdbarch_register_name (arch, m32c_register_name);
  set_gdbarch_register_type (arch, m32c_register_type);
  set_gdbarch_pseudo_register_read (arch, m32c_pseudo_register_read);
  set_gdbarch_pseudo_register_write (arch, m32c_pseudo_register_write);
  set_gdbarch_register_sim_regno (arch, m32c_register_sim_regno);
  set_gdbarch_stab_reg_to_regnum (arch, m32c_debug_info_reg_to_regnum);
  set_gdbarch_dwarf_reg_to_regnum (arch, m32c_debug_info_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (arch, m32c_debug_info_reg_to_regnum);
  set_gdbarch_register_reggroup_p (arch, m32c_register_reggroup_p);

  reggroup_add (arch, general_reggroup);
  reggroup_add (arch, all_reggroup);
  reggroup_add (arch, save_reggroup);
  reggroup_add (arch, restore_reggroup);
  reggroup_add (arch, system_reggroup);
  reggroup_add (arch, m32c_dma_reggroup);
}



/* Breakpoints.  */

static const unsigned char *
m32c_breakpoint_from_pc (CORE_ADDR *pc, int *len)
{
  static unsigned char break_insn[] = { 0x00 };	/* brk */

  *len = sizeof (break_insn);
  return break_insn;
}



/* Prologue analysis.  */

struct m32c_prologue
{
  /* For consistency with the DWARF 2 .debug_frame info generated by
     GCC, a frame's CFA is the address immediately after the saved
     return address.  */

  /* The architecture for which we generated this prologue info.  */
  struct gdbarch *arch;

  enum {
    /* This function uses a frame pointer.  */
    prologue_with_frame_ptr,

    /* This function has no frame pointer.  */
    prologue_sans_frame_ptr,

    /* This function sets up the stack, so its frame is the first
       frame on the stack.  */
    prologue_first_frame

  } kind;

  /* If KIND is prologue_with_frame_ptr, this is the offset from the
     CFA to where the frame pointer points.  This is always zero or
     negative.  */
  LONGEST frame_ptr_offset;

  /* If KIND is prologue_sans_frame_ptr, the offset from the CFA to
     the stack pointer --- always zero or negative.

     Calling this a "size" is a bit misleading, but given that the
     stack grows downwards, using offsets for everything keeps one
     from going completely sign-crazy: you never change anything's
     sign for an ADD instruction; always change the second operand's
     sign for a SUB instruction; and everything takes care of
     itself.

     Functions that use alloca don't have a constant frame size.  But
     they always have frame pointers, so we must use that to find the
     CFA (and perhaps to unwind the stack pointer).  */
  LONGEST frame_size;

  /* The address of the first instruction at which the frame has been
     set up and the arguments are where the debug info says they are
     --- as best as we can tell.  */
  CORE_ADDR prologue_end;

  /* reg_offset[R] is the offset from the CFA at which register R is
     saved, or 1 if register R has not been saved.  (Real values are
     always zero or negative.)  */
  LONGEST reg_offset[M32C_MAX_NUM_REGS];
};


/* The longest I've seen, anyway.  */
#define M32C_MAX_INSN_LEN (9)

/* Processor state, for the prologue analyzer.  */
struct m32c_pv_state
{
  struct gdbarch *arch;
  pv_t r0, r1, r2, r3;
  pv_t a0, a1;
  pv_t sb, fb, sp;
  pv_t pc;
  struct pv_area *stack;

  /* Bytes from the current PC, the address they were read from,
     and the address of the next unconsumed byte.  */
  gdb_byte insn[M32C_MAX_INSN_LEN];
  CORE_ADDR scan_pc, next_addr;
};


/* Push VALUE on STATE's stack, occupying SIZE bytes.  Return zero if
   all went well, or non-zero if simulating the action would trash our
   state.  */
static int
m32c_pv_push (struct m32c_pv_state *state, pv_t value, int size)
{
  if (pv_area_store_would_trash (state->stack, state->sp))
    return 1;

  state->sp = pv_add_constant (state->sp, -size);
  pv_area_store (state->stack, state->sp, size, value);

  return 0;
}


/* A source or destination location for an m16c or m32c
   instruction.  */
struct srcdest
{
  /* If srcdest_reg, the location is a register pointed to by REG.
     If srcdest_partial_reg, the location is part of a register pointed
     to by REG.  We don't try to handle this too well.
     If srcdest_mem, the location is memory whose address is ADDR.  */
  enum { srcdest_reg, srcdest_partial_reg, srcdest_mem } kind;
  pv_t *reg, addr;
};


/* Return the SIZE-byte value at LOC in STATE.  */
static pv_t
m32c_srcdest_fetch (struct m32c_pv_state *state, struct srcdest loc, int size)
{
  if (loc.kind == srcdest_mem)
    return pv_area_fetch (state->stack, loc.addr, size);
  else if (loc.kind == srcdest_partial_reg)
    return pv_unknown ();
  else
    return *loc.reg;
}


/* Write VALUE, a SIZE-byte value, to LOC in STATE.  Return zero if
   all went well, or non-zero if simulating the store would trash our
   state.  */
static int
m32c_srcdest_store (struct m32c_pv_state *state, struct srcdest loc,
		    pv_t value, int size)
{
  if (loc.kind == srcdest_mem)
    {
      if (pv_area_store_would_trash (state->stack, loc.addr))
	return 1;
      pv_area_store (state->stack, loc.addr, size, value);
    }
  else if (loc.kind == srcdest_partial_reg)
    *loc.reg = pv_unknown ();
  else
    *loc.reg = value;

  return 0;
}


static int
m32c_sign_ext (int v, int bits)
{
  int mask = 1 << (bits - 1);
  return (v ^ mask) - mask;
}

static unsigned int
m32c_next_byte (struct m32c_pv_state *st)
{
  gdb_assert (st->next_addr - st->scan_pc < sizeof (st->insn));
  return st->insn[st->next_addr++ - st->scan_pc];
}

static int
m32c_udisp8 (struct m32c_pv_state *st)
{
  return m32c_next_byte (st);
}


static int
m32c_sdisp8 (struct m32c_pv_state *st)
{
  return m32c_sign_ext (m32c_next_byte (st), 8);
}


static int
m32c_udisp16 (struct m32c_pv_state *st)
{
  int low  = m32c_next_byte (st);
  int high = m32c_next_byte (st);

  return low + (high << 8);
}


static int
m32c_sdisp16 (struct m32c_pv_state *st)
{
  int low  = m32c_next_byte (st);
  int high = m32c_next_byte (st);

  return m32c_sign_ext (low + (high << 8), 16);
}


static int
m32c_udisp24 (struct m32c_pv_state *st)
{
  int low  = m32c_next_byte (st);
  int mid  = m32c_next_byte (st);
  int high = m32c_next_byte (st);

  return low + (mid << 8) + (high << 16);
}


/* Extract the 'source' field from an m32c MOV.size:G-format instruction.  */
static int
m32c_get_src23 (unsigned char *i)
{
  return (((i[0] & 0x70) >> 2)
	  | ((i[1] & 0x30) >> 4));
}


/* Extract the 'dest' field from an m32c MOV.size:G-format instruction.  */
static int
m32c_get_dest23 (unsigned char *i)
{
  return (((i[0] & 0x0e) << 1)
	  | ((i[1] & 0xc0) >> 6));
}


static struct srcdest
m32c_decode_srcdest4 (struct m32c_pv_state *st,
		      int code, int size)
{
  struct srcdest sd;

  if (code < 6)
    sd.kind = (size == 2 ? srcdest_reg : srcdest_partial_reg);
  else
    sd.kind = srcdest_mem;

  sd.addr = pv_unknown ();
  sd.reg = 0;

  switch (code)
    {
    case 0x0: sd.reg = (size == 1 ? &st->r0 : &st->r0); break;
    case 0x1: sd.reg = (size == 1 ? &st->r0 : &st->r1); break;
    case 0x2: sd.reg = (size == 1 ? &st->r1 : &st->r2); break;
    case 0x3: sd.reg = (size == 1 ? &st->r1 : &st->r3); break;

    case 0x4: sd.reg = &st->a0; break;
    case 0x5: sd.reg = &st->a1; break;

    case 0x6: sd.addr = st->a0; break;
    case 0x7: sd.addr = st->a1; break;

    case 0x8: sd.addr = pv_add_constant (st->a0, m32c_udisp8 (st)); break;
    case 0x9: sd.addr = pv_add_constant (st->a1, m32c_udisp8 (st)); break;
    case 0xa: sd.addr = pv_add_constant (st->sb, m32c_udisp8 (st)); break;
    case 0xb: sd.addr = pv_add_constant (st->fb, m32c_sdisp8 (st)); break;

    case 0xc: sd.addr = pv_add_constant (st->a0, m32c_udisp16 (st)); break;
    case 0xd: sd.addr = pv_add_constant (st->a1, m32c_udisp16 (st)); break;
    case 0xe: sd.addr = pv_add_constant (st->sb, m32c_udisp16 (st)); break;
    case 0xf: sd.addr = pv_constant (m32c_udisp16 (st)); break;

    default:
      gdb_assert (0);
    }

  return sd;
}


static struct srcdest
m32c_decode_sd23 (struct m32c_pv_state *st, int code, int size, int ind)
{
  struct srcdest sd;

  sd.addr = pv_unknown ();
  sd.reg = 0;

  switch (code)
    {
    case 0x12:
    case 0x13:
    case 0x10:
    case 0x11:
      sd.kind = (size == 1) ? srcdest_partial_reg : srcdest_reg;
      break;

    case 0x02:
    case 0x03:
      sd.kind = (size == 4) ? srcdest_reg : srcdest_partial_reg;
      break;

    default:
      sd.kind = srcdest_mem;
      break;

    }

  switch (code)
    {
    case 0x12: sd.reg = &st->r0; break;
    case 0x13: sd.reg = &st->r1; break;
    case 0x10: sd.reg = ((size == 1) ? &st->r0 : &st->r2); break;
    case 0x11: sd.reg = ((size == 1) ? &st->r1 : &st->r3); break;
    case 0x02: sd.reg = &st->a0; break;
    case 0x03: sd.reg = &st->a1; break;

    case 0x00: sd.addr = st->a0; break;
    case 0x01: sd.addr = st->a1; break;
    case 0x04: sd.addr = pv_add_constant (st->a0, m32c_udisp8 (st)); break;
    case 0x05: sd.addr = pv_add_constant (st->a1, m32c_udisp8 (st)); break;
    case 0x06: sd.addr = pv_add_constant (st->sb, m32c_udisp8 (st)); break;
    case 0x07: sd.addr = pv_add_constant (st->fb, m32c_sdisp8 (st)); break;
    case 0x08: sd.addr = pv_add_constant (st->a0, m32c_udisp16 (st)); break;
    case 0x09: sd.addr = pv_add_constant (st->a1, m32c_udisp16 (st)); break;
    case 0x0a: sd.addr = pv_add_constant (st->sb, m32c_udisp16 (st)); break;
    case 0x0b: sd.addr = pv_add_constant (st->fb, m32c_sdisp16 (st)); break;
    case 0x0c: sd.addr = pv_add_constant (st->a0, m32c_udisp24 (st)); break;
    case 0x0d: sd.addr = pv_add_constant (st->a1, m32c_udisp24 (st)); break;
    case 0x0f: sd.addr = pv_constant (m32c_udisp16 (st)); break;
    case 0x0e: sd.addr = pv_constant (m32c_udisp24 (st)); break;
    default:
      gdb_assert (0);
    }

  if (ind)
    {
      sd.addr = m32c_srcdest_fetch (st, sd, 4);
      sd.kind = srcdest_mem;
    }

  return sd;
}


/* The r16c and r32c machines have instructions with similar
   semantics, but completely different machine language encodings.  So
   we break out the semantics into their own functions, and leave
   machine-specific decoding in m32c_analyze_prologue.

   The following functions all expect their arguments already decoded,
   and they all return zero if analysis should continue past this
   instruction, or non-zero if analysis should stop.  */


/* Simulate an 'enter SIZE' instruction in STATE.  */
static int
m32c_pv_enter (struct m32c_pv_state *state, int size)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (state->arch);

  /* If simulating this store would require us to forget
     everything we know about the stack frame in the name of
     accuracy, it would be better to just quit now.  */
  if (pv_area_store_would_trash (state->stack, state->sp))
    return 1;

  if (m32c_pv_push (state, state->fb, tdep->push_addr_bytes))
    return 1;
  state->fb = state->sp;
  state->sp = pv_add_constant (state->sp, -size);

  return 0;
}


static int
m32c_pv_pushm_one (struct m32c_pv_state *state, pv_t reg,
		   int bit, int src, int size)
{
  if (bit & src)
    {
      if (m32c_pv_push (state, reg, size))
	return 1;
    }

  return 0;
}


/* Simulate a 'pushm SRC' instruction in STATE.  */
static int
m32c_pv_pushm (struct m32c_pv_state *state, int src)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (state->arch);

  /* The bits in SRC indicating which registers to save are:
     r0 r1 r2 r3 a0 a1 sb fb */
  return
    (   m32c_pv_pushm_one (state, state->fb, 0x01, src, tdep->push_addr_bytes)
     || m32c_pv_pushm_one (state, state->sb, 0x02, src, tdep->push_addr_bytes)
     || m32c_pv_pushm_one (state, state->a1, 0x04, src, tdep->push_addr_bytes)
     || m32c_pv_pushm_one (state, state->a0, 0x08, src, tdep->push_addr_bytes)
     || m32c_pv_pushm_one (state, state->r3, 0x10, src, 2)
     || m32c_pv_pushm_one (state, state->r2, 0x20, src, 2)
     || m32c_pv_pushm_one (state, state->r1, 0x40, src, 2)
     || m32c_pv_pushm_one (state, state->r0, 0x80, src, 2));
}

/* Return non-zero if VALUE is the first incoming argument register.  */

static int
m32c_is_1st_arg_reg (struct m32c_pv_state *state, pv_t value)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (state->arch);
  return (value.kind == pvk_register
          && (gdbarch_bfd_arch_info (state->arch)->mach == bfd_mach_m16c
	      ? (value.reg == tdep->r1->num)
	      : (value.reg == tdep->r0->num))
          && value.k == 0);
}

/* Return non-zero if VALUE is an incoming argument register.  */

static int
m32c_is_arg_reg (struct m32c_pv_state *state, pv_t value)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (state->arch);
  return (value.kind == pvk_register
          && (gdbarch_bfd_arch_info (state->arch)->mach == bfd_mach_m16c
	      ? (value.reg == tdep->r1->num || value.reg == tdep->r2->num)
	      : (value.reg == tdep->r0->num))
          && value.k == 0);
}

/* Return non-zero if a store of VALUE to LOC is probably spilling an
   argument register to its stack slot in STATE.  Such instructions
   should be included in the prologue, if possible.

   The store is a spill if:
   - the value being stored is the original value of an argument register;
   - the value has not already been stored somewhere in STACK; and
   - LOC is a stack slot (e.g., a memory location whose address is
     relative to the original value of the SP).  */

static int
m32c_is_arg_spill (struct m32c_pv_state *st, 
		   struct srcdest loc, 
		   pv_t value)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (st->arch);

  return (m32c_is_arg_reg (st, value)
	  && loc.kind == srcdest_mem
          && pv_is_register (loc.addr, tdep->sp->num)
          && ! pv_area_find_reg (st->stack, st->arch, value.reg, 0));
}

/* Return non-zero if a store of VALUE to LOC is probably 
   copying the struct return address into an address register
   for immediate use.  This is basically a "spill" into the
   address register, instead of onto the stack. 

   The prerequisites are:
   - value being stored is original value of the FIRST arg register;
   - value has not already been stored on stack; and
   - LOC is an address register (a0 or a1).  */

static int
m32c_is_struct_return (struct m32c_pv_state *st,
		       struct srcdest loc, 
		       pv_t value)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (st->arch);

  return (m32c_is_1st_arg_reg (st, value)
	  && !pv_area_find_reg (st->stack, st->arch, value.reg, 0)
	  && loc.kind == srcdest_reg
	  && (pv_is_register (*loc.reg, tdep->a0->num)
	      || pv_is_register (*loc.reg, tdep->a1->num)));
}

/* Return non-zero if a 'pushm' saving the registers indicated by SRC
   was a register save:
   - all the named registers should have their original values, and
   - the stack pointer should be at a constant offset from the
     original stack pointer.  */
static int
m32c_pushm_is_reg_save (struct m32c_pv_state *st, int src)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (st->arch);
  /* The bits in SRC indicating which registers to save are:
     r0 r1 r2 r3 a0 a1 sb fb */
  return
    (pv_is_register (st->sp, tdep->sp->num)
     && (! (src & 0x01) || pv_is_register_k (st->fb, tdep->fb->num, 0))
     && (! (src & 0x02) || pv_is_register_k (st->sb, tdep->sb->num, 0))
     && (! (src & 0x04) || pv_is_register_k (st->a1, tdep->a1->num, 0))
     && (! (src & 0x08) || pv_is_register_k (st->a0, tdep->a0->num, 0))
     && (! (src & 0x10) || pv_is_register_k (st->r3, tdep->r3->num, 0))
     && (! (src & 0x20) || pv_is_register_k (st->r2, tdep->r2->num, 0))
     && (! (src & 0x40) || pv_is_register_k (st->r1, tdep->r1->num, 0))
     && (! (src & 0x80) || pv_is_register_k (st->r0, tdep->r0->num, 0)));
}


/* Function for finding saved registers in a 'struct pv_area'; we pass
   this to pv_area_scan.

   If VALUE is a saved register, ADDR says it was saved at a constant
   offset from the frame base, and SIZE indicates that the whole
   register was saved, record its offset in RESULT_UNTYPED.  */
static void
check_for_saved (void *prologue_untyped, pv_t addr, CORE_ADDR size, pv_t value)
{
  struct m32c_prologue *prologue = (struct m32c_prologue *) prologue_untyped;
  struct gdbarch *arch = prologue->arch;
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);

  /* Is this the unchanged value of some register being saved on the
     stack?  */
  if (value.kind == pvk_register
      && value.k == 0
      && pv_is_register (addr, tdep->sp->num))
    {
      /* Some registers require special handling: they're saved as a
	 larger value than the register itself.  */
      CORE_ADDR saved_size = register_size (arch, value.reg);

      if (value.reg == tdep->pc->num)
	saved_size = tdep->ret_addr_bytes;
      else if (register_type (arch, value.reg)
	       == tdep->data_addr_reg_type)
	saved_size = tdep->push_addr_bytes;

      if (size == saved_size)
	{
	  /* Find which end of the saved value corresponds to our
	     register.  */
	  if (gdbarch_byte_order (arch) == BFD_ENDIAN_BIG)
	    prologue->reg_offset[value.reg]
	      = (addr.k + saved_size - register_size (arch, value.reg));
	  else
	    prologue->reg_offset[value.reg] = addr.k;
	}
    }
}


/* Analyze the function prologue for ARCH at START, going no further
   than LIMIT, and place a description of what we found in
   PROLOGUE.  */
void
m32c_analyze_prologue (struct gdbarch *arch,
		       CORE_ADDR start, CORE_ADDR limit,
		       struct m32c_prologue *prologue)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  unsigned long mach = gdbarch_bfd_arch_info (arch)->mach;
  CORE_ADDR after_last_frame_related_insn;
  struct cleanup *back_to;
  struct m32c_pv_state st;

  st.arch = arch;
  st.r0 = pv_register (tdep->r0->num, 0);
  st.r1 = pv_register (tdep->r1->num, 0);
  st.r2 = pv_register (tdep->r2->num, 0);
  st.r3 = pv_register (tdep->r3->num, 0);
  st.a0 = pv_register (tdep->a0->num, 0);
  st.a1 = pv_register (tdep->a1->num, 0);
  st.sb = pv_register (tdep->sb->num, 0);
  st.fb = pv_register (tdep->fb->num, 0);
  st.sp = pv_register (tdep->sp->num, 0);
  st.pc = pv_register (tdep->pc->num, 0);
  st.stack = make_pv_area (tdep->sp->num);
  back_to = make_cleanup_free_pv_area (st.stack);

  /* Record that the call instruction has saved the return address on
     the stack.  */
  m32c_pv_push (&st, st.pc, tdep->ret_addr_bytes);

  memset (prologue, 0, sizeof (*prologue));
  prologue->arch = arch;
  {
    int i;
    for (i = 0; i < M32C_MAX_NUM_REGS; i++)
      prologue->reg_offset[i] = 1;
  }

  st.scan_pc = after_last_frame_related_insn = start;

  while (st.scan_pc < limit)
    {
      pv_t pre_insn_fb = st.fb;
      pv_t pre_insn_sp = st.sp;

      /* In theory we could get in trouble by trying to read ahead
	 here, when we only know we're expecting one byte.  In
	 practice I doubt anyone will care, and it makes the rest of
	 the code easier.  */
      if (target_read_memory (st.scan_pc, st.insn, sizeof (st.insn)))
	/* If we can't fetch the instruction from memory, stop here
	   and hope for the best.  */
	break;
      st.next_addr = st.scan_pc;

      /* The assembly instructions are written as they appear in the
	 section of the processor manuals that describe the
	 instruction encodings.

	 When a single assembly language instruction has several
	 different machine-language encodings, the manual
	 distinguishes them by a number in parens, before the
	 mnemonic.  Those numbers are included, as well.

	 The srcdest decoding instructions have the same names as the
	 analogous functions in the simulator.  */
      if (mach == bfd_mach_m16c)
	{
	  /* (1) ENTER #imm8 */
	  if (st.insn[0] == 0x7c && st.insn[1] == 0xf2)
	    {
	      if (m32c_pv_enter (&st, st.insn[2]))
		break;
	      st.next_addr += 3;
	    }
	  /* (1) PUSHM src */
	  else if (st.insn[0] == 0xec)
	    {
	      int src = st.insn[1];
	      if (m32c_pv_pushm (&st, src))
		break;
	      st.next_addr += 2;

	      if (m32c_pushm_is_reg_save (&st, src))
		after_last_frame_related_insn = st.next_addr;
	    }

	  /* (6) MOV.size:G src, dest */
	  else if ((st.insn[0] & 0xfe) == 0x72)
	    {
	      int size = (st.insn[0] & 0x01) ? 2 : 1;
	      struct srcdest src;
	      struct srcdest dest;
	      pv_t src_value;
	      st.next_addr += 2;

	      src
		= m32c_decode_srcdest4 (&st, (st.insn[1] >> 4) & 0xf, size);
	      dest
		= m32c_decode_srcdest4 (&st, st.insn[1] & 0xf, size);
	      src_value = m32c_srcdest_fetch (&st, src, size);

	      if (m32c_is_arg_spill (&st, dest, src_value))
		after_last_frame_related_insn = st.next_addr;
	      else if (m32c_is_struct_return (&st, dest, src_value))
		after_last_frame_related_insn = st.next_addr;

	      if (m32c_srcdest_store (&st, dest, src_value, size))
		break;
	    }

	  /* (1) LDC #IMM16, sp */
	  else if (st.insn[0] == 0xeb
		   && st.insn[1] == 0x50)
	    {
	      st.next_addr += 2;
	      st.sp = pv_constant (m32c_udisp16 (&st));
	    }

	  else
	    /* We've hit some instruction we don't know how to simulate.
	       Strictly speaking, we should set every value we're
	       tracking to "unknown".  But we'll be optimistic, assume
	       that we have enough information already, and stop
	       analysis here.  */
	    break;
	}
      else
	{
	  int src_indirect = 0;
	  int dest_indirect = 0;
	  int i = 0;

	  gdb_assert (mach == bfd_mach_m32c);

	  /* Check for prefix bytes indicating indirect addressing.  */
	  if (st.insn[0] == 0x41)
	    {
	      src_indirect = 1;
	      i++;
	    }
	  else if (st.insn[0] == 0x09)
	    {
	      dest_indirect = 1;
	      i++;
	    }
	  else if (st.insn[0] == 0x49)
	    {
	      src_indirect = dest_indirect = 1;
	      i++;
	    }

	  /* (1) ENTER #imm8 */
	  if (st.insn[i] == 0xec)
	    {
	      if (m32c_pv_enter (&st, st.insn[i + 1]))
		break;
	      st.next_addr += 2;
	    }

	  /* (1) PUSHM src */
	  else if (st.insn[i] == 0x8f)
	    {
	      int src = st.insn[i + 1];
	      if (m32c_pv_pushm (&st, src))
		break;
	      st.next_addr += 2;

	      if (m32c_pushm_is_reg_save (&st, src))
		after_last_frame_related_insn = st.next_addr;
	    }

	  /* (7) MOV.size:G src, dest */
	  else if ((st.insn[i] & 0x80) == 0x80
		   && (st.insn[i + 1] & 0x0f) == 0x0b
		   && m32c_get_src23 (&st.insn[i]) < 20
		   && m32c_get_dest23 (&st.insn[i]) < 20)
	    {
	      struct srcdest src;
	      struct srcdest dest;
	      pv_t src_value;
	      int bw = st.insn[i] & 0x01;
	      int size = bw ? 2 : 1;
	      st.next_addr += 2;

	      src
		= m32c_decode_sd23 (&st, m32c_get_src23 (&st.insn[i]),
				    size, src_indirect);
	      dest
		= m32c_decode_sd23 (&st, m32c_get_dest23 (&st.insn[i]),
				    size, dest_indirect);
	      src_value = m32c_srcdest_fetch (&st, src, size);

	      if (m32c_is_arg_spill (&st, dest, src_value))
		after_last_frame_related_insn = st.next_addr;

	      if (m32c_srcdest_store (&st, dest, src_value, size))
		break;
	    }
	  /* (2) LDC #IMM24, sp */
	  else if (st.insn[i] == 0xd5
		   && st.insn[i + 1] == 0x29)
	    {
	      st.next_addr += 2;
	      st.sp = pv_constant (m32c_udisp24 (&st));
	    }
	  else
	    /* We've hit some instruction we don't know how to simulate.
	       Strictly speaking, we should set every value we're
	       tracking to "unknown".  But we'll be optimistic, assume
	       that we have enough information already, and stop
	       analysis here.  */
	    break;
	}

      /* If this instruction changed the FB or decreased the SP (i.e.,
         allocated more stack space), then this may be a good place to
         declare the prologue finished.  However, there are some
         exceptions:

         - If the instruction just changed the FB back to its original
           value, then that's probably a restore instruction.  The
           prologue should definitely end before that.

         - If the instruction increased the value of the SP (that is,
           shrunk the frame), then it's probably part of a frame
           teardown sequence, and the prologue should end before
           that.  */

      if (! pv_is_identical (st.fb, pre_insn_fb))
        {
          if (! pv_is_register_k (st.fb, tdep->fb->num, 0))
            after_last_frame_related_insn = st.next_addr;
        }
      else if (! pv_is_identical (st.sp, pre_insn_sp))
        {
          /* The comparison of the constants looks odd, there, because
             .k is unsigned.  All it really means is that the SP is
             lower than it was before the instruction.  */
          if (   pv_is_register (pre_insn_sp, tdep->sp->num)
              && pv_is_register (st.sp,       tdep->sp->num)
              && ((pre_insn_sp.k - st.sp.k) < (st.sp.k - pre_insn_sp.k)))
            after_last_frame_related_insn = st.next_addr;
        }

      st.scan_pc = st.next_addr;
    }

  /* Did we load a constant value into the stack pointer?  */
  if (pv_is_constant (st.sp))
    prologue->kind = prologue_first_frame;

  /* Alternatively, did we initialize the frame pointer?  Remember
     that the CFA is the address after the return address.  */
  if (pv_is_register (st.fb, tdep->sp->num))
    {
      prologue->kind = prologue_with_frame_ptr;
      prologue->frame_ptr_offset = st.fb.k;
    }

  /* Is the frame size a known constant?  Remember that frame_size is
     actually the offset from the CFA to the SP (i.e., a negative
     value).  */
  else if (pv_is_register (st.sp, tdep->sp->num))
    {
      prologue->kind = prologue_sans_frame_ptr;
      prologue->frame_size = st.sp.k;
    }

  /* We haven't been able to make sense of this function's frame.  Treat
     it as the first frame.  */
  else
    prologue->kind = prologue_first_frame;

  /* Record where all the registers were saved.  */
  pv_area_scan (st.stack, check_for_saved, (void *) prologue);

  prologue->prologue_end = after_last_frame_related_insn;

  do_cleanups (back_to);
}


static CORE_ADDR
m32c_skip_prologue (CORE_ADDR ip)
{
  char *name;
  CORE_ADDR func_addr, func_end, sal_end;
  struct m32c_prologue p;

  /* Try to find the extent of the function that contains IP.  */
  if (! find_pc_partial_function (ip, &name, &func_addr, &func_end))
    return ip;

  /* Find end by prologue analysis.  */
  m32c_analyze_prologue (current_gdbarch, ip, func_end, &p);
  /* Find end by line info.  */
  sal_end = skip_prologue_using_sal (ip);
  /* Return whichever is lower.  */
  if (sal_end != 0 && sal_end != ip && sal_end < p.prologue_end)
    return sal_end;
  else
    return p.prologue_end;
}



/* Stack unwinding.  */

static struct m32c_prologue *
m32c_analyze_frame_prologue (struct frame_info *next_frame,
			     void **this_prologue_cache)
{
  if (! *this_prologue_cache)
    {
      CORE_ADDR func_start = frame_func_unwind (next_frame, NORMAL_FRAME);
      CORE_ADDR stop_addr = frame_pc_unwind (next_frame);

      /* If we couldn't find any function containing the PC, then
         just initialize the prologue cache, but don't do anything.  */
      if (! func_start)
        stop_addr = func_start;

      *this_prologue_cache = FRAME_OBSTACK_ZALLOC (struct m32c_prologue);
      m32c_analyze_prologue (get_frame_arch (next_frame),
			     func_start, stop_addr, *this_prologue_cache);
    }

  return *this_prologue_cache;
}


static CORE_ADDR
m32c_frame_base (struct frame_info *next_frame,
                void **this_prologue_cache)
{
  struct m32c_prologue *p
    = m32c_analyze_frame_prologue (next_frame, this_prologue_cache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_frame_arch (next_frame));

  /* In functions that use alloca, the distance between the stack
     pointer and the frame base varies dynamically, so we can't use
     the SP plus static information like prologue analysis to find the
     frame base.  However, such functions must have a frame pointer,
     to be able to restore the SP on exit.  So whenever we do have a
     frame pointer, use that to find the base.  */
  switch (p->kind)
    {
    case prologue_with_frame_ptr:
      {
	CORE_ADDR fb
	  = frame_unwind_register_unsigned (next_frame, tdep->fb->num);
	return fb - p->frame_ptr_offset;
      }

    case prologue_sans_frame_ptr:
      {
	CORE_ADDR sp
	  = frame_unwind_register_unsigned (next_frame, tdep->sp->num);
	return sp - p->frame_size;
      }

    case prologue_first_frame:
      return 0;

    default:
      gdb_assert (0);
    }
}


static void
m32c_this_id (struct frame_info *next_frame,
	      void **this_prologue_cache,
	      struct frame_id *this_id)
{
  CORE_ADDR base = m32c_frame_base (next_frame, this_prologue_cache);

  if (base)
    *this_id = frame_id_build (base,
			       frame_func_unwind (next_frame, NORMAL_FRAME));
  /* Otherwise, leave it unset, and that will terminate the backtrace.  */
}


static void
m32c_prev_register (struct frame_info *next_frame,
		    void **this_prologue_cache,
		    int regnum, int *optimizedp,
		    enum lval_type *lvalp, CORE_ADDR *addrp,
		    int *realnump, gdb_byte *bufferp)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_frame_arch (next_frame));
  struct m32c_prologue *p
    = m32c_analyze_frame_prologue (next_frame, this_prologue_cache);
  CORE_ADDR frame_base = m32c_frame_base (next_frame, this_prologue_cache);
  int reg_size = register_size (get_frame_arch (next_frame), regnum);

  if (regnum == tdep->sp->num)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (bufferp)
	store_unsigned_integer (bufferp, reg_size, frame_base);
    }

  /* If prologue analysis says we saved this register somewhere,
     return a description of the stack slot holding it.  */
  else if (p->reg_offset[regnum] != 1)
    {
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = frame_base + p->reg_offset[regnum];
      *realnump = -1;
      if (bufferp)
	get_frame_memory (next_frame, *addrp, bufferp, reg_size);
    }

  /* Otherwise, presume we haven't changed the value of this
     register, and get it from the next frame.  */
  else
    {
      *optimizedp = 0;
      *lvalp = lval_register;
      *addrp = 0;
      *realnump = regnum;
      if (bufferp)
	frame_unwind_register (next_frame, *realnump, bufferp);
    }
}


static const struct frame_unwind m32c_unwind = {
  NORMAL_FRAME,
  m32c_this_id,
  m32c_prev_register
};


static const struct frame_unwind *
m32c_frame_sniffer (struct frame_info *next_frame)
{
  return &m32c_unwind;
}


static CORE_ADDR
m32c_unwind_pc (struct gdbarch *arch, struct frame_info *next_frame)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  return frame_unwind_register_unsigned (next_frame, tdep->pc->num);
}


static CORE_ADDR
m32c_unwind_sp (struct gdbarch *arch, struct frame_info *next_frame)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  return frame_unwind_register_unsigned (next_frame, tdep->sp->num);
}


/* Inferior calls.  */

/* The calling conventions, according to GCC:

   r8c, m16c
   ---------
   First arg may be passed in r1l or r1 if it (1) fits (QImode or
   HImode), (2) is named, and (3) is an integer or pointer type (no
   structs, floats, etc).  Otherwise, it's passed on the stack.

   Second arg may be passed in r2, same restrictions (but not QImode),
   even if the first arg is passed on the stack.

   Third and further args are passed on the stack.  No padding is
   used, stack "alignment" is 8 bits.

   m32cm, m32c
   -----------

   First arg may be passed in r0l or r0, same restrictions as above.

   Second and further args are passed on the stack.  Padding is used
   after QImode parameters (i.e. lower-addressed byte is the value,
   higher-addressed byte is the padding), stack "alignment" is 16
   bits.  */


/* Return true if TYPE is a type that can be passed in registers.  (We
   ignore the size, and pay attention only to the type code;
   acceptable sizes depends on which register is being considered to
   hold it.)  */
static int
m32c_reg_arg_type (struct type *type)
{
  enum type_code code = TYPE_CODE (type);

  return (code == TYPE_CODE_INT
	  || code == TYPE_CODE_ENUM
	  || code == TYPE_CODE_PTR
	  || code == TYPE_CODE_REF
	  || code == TYPE_CODE_BOOL
	  || code == TYPE_CODE_CHAR);
}


static CORE_ADDR
m32c_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  unsigned long mach = gdbarch_bfd_arch_info (gdbarch)->mach;
  CORE_ADDR cfa;
  int i;

  /* The number of arguments given in this function's prototype, or
     zero if it has a non-prototyped function type.  The m32c ABI
     passes arguments mentioned in the prototype differently from
     those in the ellipsis of a varargs function, or from those passed
     to a non-prototyped function.  */
  int num_prototyped_args = 0;

  {
    struct type *func_type = value_type (function);

    gdb_assert (TYPE_CODE (func_type) == TYPE_CODE_FUNC ||
		TYPE_CODE (func_type) == TYPE_CODE_METHOD);

#if 0
    /* The ABI description in gcc/config/m32c/m32c.abi says that
       we need to handle prototyped and non-prototyped functions
       separately, but the code in GCC doesn't actually do so.  */
    if (TYPE_PROTOTYPED (func_type))
#endif
      num_prototyped_args = TYPE_NFIELDS (func_type);
  }

  /* First, if the function returns an aggregate by value, push a
     pointer to a buffer for it.  This doesn't affect the way
     subsequent arguments are allocated to registers.  */
  if (struct_return)
    {
      int ptr_len = TYPE_LENGTH (tdep->ptr_voyd);
      sp -= ptr_len;
      write_memory_unsigned_integer (sp, ptr_len, struct_addr);
    }

  /* Push the arguments.  */
  for (i = nargs - 1; i >= 0; i--)
    {
      struct value *arg = args[i];
      const gdb_byte *arg_bits = value_contents (arg);
      struct type *arg_type = value_type (arg);
      ULONGEST arg_size = TYPE_LENGTH (arg_type);

      /* Can it go in r1 or r1l (for m16c) or r0 or r0l (for m32c)?  */
      if (i == 0
	  && arg_size <= 2
	  && i < num_prototyped_args
	  && m32c_reg_arg_type (arg_type))
	{
	  /* Extract and re-store as an integer as a terse way to make
	     sure it ends up in the least significant end of r1.  (GDB
	     should avoid assuming endianness, even on uni-endian
	     processors.)  */
	  ULONGEST u = extract_unsigned_integer (arg_bits, arg_size);
	  struct m32c_reg *reg = (mach == bfd_mach_m16c) ? tdep->r1 : tdep->r0;
	  regcache_cooked_write_unsigned (regcache, reg->num, u);
	}

      /* Can it go in r2?  */
      else if (mach == bfd_mach_m16c
	       && i == 1
	       && arg_size == 2
	       && i < num_prototyped_args
	       && m32c_reg_arg_type (arg_type))
	regcache_cooked_write (regcache, tdep->r2->num, arg_bits);

      /* Everything else goes on the stack.  */
      else
	{
	  sp -= arg_size;

	  /* Align the stack.  */
	  if (mach == bfd_mach_m32c)
	    sp &= ~1;

	  write_memory (sp, arg_bits, arg_size);
	}
    }

  /* This is the CFA we use to identify the dummy frame.  */
  cfa = sp;

  /* Push the return address.  */
  sp -= tdep->ret_addr_bytes;
  write_memory_unsigned_integer (sp, tdep->ret_addr_bytes, bp_addr);

  /* Update the stack pointer.  */
  regcache_cooked_write_unsigned (regcache, tdep->sp->num, sp);

  /* We need to borrow an odd trick from the i386 target here.

     The value we return from this function gets used as the stack
     address (the CFA) for the dummy frame's ID.  The obvious thing is
     to return the new TOS.  However, that points at the return
     address, saved on the stack, which is inconsistent with the CFA's
     described by GCC's DWARF 2 .debug_frame information: DWARF 2
     .debug_frame info uses the address immediately after the saved
     return address.  So you end up with a dummy frame whose CFA
     points at the return address, but the frame for the function
     being called has a CFA pointing after the return address: the
     younger CFA is *greater than* the older CFA.  The sanity checks
     in frame.c don't like that.

     So we try to be consistent with the CFA's used by DWARF 2.
     Having a dummy frame and a real frame with the *same* CFA is
     tolerable.  */
  return cfa;
}


static struct frame_id
m32c_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  /* This needs to return a frame ID whose PC is the return address
     passed to m32c_push_dummy_call, and whose stack_addr is the SP
     m32c_push_dummy_call returned.

     m32c_unwind_sp gives us the CFA, which is the value the SP had
     before the return address was pushed.  */
  return frame_id_build (m32c_unwind_sp (gdbarch, next_frame),
                         frame_pc_unwind (next_frame));
}



/* Return values.  */

/* Return value conventions, according to GCC:

   r8c, m16c
   ---------

   QImode in r0l
   HImode in r0
   SImode in r2r0
   near pointer in r0
   far pointer in r2r0

   Aggregate values (regardless of size) are returned by pushing a
   pointer to a temporary area on the stack after the args are pushed.
   The function fills in this area with the value.  Note that this
   pointer on the stack does not affect how register arguments, if any,
   are configured.

   m32cm, m32c
   -----------
   Same.  */

/* Return non-zero if values of type TYPE are returned by storing them
   in a buffer whose address is passed on the stack, ahead of the
   other arguments.  */
static int
m32c_return_by_passed_buf (struct type *type)
{
  enum type_code code = TYPE_CODE (type);

  return (code == TYPE_CODE_STRUCT
	  || code == TYPE_CODE_UNION);
}

static enum return_value_convention
m32c_return_value (struct gdbarch *gdbarch,
		   struct type *valtype,
		   struct regcache *regcache,
		   gdb_byte *readbuf,
		   const gdb_byte *writebuf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  enum return_value_convention conv;
  ULONGEST valtype_len = TYPE_LENGTH (valtype);

  if (m32c_return_by_passed_buf (valtype))
    conv = RETURN_VALUE_STRUCT_CONVENTION;
  else
    conv = RETURN_VALUE_REGISTER_CONVENTION;

  if (readbuf)
    {
      /* We should never be called to find values being returned by
	 RETURN_VALUE_STRUCT_CONVENTION.  Those can't be located,
	 unless we made the call ourselves.  */
      gdb_assert (conv == RETURN_VALUE_REGISTER_CONVENTION);

      gdb_assert (valtype_len <= 8);

      /* Anything that fits in r0 is returned there.  */
      if (valtype_len <= TYPE_LENGTH (tdep->r0->type))
	{
	  ULONGEST u;
	  regcache_cooked_read_unsigned (regcache, tdep->r0->num, &u);
	  store_unsigned_integer (readbuf, valtype_len, u);
	}
      else
	{
	  /* Everything else is passed in mem0, using as many bytes as
	     needed.  This is not what the Renesas tools do, but it's
	     what GCC does at the moment.  */
	  struct minimal_symbol *mem0
	    = lookup_minimal_symbol ("mem0", NULL, NULL);

	  if (! mem0)
	    error ("The return value is stored in memory at 'mem0', "
		   "but GDB cannot find\n"
		   "its address.");
	  read_memory (SYMBOL_VALUE_ADDRESS (mem0), readbuf, valtype_len);
	}
    }

  if (writebuf)
    {
      /* We should never be called to store values to be returned
	 using RETURN_VALUE_STRUCT_CONVENTION.  We have no way of
	 finding the buffer, unless we made the call ourselves.  */
      gdb_assert (conv == RETURN_VALUE_REGISTER_CONVENTION);

      gdb_assert (valtype_len <= 8);

      /* Anything that fits in r0 is returned there.  */
      if (valtype_len <= TYPE_LENGTH (tdep->r0->type))
	{
	  ULONGEST u = extract_unsigned_integer (writebuf, valtype_len);
	  regcache_cooked_write_unsigned (regcache, tdep->r0->num, u);
	}
      else
	{
	  /* Everything else is passed in mem0, using as many bytes as
	     needed.  This is not what the Renesas tools do, but it's
	     what GCC does at the moment.  */
	  struct minimal_symbol *mem0
	    = lookup_minimal_symbol ("mem0", NULL, NULL);

	  if (! mem0)
	    error ("The return value is stored in memory at 'mem0', "
		   "but GDB cannot find\n"
		   " its address.");
	  write_memory (SYMBOL_VALUE_ADDRESS (mem0),
                        (char *) writebuf, valtype_len);
	}
    }

  return conv;
}



/* Trampolines.  */

/* The m16c and m32c use a trampoline function for indirect function
   calls.  An indirect call looks like this:

	     ... push arguments ...
	     ... push target function address ...
	     jsr.a m32c_jsri16

   The code for m32c_jsri16 looks like this:

     m32c_jsri16:

             # Save return address.
	     pop.w	m32c_jsri_ret
	     pop.b	m32c_jsri_ret+2

             # Store target function address.
	     pop.w	m32c_jsri_addr

	     # Re-push return address.
	     push.b	m32c_jsri_ret+2
	     push.w	m32c_jsri_ret

	     # Call the target function.
	     jmpi.a	m32c_jsri_addr

   Without further information, GDB will treat calls to m32c_jsri16
   like calls to any other function.  Since m32c_jsri16 doesn't have
   debugging information, that normally means that GDB sets a step-
   resume breakpoint and lets the program continue --- which is not
   what the user wanted.  (Giving the trampoline debugging info
   doesn't help: the user expects the program to stop in the function
   their program is calling, not in some trampoline code they've never
   seen before.)

   The gdbarch_skip_trampoline_code method tells GDB how to step
   through such trampoline functions transparently to the user.  When
   given the address of a trampoline function's first instruction,
   gdbarch_skip_trampoline_code should return the address of the first
   instruction of the function really being called.  If GDB decides it
   wants to step into that function, it will set a breakpoint there
   and silently continue to it.

   We recognize the trampoline by name, and extract the target address
   directly from the stack.  This isn't great, but recognizing by its
   code sequence seems more fragile.  */

static CORE_ADDR
m32c_skip_trampoline_code (struct frame_info *frame, CORE_ADDR stop_pc)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  /* It would be nicer to simply look up the addresses of known
     trampolines once, and then compare stop_pc with them.  However,
     we'd need to ensure that that cached address got invalidated when
     someone loaded a new executable, and I'm not quite sure of the
     best way to do that.  find_pc_partial_function does do some
     caching, so we'll see how this goes.  */
  char *name;
  CORE_ADDR start, end;

  if (find_pc_partial_function (stop_pc, &name, &start, &end))
    {
      /* Are we stopped at the beginning of the trampoline function?  */
      if (strcmp (name, "m32c_jsri16") == 0
	  && stop_pc == start)
	{
	  /* Get the stack pointer.  The return address is at the top,
	     and the target function's address is just below that.  We
	     know it's a two-byte address, since the trampoline is
	     m32c_jsri*16*.  */
	  CORE_ADDR sp = get_frame_sp (get_current_frame ());
	  CORE_ADDR target
	    = read_memory_unsigned_integer (sp + tdep->ret_addr_bytes, 2);

	  /* What we have now is the address of a jump instruction.
	     What we need is the destination of that jump.
	     The opcode is 1 byte, and the destination is the next 3 bytes.
	  */
	  target = read_memory_unsigned_integer (target + 1, 3);
	  return target;
	}
    }

  return 0;
}


/* Address/pointer conversions.  */

/* On the m16c, there is a 24-bit address space, but only a very few
   instructions can generate addresses larger than 0xffff: jumps,
   jumps to subroutines, and the lde/std (load/store extended)
   instructions.

   Since GCC can only support one size of pointer, we can't have
   distinct 'near' and 'far' pointer types; we have to pick one size
   for everything.  If we wanted to use 24-bit pointers, then GCC
   would have to use lde and ste for all memory references, which
   would be terrible for performance and code size.  So the GNU
   toolchain uses 16-bit pointers for everything, and gives up the
   ability to have pointers point outside the first 64k of memory.

   However, as a special hack, we let the linker place functions at
   addresses above 0xffff, as long as it also places a trampoline in
   the low 64k for every function whose address is taken.  Each
   trampoline consists of a single jmp.a instruction that jumps to the
   function's real entry point.  Pointers to functions can be 16 bits
   long, even though the functions themselves are at higher addresses:
   the pointers refer to the trampolines, not the functions.

   This complicates things for GDB, however: given the address of a
   function (from debug info or linker symbols, say) which could be
   anywhere in the 24-bit address space, how can we find an
   appropriate 16-bit value to use as a pointer to it?

   If the linker has not generated a trampoline for the function,
   we're out of luck.  Well, I guess we could malloc some space and
   write a jmp.a instruction to it, but I'm not going to get into that
   at the moment.

   If the linker has generated a trampoline for the function, then it
   also emitted a symbol for the trampoline: if the function's linker
   symbol is named NAME, then the function's trampoline's linker
   symbol is named NAME.plt.

   So, given a code address:
   - We try to find a linker symbol at that address.
   - If we find such a symbol named NAME, we look for a linker symbol
     named NAME.plt.
   - If we find such a symbol, we assume it is a trampoline, and use
     its address as the pointer value.

   And, given a function pointer:
   - We try to find a linker symbol at that address named NAME.plt.
   - If we find such a symbol, we look for a linker symbol named NAME.
   - If we find that, we provide that as the function's address.
   - If any of the above steps fail, we return the original address
     unchanged; it might really be a function in the low 64k.

   See?  You *knew* there was a reason you wanted to be a computer
   programmer!  :)  */

static void
m32c_m16c_address_to_pointer (struct type *type, gdb_byte *buf, CORE_ADDR addr)
{
  enum type_code target_code;
  gdb_assert (TYPE_CODE (type) == TYPE_CODE_PTR ||
	      TYPE_CODE (type) == TYPE_CODE_REF);

  target_code = TYPE_CODE (TYPE_TARGET_TYPE (type));

  if (target_code == TYPE_CODE_FUNC || target_code == TYPE_CODE_METHOD)
    {
      char *func_name;
      char *tramp_name;
      struct minimal_symbol *tramp_msym;

      /* Try to find a linker symbol at this address.  */
      struct minimal_symbol *func_msym = lookup_minimal_symbol_by_pc (addr);

      if (! func_msym)
        error ("Cannot convert code address %s to function pointer:\n"
               "couldn't find a symbol at that address, to find trampoline.",
               paddr_nz (addr));

      func_name = SYMBOL_LINKAGE_NAME (func_msym);
      tramp_name = xmalloc (strlen (func_name) + 5);
      strcpy (tramp_name, func_name);
      strcat (tramp_name, ".plt");

      /* Try to find a linker symbol for the trampoline.  */
      tramp_msym = lookup_minimal_symbol (tramp_name, NULL, NULL);

      /* We've either got another copy of the name now, or don't need
         the name any more.  */
      xfree (tramp_name);

      if (! tramp_msym)
        error ("Cannot convert code address %s to function pointer:\n"
               "couldn't find trampoline named '%s.plt'.",
               paddr_nz (addr), func_name);

      /* The trampoline's address is our pointer.  */
      addr = SYMBOL_VALUE_ADDRESS (tramp_msym);
    }

  store_unsigned_integer (buf, TYPE_LENGTH (type), addr);
}


static CORE_ADDR
m32c_m16c_pointer_to_address (struct type *type, const gdb_byte *buf)
{
  CORE_ADDR ptr;
  enum type_code target_code;

  gdb_assert (TYPE_CODE (type) == TYPE_CODE_PTR ||
	      TYPE_CODE (type) == TYPE_CODE_REF);

  ptr = extract_unsigned_integer (buf, TYPE_LENGTH (type));

  target_code = TYPE_CODE (TYPE_TARGET_TYPE (type));

  if (target_code == TYPE_CODE_FUNC || target_code == TYPE_CODE_METHOD)
    {
      /* See if there is a minimal symbol at that address whose name is
         "NAME.plt".  */
      struct minimal_symbol *ptr_msym = lookup_minimal_symbol_by_pc (ptr);

      if (ptr_msym)
        {
          char *ptr_msym_name = SYMBOL_LINKAGE_NAME (ptr_msym);
          int len = strlen (ptr_msym_name);

          if (len > 4
              && strcmp (ptr_msym_name + len - 4, ".plt") == 0)
            {
	      struct minimal_symbol *func_msym;
              /* We have a .plt symbol; try to find the symbol for the
                 corresponding function.

                 Since the trampoline contains a jump instruction, we
                 could also just extract the jump's target address.  I
                 don't see much advantage one way or the other.  */
              char *func_name = xmalloc (len - 4 + 1);
              memcpy (func_name, ptr_msym_name, len - 4);
              func_name[len - 4] = '\0';
              func_msym
                = lookup_minimal_symbol (func_name, NULL, NULL);

              /* If we do have such a symbol, return its value as the
                 function's true address.  */
              if (func_msym)
                ptr = SYMBOL_VALUE_ADDRESS (func_msym);
            }
        }
    }

  return ptr;
}

void
m32c_virtual_frame_pointer (CORE_ADDR pc,
			    int *frame_regnum,
			    LONGEST *frame_offset)
{
  char *name;
  CORE_ADDR func_addr, func_end, sal_end;
  struct m32c_prologue p;

  struct regcache *regcache = get_current_regcache ();
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  if (!find_pc_partial_function (pc, &name, &func_addr, &func_end))
    internal_error (__FILE__, __LINE__, _("No virtual frame pointer available"));

  m32c_analyze_prologue (current_gdbarch, func_addr, pc, &p);
  switch (p.kind)
    {
    case prologue_with_frame_ptr:
      *frame_regnum = m32c_banked_register (tdep->fb, regcache)->num;
      *frame_offset = p.frame_ptr_offset;
      break;
    case prologue_sans_frame_ptr:
      *frame_regnum = m32c_banked_register (tdep->sp, regcache)->num;
      *frame_offset = p.frame_size;
      break;
    default:
      *frame_regnum = m32c_banked_register (tdep->sp, regcache)->num;
      *frame_offset = 0;
      break;
    }
  /* Sanity check */
  if (*frame_regnum > gdbarch_num_regs (current_gdbarch))
    internal_error (__FILE__, __LINE__, _("No virtual frame pointer available"));
}


/* Initialization.  */

static struct gdbarch *
m32c_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *arch;
  struct gdbarch_tdep *tdep;
  unsigned long mach = info.bfd_arch_info->mach;

  /* Find a candidate among the list of architectures we've created
     already.  */
  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    return arches->gdbarch;

  tdep = xcalloc (1, sizeof (*tdep));
  arch = gdbarch_alloc (&info, tdep);

  /* Essential types.  */
  make_types (arch);

  /* Address/pointer conversions.  */
  if (mach == bfd_mach_m16c)
    {
      set_gdbarch_address_to_pointer (arch, m32c_m16c_address_to_pointer);
      set_gdbarch_pointer_to_address (arch, m32c_m16c_pointer_to_address);
    }

  /* Register set.  */
  make_regs (arch);

  /* Disassembly.  */
  set_gdbarch_print_insn (arch, print_insn_m32c);

  /* Breakpoints.  */
  set_gdbarch_breakpoint_from_pc (arch, m32c_breakpoint_from_pc);

  /* Prologue analysis and unwinding.  */
  set_gdbarch_inner_than (arch, core_addr_lessthan);
  set_gdbarch_skip_prologue (arch, m32c_skip_prologue);
  set_gdbarch_unwind_pc (arch, m32c_unwind_pc);
  set_gdbarch_unwind_sp (arch, m32c_unwind_sp);
#if 0
  /* I'm dropping the dwarf2 sniffer because it has a few problems.
     They may be in the dwarf2 cfi code in GDB, or they may be in
     the debug info emitted by the upstream toolchain.  I don't 
     know which, but I do know that the prologue analyzer works better.
     MVS 04/13/06
  */
  frame_unwind_append_sniffer (arch, dwarf2_frame_sniffer);
#endif
  frame_unwind_append_sniffer (arch, m32c_frame_sniffer);

  /* Inferior calls.  */
  set_gdbarch_push_dummy_call (arch, m32c_push_dummy_call);
  set_gdbarch_return_value (arch, m32c_return_value);
  set_gdbarch_unwind_dummy_id (arch, m32c_unwind_dummy_id);

  /* Trampolines.  */
  set_gdbarch_skip_trampoline_code (arch, m32c_skip_trampoline_code);

  set_gdbarch_virtual_frame_pointer (arch, m32c_virtual_frame_pointer);

  return arch;
}


void
_initialize_m32c_tdep (void)
{
  register_gdbarch_init (bfd_arch_m32c, m32c_gdbarch_init);

  m32c_dma_reggroup = reggroup_new ("dma", USER_REGGROUP);
}
