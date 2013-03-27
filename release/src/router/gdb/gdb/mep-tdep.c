/* Target-dependent code for the Toshiba MeP for GDB, the GNU debugger.

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   Contributed by Red Hat, Inc.

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
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "value.h"
#include "inferior.h"
#include "dis-asm.h"
#include "symfile.h"
#include "objfiles.h"
#include "language.h"
#include "arch-utils.h"
#include "regcache.h"
#include "remote.h"
#include "floatformat.h"
#include "sim-regno.h"
#include "disasm.h"
#include "trad-frame.h"
#include "reggroups.h"
#include "elf-bfd.h"
#include "elf/mep.h"
#include "prologue-value.h"
#include "opcode/cgen-bitset.h"
#include "infcall.h"

#include "gdb_assert.h"

/* Get the user's customized MeP coprocessor register names from
   libopcodes.  */
#include "opcodes/mep-desc.h"
#include "opcodes/mep-opc.h"


/* The gdbarch_tdep structure.  */

/* A quick recap for GDB hackers not familiar with the whole Toshiba
   Media Processor story:

   The MeP media engine is a configureable processor: users can design
   their own coprocessors, implement custom instructions, adjust cache
   sizes, select optional standard facilities like add-and-saturate
   instructions, and so on.  Then, they can build custom versions of
   the GNU toolchain to support their customized chips.  The
   MeP-Integrator program (see utils/mep) takes a GNU toolchain source
   tree, and a config file pointing to various files provided by the
   user describing their customizations, and edits the source tree to
   produce a compiler that can generate their custom instructions, an
   assembler that can assemble them and recognize their custom
   register names, and so on.

   Furthermore, the user can actually specify several of these custom
   configurations, called 'me_modules', and get a toolchain which can
   produce code for any of them, given a compiler/assembler switch;
   you say something like 'gcc -mconfig=mm_max' to generate code for
   the me_module named 'mm_max'.

   GDB, in particular, needs to:

   - use the coprocessor control register names provided by the user
     in their hardware description, in expressions, 'info register'
     output, and disassembly,

   - know the number, names, and types of the coprocessor's
     general-purpose registers, adjust the 'info all-registers' output
     accordingly, and print error messages if the user refers to one
     that doesn't exist

   - allow access to the control bus space only when the configuration
     actually has a control bus, and recognize which regions of the
     control bus space are actually populated,

   - disassemble using the user's provided mnemonics for their custom
     instructions, and

   - recognize whether the $hi and $lo registers are present, and
     allow access to them only when they are actually there.

   There are three sources of information about what sort of me_module
   we're actually dealing with:

   - A MeP executable file indicates which me_module it was compiled
     for, and libopcodes has tables describing each module.  So, given
     an executable file, we can find out about the processor it was
     compiled for.

   - There are SID command-line options to select a particular
     me_module, overriding the one specified in the ELF file.  SID
     provides GDB with a fake read-only register, 'module', which
     indicates which me_module GDB is communicating with an instance
     of.

   - There are SID command-line options to enable or disable certain
     optional processor features, overriding the defaults for the
     selected me_module.  The MeP $OPT register indicates which
     options are present on the current processor.  */


struct gdbarch_tdep
{
  /* A CGEN cpu descriptor for this BFD architecture and machine.

     Note: this is *not* customized for any particular me_module; the
     MeP libopcodes machinery actually puts off module-specific
     customization until the last minute.  So this contains
     information about all supported me_modules.  */
  CGEN_CPU_DESC cpu_desc;

  /* The me_module index from the ELF file we used to select this
     architecture, or CONFIG_NONE if there was none.

     Note that we should prefer to use the me_module number available
     via the 'module' register, whenever we're actually talking to a
     real target.

     In the absence of live information, we'd like to get the
     me_module number from the ELF file.  But which ELF file: the
     executable file, the core file, ... ?  The answer is, "the last
     ELF file we used to set the current architecture".  Thus, we
     create a separate instance of the gdbarch structure for each
     me_module value mep_gdbarch_init sees, and store the me_module
     value from the ELF file here.  */
  CONFIG_ATTR me_module;
};



/* Getting me_module information from the CGEN tables.  */


/* Find an entry in the DESC's hardware table whose name begins with
   PREFIX, and whose ISA mask intersects COPRO_ISA_MASK, but does not
   intersect with GENERIC_ISA_MASK.  If there is no matching entry,
   return zero.  */
static const CGEN_HW_ENTRY *
find_hw_entry_by_prefix_and_isa (CGEN_CPU_DESC desc,
                                 const char *prefix,
                                 CGEN_BITSET *copro_isa_mask,
                                 CGEN_BITSET *generic_isa_mask)
{
  int prefix_len = strlen (prefix);
  int i;

  for (i = 0; i < desc->hw_table.num_entries; i++)
    {
      const CGEN_HW_ENTRY *hw = desc->hw_table.entries[i];
      if (strncmp (prefix, hw->name, prefix_len) == 0)
        {
          CGEN_BITSET *hw_isa_mask
            = ((CGEN_BITSET *)
               &CGEN_ATTR_CGEN_HW_ISA_VALUE (CGEN_HW_ATTRS (hw)));

          if (cgen_bitset_intersect_p (hw_isa_mask, copro_isa_mask)
              && ! cgen_bitset_intersect_p (hw_isa_mask, generic_isa_mask))
            return hw;
        }
    }

  return 0;
}


/* Find an entry in DESC's hardware table whose type is TYPE.  Return
   zero if there is none.  */
static const CGEN_HW_ENTRY *
find_hw_entry_by_type (CGEN_CPU_DESC desc, CGEN_HW_TYPE type)
{
  int i;

  for (i = 0; i < desc->hw_table.num_entries; i++)
    {
      const CGEN_HW_ENTRY *hw = desc->hw_table.entries[i];

      if (hw->type == type)
        return hw;
    }

  return 0;
}


/* Return the CGEN hardware table entry for the coprocessor register
   set for ME_MODULE, whose name prefix is PREFIX.  If ME_MODULE has
   no such register set, return zero.  If ME_MODULE is the generic
   me_module CONFIG_NONE, return the table entry for the register set
   whose hardware type is GENERIC_TYPE.  */
static const CGEN_HW_ENTRY *
me_module_register_set (CONFIG_ATTR me_module,
                        const char *prefix,
                        CGEN_HW_TYPE generic_type)
{
  /* This is kind of tricky, because the hardware table is constructed
     in a way that isn't very helpful.  Perhaps we can fix that, but
     here's how it works at the moment:

     The configuration map, `mep_config_map', is indexed by me_module
     number, and indicates which coprocessor and core ISAs that
     me_module supports.  The 'core_isa' mask includes all the core
     ISAs, and the 'cop_isa' mask includes all the coprocessor ISAs.
     The entry for the generic me_module, CONFIG_NONE, has an empty
     'cop_isa', and its 'core_isa' selects only the standard MeP
     instruction set.

     The CGEN CPU descriptor's hardware table, desc->hw_table, has
     entries for all the register sets, for all me_modules.  Each
     entry has a mask indicating which ISAs use that register set.
     So, if an me_module supports some coprocessor ISA, we can find
     applicable register sets by scanning the hardware table for
     register sets whose masks include (at least some of) those ISAs.

     Each hardware table entry also has a name, whose prefix says
     whether it's a general-purpose ("h-cr") or control ("h-ccr")
     coprocessor register set.  It might be nicer to have an attribute
     indicating what sort of register set it was, that we could use
     instead of pattern-matching on the name.

     When there is no hardware table entry whose mask includes a
     particular coprocessor ISA and whose name starts with a given
     prefix, then that means that that coprocessor doesn't have any
     registers of that type.  In such cases, this function must return
     a null pointer.

     Coprocessor register sets' masks may or may not include the core
     ISA for the me_module they belong to.  Those generated by a2cgen
     do, but the sample me_module included in the unconfigured tree,
     'ccfx', does not.

     There are generic coprocessor register sets, intended only for
     use with the generic me_module.  Unfortunately, their masks
     include *all* ISAs --- even those for coprocessors that don't
     have such register sets.  This makes detecting the case where a
     coprocessor lacks a particular register set more complicated.

     So, here's the approach we take:

     - For CONFIG_NONE, we return the generic coprocessor register set.

     - For any other me_module, we search for a register set whose
       mask contains any of the me_module's coprocessor ISAs,
       specifically excluding the generic coprocessor register sets.  */

  CGEN_CPU_DESC desc = gdbarch_tdep (current_gdbarch)->cpu_desc;
  const CGEN_HW_ENTRY *hw;

  if (me_module == CONFIG_NONE)
    hw = find_hw_entry_by_type (desc, generic_type);
  else
    {
      CGEN_BITSET *cop = &mep_config_map[me_module].cop_isa;
      CGEN_BITSET *core = &mep_config_map[me_module].core_isa;
      CGEN_BITSET *generic = &mep_config_map[CONFIG_NONE].core_isa;
      CGEN_BITSET *cop_and_core;

      /* The coprocessor ISAs include the ISA for the specific core which
	 has that coprocessor.  */
      cop_and_core = cgen_bitset_copy (cop);
      cgen_bitset_union (cop, core, cop_and_core);
      hw = find_hw_entry_by_prefix_and_isa (desc, prefix, cop_and_core, generic);
    }

  return hw;
}


/* Given a hardware table entry HW representing a register set, return
   a pointer to the keyword table with all the register names.  If HW
   is NULL, return NULL, to propage the "no such register set" info
   along.  */
static CGEN_KEYWORD *
register_set_keyword_table (const CGEN_HW_ENTRY *hw)
{
  if (! hw)
    return NULL;

  /* Check that HW is actually a keyword table.  */
  gdb_assert (hw->asm_type == CGEN_ASM_KEYWORD);

  /* The 'asm_data' field of a register set's hardware table entry
     refers to a keyword table.  */
  return (CGEN_KEYWORD *) hw->asm_data;
}


/* Given a keyword table KEYWORD and a register number REGNUM, return
   the name of the register, or "" if KEYWORD contains no register
   whose number is REGNUM.  */
static char *
register_name_from_keyword (CGEN_KEYWORD *keyword_table, int regnum)
{
  const CGEN_KEYWORD_ENTRY *entry
    = cgen_keyword_lookup_value (keyword_table, regnum);

  if (entry)
    {
      char *name = entry->name;

      /* The CGEN keyword entries for register names include the
         leading $, which appears in MeP assembly as well as in GDB.
         But we don't want to return that; GDB core code adds that
         itself.  */
      if (name[0] == '$')
        name++;

      return name;
    }
  else
    return "";
}

  
/* Masks for option bits in the OPT special-purpose register.  */
enum {
  MEP_OPT_DIV = 1 << 25,        /* 32-bit divide instruction option */
  MEP_OPT_MUL = 1 << 24,        /* 32-bit multiply instruction option */
  MEP_OPT_BIT = 1 << 23,        /* bit manipulation instruction option */
  MEP_OPT_SAT = 1 << 22,        /* saturation instruction option */
  MEP_OPT_CLP = 1 << 21,        /* clip instruction option */
  MEP_OPT_MIN = 1 << 20,        /* min/max instruction option */
  MEP_OPT_AVE = 1 << 19,        /* average instruction option */
  MEP_OPT_ABS = 1 << 18,        /* absolute difference instruction option */
  MEP_OPT_LDZ = 1 << 16,        /* leading zero instruction option */
  MEP_OPT_VL64 = 1 << 6,        /* 64-bit VLIW operation mode option */
  MEP_OPT_VL32 = 1 << 5,        /* 32-bit VLIW operation mode option */
  MEP_OPT_COP = 1 << 4,         /* coprocessor option */
  MEP_OPT_DSP = 1 << 2,         /* DSP option */
  MEP_OPT_UCI = 1 << 1,         /* UCI option */
  MEP_OPT_DBG = 1 << 0,         /* DBG function option */
};


/* Given the option_mask value for a particular entry in
   mep_config_map, produce the value the processor's OPT register
   would use to represent the same set of options.  */
static unsigned int
opt_from_option_mask (unsigned int option_mask)
{
  /* A table mapping OPT register bits onto CGEN config map option
     bits.  */
  struct {
    unsigned int opt_bit, option_mask_bit;
  } bits[] = {
    { MEP_OPT_DIV, 1 << CGEN_INSN_OPTIONAL_DIV_INSN },
    { MEP_OPT_MUL, 1 << CGEN_INSN_OPTIONAL_MUL_INSN },
    { MEP_OPT_DIV, 1 << CGEN_INSN_OPTIONAL_DIV_INSN },
    { MEP_OPT_DBG, 1 << CGEN_INSN_OPTIONAL_DEBUG_INSN },
    { MEP_OPT_LDZ, 1 << CGEN_INSN_OPTIONAL_LDZ_INSN },
    { MEP_OPT_ABS, 1 << CGEN_INSN_OPTIONAL_ABS_INSN },
    { MEP_OPT_AVE, 1 << CGEN_INSN_OPTIONAL_AVE_INSN },
    { MEP_OPT_MIN, 1 << CGEN_INSN_OPTIONAL_MINMAX_INSN },
    { MEP_OPT_CLP, 1 << CGEN_INSN_OPTIONAL_CLIP_INSN },
    { MEP_OPT_SAT, 1 << CGEN_INSN_OPTIONAL_SAT_INSN },
    { MEP_OPT_UCI, 1 << CGEN_INSN_OPTIONAL_UCI_INSN },
    { MEP_OPT_DSP, 1 << CGEN_INSN_OPTIONAL_DSP_INSN },
    { MEP_OPT_COP, 1 << CGEN_INSN_OPTIONAL_CP_INSN },
  };

  int i;
  unsigned int opt = 0;

  for (i = 0; i < (sizeof (bits) / sizeof (bits[0])); i++)
    if (option_mask & bits[i].option_mask_bit)
      opt |= bits[i].opt_bit;

  return opt;
}


/* Return the value the $OPT register would use to represent the set
   of options for ME_MODULE.  */
static unsigned int
me_module_opt (CONFIG_ATTR me_module)
{
  return opt_from_option_mask (mep_config_map[me_module].option_mask);
}


/* Return the width of ME_MODULE's coprocessor data bus, in bits.
   This is either 32 or 64.  */
static int
me_module_cop_data_bus_width (CONFIG_ATTR me_module)
{
  if (mep_config_map[me_module].option_mask
      & (1 << CGEN_INSN_OPTIONAL_CP64_INSN))
    return 64;
  else
    return 32;
}


/* Return true if ME_MODULE is big-endian, false otherwise.  */
static int
me_module_big_endian (CONFIG_ATTR me_module)
{
  return mep_config_map[me_module].big_endian;
}


/* Return the name of ME_MODULE, or NULL if it has no name.  */
static const char *
me_module_name (CONFIG_ATTR me_module)
{
  /* The default me_module has "" as its name, but it's easier for our
     callers to test for NULL.  */
  if (! mep_config_map[me_module].name
      || mep_config_map[me_module].name[0] == '\0')
    return NULL;
  else
    return mep_config_map[me_module].name;
}

/* Register set.  */


/* The MeP spec defines the following registers:
   16 general purpose registers (r0-r15) 
   32 control/special registers (csr0-csr31)
   32 coprocessor general-purpose registers (c0 -- c31)
   64 coprocessor control registers (ccr0 -- ccr63)

   For the raw registers, we assign numbers here explicitly, instead
   of letting the enum assign them for us; the numbers are a matter of
   external protocol, and shouldn't shift around as things are edited.

   We access the control/special registers via pseudoregisters, to
   enforce read-only portions that some registers have.

   We access the coprocessor general purpose and control registers via
   pseudoregisters, to make sure they appear in the proper order in
   the 'info all-registers' command (which uses the register number
   ordering), and also to allow them to be renamed and resized
   depending on the me_module in use.

   The MeP allows coprocessor general-purpose registers to be either
   32 or 64 bits long, depending on the configuration.  Since we don't
   want the format of the 'g' packet to vary from one core to another,
   the raw coprocessor GPRs are always 64 bits.  GDB doesn't allow the
   types of registers to change (see the implementation of
   register_type), so we have four banks of pseudoregisters for the
   coprocessor gprs --- 32-bit vs. 64-bit, and integer
   vs. floating-point --- and we show or hide them depending on the
   configuration.  */
enum
{
  MEP_FIRST_RAW_REGNUM = 0,

  MEP_FIRST_GPR_REGNUM = 0,
  MEP_R0_REGNUM = 0,
  MEP_R1_REGNUM = 1,
  MEP_R2_REGNUM = 2,
  MEP_R3_REGNUM = 3,
  MEP_R4_REGNUM = 4,
  MEP_R5_REGNUM = 5,
  MEP_R6_REGNUM = 6,
  MEP_R7_REGNUM = 7,
  MEP_R8_REGNUM = 8,
  MEP_R9_REGNUM = 9,
  MEP_R10_REGNUM = 10,
  MEP_R11_REGNUM = 11,
  MEP_R12_REGNUM = 12,
  MEP_FP_REGNUM = MEP_R8_REGNUM,
  MEP_R13_REGNUM = 13,
  MEP_TP_REGNUM = MEP_R13_REGNUM,	/* (r13) Tiny data pointer */
  MEP_R14_REGNUM = 14,
  MEP_GP_REGNUM = MEP_R14_REGNUM,	/* (r14) Global pointer */
  MEP_R15_REGNUM = 15,
  MEP_SP_REGNUM = MEP_R15_REGNUM,	/* (r15) Stack pointer */
  MEP_LAST_GPR_REGNUM = MEP_R15_REGNUM,

  /* The raw control registers.  These are the values as received via
     the remote protocol, directly from the target; we only let user
     code touch the via the pseudoregisters, which enforce read-only
     bits.  */
  MEP_FIRST_RAW_CSR_REGNUM = 16,
  MEP_RAW_PC_REGNUM    = 16,    /* Program counter */
  MEP_RAW_LP_REGNUM    = 17,    /* Link pointer */
  MEP_RAW_SAR_REGNUM   = 18,    /* Raw shift amount */
  MEP_RAW_CSR3_REGNUM  = 19,    /* csr3: reserved */
  MEP_RAW_RPB_REGNUM   = 20,    /* Raw repeat begin address */
  MEP_RAW_RPE_REGNUM   = 21,    /* Repeat end address */
  MEP_RAW_RPC_REGNUM   = 22,    /* Repeat count */
  MEP_RAW_HI_REGNUM    = 23, /* Upper 32 bits of result of 64 bit mult/div */
  MEP_RAW_LO_REGNUM    = 24, /* Lower 32 bits of result of 64 bit mult/div */
  MEP_RAW_CSR9_REGNUM  = 25,    /* csr3: reserved */
  MEP_RAW_CSR10_REGNUM = 26,    /* csr3: reserved */
  MEP_RAW_CSR11_REGNUM = 27,    /* csr3: reserved */
  MEP_RAW_MB0_REGNUM   = 28,    /* Raw modulo begin address 0 */
  MEP_RAW_ME0_REGNUM   = 29,    /* Raw modulo end address 0 */
  MEP_RAW_MB1_REGNUM   = 30,    /* Raw modulo begin address 1 */
  MEP_RAW_ME1_REGNUM   = 31,    /* Raw modulo end address 1 */
  MEP_RAW_PSW_REGNUM   = 32,    /* Raw program status word */
  MEP_RAW_ID_REGNUM    = 33,    /* Raw processor ID/revision */
  MEP_RAW_TMP_REGNUM   = 34,    /* Temporary */
  MEP_RAW_EPC_REGNUM   = 35,    /* Exception program counter */
  MEP_RAW_EXC_REGNUM   = 36,    /* Raw exception cause */
  MEP_RAW_CFG_REGNUM   = 37,    /* Raw processor configuration*/
  MEP_RAW_CSR22_REGNUM = 38,    /* csr3: reserved */
  MEP_RAW_NPC_REGNUM   = 39,    /* Nonmaskable interrupt PC */
  MEP_RAW_DBG_REGNUM   = 40,    /* Raw debug */
  MEP_RAW_DEPC_REGNUM  = 41,    /* Debug exception PC */
  MEP_RAW_OPT_REGNUM   = 42,    /* Raw options */
  MEP_RAW_RCFG_REGNUM  = 43,    /* Raw local ram config */
  MEP_RAW_CCFG_REGNUM  = 44,    /* Raw cache config */
  MEP_RAW_CSR29_REGNUM = 45,    /* csr3: reserved */
  MEP_RAW_CSR30_REGNUM = 46,    /* csr3: reserved */
  MEP_RAW_CSR31_REGNUM = 47,    /* csr3: reserved */
  MEP_LAST_RAW_CSR_REGNUM = MEP_RAW_CSR31_REGNUM,

  /* The raw coprocessor general-purpose registers.  These are all 64
     bits wide.  */
  MEP_FIRST_RAW_CR_REGNUM = 48,
  MEP_LAST_RAW_CR_REGNUM = MEP_FIRST_RAW_CR_REGNUM + 31,

  MEP_FIRST_RAW_CCR_REGNUM = 80,
  MEP_LAST_RAW_CCR_REGNUM = MEP_FIRST_RAW_CCR_REGNUM + 63,

  /* The module number register.  This is the index of the me_module
     of which the current target is an instance.  (This is not a real
     MeP-specified register; it's provided by SID.)  */
  MEP_MODULE_REGNUM,

  MEP_LAST_RAW_REGNUM = MEP_MODULE_REGNUM,

  MEP_NUM_RAW_REGS = MEP_LAST_RAW_REGNUM + 1,

  /* Pseudoregisters.  See mep_pseudo_register_read and
     mep_pseudo_register_write.  */
  MEP_FIRST_PSEUDO_REGNUM = MEP_NUM_RAW_REGS,

  /* We have a pseudoregister for every control/special register, to
     implement registers with read-only bits.  */
  MEP_FIRST_CSR_REGNUM = MEP_FIRST_PSEUDO_REGNUM,
  MEP_PC_REGNUM = MEP_FIRST_CSR_REGNUM, /* Program counter */
  MEP_LP_REGNUM,                /* Link pointer */
  MEP_SAR_REGNUM,               /* shift amount */
  MEP_CSR3_REGNUM,              /* csr3: reserved */
  MEP_RPB_REGNUM,               /* repeat begin address */
  MEP_RPE_REGNUM,               /* Repeat end address */
  MEP_RPC_REGNUM,               /* Repeat count */
  MEP_HI_REGNUM,  /* Upper 32 bits of the result of 64 bit mult/div */
  MEP_LO_REGNUM,  /* Lower 32 bits of the result of 64 bit mult/div */
  MEP_CSR9_REGNUM,              /* csr3: reserved */
  MEP_CSR10_REGNUM,             /* csr3: reserved */
  MEP_CSR11_REGNUM,             /* csr3: reserved */
  MEP_MB0_REGNUM,               /* modulo begin address 0 */
  MEP_ME0_REGNUM,               /* modulo end address 0 */
  MEP_MB1_REGNUM,               /* modulo begin address 1 */
  MEP_ME1_REGNUM,               /* modulo end address 1 */
  MEP_PSW_REGNUM,               /* program status word */
  MEP_ID_REGNUM,                /* processor ID/revision */
  MEP_TMP_REGNUM,               /* Temporary */
  MEP_EPC_REGNUM,               /* Exception program counter */
  MEP_EXC_REGNUM,               /* exception cause */
  MEP_CFG_REGNUM,               /* processor configuration*/
  MEP_CSR22_REGNUM,             /* csr3: reserved */
  MEP_NPC_REGNUM,               /* Nonmaskable interrupt PC */
  MEP_DBG_REGNUM,               /* debug */
  MEP_DEPC_REGNUM,              /* Debug exception PC */
  MEP_OPT_REGNUM,               /* options */
  MEP_RCFG_REGNUM,              /* local ram config */
  MEP_CCFG_REGNUM,              /* cache config */
  MEP_CSR29_REGNUM,             /* csr3: reserved */
  MEP_CSR30_REGNUM,             /* csr3: reserved */
  MEP_CSR31_REGNUM,             /* csr3: reserved */
  MEP_LAST_CSR_REGNUM = MEP_CSR31_REGNUM,

  /* The 32-bit integer view of the coprocessor GPR's.  */
  MEP_FIRST_CR32_REGNUM,
  MEP_LAST_CR32_REGNUM = MEP_FIRST_CR32_REGNUM + 31,

  /* The 32-bit floating-point view of the coprocessor GPR's.  */
  MEP_FIRST_FP_CR32_REGNUM,
  MEP_LAST_FP_CR32_REGNUM = MEP_FIRST_FP_CR32_REGNUM + 31,

  /* The 64-bit integer view of the coprocessor GPR's.  */
  MEP_FIRST_CR64_REGNUM,
  MEP_LAST_CR64_REGNUM = MEP_FIRST_CR64_REGNUM + 31,

  /* The 64-bit floating-point view of the coprocessor GPR's.  */
  MEP_FIRST_FP_CR64_REGNUM,
  MEP_LAST_FP_CR64_REGNUM = MEP_FIRST_FP_CR64_REGNUM + 31,

  MEP_FIRST_CCR_REGNUM,
  MEP_LAST_CCR_REGNUM = MEP_FIRST_CCR_REGNUM + 63,

  MEP_LAST_PSEUDO_REGNUM = MEP_LAST_CCR_REGNUM,

  MEP_NUM_PSEUDO_REGS = (MEP_LAST_PSEUDO_REGNUM - MEP_LAST_RAW_REGNUM),

  MEP_NUM_REGS = MEP_NUM_RAW_REGS + MEP_NUM_PSEUDO_REGS
};


#define IN_SET(set, n) \
  (MEP_FIRST_ ## set ## _REGNUM <= (n) && (n) <= MEP_LAST_ ## set ## _REGNUM)

#define IS_GPR_REGNUM(n)     (IN_SET (GPR,     (n)))
#define IS_RAW_CSR_REGNUM(n) (IN_SET (RAW_CSR, (n)))
#define IS_RAW_CR_REGNUM(n)  (IN_SET (RAW_CR,  (n)))
#define IS_RAW_CCR_REGNUM(n) (IN_SET (RAW_CCR, (n)))

#define IS_CSR_REGNUM(n)     (IN_SET (CSR,     (n)))
#define IS_CR32_REGNUM(n)    (IN_SET (CR32,    (n)))
#define IS_FP_CR32_REGNUM(n) (IN_SET (FP_CR32, (n)))
#define IS_CR64_REGNUM(n)    (IN_SET (CR64,    (n)))
#define IS_FP_CR64_REGNUM(n) (IN_SET (FP_CR64, (n)))
#define IS_CR_REGNUM(n)      (IS_CR32_REGNUM (n) || IS_FP_CR32_REGNUM (n) \
                              || IS_CR64_REGNUM (n) || IS_FP_CR64_REGNUM (n))
#define IS_CCR_REGNUM(n)     (IN_SET (CCR,     (n)))

#define IS_RAW_REGNUM(n)     (IN_SET (RAW,     (n)))
#define IS_PSEUDO_REGNUM(n)  (IN_SET (PSEUDO,  (n)))

#define NUM_REGS_IN_SET(set) \
  (MEP_LAST_ ## set ## _REGNUM - MEP_FIRST_ ## set ## _REGNUM + 1)

#define MEP_GPR_SIZE (4)        /* Size of a MeP general-purpose register.  */
#define MEP_PSW_SIZE (4)        /* Size of the PSW register.  */
#define MEP_LP_SIZE (4)         /* Size of the LP register.  */


/* Many of the control/special registers contain bits that cannot be
   written to; some are entirely read-only.  So we present them all as
   pseudoregisters.

   The following table describes the special properties of each CSR.  */
struct mep_csr_register
{
  /* The number of this CSR's raw register.  */
  int raw;

  /* The number of this CSR's pseudoregister.  */
  int pseudo;

  /* A mask of the bits that are writeable: if a bit is set here, then
     it can be modified; if the bit is clear, then it cannot.  */
  LONGEST writeable_bits;
};


/* mep_csr_registers[i] describes the i'th CSR.
   We just list the register numbers here explicitly to help catch
   typos.  */
#define CSR(name) MEP_RAW_ ## name ## _REGNUM, MEP_ ## name ## _REGNUM
struct mep_csr_register mep_csr_registers[] = {
  { CSR(PC),    0xffffffff },   /* manual says r/o, but we can write it */
  { CSR(LP),    0xffffffff },
  { CSR(SAR),   0x0000003f },
  { CSR(CSR3),  0xffffffff },
  { CSR(RPB),   0xfffffffe },
  { CSR(RPE),   0xffffffff },
  { CSR(RPC),   0xffffffff },
  { CSR(HI),    0xffffffff },
  { CSR(LO),    0xffffffff },
  { CSR(CSR9),  0xffffffff },
  { CSR(CSR10), 0xffffffff },
  { CSR(CSR11), 0xffffffff },
  { CSR(MB0),   0x0000ffff },
  { CSR(ME0),   0x0000ffff },
  { CSR(MB1),   0x0000ffff },
  { CSR(ME1),   0x0000ffff },
  { CSR(PSW),   0x000003ff },
  { CSR(ID),    0x00000000 },
  { CSR(TMP),   0xffffffff },
  { CSR(EPC),   0xffffffff },
  { CSR(EXC),   0x000030f0 },
  { CSR(CFG),   0x00c0001b },
  { CSR(CSR22), 0xffffffff },
  { CSR(NPC),   0xffffffff },
  { CSR(DBG),   0x00000580 },
  { CSR(DEPC),  0xffffffff },
  { CSR(OPT),   0x00000000 },
  { CSR(RCFG),  0x00000000 },
  { CSR(CCFG),  0x00000000 },
  { CSR(CSR29), 0xffffffff },
  { CSR(CSR30), 0xffffffff },
  { CSR(CSR31), 0xffffffff },
};


/* If R is the number of a raw register, then mep_raw_to_pseudo[R] is
   the number of the corresponding pseudoregister.  Otherwise,
   mep_raw_to_pseudo[R] == R.  */
static int mep_raw_to_pseudo[MEP_NUM_REGS];

/* If R is the number of a pseudoregister, then mep_pseudo_to_raw[R]
   is the number of the underlying raw register.  Otherwise
   mep_pseudo_to_raw[R] == R.  */
static int mep_pseudo_to_raw[MEP_NUM_REGS];

static void
mep_init_pseudoregister_maps (void)
{
  int i;

  /* Verify that mep_csr_registers covers all the CSRs, in order.  */
  gdb_assert (ARRAY_SIZE (mep_csr_registers) == NUM_REGS_IN_SET (CSR));
  gdb_assert (ARRAY_SIZE (mep_csr_registers) == NUM_REGS_IN_SET (RAW_CSR));

  /* Verify that the raw and pseudo ranges have matching sizes.  */
  gdb_assert (NUM_REGS_IN_SET (RAW_CSR) == NUM_REGS_IN_SET (CSR));
  gdb_assert (NUM_REGS_IN_SET (RAW_CR)  == NUM_REGS_IN_SET (CR32));
  gdb_assert (NUM_REGS_IN_SET (RAW_CR)  == NUM_REGS_IN_SET (CR64));
  gdb_assert (NUM_REGS_IN_SET (RAW_CCR) == NUM_REGS_IN_SET (CCR));

  for (i = 0; i < ARRAY_SIZE (mep_csr_registers); i++)
    {
      struct mep_csr_register *r = &mep_csr_registers[i];

      gdb_assert (r->pseudo == MEP_FIRST_CSR_REGNUM + i);
      gdb_assert (r->raw    == MEP_FIRST_RAW_CSR_REGNUM + i);
    }

  /* Set up the initial  raw<->pseudo mappings.  */
  for (i = 0; i < MEP_NUM_REGS; i++)
    {
      mep_raw_to_pseudo[i] = i;
      mep_pseudo_to_raw[i] = i;
    }

  /* Add the CSR raw<->pseudo mappings.  */
  for (i = 0; i < ARRAY_SIZE (mep_csr_registers); i++)
    {
      struct mep_csr_register *r = &mep_csr_registers[i];

      mep_raw_to_pseudo[r->raw] = r->pseudo;
      mep_pseudo_to_raw[r->pseudo] = r->raw;
    }

  /* Add the CR raw<->pseudo mappings.  */
  for (i = 0; i < NUM_REGS_IN_SET (RAW_CR); i++)
    {
      int raw = MEP_FIRST_RAW_CR_REGNUM + i;
      int pseudo32 = MEP_FIRST_CR32_REGNUM + i;
      int pseudofp32 = MEP_FIRST_FP_CR32_REGNUM + i;
      int pseudo64 = MEP_FIRST_CR64_REGNUM + i;
      int pseudofp64 = MEP_FIRST_FP_CR64_REGNUM + i;

      /* Truly, the raw->pseudo mapping depends on the current module.
         But we use the raw->pseudo mapping when we read the debugging
         info; at that point, we don't know what module we'll actually
         be running yet.  So, we always supply the 64-bit register
         numbers; GDB knows how to pick a smaller value out of a
         larger register properly.  */
      mep_raw_to_pseudo[raw] = pseudo64;
      mep_pseudo_to_raw[pseudo32] = raw;
      mep_pseudo_to_raw[pseudofp32] = raw;
      mep_pseudo_to_raw[pseudo64] = raw;
      mep_pseudo_to_raw[pseudofp64] = raw;
    }

  /* Add the CCR raw<->pseudo mappings.  */
  for (i = 0; i < NUM_REGS_IN_SET (CCR); i++)
    {
      int raw = MEP_FIRST_RAW_CCR_REGNUM + i;
      int pseudo = MEP_FIRST_CCR_REGNUM + i;
      mep_raw_to_pseudo[raw] = pseudo;
      mep_pseudo_to_raw[pseudo] = raw;
    }
}


static int
mep_debug_reg_to_regnum (int debug_reg)
{
  /* The debug info uses the raw register numbers.  */
  return mep_raw_to_pseudo[debug_reg];
}


/* Return the size, in bits, of the coprocessor pseudoregister
   numbered PSEUDO.  */
static int
mep_pseudo_cr_size (int pseudo)
{
  if (IS_CR32_REGNUM (pseudo)
      || IS_FP_CR32_REGNUM (pseudo))
    return 32;
  else if (IS_CR64_REGNUM (pseudo)
           || IS_FP_CR64_REGNUM (pseudo))
    return 64;
  else
    gdb_assert (0);
}


/* If the coprocessor pseudoregister numbered PSEUDO is a
   floating-point register, return non-zero; if it is an integer
   register, return zero.  */
static int
mep_pseudo_cr_is_float (int pseudo)
{
  return (IS_FP_CR32_REGNUM (pseudo)
          || IS_FP_CR64_REGNUM (pseudo));
}


/* Given a coprocessor GPR pseudoregister number, return its index
   within that register bank.  */
static int
mep_pseudo_cr_index (int pseudo)
{
  if (IS_CR32_REGNUM (pseudo))
    return pseudo - MEP_FIRST_CR32_REGNUM;
  else if (IS_FP_CR32_REGNUM (pseudo))
      return pseudo - MEP_FIRST_FP_CR32_REGNUM;
  else if (IS_CR64_REGNUM (pseudo))
      return pseudo - MEP_FIRST_CR64_REGNUM;
  else if (IS_FP_CR64_REGNUM (pseudo))
      return pseudo - MEP_FIRST_FP_CR64_REGNUM;
  else
    gdb_assert (0);
}


/* Return the me_module index describing the current target.

   If the current target has registers (e.g., simulator, remote
   target), then this uses the value of the 'module' register, raw
   register MEP_MODULE_REGNUM.  Otherwise, this retrieves the value
   from the ELF header's e_flags field of the current executable
   file.  */
static CONFIG_ATTR
current_me_module ()
{
  if (target_has_registers)
    {
      ULONGEST regval;
      regcache_cooked_read_unsigned (get_current_regcache (),
				     MEP_MODULE_REGNUM, &regval);
      return regval;
    }
  else
    return gdbarch_tdep (current_gdbarch)->me_module;
}


/* Return the set of options for the current target, in the form that
   the OPT register would use.

   If the current target has registers (e.g., simulator, remote
   target), then this is the actual value of the OPT register.  If the
   current target does not have registers (e.g., an executable file),
   then use the 'module_opt' field we computed when we build the
   gdbarch object for this module.  */
static unsigned int
current_options ()
{
  if (target_has_registers)
    {
      ULONGEST regval;
      regcache_cooked_read_unsigned (get_current_regcache (),
				     MEP_OPT_REGNUM, &regval);
      return regval;
    }
  else
    return me_module_opt (current_me_module ());
}


/* Return the width of the current me_module's coprocessor data bus,
   in bits.  This is either 32 or 64.  */
static int
current_cop_data_bus_width ()
{
  return me_module_cop_data_bus_width (current_me_module ());
}


/* Return the keyword table of coprocessor general-purpose register
   names appropriate for the me_module we're dealing with.  */
static CGEN_KEYWORD *
current_cr_names ()
{
  const CGEN_HW_ENTRY *hw
    = me_module_register_set (current_me_module (), "h-cr-", HW_H_CR);

  return register_set_keyword_table (hw);
}


/* Return non-zero if the coprocessor general-purpose registers are
   floating-point values, zero otherwise.  */
static int
current_cr_is_float ()
{
  const CGEN_HW_ENTRY *hw
    = me_module_register_set (current_me_module (), "h-cr-", HW_H_CR);

  return CGEN_ATTR_CGEN_HW_IS_FLOAT_VALUE (CGEN_HW_ATTRS (hw));
}


/* Return the keyword table of coprocessor control register names
   appropriate for the me_module we're dealing with.  */
static CGEN_KEYWORD *
current_ccr_names ()
{
  const CGEN_HW_ENTRY *hw
    = me_module_register_set (current_me_module (), "h-ccr-", HW_H_CCR);

  return register_set_keyword_table (hw);
}


static const char *
mep_register_name (int regnr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);  

  /* General-purpose registers.  */
  static const char *gpr_names[] = {
    "r0",   "r1",   "r2",   "r3",   /* 0 */
    "r4",   "r5",   "r6",   "r7",   /* 4 */
    "fp",   "r9",   "r10",  "r11",  /* 8 */
    "r12",  "tp",   "gp",   "sp"    /* 12 */
  };

  /* Special-purpose registers.  */
  static const char *csr_names[] = {
    "pc",   "lp",   "sar",  "",     /* 0  csr3: reserved */ 
    "rpb",  "rpe",  "rpc",  "hi",   /* 4 */
    "lo",   "",     "",     "",     /* 8  csr9-csr11: reserved */
    "mb0",  "me0",  "mb1",  "me1",  /* 12 */

    "psw",  "id",   "tmp",  "epc",  /* 16 */
    "exc",  "cfg",  "",     "npc",  /* 20  csr22: reserved */
    "dbg",  "depc", "opt",  "rcfg", /* 24 */
    "ccfg", "",     "",     ""      /* 28  csr29-csr31: reserved */
  };

  if (IS_GPR_REGNUM (regnr))
    return gpr_names[regnr - MEP_R0_REGNUM];
  else if (IS_CSR_REGNUM (regnr))
    {
      /* The 'hi' and 'lo' registers are only present on processors
         that have the 'MUL' or 'DIV' instructions enabled.  */
      if ((regnr == MEP_HI_REGNUM || regnr == MEP_LO_REGNUM)
          && (! (current_options () & (MEP_OPT_MUL | MEP_OPT_DIV))))
        return "";

      return csr_names[regnr - MEP_FIRST_CSR_REGNUM];
    }
  else if (IS_CR_REGNUM (regnr))
    {
      CGEN_KEYWORD *names;
      int cr_size;
      int cr_is_float;

      /* Does this module have a coprocessor at all?  */
      if (! (current_options () & MEP_OPT_COP))
        return "";

      names = current_cr_names ();
      if (! names)
        /* This module's coprocessor has no general-purpose registers.  */
        return "";

      cr_size = current_cop_data_bus_width ();
      if (cr_size != mep_pseudo_cr_size (regnr))
        /* This module's coprocessor's GPR's are of a different size.  */
        return "";

      cr_is_float = current_cr_is_float ();
      /* The extra ! operators ensure we get boolean equality, not
         numeric equality.  */
      if (! cr_is_float != ! mep_pseudo_cr_is_float (regnr))
        /* This module's coprocessor's GPR's are of a different type.  */
        return "";

      return register_name_from_keyword (names, mep_pseudo_cr_index (regnr));
    }
  else if (IS_CCR_REGNUM (regnr))
    {
      /* Does this module have a coprocessor at all?  */
      if (! (current_options () & MEP_OPT_COP))
        return "";

      {
        CGEN_KEYWORD *names = current_ccr_names ();

        if (! names)
          /* This me_module's coprocessor has no control registers.  */
          return "";

        return register_name_from_keyword (names, regnr-MEP_FIRST_CCR_REGNUM);
      }
    }

  /* It might be nice to give the 'module' register a name, but that
     would affect the output of 'info all-registers', which would
     disturb the test suites.  So we leave it invisible.  */
  else
    return NULL;
}


/* Custom register groups for the MeP.  */
static struct reggroup *mep_csr_reggroup; /* control/special */
static struct reggroup *mep_cr_reggroup;  /* coprocessor general-purpose */
static struct reggroup *mep_ccr_reggroup; /* coprocessor control */


static int
mep_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
                         struct reggroup *group)
{
  /* Filter reserved or unused register numbers.  */
  {
    const char *name = mep_register_name (regnum);

    if (! name || name[0] == '\0')
      return 0;
  }

  /* We could separate the GPRs and the CSRs.  Toshiba has approved of
     the existing behavior, so we'd want to run that by them.  */
  if (group == general_reggroup)
    return (IS_GPR_REGNUM (regnum)
            || IS_CSR_REGNUM (regnum));

  /* Everything is in the 'all' reggroup, except for the raw CSR's.  */
  else if (group == all_reggroup)
    return (IS_GPR_REGNUM (regnum)
            || IS_CSR_REGNUM (regnum)
            || IS_CR_REGNUM (regnum)
            || IS_CCR_REGNUM (regnum));

  /* All registers should be saved and restored, except for the raw
     CSR's.

     This is probably right if the coprocessor is something like a
     floating-point unit, but would be wrong if the coprocessor is
     something that does I/O, where register accesses actually cause
     externally-visible actions.  But I get the impression that the
     coprocessor isn't supposed to do things like that --- you'd use a
     hardware engine, perhaps.  */
  else if (group == save_reggroup || group == restore_reggroup)
    return (IS_GPR_REGNUM (regnum)
            || IS_CSR_REGNUM (regnum)
            || IS_CR_REGNUM (regnum)
            || IS_CCR_REGNUM (regnum));

  else if (group == mep_csr_reggroup)
    return IS_CSR_REGNUM (regnum);
  else if (group == mep_cr_reggroup)
    return IS_CR_REGNUM (regnum);
  else if (group == mep_ccr_reggroup)
    return IS_CCR_REGNUM (regnum);
  else
    return 0;
}


static struct type *
mep_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  /* Coprocessor general-purpose registers may be either 32 or 64 bits
     long.  So for them, the raw registers are always 64 bits long (to
     keep the 'g' packet format fixed), and the pseudoregisters vary
     in length.  */
  if (IS_RAW_CR_REGNUM (reg_nr))
    return builtin_type_uint64;

  /* Since GDB doesn't allow registers to change type, we have two
     banks of pseudoregisters for the coprocessor general-purpose
     registers: one that gives a 32-bit view, and one that gives a
     64-bit view.  We hide or show one or the other depending on the
     current module.  */
  if (IS_CR_REGNUM (reg_nr))
    {
      int size = mep_pseudo_cr_size (reg_nr);
      if (size == 32)
        {
          if (mep_pseudo_cr_is_float (reg_nr))
            return builtin_type_float;
          else
            return builtin_type_uint32;
        }
      else if (size == 64)
        {
          if (mep_pseudo_cr_is_float (reg_nr))
            return builtin_type_double;
          else
            return builtin_type_uint64;
        }
      else
        gdb_assert (0);
    }

  /* All other registers are 32 bits long.  */
  else
    return builtin_type_uint32;
}


static CORE_ADDR
mep_read_pc (struct regcache *regcache)
{
  ULONGEST pc;
  regcache_cooked_read_unsigned (regcache, MEP_PC_REGNUM, &pc);
  return pc;
}

static void
mep_write_pc (struct regcache *regcache, CORE_ADDR pc)
{
  regcache_cooked_write_unsigned (regcache, MEP_PC_REGNUM, pc);
}


static void
mep_pseudo_cr32_read (struct gdbarch *gdbarch,
                      struct regcache *regcache,
                      int cookednum,
                      void *buf)
{
  /* Read the raw register into a 64-bit buffer, and then return the
     appropriate end of that buffer.  */
  int rawnum = mep_pseudo_to_raw[cookednum];
  char buf64[8];

  gdb_assert (TYPE_LENGTH (register_type (gdbarch, rawnum)) == sizeof (buf64));
  gdb_assert (TYPE_LENGTH (register_type (gdbarch, cookednum)) == 4);
  regcache_raw_read (regcache, rawnum, buf64);
  /* Slow, but legible.  */
  store_unsigned_integer (buf, 4, extract_unsigned_integer (buf64, 8));
}


static void
mep_pseudo_cr64_read (struct gdbarch *gdbarch,
                      struct regcache *regcache,
                      int cookednum,
                      void *buf)
{
  regcache_raw_read (regcache, mep_pseudo_to_raw[cookednum], buf);
}


static void
mep_pseudo_register_read (struct gdbarch *gdbarch,
                          struct regcache *regcache,
                          int cookednum,
                          gdb_byte *buf)
{
  if (IS_CSR_REGNUM (cookednum)
      || IS_CCR_REGNUM (cookednum))
    regcache_raw_read (regcache, mep_pseudo_to_raw[cookednum], buf);
  else if (IS_CR32_REGNUM (cookednum)
           || IS_FP_CR32_REGNUM (cookednum))
    mep_pseudo_cr32_read (gdbarch, regcache, cookednum, buf);
  else if (IS_CR64_REGNUM (cookednum)
           || IS_FP_CR64_REGNUM (cookednum))
    mep_pseudo_cr64_read (gdbarch, regcache, cookednum, buf);
  else
    gdb_assert (0);
}


static void
mep_pseudo_csr_write (struct gdbarch *gdbarch,
                      struct regcache *regcache,
                      int cookednum,
                      const void *buf)
{
  int size = register_size (gdbarch, cookednum);
  struct mep_csr_register *r
    = &mep_csr_registers[cookednum - MEP_FIRST_CSR_REGNUM];

  if (r->writeable_bits == 0)
    /* A completely read-only register; avoid the read-modify-
       write cycle, and juts ignore the entire write.  */
    ;
  else
    {
      /* A partially writeable register; do a read-modify-write cycle.  */
      ULONGEST old_bits;
      ULONGEST new_bits;
      ULONGEST mixed_bits;
          
      regcache_raw_read_unsigned (regcache, r->raw, &old_bits);
      new_bits = extract_unsigned_integer (buf, size);
      mixed_bits = ((r->writeable_bits & new_bits)
                    | (~r->writeable_bits & old_bits));
      regcache_raw_write_unsigned (regcache, r->raw, mixed_bits);
    }
}
                      

static void
mep_pseudo_cr32_write (struct gdbarch *gdbarch,
                       struct regcache *regcache,
                       int cookednum,
                       const void *buf)
{
  /* Expand the 32-bit value into a 64-bit value, and write that to
     the pseudoregister.  */
  int rawnum = mep_pseudo_to_raw[cookednum];
  char buf64[8];
  
  gdb_assert (TYPE_LENGTH (register_type (gdbarch, rawnum)) == sizeof (buf64));
  gdb_assert (TYPE_LENGTH (register_type (gdbarch, cookednum)) == 4);
  /* Slow, but legible.  */
  store_unsigned_integer (buf64, 8, extract_unsigned_integer (buf, 4));
  regcache_raw_write (regcache, rawnum, buf64);
}


static void
mep_pseudo_cr64_write (struct gdbarch *gdbarch,
                     struct regcache *regcache,
                     int cookednum,
                     const void *buf)
{
  regcache_raw_write (regcache, mep_pseudo_to_raw[cookednum], buf);
}


static void
mep_pseudo_register_write (struct gdbarch *gdbarch,
                           struct regcache *regcache,
                           int cookednum,
                           const gdb_byte *buf)
{
  if (IS_CSR_REGNUM (cookednum))
    mep_pseudo_csr_write (gdbarch, regcache, cookednum, buf);
  else if (IS_CR32_REGNUM (cookednum)
           || IS_FP_CR32_REGNUM (cookednum))
    mep_pseudo_cr32_write (gdbarch, regcache, cookednum, buf);
  else if (IS_CR64_REGNUM (cookednum)
           || IS_FP_CR64_REGNUM (cookednum))
    mep_pseudo_cr64_write (gdbarch, regcache, cookednum, buf);
  else if (IS_CCR_REGNUM (cookednum))
    regcache_raw_write (regcache, mep_pseudo_to_raw[cookednum], buf);
  else
    gdb_assert (0);
}



/* Disassembly.  */

/* The mep disassembler needs to know about the section in order to
   work correctly. */
int 
mep_gdb_print_insn (bfd_vma pc, disassemble_info * info)
{
  struct obj_section * s = find_pc_section (pc);

  if (s)
    {
      /* The libopcodes disassembly code uses the section to find the
         BFD, the BFD to find the ELF header, the ELF header to find
         the me_module index, and the me_module index to select the
         right instructions to print.  */
      info->section = s->the_bfd_section;
      info->arch = bfd_arch_mep;
	
      return print_insn_mep (pc, info);
    }
  
  return 0;
}


/* Prologue analysis.  */


/* The MeP has two classes of instructions: "core" instructions, which
   are pretty normal RISC chip stuff, and "coprocessor" instructions,
   which are mostly concerned with moving data in and out of
   coprocessor registers, and branching on coprocessor condition
   codes.  There's space in the instruction set for custom coprocessor
   instructions, too.

   Instructions can be 16 or 32 bits long; the top two bits of the
   first byte indicate the length.  The coprocessor instructions are
   mixed in with the core instructions, and there's no easy way to
   distinguish them; you have to completely decode them to tell one
   from the other.

   The MeP also supports a "VLIW" operation mode, where instructions
   always occur in fixed-width bundles.  The bundles are either 32
   bits or 64 bits long, depending on a fixed configuration flag.  You
   decode the first part of the bundle as normal; if it's a core
   instruction, and there's any space left in the bundle, the
   remainder of the bundle is a coprocessor instruction, which will
   execute in parallel with the core instruction.  If the first part
   of the bundle is a coprocessor instruction, it occupies the entire
   bundle.

   So, here are all the cases:

   - 32-bit VLIW mode:
     Every bundle is four bytes long, and naturally aligned, and can hold
     one or two instructions:
     - 16-bit core instruction; 16-bit coprocessor instruction
       These execute in parallel.       
     - 32-bit core instruction
     - 32-bit coprocessor instruction

   - 64-bit VLIW mode:
     Every bundle is eight bytes long, and naturally aligned, and can hold
     one or two instructions:
     - 16-bit core instruction; 48-bit (!) coprocessor instruction
       These execute in parallel.       
     - 32-bit core instruction; 32-bit coprocessor instruction
       These execute in parallel.       
     - 64-bit coprocessor instruction

   Now, the MeP manual doesn't define any 48- or 64-bit coprocessor
   instruction, so I don't really know what's up there; perhaps these
   are always the user-defined coprocessor instructions.  */


/* Return non-zero if PC is in a VLIW code section, zero
   otherwise.  */
static int
mep_pc_in_vliw_section (CORE_ADDR pc)
{
  struct obj_section *s = find_pc_section (pc);
  if (s)
    return (s->the_bfd_section->flags & SEC_MEP_VLIW);
  return 0;
}


/* Set *INSN to the next core instruction at PC, and return the
   address of the next instruction.

   The MeP instruction encoding is endian-dependent.  16- and 32-bit
   instructions are encoded as one or two two-byte parts, and each
   part is byte-swapped independently.  Thus:

      void
      foo (void)
      {
        asm ("movu $1, 0x123456");
        asm ("sb $1,0x5678($2)");
        asm ("clip $1, 19");
      }

   compiles to this big-endian code:

       0:	d1 56 12 34 	movu $1,0x123456
       4:	c1 28 56 78 	sb $1,22136($2)
       8:	f1 01 10 98 	clip $1,0x13
       c:	70 02       	ret

   and this little-endian code:

       0:	56 d1 34 12 	movu $1,0x123456
       4:	28 c1 78 56 	sb $1,22136($2)
       8:	01 f1 98 10 	clip $1,0x13
       c:	02 70       	ret

   Instructions are returned in *INSN in an endian-independent form: a
   given instruction always appears in *INSN the same way, regardless
   of whether the instruction stream is big-endian or little-endian.

   *INSN's most significant 16 bits are the first (i.e., at lower
   addresses) 16 bit part of the instruction.  Its least significant
   16 bits are the second (i.e., higher-addressed) 16 bit part of the
   instruction, or zero for a 16-bit instruction.  Both 16-bit parts
   are fetched using the current endianness.

   So, the *INSN values for the instruction sequence above would be
   the following, in either endianness:

       0xd1561234       movu $1,0x123456     
       0xc1285678 	sb $1,22136($2)
       0xf1011098 	clip $1,0x13
       0x70020000      	ret

   (In a sense, it would be more natural to return 16-bit instructions
   in the least significant 16 bits of *INSN, but that would be
   ambiguous.  In order to tell whether you're looking at a 16- or a
   32-bit instruction, you have to consult the major opcode field ---
   the most significant four bits of the instruction's first 16-bit
   part.  But if we put 16-bit instructions at the least significant
   end of *INSN, then you don't know where to find the major opcode
   field until you know if it's a 16- or a 32-bit instruction ---
   which is where we started.)

   If PC points to a core / coprocessor bundle in a VLIW section, set
   *INSN to the core instruction, and return the address of the next
   bundle.  This has the effect of skipping the bundled coprocessor
   instruction.  That's okay, since coprocessor instructions aren't
   significant to prologue analysis --- for the time being,
   anyway.  */

static CORE_ADDR 
mep_get_insn (CORE_ADDR pc, long *insn)
{
  int pc_in_vliw_section;
  int vliw_mode;
  int insn_len;
  char buf[2];

  *insn = 0;

  /* Are we in a VLIW section?  */
  pc_in_vliw_section = mep_pc_in_vliw_section (pc);
  if (pc_in_vliw_section)
    {
      /* Yes, find out which bundle size.  */
      vliw_mode = current_options () & (MEP_OPT_VL32 | MEP_OPT_VL64);

      /* If PC is in a VLIW section, but the current core doesn't say
         that it supports either VLIW mode, then we don't have enough
         information to parse the instruction stream it contains.
         Since the "undifferentiated" standard core doesn't have
         either VLIW mode bit set, this could happen.

         But it shouldn't be an error to (say) set a breakpoint in a
         VLIW section, if you know you'll never reach it.  (Perhaps
         you have a script that sets a bunch of standard breakpoints.)

         So we'll just return zero here, and hope for the best.  */
      if (! (vliw_mode & (MEP_OPT_VL32 | MEP_OPT_VL64)))
        return 0;

      /* If both VL32 and VL64 are set, that's bogus, too.  */
      if (vliw_mode == (MEP_OPT_VL32 | MEP_OPT_VL64))
        return 0;
    }
  else
    vliw_mode = 0;

  read_memory (pc, buf, sizeof (buf));
  *insn = extract_unsigned_integer (buf, 2) << 16;

  /* The major opcode --- the top four bits of the first 16-bit
     part --- indicates whether this instruction is 16 or 32 bits
     long.  All 32-bit instructions have a major opcode whose top
     two bits are 11; all the rest are 16-bit instructions.  */
  if ((*insn & 0xc0000000) == 0xc0000000)
    {
      /* Fetch the second 16-bit part of the instruction.  */
      read_memory (pc + 2, buf, sizeof (buf));
      *insn = *insn | extract_unsigned_integer (buf, 2);
    }

  /* If we're in VLIW code, then the VLIW width determines the address
     of the next instruction.  */
  if (vliw_mode)
    {
      /* In 32-bit VLIW code, all bundles are 32 bits long.  We ignore the
         coprocessor half of a core / copro bundle.  */
      if (vliw_mode == MEP_OPT_VL32)
        insn_len = 4;

      /* In 64-bit VLIW code, all bundles are 64 bits long.  We ignore the
         coprocessor half of a core / copro bundle.  */
      else if (vliw_mode == MEP_OPT_VL64)
        insn_len = 8;

      /* We'd better be in either core, 32-bit VLIW, or 64-bit VLIW mode.  */
      else
        gdb_assert (0);
    }
  
  /* Otherwise, the top two bits of the major opcode are (again) what
     we need to check.  */
  else if ((*insn & 0xc0000000) == 0xc0000000)
    insn_len = 4;
  else
    insn_len = 2;

  return pc + insn_len;
}


/* Sign-extend the LEN-bit value N.  */
#define SEXT(n, len) ((((int) (n)) ^ (1 << ((len) - 1))) - (1 << ((len) - 1)))

/* Return the LEN-bit field at POS from I.  */
#define FIELD(i, pos, len) (((i) >> (pos)) & ((1 << (len)) - 1))

/* Like FIELD, but sign-extend the field's value.  */
#define SFIELD(i, pos, len) (SEXT (FIELD ((i), (pos), (len)), (len)))


/* Macros for decoding instructions.

   Remember that 16-bit instructions are placed in bits 16..31 of i,
   not at the least significant end; this means that the major opcode
   field is always in the same place, regardless of the width of the
   instruction.  As a reminder of this, we show the lower 16 bits of a
   16-bit instruction as xxxx_xxxx_xxxx_xxxx.  */

/* SB Rn,(Rm)		      0000_nnnn_mmmm_1000 */
/* SH Rn,(Rm)		      0000_nnnn_mmmm_1001 */
/* SW Rn,(Rm)		      0000_nnnn_mmmm_1010 */

/* SW Rn,disp16(Rm)	      1100_nnnn_mmmm_1010 dddd_dddd_dddd_dddd */
#define IS_SW(i)	      (((i) & 0xf00f0000) == 0xc00a0000)
/* SB Rn,disp16(Rm)	      1100_nnnn_mmmm_1000 dddd_dddd_dddd_dddd */
#define IS_SB(i)	      (((i) & 0xf00f0000) == 0xc0080000)
/* SH Rn,disp16(Rm)	      1100_nnnn_mmmm_1001 dddd_dddd_dddd_dddd */
#define IS_SH(i)	      (((i) & 0xf00f0000) == 0xc0090000)
#define SWBH_32_BASE(i)       (FIELD (i, 20, 4))
#define SWBH_32_SOURCE(i)     (FIELD (i, 24, 4))
#define SWBH_32_OFFSET(i)     (SFIELD (i, 0, 16))

/* SW Rn,disp7.align4(SP)     0100_nnnn_0ddd_dd10 xxxx_xxxx_xxxx_xxxx */
#define IS_SW_IMMD(i)	      (((i) & 0xf0830000) == 0x40020000)
#define SW_IMMD_SOURCE(i)     (FIELD (i, 24, 4))
#define SW_IMMD_OFFSET(i)     (FIELD (i, 18, 5) << 2)

/* SW Rn,(Rm)                 0000_nnnn_mmmm_1010 xxxx_xxxx_xxxx_xxxx */
#define IS_SW_REG(i)	      (((i) & 0xf00f0000) == 0x000a0000)
#define SW_REG_SOURCE(i)      (FIELD (i, 24, 4))
#define SW_REG_BASE(i)        (FIELD (i, 20, 4))

/* ADD3 Rl,Rn,Rm              1001_nnnn_mmmm_llll xxxx_xxxx_xxxx_xxxx */
#define IS_ADD3_16_REG(i)     (((i) & 0xf0000000) == 0x90000000)
#define ADD3_16_REG_SRC1(i)   (FIELD (i, 20, 4))               /* n */
#define ADD3_16_REG_SRC2(i)   (FIELD (i, 24, 4))               /* m */

/* ADD3 Rn,Rm,imm16           1100_nnnn_mmmm_0000 iiii_iiii_iiii_iiii */
#define IS_ADD3_32(i)	      (((i) & 0xf00f0000) == 0xc0000000)
#define ADD3_32_TARGET(i)     (FIELD (i, 24, 4))
#define ADD3_32_SOURCE(i)     (FIELD (i, 20, 4))
#define ADD3_32_OFFSET(i)     (SFIELD (i, 0, 16))

/* ADD3 Rn,SP,imm7.align4     0100_nnnn_0iii_ii00 xxxx_xxxx_xxxx_xxxx */
#define IS_ADD3_16(i)  	      (((i) & 0xf0830000) == 0x40000000)
#define ADD3_16_TARGET(i)     (FIELD (i, 24, 4))
#define ADD3_16_OFFSET(i)     (FIELD (i, 18, 5) << 2)

/* ADD Rn,imm6		      0110_nnnn_iiii_ii00 xxxx_xxxx_xxxx_xxxx */
#define IS_ADD(i) 	      (((i) & 0xf0030000) == 0x60000000)
#define ADD_TARGET(i)	      (FIELD (i, 24, 4))
#define ADD_OFFSET(i)         (SFIELD (i, 18, 6))

/* LDC Rn,imm5		      0111_nnnn_iiii_101I xxxx_xxxx_xxxx_xxxx
                              imm5 = I||i[7:4] */
#define IS_LDC(i)	      (((i) & 0xf00e0000) == 0x700a0000)
#define LDC_IMM(i)            ((FIELD (i, 16, 1) << 4) | FIELD (i, 20, 4))
#define LDC_TARGET(i)         (FIELD (i, 24, 4))

/* LW Rn,disp16(Rm)           1100_nnnn_mmmm_1110 dddd_dddd_dddd_dddd  */
#define IS_LW(i)              (((i) & 0xf00f0000) == 0xc00e0000)
#define LW_TARGET(i)          (FIELD (i, 24, 4))
#define LW_BASE(i)            (FIELD (i, 20, 4))
#define LW_OFFSET(i)          (SFIELD (i, 0, 16))

/* MOV Rn,Rm		      0000_nnnn_mmmm_0000 xxxx_xxxx_xxxx_xxxx */
#define IS_MOV(i)	      (((i) & 0xf00f0000) == 0x00000000)
#define MOV_TARGET(i)	      (FIELD (i, 24, 4))
#define MOV_SOURCE(i)	      (FIELD (i, 20, 4))

/* BRA disp12.align2	      1011_dddd_dddd_ddd0 xxxx_xxxx_xxxx_xxxx */
#define IS_BRA(i)	      (((i) & 0xf0010000) == 0xb0000000)
#define BRA_DISP(i)           (SFIELD (i, 17, 11) << 1)


/* This structure holds the results of a prologue analysis.  */
struct mep_prologue
{
  /* The offset from the frame base to the stack pointer --- always
     zero or negative.

     Calling this a "size" is a bit misleading, but given that the
     stack grows downwards, using offsets for everything keeps one
     from going completely sign-crazy: you never change anything's
     sign for an ADD instruction; always change the second operand's
     sign for a SUB instruction; and everything takes care of
     itself.  */
  int frame_size;

  /* Non-zero if this function has initialized the frame pointer from
     the stack pointer, zero otherwise.  */
  int has_frame_ptr;

  /* If has_frame_ptr is non-zero, this is the offset from the frame
     base to where the frame pointer points.  This is always zero or
     negative.  */
  int frame_ptr_offset;

  /* The address of the first instruction at which the frame has been
     set up and the arguments are where the debug info says they are
     --- as best as we can tell.  */
  CORE_ADDR prologue_end;

  /* reg_offset[R] is the offset from the CFA at which register R is
     saved, or 1 if register R has not been saved.  (Real values are
     always zero or negative.)  */
  int reg_offset[MEP_NUM_REGS];
};

/* Return non-zero if VALUE is an incoming argument register.  */

static int
is_arg_reg (pv_t value)
{
  return (value.kind == pvk_register
          && MEP_R1_REGNUM <= value.reg && value.reg <= MEP_R4_REGNUM
          && value.k == 0);
}

/* Return non-zero if a store of REG's current value VALUE to ADDR is
   probably spilling an argument register to its stack slot in STACK.
   Such instructions should be included in the prologue, if possible.

   The store is a spill if:
   - the value being stored is REG's original value;
   - the value has not already been stored somewhere in STACK; and
   - ADDR is a stack slot's address (e.g., relative to the original
     value of the SP).  */
static int
is_arg_spill (pv_t value, pv_t addr, struct pv_area *stack)
{
  return (is_arg_reg (value)
          && pv_is_register (addr, MEP_SP_REGNUM)
          && ! pv_area_find_reg (stack, current_gdbarch, value.reg, 0));
}


/* Function for finding saved registers in a 'struct pv_area'; we pass
   this to pv_area_scan.

   If VALUE is a saved register, ADDR says it was saved at a constant
   offset from the frame base, and SIZE indicates that the whole
   register was saved, record its offset in RESULT_UNTYPED.  */
static void
check_for_saved (void *result_untyped, pv_t addr, CORE_ADDR size, pv_t value)
{
  struct mep_prologue *result = (struct mep_prologue *) result_untyped;

  if (value.kind == pvk_register
      && value.k == 0
      && pv_is_register (addr, MEP_SP_REGNUM)
      && size == register_size (current_gdbarch, value.reg))
    result->reg_offset[value.reg] = addr.k;
}


/* Analyze a prologue starting at START_PC, going no further than
   LIMIT_PC.  Fill in RESULT as appropriate.  */
static void
mep_analyze_prologue (CORE_ADDR start_pc, CORE_ADDR limit_pc,
                      struct mep_prologue *result)
{
  CORE_ADDR pc;
  unsigned long insn;
  int rn;
  int found_lp = 0;
  pv_t reg[MEP_NUM_REGS];
  struct pv_area *stack;
  struct cleanup *back_to;
  CORE_ADDR after_last_frame_setup_insn = start_pc;

  memset (result, 0, sizeof (*result));

  for (rn = 0; rn < MEP_NUM_REGS; rn++)
    {
      reg[rn] = pv_register (rn, 0);
      result->reg_offset[rn] = 1;
    }

  stack = make_pv_area (MEP_SP_REGNUM);
  back_to = make_cleanup_free_pv_area (stack);

  pc = start_pc;
  while (pc < limit_pc)
    {
      CORE_ADDR next_pc;
      pv_t pre_insn_fp, pre_insn_sp;

      next_pc = mep_get_insn (pc, &insn);

      /* A zero return from mep_get_insn means that either we weren't
         able to read the instruction from memory, or that we don't
         have enough information to be able to reliably decode it.  So
         we'll store here and hope for the best.  */
      if (! next_pc)
        break;

      /* Note the current values of the SP and FP, so we can tell if
         this instruction changed them, below.  */
      pre_insn_fp = reg[MEP_FP_REGNUM];
      pre_insn_sp = reg[MEP_SP_REGNUM];

      if (IS_ADD (insn))
        {
          int rn = ADD_TARGET (insn);
          CORE_ADDR imm6 = ADD_OFFSET (insn);

          reg[rn] = pv_add_constant (reg[rn], imm6);
        }
      else if (IS_ADD3_16 (insn))
	{
          int rn = ADD3_16_TARGET (insn);
          int imm7 = ADD3_16_OFFSET (insn);

          reg[rn] = pv_add_constant (reg[MEP_SP_REGNUM], imm7);
        }
      else if (IS_ADD3_32 (insn))
	{
          int rn = ADD3_32_TARGET (insn);
          int rm = ADD3_32_SOURCE (insn);
          int imm16 = ADD3_32_OFFSET (insn);

          reg[rn] = pv_add_constant (reg[rm], imm16);
	}
      else if (IS_SW_REG (insn))
        {
          int rn = SW_REG_SOURCE (insn);
          int rm = SW_REG_BASE (insn);

          /* If simulating this store would require us to forget
             everything we know about the stack frame in the name of
             accuracy, it would be better to just quit now.  */
          if (pv_area_store_would_trash (stack, reg[rm]))
            break;
          
          if (is_arg_spill (reg[rn], reg[rm], stack))
            after_last_frame_setup_insn = next_pc;

          pv_area_store (stack, reg[rm], 4, reg[rn]);
        }
      else if (IS_SW_IMMD (insn))
        {
          int rn = SW_IMMD_SOURCE (insn);
          int offset = SW_IMMD_OFFSET (insn);
          pv_t addr = pv_add_constant (reg[MEP_SP_REGNUM], offset);

          /* If simulating this store would require us to forget
             everything we know about the stack frame in the name of
             accuracy, it would be better to just quit now.  */
          if (pv_area_store_would_trash (stack, addr))
            break;

          if (is_arg_spill (reg[rn], addr, stack))
            after_last_frame_setup_insn = next_pc;

          pv_area_store (stack, addr, 4, reg[rn]);
        }
      else if (IS_MOV (insn))
	{
          int rn = MOV_TARGET (insn);
          int rm = MOV_SOURCE (insn);

          reg[rn] = reg[rm];

	  if (pv_is_register (reg[rm], rm) && is_arg_reg (reg[rm]))
	    after_last_frame_setup_insn = next_pc;
	}
      else if (IS_SB (insn) || IS_SH (insn) || IS_SW (insn))
	{
          int rn = SWBH_32_SOURCE (insn);
          int rm = SWBH_32_BASE (insn);
          int disp = SWBH_32_OFFSET (insn);
          int size = (IS_SB (insn) ? 1
                      : IS_SH (insn) ? 2
                      : IS_SW (insn) ? 4
                      : (gdb_assert (0), 1));
          pv_t addr = pv_add_constant (reg[rm], disp);

          if (pv_area_store_would_trash (stack, addr))
            break;

          if (is_arg_spill (reg[rn], addr, stack))
            after_last_frame_setup_insn = next_pc;

          pv_area_store (stack, addr, size, reg[rn]);
	}
      else if (IS_LDC (insn))
	{
          int rn = LDC_TARGET (insn);
          int cr = LDC_IMM (insn) + MEP_FIRST_CSR_REGNUM;

          reg[rn] = reg[cr];
	}
      else if (IS_LW (insn))
        {
          int rn = LW_TARGET (insn);
          int rm = LW_BASE (insn);
          int offset = LW_OFFSET (insn);
          pv_t addr = pv_add_constant (reg[rm], offset);

          reg[rn] = pv_area_fetch (stack, addr, 4);
        }
      else if (IS_BRA (insn) && BRA_DISP (insn) > 0)
	{
	  /* When a loop appears as the first statement of a function
	     body, gcc 4.x will use a BRA instruction to branch to the
	     loop condition checking code.  This BRA instruction is
	     marked as part of the prologue.  We therefore set next_pc
	     to this branch target and also stop the prologue scan. 
	     The instructions at and beyond the branch target should
	     no longer be associated with the prologue.
	     
	     Note that we only consider forward branches here.  We
	     presume that a forward branch is being used to skip over
	     a loop body.
	     
	     A backwards branch is covered by the default case below.
	     If we were to encounter a backwards branch, that would
	     most likely mean that we've scanned through a loop body.
	     We definitely want to stop the prologue scan when this
	     happens and that is precisely what is done by the default
	     case below.  */
	  next_pc = pc + BRA_DISP (insn);
	  after_last_frame_setup_insn = next_pc;
	  break;
	}
      else
        /* We've hit some instruction we don't know how to simulate.
           Strictly speaking, we should set every value we're
           tracking to "unknown".  But we'll be optimistic, assume
           that we have enough information already, and stop
           analysis here.  */
        break;

      /* If this instruction changed the FP or decreased the SP (i.e.,
         allocated more stack space), then this may be a good place to
         declare the prologue finished.  However, there are some
         exceptions:

         - If the instruction just changed the FP back to its original
           value, then that's probably a restore instruction.  The
           prologue should definitely end before that.  

         - If the instruction increased the value of the SP (that is,
           shrunk the frame), then it's probably part of a frame
           teardown sequence, and the prologue should end before that.  */

      if (! pv_is_identical (reg[MEP_FP_REGNUM], pre_insn_fp))
        {
          if (! pv_is_register_k (reg[MEP_FP_REGNUM], MEP_FP_REGNUM, 0))
            after_last_frame_setup_insn = next_pc;
        }
      else if (! pv_is_identical (reg[MEP_SP_REGNUM], pre_insn_sp))
        {
          /* The comparison of constants looks odd, there, because .k
             is unsigned.  All it really means is that the new value
             is lower than it was before the instruction.  */
          if (pv_is_register (pre_insn_sp, MEP_SP_REGNUM)
              && pv_is_register (reg[MEP_SP_REGNUM], MEP_SP_REGNUM)
              && ((pre_insn_sp.k - reg[MEP_SP_REGNUM].k)
                  < (reg[MEP_SP_REGNUM].k - pre_insn_sp.k)))
            after_last_frame_setup_insn = next_pc;
        }

      pc = next_pc;
    }

  /* Is the frame size (offset, really) a known constant?  */
  if (pv_is_register (reg[MEP_SP_REGNUM], MEP_SP_REGNUM))
    result->frame_size = reg[MEP_SP_REGNUM].k;

  /* Was the frame pointer initialized?  */
  if (pv_is_register (reg[MEP_FP_REGNUM], MEP_SP_REGNUM))
    {
      result->has_frame_ptr = 1;
      result->frame_ptr_offset = reg[MEP_FP_REGNUM].k;
    }

  /* Record where all the registers were saved.  */
  pv_area_scan (stack, check_for_saved, (void *) result);

  result->prologue_end = after_last_frame_setup_insn;

  do_cleanups (back_to);
}


static CORE_ADDR
mep_skip_prologue (CORE_ADDR pc)
{
  char *name;
  CORE_ADDR func_addr, func_end;
  struct mep_prologue p;

  /* Try to find the extent of the function that contains PC.  */
  if (! find_pc_partial_function (pc, &name, &func_addr, &func_end))
    return pc;

  mep_analyze_prologue (pc, func_end, &p);
  return p.prologue_end;
}



/* Breakpoints.  */

static const unsigned char *
mep_breakpoint_from_pc (CORE_ADDR * pcptr, int *lenptr)
{
  static unsigned char breakpoint[] = { 0x70, 0x32 };
  *lenptr = sizeof (breakpoint);
  return breakpoint;
}



/* Frames and frame unwinding.  */


static struct mep_prologue *
mep_analyze_frame_prologue (struct frame_info *next_frame,
                            void **this_prologue_cache)
{
  if (! *this_prologue_cache)
    {
      CORE_ADDR func_start, stop_addr;

      *this_prologue_cache 
        = FRAME_OBSTACK_ZALLOC (struct mep_prologue);

      func_start = frame_func_unwind (next_frame, NORMAL_FRAME);
      stop_addr = frame_pc_unwind (next_frame);

      /* If we couldn't find any function containing the PC, then
         just initialize the prologue cache, but don't do anything.  */
      if (! func_start)
        stop_addr = func_start;

      mep_analyze_prologue (func_start, stop_addr, *this_prologue_cache);
    }

  return *this_prologue_cache;
}


/* Given the next frame and a prologue cache, return this frame's
   base.  */
static CORE_ADDR
mep_frame_base (struct frame_info *next_frame,
                void **this_prologue_cache)
{
  struct mep_prologue *p
    = mep_analyze_frame_prologue (next_frame, this_prologue_cache);

  /* In functions that use alloca, the distance between the stack
     pointer and the frame base varies dynamically, so we can't use
     the SP plus static information like prologue analysis to find the
     frame base.  However, such functions must have a frame pointer,
     to be able to restore the SP on exit.  So whenever we do have a
     frame pointer, use that to find the base.  */
  if (p->has_frame_ptr)
    {
      CORE_ADDR fp
        = frame_unwind_register_unsigned (next_frame, MEP_FP_REGNUM);
      return fp - p->frame_ptr_offset;
    }
  else
    {
      CORE_ADDR sp
        = frame_unwind_register_unsigned (next_frame, MEP_SP_REGNUM);
      return sp - p->frame_size;
    }
}


static void
mep_frame_this_id (struct frame_info *next_frame,
                   void **this_prologue_cache,
                   struct frame_id *this_id)
{
  *this_id = frame_id_build (mep_frame_base (next_frame, this_prologue_cache),
                             frame_func_unwind (next_frame, NORMAL_FRAME));
}


static void
mep_frame_prev_register (struct frame_info *next_frame,
                         void **this_prologue_cache,
                         int regnum, int *optimizedp,
                         enum lval_type *lvalp, CORE_ADDR *addrp,
                         int *realnump, gdb_byte *bufferp)
{
  struct mep_prologue *p
    = mep_analyze_frame_prologue (next_frame, this_prologue_cache);

  /* There are a number of complications in unwinding registers on the
     MeP, having to do with core functions calling VLIW functions and
     vice versa.

     The least significant bit of the link register, LP.LTOM, is the
     VLIW mode toggle bit: it's set if a core function called a VLIW
     function, or vice versa, and clear when the caller and callee
     were both in the same mode.

     So, if we're asked to unwind the PC, then we really want to
     unwind the LP and clear the least significant bit.  (Real return
     addresses are always even.)  And if we want to unwind the program
     status word (PSW), we need to toggle PSW.OM if LP.LTOM is set.

     Tweaking the register values we return in this way means that the
     bits in BUFFERP[] are not the same as the bits you'd find at
     ADDRP in the inferior, so we make sure lvalp is not_lval when we
     do this.  */
  if (regnum == MEP_PC_REGNUM)
    {
      mep_frame_prev_register (next_frame, this_prologue_cache, MEP_LP_REGNUM,
                               optimizedp, lvalp, addrp, realnump, bufferp);
      store_unsigned_integer (bufferp, MEP_LP_SIZE, 
                              (extract_unsigned_integer (bufferp, MEP_LP_SIZE)
                               & ~1));
      *lvalp = not_lval;
    }
  else
    {
      CORE_ADDR frame_base = mep_frame_base (next_frame, this_prologue_cache);
      int reg_size = register_size (get_frame_arch (next_frame), regnum);

      /* Our caller's SP is our frame base.  */
      if (regnum == MEP_SP_REGNUM)
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
        frame_register_unwind (next_frame, regnum,
                               optimizedp, lvalp, addrp, realnump, bufferp);

      /* If we need to toggle the operating mode, do so.  */
      if (regnum == MEP_PSW_REGNUM)
        {
          int lp_optimized;
          enum lval_type lp_lval;
          CORE_ADDR lp_addr;
          int lp_realnum;
          char lp_buffer[MEP_LP_SIZE];

          /* Get the LP's value, too.  */
          frame_register_unwind (next_frame, MEP_LP_REGNUM,
                                 &lp_optimized, &lp_lval, &lp_addr,
                                 &lp_realnum, lp_buffer);

          /* If LP.LTOM is set, then toggle PSW.OM.  */
          if (extract_unsigned_integer (lp_buffer, MEP_LP_SIZE) & 0x1)
            store_unsigned_integer
              (bufferp, MEP_PSW_SIZE,
               (extract_unsigned_integer (bufferp, MEP_PSW_SIZE) ^ 0x1000));
          *lvalp = not_lval;
        }
    }
}


static const struct frame_unwind mep_frame_unwind = {
  NORMAL_FRAME,
  mep_frame_this_id,
  mep_frame_prev_register
};


static const struct frame_unwind *
mep_frame_sniffer (struct frame_info *next_frame)
{
  return &mep_frame_unwind;
}


/* Our general unwinding function can handle unwinding the PC.  */
static CORE_ADDR
mep_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, MEP_PC_REGNUM);
}


/* Our general unwinding function can handle unwinding the SP.  */
static CORE_ADDR
mep_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, MEP_SP_REGNUM);
}



/* Return values.  */


static int
mep_use_struct_convention (struct type *type)
{
  return (TYPE_LENGTH (type) > MEP_GPR_SIZE);
}


static void
mep_extract_return_value (struct gdbarch *arch,
                          struct type *type,
                          struct regcache *regcache,
                          gdb_byte *valbuf)
{
  int byte_order = gdbarch_byte_order (arch);

  /* Values that don't occupy a full register appear at the less
     significant end of the value.  This is the offset to where the
     value starts.  */
  int offset;

  /* Return values > MEP_GPR_SIZE bytes are returned in memory,
     pointed to by R0.  */
  gdb_assert (TYPE_LENGTH (type) <= MEP_GPR_SIZE);

  if (byte_order == BFD_ENDIAN_BIG)
    offset = MEP_GPR_SIZE - TYPE_LENGTH (type);
  else
    offset = 0;

  /* Return values that do fit in a single register are returned in R0. */
  regcache_cooked_read_part (regcache, MEP_R0_REGNUM,
                             offset, TYPE_LENGTH (type),
                             valbuf);
}


static void
mep_store_return_value (struct gdbarch *arch,
                        struct type *type,
                        struct regcache *regcache,
                        const gdb_byte *valbuf)
{
  int byte_order = gdbarch_byte_order (arch);

  /* Values that fit in a single register go in R0.  */
  if (TYPE_LENGTH (type) <= MEP_GPR_SIZE)
    {
      /* Values that don't occupy a full register appear at the least
         significant end of the value.  This is the offset to where the
         value starts.  */
      int offset;

      if (byte_order == BFD_ENDIAN_BIG)
        offset = MEP_GPR_SIZE - TYPE_LENGTH (type);
      else
        offset = 0;

      regcache_cooked_write_part (regcache, MEP_R0_REGNUM,
                                  offset, TYPE_LENGTH (type),
                                  valbuf);
    }

  /* Return values larger than a single register are returned in
     memory, pointed to by R0.  Unfortunately, we can't count on R0
     pointing to the return buffer, so we raise an error here. */
  else
    error ("GDB cannot set return values larger than four bytes; "
           "the Media Processor's\n"
           "calling conventions do not provide enough information "
           "to do this.\n"
           "Try using the 'return' command with no argument.");
}

enum return_value_convention
mep_return_value (struct gdbarch *gdbarch, struct type *type,
		  struct regcache *regcache, gdb_byte *readbuf,
		  const gdb_byte *writebuf)
{
  if (mep_use_struct_convention (type))
    {
      if (readbuf)
	{
	  ULONGEST addr;
	  /* Although the address of the struct buffer gets passed in R1, it's
	     returned in R0.  Fetch R0's value and then read the memory
	     at that address.  */
	  regcache_raw_read_unsigned (regcache, MEP_R0_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}
      if (writebuf)
	{
	  /* Return values larger than a single register are returned in
	     memory, pointed to by R0.  Unfortunately, we can't count on R0
	     pointing to the return buffer, so we raise an error here. */
	  error ("GDB cannot set return values larger than four bytes; "
		 "the Media Processor's\n"
		 "calling conventions do not provide enough information "
		 "to do this.\n"
		 "Try using the 'return' command with no argument.");
	}
      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  if (readbuf)
    mep_extract_return_value (gdbarch, type, regcache, readbuf);
  if (writebuf)
    mep_store_return_value (gdbarch, type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}


/* Inferior calls.  */


static CORE_ADDR
mep_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Require word alignment.  */
  return sp & -4;
}


/* From "lang_spec2.txt":

   4.2 Calling conventions

   4.2.1 Core register conventions

   - Parameters should be evaluated from left to right, and they
     should be held in $1,$2,$3,$4 in order. The fifth parameter or
     after should be held in the stack. If the size is larger than 4
     bytes in the first four parameters, the pointer should be held in
     the registers instead. If the size is larger than 4 bytes in the
     fifth parameter or after, the pointer should be held in the stack.

   - Return value of a function should be held in register $0. If the
     size of return value is larger than 4 bytes, $1 should hold the
     pointer pointing memory that would hold the return value. In this
     case, the first parameter should be held in $2, the second one in
     $3, and the third one in $4, and the forth parameter or after
     should be held in the stack.

   [This doesn't say so, but arguments shorter than four bytes are
   passed in the least significant end of a four-byte word when
   they're passed on the stack.]  */


/* Traverse the list of ARGC arguments ARGV; for every ARGV[i] too
   large to fit in a register, save it on the stack, and place its
   address in COPY[i].  SP is the initial stack pointer; return the
   new stack pointer.  */
static CORE_ADDR
push_large_arguments (CORE_ADDR sp, int argc, struct value **argv,
                      CORE_ADDR copy[])
{
  int i;

  for (i = 0; i < argc; i++)
    {
      unsigned arg_len = TYPE_LENGTH (value_type (argv[i]));

      if (arg_len > MEP_GPR_SIZE)
        {
          /* Reserve space for the copy, and then round the SP down, to
             make sure it's all aligned properly.  */
          sp = (sp - arg_len) & -4;
          write_memory (sp, value_contents (argv[i]), arg_len);
          copy[i] = sp;
        }
    }

  return sp;
}


static CORE_ADDR
mep_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
                     struct regcache *regcache, CORE_ADDR bp_addr,
                     int argc, struct value **argv, CORE_ADDR sp,
                     int struct_return,
                     CORE_ADDR struct_addr)
{
  CORE_ADDR *copy = (CORE_ADDR *) alloca (argc * sizeof (copy[0]));
  CORE_ADDR func_addr = find_function_addr (function, NULL);
  int i;

  /* The number of the next register available to hold an argument.  */
  int arg_reg;

  /* The address of the next stack slot available to hold an argument.  */
  CORE_ADDR arg_stack;

  /* The address of the end of the stack area for arguments.  This is
     just for error checking.  */
  CORE_ADDR arg_stack_end;
  
  sp = push_large_arguments (sp, argc, argv, copy);

  /* Reserve space for the stack arguments, if any.  */
  arg_stack_end = sp;
  if (argc + (struct_addr ? 1 : 0) > 4)
    sp -= ((argc + (struct_addr ? 1 : 0)) - 4) * MEP_GPR_SIZE;

  arg_reg = MEP_R1_REGNUM;
  arg_stack = sp;

  /* If we're returning a structure by value, push the pointer to the
     buffer as the first argument.  */
  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, arg_reg, struct_addr);
      arg_reg++;
    }

  for (i = 0; i < argc; i++)
    {
      unsigned arg_size = TYPE_LENGTH (value_type (argv[i]));
      ULONGEST value;

      /* Arguments that fit in a GPR get expanded to fill the GPR.  */
      if (arg_size <= MEP_GPR_SIZE)
        value = extract_unsigned_integer (value_contents (argv[i]),
                                          TYPE_LENGTH (value_type (argv[i])));

      /* Arguments too large to fit in a GPR get copied to the stack,
         and we pass a pointer to the copy.  */
      else
        value = copy[i];

      /* We use $1 -- $4 for passing arguments, then use the stack.  */
      if (arg_reg <= MEP_R4_REGNUM)
        {
          regcache_cooked_write_unsigned (regcache, arg_reg, value);
          arg_reg++;
        }
      else
        {
          char buf[MEP_GPR_SIZE];
          store_unsigned_integer (buf, MEP_GPR_SIZE, value);
          write_memory (arg_stack, buf, MEP_GPR_SIZE);
          arg_stack += MEP_GPR_SIZE;
        }
    }

  gdb_assert (arg_stack <= arg_stack_end);

  /* Set the return address.  */
  regcache_cooked_write_unsigned (regcache, MEP_LP_REGNUM, bp_addr);

  /* Update the stack pointer.  */
  regcache_cooked_write_unsigned (regcache, MEP_SP_REGNUM, sp);
  
  return sp;
}


static struct frame_id
mep_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (mep_unwind_sp (gdbarch, next_frame),
                         frame_pc_unwind (next_frame));
}



/* Initialization.  */


static struct gdbarch *
mep_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  /* Which me_module are we building a gdbarch object for?  */
  CONFIG_ATTR me_module;

  /* If we have a BFD in hand, figure out which me_module it was built
     for.  Otherwise, use the no-particular-me_module code.  */
  if (info.abfd)
    {
      /* The way to get the me_module code depends on the object file
         format.  At the moment, we only know how to handle ELF.  */
      if (bfd_get_flavour (info.abfd) == bfd_target_elf_flavour)
        me_module = elf_elfheader (info.abfd)->e_flags & EF_MEP_INDEX_MASK;
      else
        me_module = CONFIG_NONE;
    }
  else
    me_module = CONFIG_NONE;

  /* If we're setting the architecture from a file, check the
     endianness of the file against that of the me_module.  */
  if (info.abfd)
    {
      /* The negations on either side make the comparison treat all
         non-zero (true) values as equal.  */
      if (! bfd_big_endian (info.abfd) != ! me_module_big_endian (me_module))
        {
          const char *module_name = me_module_name (me_module);
          const char *module_endianness
            = me_module_big_endian (me_module) ? "big" : "little";
          const char *file_name = bfd_get_filename (info.abfd);
          const char *file_endianness
            = bfd_big_endian (info.abfd) ? "big" : "little";
          
          fputc_unfiltered ('\n', gdb_stderr);
          if (module_name)
            warning ("the MeP module '%s' is %s-endian, but the executable\n"
                     "%s is %s-endian.",
                     module_name, module_endianness,
                     file_name, file_endianness);
          else
            warning ("the selected MeP module is %s-endian, but the "
                     "executable\n"
                     "%s is %s-endian.",
                     module_endianness, file_name, file_endianness);
        }
    }

  /* Find a candidate among the list of architectures we've created
     already.  info->bfd_arch_info needs to match, but we also want
     the right me_module: the ELF header's e_flags field needs to
     match as well.  */
  for (arches = gdbarch_list_lookup_by_info (arches, &info); 
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    if (gdbarch_tdep (arches->gdbarch)->me_module == me_module)
      return arches->gdbarch;

  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  /* Get a CGEN CPU descriptor for this architecture.  */
  {
    const char *mach_name = info.bfd_arch_info->printable_name;
    enum cgen_endian endian = (info.byte_order == BFD_ENDIAN_BIG
                               ? CGEN_ENDIAN_BIG
                               : CGEN_ENDIAN_LITTLE);

    tdep->cpu_desc = mep_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
                                        CGEN_CPU_OPEN_ENDIAN, endian,
                                        CGEN_CPU_OPEN_END);
  }

  tdep->me_module = me_module;

  /* Register set.  */
  set_gdbarch_read_pc (gdbarch, mep_read_pc);
  set_gdbarch_write_pc (gdbarch, mep_write_pc);
  set_gdbarch_num_regs (gdbarch, MEP_NUM_RAW_REGS);
  set_gdbarch_sp_regnum (gdbarch, MEP_SP_REGNUM);
  set_gdbarch_register_name (gdbarch, mep_register_name);
  set_gdbarch_register_type (gdbarch, mep_register_type);
  set_gdbarch_num_pseudo_regs (gdbarch, MEP_NUM_PSEUDO_REGS);
  set_gdbarch_pseudo_register_read (gdbarch, mep_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, mep_pseudo_register_write);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, mep_debug_reg_to_regnum);
  set_gdbarch_stab_reg_to_regnum (gdbarch, mep_debug_reg_to_regnum);

  set_gdbarch_register_reggroup_p (gdbarch, mep_register_reggroup_p);
  reggroup_add (gdbarch, all_reggroup);
  reggroup_add (gdbarch, general_reggroup);
  reggroup_add (gdbarch, save_reggroup);
  reggroup_add (gdbarch, restore_reggroup);
  reggroup_add (gdbarch, mep_csr_reggroup);
  reggroup_add (gdbarch, mep_cr_reggroup);
  reggroup_add (gdbarch, mep_ccr_reggroup);

  /* Disassembly.  */
  set_gdbarch_print_insn (gdbarch, mep_gdb_print_insn); 

  /* Breakpoints.  */
  set_gdbarch_breakpoint_from_pc (gdbarch, mep_breakpoint_from_pc);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_skip_prologue (gdbarch, mep_skip_prologue);

  /* Frames and frame unwinding.  */
  frame_unwind_append_sniffer (gdbarch, mep_frame_sniffer);
  set_gdbarch_unwind_pc (gdbarch, mep_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, mep_unwind_sp);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_frame_args_skip (gdbarch, 0);

  /* Return values.  */
  set_gdbarch_return_value (gdbarch, mep_return_value);
  
  /* Inferior function calls.  */
  set_gdbarch_frame_align (gdbarch, mep_frame_align);
  set_gdbarch_push_dummy_call (gdbarch, mep_push_dummy_call);
  set_gdbarch_unwind_dummy_id (gdbarch, mep_unwind_dummy_id);

  return gdbarch;
}


void
_initialize_mep_tdep (void)
{
  mep_csr_reggroup = reggroup_new ("csr", USER_REGGROUP);
  mep_cr_reggroup  = reggroup_new ("cr", USER_REGGROUP); 
  mep_ccr_reggroup = reggroup_new ("ccr", USER_REGGROUP);

  register_gdbarch_init (bfd_arch_mep, mep_gdbarch_init);

  mep_init_pseudoregister_maps ();
}
