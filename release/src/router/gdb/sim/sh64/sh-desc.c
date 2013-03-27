/* CPU data for sh.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "sysdep.h"
#include <stdio.h>
#include <stdarg.h>
#include "ansidecl.h"
#include "bfd.h"
#include "symcat.h"
#include "sh-desc.h"
#include "sh-opc.h"
#include "opintl.h"
#include "libiberty.h"
#include "xregex.h"

/* Attributes.  */

static const CGEN_ATTR_ENTRY bool_attr[] =
{
  { "#f", 0 },
  { "#t", 1 },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY MACH_attr[] ATTRIBUTE_UNUSED =
{
  { "base", MACH_BASE },
  { "sh2", MACH_SH2 },
  { "sh2e", MACH_SH2E },
  { "sh2a_fpu", MACH_SH2A_FPU },
  { "sh2a_nofpu", MACH_SH2A_NOFPU },
  { "sh3", MACH_SH3 },
  { "sh3e", MACH_SH3E },
  { "sh4_nofpu", MACH_SH4_NOFPU },
  { "sh4", MACH_SH4 },
  { "sh4a_nofpu", MACH_SH4A_NOFPU },
  { "sh4a", MACH_SH4A },
  { "sh4al", MACH_SH4AL },
  { "sh5", MACH_SH5 },
  { "max", MACH_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY ISA_attr[] ATTRIBUTE_UNUSED =
{
  { "compact", ISA_COMPACT },
  { "media", ISA_MEDIA },
  { "max", ISA_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY SH4_GROUP_attr[] ATTRIBUTE_UNUSED =
{
  { "NONE", SH4_GROUP_NONE },
  { "MT", SH4_GROUP_MT },
  { "EX", SH4_GROUP_EX },
  { "BR", SH4_GROUP_BR },
  { "LS", SH4_GROUP_LS },
  { "FE", SH4_GROUP_FE },
  { "CO", SH4_GROUP_CO },
  { "MAX", SH4_GROUP_MAX },
  { 0, 0 }
};

static const CGEN_ATTR_ENTRY SH4A_GROUP_attr[] ATTRIBUTE_UNUSED =
{
  { "NONE", SH4A_GROUP_NONE },
  { "MT", SH4A_GROUP_MT },
  { "EX", SH4A_GROUP_EX },
  { "BR", SH4A_GROUP_BR },
  { "LS", SH4A_GROUP_LS },
  { "FE", SH4A_GROUP_FE },
  { "CO", SH4A_GROUP_CO },
  { "MAX", SH4A_GROUP_MAX },
  { 0, 0 }
};

const CGEN_ATTR_TABLE sh_cgen_ifield_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "ISA", & ISA_attr[0], & ISA_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "RESERVED", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE sh_cgen_hardware_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "ISA", & ISA_attr[0], & ISA_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "CACHE-ADDR", &bool_attr[0], &bool_attr[0] },
  { "PC", &bool_attr[0], &bool_attr[0] },
  { "PROFILE", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE sh_cgen_operand_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "ISA", & ISA_attr[0], & ISA_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "PCREL-ADDR", &bool_attr[0], &bool_attr[0] },
  { "ABS-ADDR", &bool_attr[0], &bool_attr[0] },
  { "SIGN-OPT", &bool_attr[0], &bool_attr[0] },
  { "SIGNED", &bool_attr[0], &bool_attr[0] },
  { "NEGATIVE", &bool_attr[0], &bool_attr[0] },
  { "RELAX", &bool_attr[0], &bool_attr[0] },
  { "SEM-ONLY", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

const CGEN_ATTR_TABLE sh_cgen_insn_attr_table[] =
{
  { "MACH", & MACH_attr[0], & MACH_attr[0] },
  { "ISA", & ISA_attr[0], & ISA_attr[0] },
  { "SH4-GROUP", & SH4_GROUP_attr[0], & SH4_GROUP_attr[0] },
  { "SH4A-GROUP", & SH4A_GROUP_attr[0], & SH4A_GROUP_attr[0] },
  { "ALIAS", &bool_attr[0], &bool_attr[0] },
  { "VIRTUAL", &bool_attr[0], &bool_attr[0] },
  { "UNCOND-CTI", &bool_attr[0], &bool_attr[0] },
  { "COND-CTI", &bool_attr[0], &bool_attr[0] },
  { "SKIP-CTI", &bool_attr[0], &bool_attr[0] },
  { "DELAY-SLOT", &bool_attr[0], &bool_attr[0] },
  { "RELAXABLE", &bool_attr[0], &bool_attr[0] },
  { "RELAXED", &bool_attr[0], &bool_attr[0] },
  { "NO-DIS", &bool_attr[0], &bool_attr[0] },
  { "PBB", &bool_attr[0], &bool_attr[0] },
  { "ILLSLOT", &bool_attr[0], &bool_attr[0] },
  { "FP-INSN", &bool_attr[0], &bool_attr[0] },
  { "32-BIT-INSN", &bool_attr[0], &bool_attr[0] },
  { 0, 0, 0 }
};

/* Instruction set variants.  */

static const CGEN_ISA sh_cgen_isa_table[] = {
  { "media", 32, 32, 32, 32 },
  { "compact", 32, 32, 16, 32 },
  { 0, 0, 0, 0, 0 }
};

/* Machine variants.  */

static const CGEN_MACH sh_cgen_mach_table[] = {
  { "sh2", "sh2", MACH_SH2, 0 },
  { "sh2e", "sh2e", MACH_SH2E, 0 },
  { "sh2a-fpu", "sh2a-fpu", MACH_SH2A_FPU, 0 },
  { "sh2a-nofpu", "sh2a-nofpu", MACH_SH2A_NOFPU, 0 },
  { "sh3", "sh3", MACH_SH3, 0 },
  { "sh3e", "sh3e", MACH_SH3E, 0 },
  { "sh4-nofpu", "sh4-nofpu", MACH_SH4_NOFPU, 0 },
  { "sh4", "sh4", MACH_SH4, 0 },
  { "sh4a-nofpu", "sh4a-nofpu", MACH_SH4A_NOFPU, 0 },
  { "sh4a", "sh4a", MACH_SH4A, 0 },
  { "sh4al", "sh4al", MACH_SH4AL, 0 },
  { "sh5", "sh5", MACH_SH5, 0 },
  { 0, 0, 0, 0 }
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_frc_names_entries[] =
{
  { "fr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "fr1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "fr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "fr3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "fr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "fr5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "fr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "fr7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "fr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "fr9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "fr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "fr11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "fr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "fr13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "fr14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "fr15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_frc_names =
{
  & sh_cgen_opval_frc_names_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_drc_names_entries[] =
{
  { "dr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "dr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "dr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "dr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "dr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "dr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "dr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "dr14", 14, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_drc_names =
{
  & sh_cgen_opval_drc_names_entries[0],
  8,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_xf_names_entries[] =
{
  { "xf0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "xf1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "xf2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "xf3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "xf4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "xf5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "xf6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "xf7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "xf8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "xf9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "xf10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "xf11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "xf12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "xf13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "xf14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "xf15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_xf_names =
{
  & sh_cgen_opval_xf_names_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_gr_entries[] =
{
  { "r0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "r1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "r2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "r3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "r4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "r5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "r6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "r7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "r8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "r9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "r10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "r11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "r12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "r13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "r14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "r15", 15, {0, {{{0, 0}}}}, 0, 0 },
  { "r16", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "r17", 17, {0, {{{0, 0}}}}, 0, 0 },
  { "r18", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "r19", 19, {0, {{{0, 0}}}}, 0, 0 },
  { "r20", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "r21", 21, {0, {{{0, 0}}}}, 0, 0 },
  { "r22", 22, {0, {{{0, 0}}}}, 0, 0 },
  { "r23", 23, {0, {{{0, 0}}}}, 0, 0 },
  { "r24", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "r25", 25, {0, {{{0, 0}}}}, 0, 0 },
  { "r26", 26, {0, {{{0, 0}}}}, 0, 0 },
  { "r27", 27, {0, {{{0, 0}}}}, 0, 0 },
  { "r28", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "r29", 29, {0, {{{0, 0}}}}, 0, 0 },
  { "r30", 30, {0, {{{0, 0}}}}, 0, 0 },
  { "r31", 31, {0, {{{0, 0}}}}, 0, 0 },
  { "r32", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "r33", 33, {0, {{{0, 0}}}}, 0, 0 },
  { "r34", 34, {0, {{{0, 0}}}}, 0, 0 },
  { "r35", 35, {0, {{{0, 0}}}}, 0, 0 },
  { "r36", 36, {0, {{{0, 0}}}}, 0, 0 },
  { "r37", 37, {0, {{{0, 0}}}}, 0, 0 },
  { "r38", 38, {0, {{{0, 0}}}}, 0, 0 },
  { "r39", 39, {0, {{{0, 0}}}}, 0, 0 },
  { "r40", 40, {0, {{{0, 0}}}}, 0, 0 },
  { "r41", 41, {0, {{{0, 0}}}}, 0, 0 },
  { "r42", 42, {0, {{{0, 0}}}}, 0, 0 },
  { "r43", 43, {0, {{{0, 0}}}}, 0, 0 },
  { "r44", 44, {0, {{{0, 0}}}}, 0, 0 },
  { "r45", 45, {0, {{{0, 0}}}}, 0, 0 },
  { "r46", 46, {0, {{{0, 0}}}}, 0, 0 },
  { "r47", 47, {0, {{{0, 0}}}}, 0, 0 },
  { "r48", 48, {0, {{{0, 0}}}}, 0, 0 },
  { "r49", 49, {0, {{{0, 0}}}}, 0, 0 },
  { "r50", 50, {0, {{{0, 0}}}}, 0, 0 },
  { "r51", 51, {0, {{{0, 0}}}}, 0, 0 },
  { "r52", 52, {0, {{{0, 0}}}}, 0, 0 },
  { "r53", 53, {0, {{{0, 0}}}}, 0, 0 },
  { "r54", 54, {0, {{{0, 0}}}}, 0, 0 },
  { "r55", 55, {0, {{{0, 0}}}}, 0, 0 },
  { "r56", 56, {0, {{{0, 0}}}}, 0, 0 },
  { "r57", 57, {0, {{{0, 0}}}}, 0, 0 },
  { "r58", 58, {0, {{{0, 0}}}}, 0, 0 },
  { "r59", 59, {0, {{{0, 0}}}}, 0, 0 },
  { "r60", 60, {0, {{{0, 0}}}}, 0, 0 },
  { "r61", 61, {0, {{{0, 0}}}}, 0, 0 },
  { "r62", 62, {0, {{{0, 0}}}}, 0, 0 },
  { "r63", 63, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_gr =
{
  & sh_cgen_opval_h_gr_entries[0],
  64,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_grc_entries[] =
{
  { "r0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "r1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "r2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "r3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "r4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "r5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "r6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "r7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "r8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "r9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "r10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "r11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "r12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "r13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "r14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "r15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_grc =
{
  & sh_cgen_opval_h_grc_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_cr_entries[] =
{
  { "cr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "cr1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "cr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "cr3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "cr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "cr5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "cr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "cr7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "cr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "cr9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "cr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "cr11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "cr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "cr13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "cr14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "cr15", 15, {0, {{{0, 0}}}}, 0, 0 },
  { "cr16", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "cr17", 17, {0, {{{0, 0}}}}, 0, 0 },
  { "cr18", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "cr19", 19, {0, {{{0, 0}}}}, 0, 0 },
  { "cr20", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "cr21", 21, {0, {{{0, 0}}}}, 0, 0 },
  { "cr22", 22, {0, {{{0, 0}}}}, 0, 0 },
  { "cr23", 23, {0, {{{0, 0}}}}, 0, 0 },
  { "cr24", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "cr25", 25, {0, {{{0, 0}}}}, 0, 0 },
  { "cr26", 26, {0, {{{0, 0}}}}, 0, 0 },
  { "cr27", 27, {0, {{{0, 0}}}}, 0, 0 },
  { "cr28", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "cr29", 29, {0, {{{0, 0}}}}, 0, 0 },
  { "cr30", 30, {0, {{{0, 0}}}}, 0, 0 },
  { "cr31", 31, {0, {{{0, 0}}}}, 0, 0 },
  { "cr32", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "cr33", 33, {0, {{{0, 0}}}}, 0, 0 },
  { "cr34", 34, {0, {{{0, 0}}}}, 0, 0 },
  { "cr35", 35, {0, {{{0, 0}}}}, 0, 0 },
  { "cr36", 36, {0, {{{0, 0}}}}, 0, 0 },
  { "cr37", 37, {0, {{{0, 0}}}}, 0, 0 },
  { "cr38", 38, {0, {{{0, 0}}}}, 0, 0 },
  { "cr39", 39, {0, {{{0, 0}}}}, 0, 0 },
  { "cr40", 40, {0, {{{0, 0}}}}, 0, 0 },
  { "cr41", 41, {0, {{{0, 0}}}}, 0, 0 },
  { "cr42", 42, {0, {{{0, 0}}}}, 0, 0 },
  { "cr43", 43, {0, {{{0, 0}}}}, 0, 0 },
  { "cr44", 44, {0, {{{0, 0}}}}, 0, 0 },
  { "cr45", 45, {0, {{{0, 0}}}}, 0, 0 },
  { "cr46", 46, {0, {{{0, 0}}}}, 0, 0 },
  { "cr47", 47, {0, {{{0, 0}}}}, 0, 0 },
  { "cr48", 48, {0, {{{0, 0}}}}, 0, 0 },
  { "cr49", 49, {0, {{{0, 0}}}}, 0, 0 },
  { "cr50", 50, {0, {{{0, 0}}}}, 0, 0 },
  { "cr51", 51, {0, {{{0, 0}}}}, 0, 0 },
  { "cr52", 52, {0, {{{0, 0}}}}, 0, 0 },
  { "cr53", 53, {0, {{{0, 0}}}}, 0, 0 },
  { "cr54", 54, {0, {{{0, 0}}}}, 0, 0 },
  { "cr55", 55, {0, {{{0, 0}}}}, 0, 0 },
  { "cr56", 56, {0, {{{0, 0}}}}, 0, 0 },
  { "cr57", 57, {0, {{{0, 0}}}}, 0, 0 },
  { "cr58", 58, {0, {{{0, 0}}}}, 0, 0 },
  { "cr59", 59, {0, {{{0, 0}}}}, 0, 0 },
  { "cr60", 60, {0, {{{0, 0}}}}, 0, 0 },
  { "cr61", 61, {0, {{{0, 0}}}}, 0, 0 },
  { "cr62", 62, {0, {{{0, 0}}}}, 0, 0 },
  { "cr63", 63, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_cr =
{
  & sh_cgen_opval_h_cr_entries[0],
  64,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_fr_entries[] =
{
  { "fr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "fr1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "fr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "fr3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "fr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "fr5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "fr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "fr7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "fr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "fr9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "fr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "fr11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "fr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "fr13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "fr14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "fr15", 15, {0, {{{0, 0}}}}, 0, 0 },
  { "fr16", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "fr17", 17, {0, {{{0, 0}}}}, 0, 0 },
  { "fr18", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "fr19", 19, {0, {{{0, 0}}}}, 0, 0 },
  { "fr20", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "fr21", 21, {0, {{{0, 0}}}}, 0, 0 },
  { "fr22", 22, {0, {{{0, 0}}}}, 0, 0 },
  { "fr23", 23, {0, {{{0, 0}}}}, 0, 0 },
  { "fr24", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "fr25", 25, {0, {{{0, 0}}}}, 0, 0 },
  { "fr26", 26, {0, {{{0, 0}}}}, 0, 0 },
  { "fr27", 27, {0, {{{0, 0}}}}, 0, 0 },
  { "fr28", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "fr29", 29, {0, {{{0, 0}}}}, 0, 0 },
  { "fr30", 30, {0, {{{0, 0}}}}, 0, 0 },
  { "fr31", 31, {0, {{{0, 0}}}}, 0, 0 },
  { "fr32", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "fr33", 33, {0, {{{0, 0}}}}, 0, 0 },
  { "fr34", 34, {0, {{{0, 0}}}}, 0, 0 },
  { "fr35", 35, {0, {{{0, 0}}}}, 0, 0 },
  { "fr36", 36, {0, {{{0, 0}}}}, 0, 0 },
  { "fr37", 37, {0, {{{0, 0}}}}, 0, 0 },
  { "fr38", 38, {0, {{{0, 0}}}}, 0, 0 },
  { "fr39", 39, {0, {{{0, 0}}}}, 0, 0 },
  { "fr40", 40, {0, {{{0, 0}}}}, 0, 0 },
  { "fr41", 41, {0, {{{0, 0}}}}, 0, 0 },
  { "fr42", 42, {0, {{{0, 0}}}}, 0, 0 },
  { "fr43", 43, {0, {{{0, 0}}}}, 0, 0 },
  { "fr44", 44, {0, {{{0, 0}}}}, 0, 0 },
  { "fr45", 45, {0, {{{0, 0}}}}, 0, 0 },
  { "fr46", 46, {0, {{{0, 0}}}}, 0, 0 },
  { "fr47", 47, {0, {{{0, 0}}}}, 0, 0 },
  { "fr48", 48, {0, {{{0, 0}}}}, 0, 0 },
  { "fr49", 49, {0, {{{0, 0}}}}, 0, 0 },
  { "fr50", 50, {0, {{{0, 0}}}}, 0, 0 },
  { "fr51", 51, {0, {{{0, 0}}}}, 0, 0 },
  { "fr52", 52, {0, {{{0, 0}}}}, 0, 0 },
  { "fr53", 53, {0, {{{0, 0}}}}, 0, 0 },
  { "fr54", 54, {0, {{{0, 0}}}}, 0, 0 },
  { "fr55", 55, {0, {{{0, 0}}}}, 0, 0 },
  { "fr56", 56, {0, {{{0, 0}}}}, 0, 0 },
  { "fr57", 57, {0, {{{0, 0}}}}, 0, 0 },
  { "fr58", 58, {0, {{{0, 0}}}}, 0, 0 },
  { "fr59", 59, {0, {{{0, 0}}}}, 0, 0 },
  { "fr60", 60, {0, {{{0, 0}}}}, 0, 0 },
  { "fr61", 61, {0, {{{0, 0}}}}, 0, 0 },
  { "fr62", 62, {0, {{{0, 0}}}}, 0, 0 },
  { "fr63", 63, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_fr =
{
  & sh_cgen_opval_h_fr_entries[0],
  64,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_fp_entries[] =
{
  { "fp0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "fp2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "fp4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "fp6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "fp8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "fp10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "fp12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "fp14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "fp16", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "fp18", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "fp20", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "fp22", 22, {0, {{{0, 0}}}}, 0, 0 },
  { "fp24", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "fp26", 26, {0, {{{0, 0}}}}, 0, 0 },
  { "fp28", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "fp30", 30, {0, {{{0, 0}}}}, 0, 0 },
  { "fp32", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "fp34", 34, {0, {{{0, 0}}}}, 0, 0 },
  { "fp36", 36, {0, {{{0, 0}}}}, 0, 0 },
  { "fp38", 38, {0, {{{0, 0}}}}, 0, 0 },
  { "fp40", 40, {0, {{{0, 0}}}}, 0, 0 },
  { "fp42", 42, {0, {{{0, 0}}}}, 0, 0 },
  { "fp44", 44, {0, {{{0, 0}}}}, 0, 0 },
  { "fp46", 46, {0, {{{0, 0}}}}, 0, 0 },
  { "fp48", 48, {0, {{{0, 0}}}}, 0, 0 },
  { "fp50", 50, {0, {{{0, 0}}}}, 0, 0 },
  { "fp52", 52, {0, {{{0, 0}}}}, 0, 0 },
  { "fp54", 54, {0, {{{0, 0}}}}, 0, 0 },
  { "fp56", 56, {0, {{{0, 0}}}}, 0, 0 },
  { "fp58", 58, {0, {{{0, 0}}}}, 0, 0 },
  { "fp60", 60, {0, {{{0, 0}}}}, 0, 0 },
  { "fp62", 62, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_fp =
{
  & sh_cgen_opval_h_fp_entries[0],
  32,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_fv_entries[] =
{
  { "fv0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "fv4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "fv8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "fv12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "fv16", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "fv20", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "fv24", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "fv28", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "fv32", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "fv36", 36, {0, {{{0, 0}}}}, 0, 0 },
  { "fv40", 40, {0, {{{0, 0}}}}, 0, 0 },
  { "fv44", 44, {0, {{{0, 0}}}}, 0, 0 },
  { "fv48", 48, {0, {{{0, 0}}}}, 0, 0 },
  { "fv52", 52, {0, {{{0, 0}}}}, 0, 0 },
  { "fv56", 56, {0, {{{0, 0}}}}, 0, 0 },
  { "fv60", 60, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_fv =
{
  & sh_cgen_opval_h_fv_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_fmtx_entries[] =
{
  { "mtrx0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "mtrx16", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "mtrx32", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "mtrx48", 48, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_fmtx =
{
  & sh_cgen_opval_h_fmtx_entries[0],
  4,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_dr_entries[] =
{
  { "dr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "dr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "dr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "dr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "dr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "dr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "dr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "dr14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "dr16", 16, {0, {{{0, 0}}}}, 0, 0 },
  { "dr18", 18, {0, {{{0, 0}}}}, 0, 0 },
  { "dr20", 20, {0, {{{0, 0}}}}, 0, 0 },
  { "dr22", 22, {0, {{{0, 0}}}}, 0, 0 },
  { "dr24", 24, {0, {{{0, 0}}}}, 0, 0 },
  { "dr26", 26, {0, {{{0, 0}}}}, 0, 0 },
  { "dr28", 28, {0, {{{0, 0}}}}, 0, 0 },
  { "dr30", 30, {0, {{{0, 0}}}}, 0, 0 },
  { "dr32", 32, {0, {{{0, 0}}}}, 0, 0 },
  { "dr34", 34, {0, {{{0, 0}}}}, 0, 0 },
  { "dr36", 36, {0, {{{0, 0}}}}, 0, 0 },
  { "dr38", 38, {0, {{{0, 0}}}}, 0, 0 },
  { "dr40", 40, {0, {{{0, 0}}}}, 0, 0 },
  { "dr42", 42, {0, {{{0, 0}}}}, 0, 0 },
  { "dr44", 44, {0, {{{0, 0}}}}, 0, 0 },
  { "dr46", 46, {0, {{{0, 0}}}}, 0, 0 },
  { "dr48", 48, {0, {{{0, 0}}}}, 0, 0 },
  { "dr50", 50, {0, {{{0, 0}}}}, 0, 0 },
  { "dr52", 52, {0, {{{0, 0}}}}, 0, 0 },
  { "dr54", 54, {0, {{{0, 0}}}}, 0, 0 },
  { "dr56", 56, {0, {{{0, 0}}}}, 0, 0 },
  { "dr58", 58, {0, {{{0, 0}}}}, 0, 0 },
  { "dr60", 60, {0, {{{0, 0}}}}, 0, 0 },
  { "dr62", 62, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_dr =
{
  & sh_cgen_opval_h_dr_entries[0],
  32,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_fsd_entries[] =
{
  { "fr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "fr1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "fr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "fr3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "fr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "fr5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "fr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "fr7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "fr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "fr9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "fr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "fr11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "fr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "fr13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "fr14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "fr15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_fsd =
{
  & sh_cgen_opval_h_fsd_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_fmov_entries[] =
{
  { "fr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "fr1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "fr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "fr3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "fr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "fr5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "fr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "fr7", 7, {0, {{{0, 0}}}}, 0, 0 },
  { "fr8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "fr9", 9, {0, {{{0, 0}}}}, 0, 0 },
  { "fr10", 10, {0, {{{0, 0}}}}, 0, 0 },
  { "fr11", 11, {0, {{{0, 0}}}}, 0, 0 },
  { "fr12", 12, {0, {{{0, 0}}}}, 0, 0 },
  { "fr13", 13, {0, {{{0, 0}}}}, 0, 0 },
  { "fr14", 14, {0, {{{0, 0}}}}, 0, 0 },
  { "fr15", 15, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_fmov =
{
  & sh_cgen_opval_h_fmov_entries[0],
  16,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_tr_entries[] =
{
  { "tr0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "tr1", 1, {0, {{{0, 0}}}}, 0, 0 },
  { "tr2", 2, {0, {{{0, 0}}}}, 0, 0 },
  { "tr3", 3, {0, {{{0, 0}}}}, 0, 0 },
  { "tr4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "tr5", 5, {0, {{{0, 0}}}}, 0, 0 },
  { "tr6", 6, {0, {{{0, 0}}}}, 0, 0 },
  { "tr7", 7, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_tr =
{
  & sh_cgen_opval_h_tr_entries[0],
  8,
  0, 0, 0, 0, ""
};

static CGEN_KEYWORD_ENTRY sh_cgen_opval_h_fvc_entries[] =
{
  { "fv0", 0, {0, {{{0, 0}}}}, 0, 0 },
  { "fv4", 4, {0, {{{0, 0}}}}, 0, 0 },
  { "fv8", 8, {0, {{{0, 0}}}}, 0, 0 },
  { "fv12", 12, {0, {{{0, 0}}}}, 0, 0 }
};

CGEN_KEYWORD sh_cgen_opval_h_fvc =
{
  & sh_cgen_opval_h_fvc_entries[0],
  4,
  0, 0, 0, 0, ""
};


/* The hardware table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_HW_##a)
#else
#define A(a) (1 << CGEN_HW_/**/a)
#endif

const CGEN_HW_ENTRY sh_cgen_hw_table[] =
{
  { "h-memory", HW_H_MEMORY, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-sint", HW_H_SINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-uint", HW_H_UINT, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-addr", HW_H_ADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-iaddr", HW_H_IADDR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-pc", HW_H_PC, CGEN_ASM_NONE, 0, { 0|A(PROFILE)|A(PC), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-gr", HW_H_GR, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_gr, { 0|A(PROFILE), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-grc", HW_H_GRC, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_grc, { 0|A(PROFILE)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-cr", HW_H_CR, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_cr, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } } },
  { "h-sr", HW_H_SR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-fpscr", HW_H_FPSCR, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-frbit", HW_H_FRBIT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-szbit", HW_H_SZBIT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-prbit", HW_H_PRBIT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-sbit", HW_H_SBIT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-mbit", HW_H_MBIT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-qbit", HW_H_QBIT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-fr", HW_H_FR, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_fr, { 0|A(PROFILE), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-fp", HW_H_FP, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_fp, { 0|A(PROFILE)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-fv", HW_H_FV, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_fv, { 0|A(PROFILE)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-fmtx", HW_H_FMTX, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_fmtx, { 0|A(PROFILE)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } } },
  { "h-dr", HW_H_DR, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_dr, { 0|A(PROFILE)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-fsd", HW_H_FSD, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_fsd, { 0|A(PROFILE), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\xc0" } } } } },
  { "h-fmov", HW_H_FMOV, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_fmov, { 0|A(PROFILE), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\xc0" } } } } },
  { "h-tr", HW_H_TR, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_tr, { 0|A(PROFILE), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } } },
  { "h-endian", HW_H_ENDIAN, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-ism", HW_H_ISM, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } } },
  { "h-frc", HW_H_FRC, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_frc_names, { 0|A(PROFILE)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-drc", HW_H_DRC, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_drc_names, { 0|A(PROFILE)|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-xf", HW_H_XF, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_xf_names, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-xd", HW_H_XD, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_frc_names, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-fvc", HW_H_FVC, CGEN_ASM_KEYWORD, (PTR) & sh_cgen_opval_h_fvc, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-gbr", HW_H_GBR, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-vbr", HW_H_VBR, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-pr", HW_H_PR, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-macl", HW_H_MACL, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-mach", HW_H_MACH, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { "h-tbit", HW_H_TBIT, CGEN_ASM_NONE, 0, { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } },
  { 0, 0, CGEN_ASM_NONE, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } }
};

#undef A


/* The instruction field table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_IFLD_##a)
#else
#define A(a) (1 << CGEN_IFLD_/**/a)
#endif

const CGEN_IFLD sh_cgen_ifld_table[] =
{
  { SH_F_NIL, "f-nil", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_ANYOF, "f-anyof", 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_OP4, "f-op4", 0, 32, 0, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_OP8, "f-op8", 0, 32, 0, 8, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_OP16, "f-op16", 0, 32, 0, 16, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_SUB4, "f-sub4", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_SUB8, "f-sub8", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_SUB10, "f-sub10", 0, 32, 6, 10, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_RN, "f-rn", 0, 32, 4, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_RM, "f-rm", 0, 32, 8, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_7_1, "f-7-1", 0, 32, 7, 1, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_11_1, "f-11-1", 0, 32, 11, 1, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_16_4, "f-16-4", 0, 32, 16, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_DISP8, "f-disp8", 0, 32, 8, 8, { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_DISP12, "f-disp12", 0, 32, 4, 12, { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM8, "f-imm8", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM4, "f-imm4", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM4X2, "f-imm4x2", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM4X4, "f-imm4x4", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM8X2, "f-imm8x2", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM8X4, "f-imm8x4", 0, 32, 8, 8, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM12X4, "f-imm12x4", 0, 32, 20, 12, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM12X8, "f-imm12x8", 0, 32, 20, 12, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_DN, "f-dn", 0, 32, 4, 3, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_DM, "f-dm", 0, 32, 8, 3, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_VN, "f-vn", 0, 32, 4, 2, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_VM, "f-vm", 0, 32, 6, 2, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_XN, "f-xn", 0, 32, 4, 3, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_XM, "f-xm", 0, 32, 8, 3, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM20_HI, "f-imm20-hi", 0, 32, 8, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM20_LO, "f-imm20-lo", 0, 32, 16, 16, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_IMM20, "f-imm20", 0, 0, 0, 0,{ 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
  { SH_F_OP, "f-op", 0, 32, 0, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_EXT, "f-ext", 0, 32, 12, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_RSVD, "f-rsvd", 0, 32, 28, 4, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_LEFT, "f-left", 0, 32, 6, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_RIGHT, "f-right", 0, 32, 16, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DEST, "f-dest", 0, 32, 22, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_LEFT_RIGHT, "f-left-right", 0, 0, 0, 0,{ 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_TRA, "f-tra", 0, 32, 25, 3, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_TRB, "f-trb", 0, 32, 9, 3, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_LIKELY, "f-likely", 0, 32, 22, 1, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_6_3, "f-6-3", 0, 32, 6, 3, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_23_2, "f-23-2", 0, 32, 23, 2, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_IMM6, "f-imm6", 0, 32, 16, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_IMM10, "f-imm10", 0, 32, 12, 10, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_IMM16, "f-imm16", 0, 32, 6, 16, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_UIMM6, "f-uimm6", 0, 32, 16, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_UIMM16, "f-uimm16", 0, 32, 6, 16, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DISP6, "f-disp6", 0, 32, 16, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DISP6X32, "f-disp6x32", 0, 32, 16, 6, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DISP10, "f-disp10", 0, 32, 12, 10, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DISP10X8, "f-disp10x8", 0, 32, 12, 10, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DISP10X4, "f-disp10x4", 0, 32, 12, 10, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DISP10X2, "f-disp10x2", 0, 32, 12, 10, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { SH_F_DISP16, "f-disp16", 0, 32, 6, 16, { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
  { 0, 0, 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } }
};

#undef A



/* multi ifield declarations */

const CGEN_MAYBE_MULTI_IFLD SH_F_IMM20_MULTI_IFIELD [];
const CGEN_MAYBE_MULTI_IFLD SH_F_LEFT_RIGHT_MULTI_IFIELD [];


/* multi ifield definitions */

const CGEN_MAYBE_MULTI_IFLD SH_F_IMM20_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM20_HI] } },
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM20_LO] } },
    { 0, { (const PTR) 0 } }
};
const CGEN_MAYBE_MULTI_IFLD SH_F_LEFT_RIGHT_MULTI_IFIELD [] =
{
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LEFT] } },
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RIGHT] } },
    { 0, { (const PTR) 0 } }
};

/* The operand table.  */

#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_OPERAND_##a)
#else
#define A(a) (1 << CGEN_OPERAND_/**/a)
#endif
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define OPERAND(op) SH_OPERAND_##op
#else
#define OPERAND(op) SH_OPERAND_/**/op
#endif

const CGEN_OPERAND sh_cgen_operand_table[] =
{
/* pc: program counter */
  { "pc", SH_OPERAND_PC, HW_H_PC, 0, 0,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_NIL] } }, 
    { 0|A(SEM_ONLY), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* endian: Endian mode */
  { "endian", SH_OPERAND_ENDIAN, HW_H_ENDIAN, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } }  },
/* ism: Instruction set mode */
  { "ism", SH_OPERAND_ISM, HW_H_ISM, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\xc0" } } } }  },
/* rm: Left general purpose register */
  { "rm", SH_OPERAND_RM, HW_H_GRC, 8, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RM] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* rn: Right general purpose register */
  { "rn", SH_OPERAND_RN, HW_H_GRC, 4, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RN] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* r0: Register 0 */
  { "r0", SH_OPERAND_R0, HW_H_GRC, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* frn: Single precision register */
  { "frn", SH_OPERAND_FRN, HW_H_FRC, 4, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RN] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* frm: Single precision register */
  { "frm", SH_OPERAND_FRM, HW_H_FRC, 8, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RM] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* fr0: Single precision register 0 */
  { "fr0", SH_OPERAND_FR0, HW_H_FRC, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* fmovn: Register for fmov */
  { "fmovn", SH_OPERAND_FMOVN, HW_H_FMOV, 4, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RN] } }, 
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } } } }  },
/* fmovm: Register for fmov */
  { "fmovm", SH_OPERAND_FMOVM, HW_H_FMOV, 8, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RM] } }, 
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } } } }  },
/* fvn: Left floating point vector */
  { "fvn", SH_OPERAND_FVN, HW_H_FVC, 4, 2,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_VN] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* fvm: Right floating point vector */
  { "fvm", SH_OPERAND_FVM, HW_H_FVC, 6, 2,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_VM] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* drn: Left double precision register */
  { "drn", SH_OPERAND_DRN, HW_H_DRC, 4, 3,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DN] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* drm: Right double precision register */
  { "drm", SH_OPERAND_DRM, HW_H_DRC, 8, 3,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DM] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm4: Immediate value (4 bits) */
  { "imm4", SH_OPERAND_IMM4, HW_H_SINT, 12, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm8: Immediate value (8 bits) */
  { "imm8", SH_OPERAND_IMM8, HW_H_SINT, 8, 8,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* uimm8: Immediate value (8 bits unsigned) */
  { "uimm8", SH_OPERAND_UIMM8, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm20: Immediate value (20 bits) */
  { "imm20", SH_OPERAND_IMM20, HW_H_SINT, 8, 20,
    { 2, { (const PTR) &SH_F_IMM20_MULTI_IFIELD[0] } }, 
    { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm4x2: Immediate value (4 bits, 2x scale) */
  { "imm4x2", SH_OPERAND_IMM4X2, HW_H_UINT, 12, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM4X2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm4x4: Immediate value (4 bits, 4x scale) */
  { "imm4x4", SH_OPERAND_IMM4X4, HW_H_UINT, 12, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM4X4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm8x2: Immediate value (8 bits, 2x scale) */
  { "imm8x2", SH_OPERAND_IMM8X2, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM8X2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm8x4: Immediate value (8 bits, 4x scale) */
  { "imm8x4", SH_OPERAND_IMM8X4, HW_H_UINT, 8, 8,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM8X4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* disp8: Displacement (8 bits) */
  { "disp8", SH_OPERAND_DISP8, HW_H_IADDR, 8, 8,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP8] } }, 
    { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* disp12: Displacement (12 bits) */
  { "disp12", SH_OPERAND_DISP12, HW_H_IADDR, 4, 12,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP12] } }, 
    { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm12x4: Displacement (12 bits) */
  { "imm12x4", SH_OPERAND_IMM12X4, HW_H_SINT, 20, 12,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM12X4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* imm12x8: Displacement (12 bits) */
  { "imm12x8", SH_OPERAND_IMM12X8, HW_H_SINT, 20, 12,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM12X8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* rm64: Register m (64 bits) */
  { "rm64", SH_OPERAND_RM64, HW_H_GR, 8, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RM] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* rn64: Register n (64 bits) */
  { "rn64", SH_OPERAND_RN64, HW_H_GR, 4, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RN] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* gbr: Global base register */
  { "gbr", SH_OPERAND_GBR, HW_H_GBR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* vbr: Vector base register */
  { "vbr", SH_OPERAND_VBR, HW_H_VBR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* pr: Procedure link register */
  { "pr", SH_OPERAND_PR, HW_H_PR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* fpscr: Floating point status/control register */
  { "fpscr", SH_OPERAND_FPSCR, HW_H_FPSCR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* tbit: Condition code flag */
  { "tbit", SH_OPERAND_TBIT, HW_H_TBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* sbit: Multiply-accumulate saturation flag */
  { "sbit", SH_OPERAND_SBIT, HW_H_SBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* mbit: Divide-step M flag */
  { "mbit", SH_OPERAND_MBIT, HW_H_MBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* qbit: Divide-step Q flag */
  { "qbit", SH_OPERAND_QBIT, HW_H_QBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* fpul: Floating point ??? */
  { "fpul", SH_OPERAND_FPUL, HW_H_FR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* frbit: Floating point register bank bit */
  { "frbit", SH_OPERAND_FRBIT, HW_H_FRBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* szbit: Floating point transfer size bit */
  { "szbit", SH_OPERAND_SZBIT, HW_H_SZBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* prbit: Floating point precision bit */
  { "prbit", SH_OPERAND_PRBIT, HW_H_PRBIT, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* macl: Multiply-accumulate low register */
  { "macl", SH_OPERAND_MACL, HW_H_MACL, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* mach: Multiply-accumulate high register */
  { "mach", SH_OPERAND_MACH, HW_H_MACH, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } }  },
/* fsdm: bar */
  { "fsdm", SH_OPERAND_FSDM, HW_H_FSD, 8, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RM] } }, 
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } } } }  },
/* fsdn: bar */
  { "fsdn", SH_OPERAND_FSDN, HW_H_FSD, 4, 4,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RN] } }, 
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } } } }  },
/* rm: Left general purpose reg */
  { "rm", SH_OPERAND_RM, HW_H_GR, 6, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LEFT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* rn: Right general purpose reg */
  { "rn", SH_OPERAND_RN, HW_H_GR, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RIGHT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* rd: Destination general purpose reg */
  { "rd", SH_OPERAND_RD, HW_H_GR, 22, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DEST] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* frg: Left single precision register */
  { "frg", SH_OPERAND_FRG, HW_H_FR, 6, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LEFT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* frh: Right single precision register */
  { "frh", SH_OPERAND_FRH, HW_H_FR, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RIGHT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* frf: Destination single precision reg */
  { "frf", SH_OPERAND_FRF, HW_H_FR, 22, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DEST] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* frgh: Single precision register pair */
  { "frgh", SH_OPERAND_FRGH, HW_H_FR, 6, 12,
    { 2, { (const PTR) &SH_F_LEFT_RIGHT_MULTI_IFIELD[0] } }, 
    { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* fpf: Pair of single precision registers */
  { "fpf", SH_OPERAND_FPF, HW_H_FP, 22, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DEST] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* fvg: Left single precision vector */
  { "fvg", SH_OPERAND_FVG, HW_H_FV, 6, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LEFT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* fvh: Right single precision vector */
  { "fvh", SH_OPERAND_FVH, HW_H_FV, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RIGHT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* fvf: Destination single precision vector */
  { "fvf", SH_OPERAND_FVF, HW_H_FV, 22, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DEST] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* mtrxg: Left single precision matrix */
  { "mtrxg", SH_OPERAND_MTRXG, HW_H_FMTX, 6, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LEFT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* drg: Left double precision register */
  { "drg", SH_OPERAND_DRG, HW_H_DR, 6, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LEFT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* drh: Right double precision register */
  { "drh", SH_OPERAND_DRH, HW_H_DR, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_RIGHT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* drf: Destination double precision reg */
  { "drf", SH_OPERAND_DRF, HW_H_DR, 22, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DEST] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* drgh: Double precision register pair */
  { "drgh", SH_OPERAND_DRGH, HW_H_DR, 6, 12,
    { 2, { (const PTR) &SH_F_LEFT_RIGHT_MULTI_IFIELD[0] } }, 
    { 0|A(VIRTUAL), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* fpscr: Floating point status register */
  { "fpscr", SH_OPERAND_FPSCR, HW_H_FPSCR, 0, 0,
    { 0, { (const PTR) 0 } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* crj: Control register j */
  { "crj", SH_OPERAND_CRJ, HW_H_CR, 22, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DEST] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* crk: Control register k */
  { "crk", SH_OPERAND_CRK, HW_H_CR, 6, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LEFT] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* tra: Target register a */
  { "tra", SH_OPERAND_TRA, HW_H_TR, 25, 3,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_TRA] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* trb: Target register b */
  { "trb", SH_OPERAND_TRB, HW_H_TR, 9, 3,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_TRB] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* disp6: Displacement (6 bits) */
  { "disp6", SH_OPERAND_DISP6, HW_H_SINT, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP6] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* disp6x32: Displacement (6 bits, scale 32) */
  { "disp6x32", SH_OPERAND_DISP6X32, HW_H_SINT, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP6X32] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* disp10: Displacement (10 bits) */
  { "disp10", SH_OPERAND_DISP10, HW_H_SINT, 12, 10,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP10] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* disp10x2: Displacement (10 bits, scale 2) */
  { "disp10x2", SH_OPERAND_DISP10X2, HW_H_SINT, 12, 10,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP10X2] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* disp10x4: Displacement (10 bits, scale 4) */
  { "disp10x4", SH_OPERAND_DISP10X4, HW_H_SINT, 12, 10,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP10X4] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* disp10x8: Displacement (10 bits, scale 8) */
  { "disp10x8", SH_OPERAND_DISP10X8, HW_H_SINT, 12, 10,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP10X8] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* disp16: Displacement (16 bits) */
  { "disp16", SH_OPERAND_DISP16, HW_H_SINT, 6, 16,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_DISP16] } }, 
    { 0|A(PCREL_ADDR), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* imm6: Immediate (6 bits) */
  { "imm6", SH_OPERAND_IMM6, HW_H_SINT, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM6] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* imm10: Immediate (10 bits) */
  { "imm10", SH_OPERAND_IMM10, HW_H_SINT, 12, 10,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM10] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* imm16: Immediate (16 bits) */
  { "imm16", SH_OPERAND_IMM16, HW_H_SINT, 6, 16,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_IMM16] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* uimm6: Immediate (6 bits) */
  { "uimm6", SH_OPERAND_UIMM6, HW_H_UINT, 16, 6,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_UIMM6] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* uimm16: Unsigned immediate (16 bits) */
  { "uimm16", SH_OPERAND_UIMM16, HW_H_UINT, 6, 16,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_UIMM16] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* likely: Likely branch? */
  { "likely", SH_OPERAND_LIKELY, HW_H_UINT, 22, 1,
    { 0, { (const PTR) &sh_cgen_ifld_table[SH_F_LIKELY] } }, 
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } } } }  },
/* sentinel */
  { 0, 0, 0, 0, 0,
    { 0, { (const PTR) 0 } },
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } } } } }
};

#undef A


/* The instruction table.  */

#define OP(field) CGEN_SYNTAX_MAKE_FIELD (OPERAND (field))
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#define A(a) (1 << CGEN_INSN_##a)
#else
#define A(a) (1 << CGEN_INSN_/**/a)
#endif

static const CGEN_IBASE sh_cgen_insn_table[MAX_INSNS] =
{
  /* Special null first entry.
     A `num' value of zero is thus invalid.
     Also, the special `invalid' insn resides here.  */
  { 0, 0, 0, 0, { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } } },
/* add $rm, $rn */
  {
    SH_INSN_ADD_COMPACT, "add-compact", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* add #$imm8, $rn */
  {
    SH_INSN_ADDI_COMPACT, "addi-compact", "add", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* addc $rm, $rn */
  {
    SH_INSN_ADDC_COMPACT, "addc-compact", "addc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* addv $rm, $rn */
  {
    SH_INSN_ADDV_COMPACT, "addv-compact", "addv", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* and $rm64, $rn64 */
  {
    SH_INSN_AND_COMPACT, "and-compact", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* and #$uimm8, r0 */
  {
    SH_INSN_ANDI_COMPACT, "andi-compact", "and", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* and.b #$imm8, @(r0, gbr) */
  {
    SH_INSN_ANDB_COMPACT, "andb-compact", "and.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* bf $disp8 */
  {
    SH_INSN_BF_COMPACT, "bf-compact", "bf", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_BR, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* bf/s $disp8 */
  {
    SH_INSN_BFS_COMPACT, "bfs-compact", "bf/s", 16,
    { 0|A(COND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_BR, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* bra $disp12 */
  {
    SH_INSN_BRA_COMPACT, "bra-compact", "bra", 16,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_BR, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* braf $rn */
  {
    SH_INSN_BRAF_COMPACT, "braf-compact", "braf", 16,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* brk */
  {
    SH_INSN_BRK_COMPACT, "brk-compact", "brk", 16,
    { 0, { { { (1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* bsr $disp12 */
  {
    SH_INSN_BSR_COMPACT, "bsr-compact", "bsr", 16,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_BR, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* bsrf $rn */
  {
    SH_INSN_BSRF_COMPACT, "bsrf-compact", "bsrf", 16,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* bt $disp8 */
  {
    SH_INSN_BT_COMPACT, "bt-compact", "bt", 16,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_BR, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* bt/s $disp8 */
  {
    SH_INSN_BTS_COMPACT, "bts-compact", "bt/s", 16,
    { 0|A(COND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_BR, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* clrmac */
  {
    SH_INSN_CLRMAC_COMPACT, "clrmac-compact", "clrmac", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* clrs */
  {
    SH_INSN_CLRS_COMPACT, "clrs-compact", "clrs", 16,
    { 0, { { { (1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* clrt */
  {
    SH_INSN_CLRT_COMPACT, "clrt-compact", "clrt", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/eq $rm, $rn */
  {
    SH_INSN_CMPEQ_COMPACT, "cmpeq-compact", "cmp/eq", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/eq #$imm8, r0 */
  {
    SH_INSN_CMPEQI_COMPACT, "cmpeqi-compact", "cmp/eq", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/ge $rm, $rn */
  {
    SH_INSN_CMPGE_COMPACT, "cmpge-compact", "cmp/ge", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/gt $rm, $rn */
  {
    SH_INSN_CMPGT_COMPACT, "cmpgt-compact", "cmp/gt", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/hi $rm, $rn */
  {
    SH_INSN_CMPHI_COMPACT, "cmphi-compact", "cmp/hi", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/hs $rm, $rn */
  {
    SH_INSN_CMPHS_COMPACT, "cmphs-compact", "cmp/hs", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/pl $rn */
  {
    SH_INSN_CMPPL_COMPACT, "cmppl-compact", "cmp/pl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/pz $rn */
  {
    SH_INSN_CMPPZ_COMPACT, "cmppz-compact", "cmp/pz", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* cmp/str $rm, $rn */
  {
    SH_INSN_CMPSTR_COMPACT, "cmpstr-compact", "cmp/str", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* div0s $rm, $rn */
  {
    SH_INSN_DIV0S_COMPACT, "div0s-compact", "div0s", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* div0u */
  {
    SH_INSN_DIV0U_COMPACT, "div0u-compact", "div0u", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* div1 $rm, $rn */
  {
    SH_INSN_DIV1_COMPACT, "div1-compact", "div1", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* divu r0, $rn */
  {
    SH_INSN_DIVU_COMPACT, "divu-compact", "divu", 16,
    { 0, { { { (1<<MACH_SH2A_NOFPU)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mulr r0, $rn */
  {
    SH_INSN_MULR_COMPACT, "mulr-compact", "mulr", 16,
    { 0, { { { (1<<MACH_SH2A_NOFPU)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* dmuls.l $rm, $rn */
  {
    SH_INSN_DMULSL_COMPACT, "dmulsl-compact", "dmuls.l", 16,
    { 0, { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* dmulu.l $rm, $rn */
  {
    SH_INSN_DMULUL_COMPACT, "dmulul-compact", "dmulu.l", 16,
    { 0, { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* dt $rn */
  {
    SH_INSN_DT_COMPACT, "dt-compact", "dt", 16,
    { 0, { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* exts.b $rm, $rn */
  {
    SH_INSN_EXTSB_COMPACT, "extsb-compact", "exts.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* exts.w $rm, $rn */
  {
    SH_INSN_EXTSW_COMPACT, "extsw-compact", "exts.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* extu.b $rm, $rn */
  {
    SH_INSN_EXTUB_COMPACT, "extub-compact", "extu.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* extu.w $rm, $rn */
  {
    SH_INSN_EXTUW_COMPACT, "extuw-compact", "extu.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* fabs $fsdn */
  {
    SH_INSN_FABS_COMPACT, "fabs-compact", "fabs", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fadd $fsdm, $fsdn */
  {
    SH_INSN_FADD_COMPACT, "fadd-compact", "fadd", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fcmp/eq $fsdm, $fsdn */
  {
    SH_INSN_FCMPEQ_COMPACT, "fcmpeq-compact", "fcmp/eq", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fcmp/gt $fsdm, $fsdn */
  {
    SH_INSN_FCMPGT_COMPACT, "fcmpgt-compact", "fcmp/gt", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fcnvds $drn, fpul */
  {
    SH_INSN_FCNVDS_COMPACT, "fcnvds-compact", "fcnvds", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fcnvsd fpul, $drn */
  {
    SH_INSN_FCNVSD_COMPACT, "fcnvsd-compact", "fcnvsd", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fdiv $fsdm, $fsdn */
  {
    SH_INSN_FDIV_COMPACT, "fdiv-compact", "fdiv", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fipr $fvm, $fvn */
  {
    SH_INSN_FIPR_COMPACT, "fipr-compact", "fipr", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* flds $frn, fpul */
  {
    SH_INSN_FLDS_COMPACT, "flds-compact", "flds", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fldi0 $frn */
  {
    SH_INSN_FLDI0_COMPACT, "fldi0-compact", "fldi0", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fldi1 $frn */
  {
    SH_INSN_FLDI1_COMPACT, "fldi1-compact", "fldi1", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* float fpul, $fsdn */
  {
    SH_INSN_FLOAT_COMPACT, "float-compact", "float", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fmac fr0, $frm, $frn */
  {
    SH_INSN_FMAC_COMPACT, "fmac-compact", "fmac", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fmov $fmovm, $fmovn */
  {
    SH_INSN_FMOV1_COMPACT, "fmov1-compact", "fmov", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fmov @$rm, $fmovn */
  {
    SH_INSN_FMOV2_COMPACT, "fmov2-compact", "fmov", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fmov @${rm}+, fmovn */
  {
    SH_INSN_FMOV3_COMPACT, "fmov3-compact", "fmov", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fmov @(r0, $rm), $fmovn */
  {
    SH_INSN_FMOV4_COMPACT, "fmov4-compact", "fmov", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fmov $fmovm, @$rn */
  {
    SH_INSN_FMOV5_COMPACT, "fmov5-compact", "fmov", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fmov $fmovm, @-$rn */
  {
    SH_INSN_FMOV6_COMPACT, "fmov6-compact", "fmov", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fmov $fmovm, @(r0, $rn) */
  {
    SH_INSN_FMOV7_COMPACT, "fmov7-compact", "fmov", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fmov.d @($imm12x8, $rm), $drn */
  {
    SH_INSN_FMOV8_COMPACT, "fmov8-compact", "fmov.d", 32,
    { 0|A(32_BIT_INSN), { { { (1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mov.l $drm, @($imm12x8, $rn) */
  {
    SH_INSN_FMOV9_COMPACT, "fmov9-compact", "mov.l", 32,
    { 0|A(32_BIT_INSN), { { { (1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmul $fsdm, $fsdn */
  {
    SH_INSN_FMUL_COMPACT, "fmul-compact", "fmul", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fneg $fsdn */
  {
    SH_INSN_FNEG_COMPACT, "fneg-compact", "fneg", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* frchg */
  {
    SH_INSN_FRCHG_COMPACT, "frchg-compact", "frchg", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fschg */
  {
    SH_INSN_FSCHG_COMPACT, "fschg-compact", "fschg", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fsqrt $fsdn */
  {
    SH_INSN_FSQRT_COMPACT, "fsqrt-compact", "fsqrt", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* fsts fpul, $frn */
  {
    SH_INSN_FSTS_COMPACT, "fsts-compact", "fsts", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* fsub $fsdm, $fsdn */
  {
    SH_INSN_FSUB_COMPACT, "fsub-compact", "fsub", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* ftrc $fsdn, fpul */
  {
    SH_INSN_FTRC_COMPACT, "ftrc-compact", "ftrc", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* ftrv xmtrx, $fvn */
  {
    SH_INSN_FTRV_COMPACT, "ftrv-compact", "ftrv", 16,
    { 0|A(FP_INSN), { { { (1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_FE, 0 } }, { { SH4A_GROUP_FE, 0 } } } }
  },
/* jmp @$rn */
  {
    SH_INSN_JMP_COMPACT, "jmp-compact", "jmp", 16,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* jsr @$rn */
  {
    SH_INSN_JSR_COMPACT, "jsr-compact", "jsr", 16,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* ldc $rn, gbr */
  {
    SH_INSN_LDC_GBR_COMPACT, "ldc-gbr-compact", "ldc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* ldc $rn, vbr */
  {
    SH_INSN_LDC_VBR_COMPACT, "ldc-vbr-compact", "ldc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* ldc $rn, sr */
  {
    SH_INSN_LDC_SR_COMPACT, "ldc-sr-compact", "ldc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* ldc.l @${rn}+, gbr */
  {
    SH_INSN_LDCL_GBR_COMPACT, "ldcl-gbr-compact", "ldc.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* ldc.l @${rn}+, vbr */
  {
    SH_INSN_LDCL_VBR_COMPACT, "ldcl-vbr-compact", "ldc.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds $rn, fpscr */
  {
    SH_INSN_LDS_FPSCR_COMPACT, "lds-fpscr-compact", "lds", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds.l @${rn}+, fpscr */
  {
    SH_INSN_LDSL_FPSCR_COMPACT, "ldsl-fpscr-compact", "lds.l", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds $rn, fpul */
  {
    SH_INSN_LDS_FPUL_COMPACT, "lds-fpul-compact", "lds", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds.l @${rn}+, fpul */
  {
    SH_INSN_LDSL_FPUL_COMPACT, "ldsl-fpul-compact", "lds.l", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds $rn, mach */
  {
    SH_INSN_LDS_MACH_COMPACT, "lds-mach-compact", "lds", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds.l @${rn}+, mach */
  {
    SH_INSN_LDSL_MACH_COMPACT, "ldsl-mach-compact", "lds.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds $rn, macl */
  {
    SH_INSN_LDS_MACL_COMPACT, "lds-macl-compact", "lds", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds.l @${rn}+, macl */
  {
    SH_INSN_LDSL_MACL_COMPACT, "ldsl-macl-compact", "lds.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds $rn, pr */
  {
    SH_INSN_LDS_PR_COMPACT, "lds-pr-compact", "lds", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* lds.l @${rn}+, pr */
  {
    SH_INSN_LDSL_PR_COMPACT, "ldsl-pr-compact", "lds.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mac.l @${rm}+, @${rn}+ */
  {
    SH_INSN_MACL_COMPACT, "macl-compact", "mac.l", 16,
    { 0, { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* mac.w @${rm}+, @${rn}+ */
  {
    SH_INSN_MACW_COMPACT, "macw-compact", "mac.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* mov $rm64, $rn64 */
  {
    SH_INSN_MOV_COMPACT, "mov-compact", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_MT, 0 } } } }
  },
/* mov #$imm8, $rn */
  {
    SH_INSN_MOVI_COMPACT, "movi-compact", "mov", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_MT, 0 } } } }
  },
/* movi20 #$imm20, $rn */
  {
    SH_INSN_MOVI20_COMPACT, "movi20-compact", "movi20", 32,
    { 0|A(32_BIT_INSN), { { { (1<<MACH_SH2A_NOFPU)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mov.b $rm, @$rn */
  {
    SH_INSN_MOVB1_COMPACT, "movb1-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b $rm, @-$rn */
  {
    SH_INSN_MOVB2_COMPACT, "movb2-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b $rm, @(r0,$rn) */
  {
    SH_INSN_MOVB3_COMPACT, "movb3-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b r0, @($imm8, gbr) */
  {
    SH_INSN_MOVB4_COMPACT, "movb4-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b r0, @($imm4, $rm) */
  {
    SH_INSN_MOVB5_COMPACT, "movb5-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b @$rm, $rn */
  {
    SH_INSN_MOVB6_COMPACT, "movb6-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b @${rm}+, $rn */
  {
    SH_INSN_MOVB7_COMPACT, "movb7-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b @(r0, $rm), $rn */
  {
    SH_INSN_MOVB8_COMPACT, "movb8-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b @($imm8, gbr), r0 */
  {
    SH_INSN_MOVB9_COMPACT, "movb9-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.b @($imm4, $rm), r0 */
  {
    SH_INSN_MOVB10_COMPACT, "movb10-compact", "mov.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l $rm, @$rn */
  {
    SH_INSN_MOVL1_COMPACT, "movl1-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l $rm, @-$rn */
  {
    SH_INSN_MOVL2_COMPACT, "movl2-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l $rm, @(r0, $rn) */
  {
    SH_INSN_MOVL3_COMPACT, "movl3-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l r0, @($imm8x4, gbr) */
  {
    SH_INSN_MOVL4_COMPACT, "movl4-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l $rm, @($imm4x4, $rn) */
  {
    SH_INSN_MOVL5_COMPACT, "movl5-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l @$rm, $rn */
  {
    SH_INSN_MOVL6_COMPACT, "movl6-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l @${rm}+, $rn */
  {
    SH_INSN_MOVL7_COMPACT, "movl7-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l @(r0, $rm), $rn */
  {
    SH_INSN_MOVL8_COMPACT, "movl8-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l @($imm8x4, gbr), r0 */
  {
    SH_INSN_MOVL9_COMPACT, "movl9-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l @($imm8x4, pc), $rn */
  {
    SH_INSN_MOVL10_COMPACT, "movl10-compact", "mov.l", 16,
    { 0|A(ILLSLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l @($imm4x4, $rm), $rn */
  {
    SH_INSN_MOVL11_COMPACT, "movl11-compact", "mov.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.l @($imm12x4, $rm), $rn */
  {
    SH_INSN_MOVL12_COMPACT, "movl12-compact", "mov.l", 32,
    { 0|A(32_BIT_INSN), { { { (1<<MACH_SH2A_NOFPU)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mov.l $rm, @($imm12x4, $rn) */
  {
    SH_INSN_MOVL13_COMPACT, "movl13-compact", "mov.l", 32,
    { 0|A(32_BIT_INSN), { { { (1<<MACH_SH2A_NOFPU)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH4)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mov.w $rm, @$rn */
  {
    SH_INSN_MOVW1_COMPACT, "movw1-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w $rm, @-$rn */
  {
    SH_INSN_MOVW2_COMPACT, "movw2-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w $rm, @(r0, $rn) */
  {
    SH_INSN_MOVW3_COMPACT, "movw3-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w r0, @($imm8x2, gbr) */
  {
    SH_INSN_MOVW4_COMPACT, "movw4-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w r0, @($imm4x2, $rm) */
  {
    SH_INSN_MOVW5_COMPACT, "movw5-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w @$rm, $rn */
  {
    SH_INSN_MOVW6_COMPACT, "movw6-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w @${rm}+, $rn */
  {
    SH_INSN_MOVW7_COMPACT, "movw7-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w @(r0, $rm), $rn */
  {
    SH_INSN_MOVW8_COMPACT, "movw8-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w @($imm8x2, gbr), r0 */
  {
    SH_INSN_MOVW9_COMPACT, "movw9-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w @($imm8x2, pc), $rn */
  {
    SH_INSN_MOVW10_COMPACT, "movw10-compact", "mov.w", 16,
    { 0|A(ILLSLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mov.w @($imm4x2, $rm), r0 */
  {
    SH_INSN_MOVW11_COMPACT, "movw11-compact", "mov.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mova @($imm8x4, pc), r0 */
  {
    SH_INSN_MOVA_COMPACT, "mova-compact", "mova", 16,
    { 0|A(ILLSLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* movca.l r0, @$rn */
  {
    SH_INSN_MOVCAL_COMPACT, "movcal-compact", "movca.l", 16,
    { 0, { { { (1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* movco.l r0, @$rn */
  {
    SH_INSN_MOVCOL_COMPACT, "movcol-compact", "movco.l", 16,
    { 0, { { { (1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* movt $rn */
  {
    SH_INSN_MOVT_COMPACT, "movt-compact", "movt", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* movua.l @$rn, r0 */
  {
    SH_INSN_MOVUAL_COMPACT, "movual-compact", "movua.l", 16,
    { 0, { { { (1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* movua.l @$rn+, r0 */
  {
    SH_INSN_MOVUAL2_COMPACT, "movual2-compact", "movua.l", 16,
    { 0, { { { (1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* mul.l $rm, $rn */
  {
    SH_INSN_MULL_COMPACT, "mull-compact", "mul.l", 16,
    { 0, { { { (1<<MACH_SH2)|(1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH2A_NOFPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* muls.w $rm, $rn */
  {
    SH_INSN_MULSW_COMPACT, "mulsw-compact", "muls.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* mulu.w $rm, $rn */
  {
    SH_INSN_MULUW_COMPACT, "muluw-compact", "mulu.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* neg $rm, $rn */
  {
    SH_INSN_NEG_COMPACT, "neg-compact", "neg", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* negc $rm, $rn */
  {
    SH_INSN_NEGC_COMPACT, "negc-compact", "negc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* nop */
  {
    SH_INSN_NOP_COMPACT, "nop-compact", "nop", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_MT, 0 } } } }
  },
/* not $rm64, $rn64 */
  {
    SH_INSN_NOT_COMPACT, "not-compact", "not", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* ocbi @$rn */
  {
    SH_INSN_OCBI_COMPACT, "ocbi-compact", "ocbi", 16,
    { 0, { { { (1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* ocbp @$rn */
  {
    SH_INSN_OCBP_COMPACT, "ocbp-compact", "ocbp", 16,
    { 0, { { { (1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* ocbwb @$rn */
  {
    SH_INSN_OCBWB_COMPACT, "ocbwb-compact", "ocbwb", 16,
    { 0, { { { (1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* or $rm64, $rn64 */
  {
    SH_INSN_OR_COMPACT, "or-compact", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* or #$uimm8, r0 */
  {
    SH_INSN_ORI_COMPACT, "ori-compact", "or", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* or.b #$imm8, @(r0, gbr) */
  {
    SH_INSN_ORB_COMPACT, "orb-compact", "or.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* pref @$rn */
  {
    SH_INSN_PREF_COMPACT, "pref-compact", "pref", 16,
    { 0, { { { (1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* rotcl $rn */
  {
    SH_INSN_ROTCL_COMPACT, "rotcl-compact", "rotcl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* rotcr $rn */
  {
    SH_INSN_ROTCR_COMPACT, "rotcr-compact", "rotcr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* rotl $rn */
  {
    SH_INSN_ROTL_COMPACT, "rotl-compact", "rotl", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* rotr $rn */
  {
    SH_INSN_ROTR_COMPACT, "rotr-compact", "rotr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* rts */
  {
    SH_INSN_RTS_COMPACT, "rts-compact", "rts", 16,
    { 0|A(UNCOND_CTI)|A(DELAY_SLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_BR, 0 } } } }
  },
/* sets */
  {
    SH_INSN_SETS_COMPACT, "sets-compact", "sets", 16,
    { 0, { { { (1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* sett */
  {
    SH_INSN_SETT_COMPACT, "sett-compact", "sett", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shad $rm, $rn */
  {
    SH_INSN_SHAD_COMPACT, "shad-compact", "shad", 16,
    { 0, { { { (1<<MACH_SH2A_NOFPU)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shal $rn */
  {
    SH_INSN_SHAL_COMPACT, "shal-compact", "shal", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shar $rn */
  {
    SH_INSN_SHAR_COMPACT, "shar-compact", "shar", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shld $rm, $rn */
  {
    SH_INSN_SHLD_COMPACT, "shld-compact", "shld", 16,
    { 0, { { { (1<<MACH_SH3)|(1<<MACH_SH3E)|(1<<MACH_SH4_NOFPU)|(1<<MACH_SH4)|(1<<MACH_SH4A_NOFPU)|(1<<MACH_SH4A)|(1<<MACH_SH4AL)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shll $rn */
  {
    SH_INSN_SHLL_COMPACT, "shll-compact", "shll", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shll2 $rn */
  {
    SH_INSN_SHLL2_COMPACT, "shll2-compact", "shll2", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shll8 $rn */
  {
    SH_INSN_SHLL8_COMPACT, "shll8-compact", "shll8", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shll16 $rn */
  {
    SH_INSN_SHLL16_COMPACT, "shll16-compact", "shll16", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shlr $rn */
  {
    SH_INSN_SHLR_COMPACT, "shlr-compact", "shlr", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shlr2 $rn */
  {
    SH_INSN_SHLR2_COMPACT, "shlr2-compact", "shlr2", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shlr8 $rn */
  {
    SH_INSN_SHLR8_COMPACT, "shlr8-compact", "shlr8", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* shlr16 $rn */
  {
    SH_INSN_SHLR16_COMPACT, "shlr16-compact", "shlr16", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* stc gbr, $rn */
  {
    SH_INSN_STC_GBR_COMPACT, "stc-gbr-compact", "stc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stc vbr, $rn */
  {
    SH_INSN_STC_VBR_COMPACT, "stc-vbr-compact", "stc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stc.l gbr, @-$rn */
  {
    SH_INSN_STCL_GBR_COMPACT, "stcl-gbr-compact", "stc.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stc.l vbr, @-$rn */
  {
    SH_INSN_STCL_VBR_COMPACT, "stcl-vbr-compact", "stc.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* sts fpscr, $rn */
  {
    SH_INSN_STS_FPSCR_COMPACT, "sts-fpscr-compact", "sts", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts.l fpscr, @-$rn */
  {
    SH_INSN_STSL_FPSCR_COMPACT, "stsl-fpscr-compact", "sts.l", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts fpul, $rn */
  {
    SH_INSN_STS_FPUL_COMPACT, "sts-fpul-compact", "sts", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_LS, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts.l fpul, @-$rn */
  {
    SH_INSN_STSL_FPUL_COMPACT, "stsl-fpul-compact", "sts.l", 16,
    { 0, { { { (1<<MACH_SH2E)|(1<<MACH_SH2A_FPU)|(1<<MACH_SH3E)|(1<<MACH_SH4)|(1<<MACH_SH4A)|(1<<MACH_SH5), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts mach, $rn */
  {
    SH_INSN_STS_MACH_COMPACT, "sts-mach-compact", "sts", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts.l mach, @-$rn */
  {
    SH_INSN_STSL_MACH_COMPACT, "stsl-mach-compact", "sts.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts macl, $rn */
  {
    SH_INSN_STS_MACL_COMPACT, "sts-macl-compact", "sts", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts.l macl, @-$rn */
  {
    SH_INSN_STSL_MACL_COMPACT, "stsl-macl-compact", "sts.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts pr, $rn */
  {
    SH_INSN_STS_PR_COMPACT, "sts-pr-compact", "sts", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sts.l pr, @-$rn */
  {
    SH_INSN_STSL_PR_COMPACT, "stsl-pr-compact", "sts.l", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_LS, 0 } } } }
  },
/* sub $rm, $rn */
  {
    SH_INSN_SUB_COMPACT, "sub-compact", "sub", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* subc $rm, $rn */
  {
    SH_INSN_SUBC_COMPACT, "subc-compact", "subc", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* subv $rm, $rn */
  {
    SH_INSN_SUBV_COMPACT, "subv-compact", "subv", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* swap.b $rm, $rn */
  {
    SH_INSN_SWAPB_COMPACT, "swapb-compact", "swap.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* swap.w $rm, $rn */
  {
    SH_INSN_SWAPW_COMPACT, "swapw-compact", "swap.w", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* tas.b @$rn */
  {
    SH_INSN_TASB_COMPACT, "tasb-compact", "tas.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* trapa #$uimm8 */
  {
    SH_INSN_TRAPA_COMPACT, "trapa-compact", "trapa", 16,
    { 0|A(ILLSLOT), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* tst $rm, $rn */
  {
    SH_INSN_TST_COMPACT, "tst-compact", "tst", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* tst #$uimm8, r0 */
  {
    SH_INSN_TSTI_COMPACT, "tsti-compact", "tst", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_MT, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* tst.b #$imm8, @(r0, gbr) */
  {
    SH_INSN_TSTB_COMPACT, "tstb-compact", "tst.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* xor $rm64, $rn64 */
  {
    SH_INSN_XOR_COMPACT, "xor-compact", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* xor #$uimm8, r0 */
  {
    SH_INSN_XORI_COMPACT, "xori-compact", "xor", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* xor.b #$imm8, @(r0, gbr) */
  {
    SH_INSN_XORB_COMPACT, "xorb-compact", "xor.b", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_CO, 0 } }, { { SH4A_GROUP_CO, 0 } } } }
  },
/* xtrct $rm, $rn */
  {
    SH_INSN_XTRCT_COMPACT, "xtrct-compact", "xtrct", 16,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x80" } }, { { SH4_GROUP_EX, 0 } }, { { SH4A_GROUP_EX, 0 } } } }
  },
/* add $rm, $rn, $rd */
  {
    SH_INSN_ADD, "add", "add", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* add.l $rm, $rn, $rd */
  {
    SH_INSN_ADDL, "addl", "add.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* addi $rm, $disp10, $rd */
  {
    SH_INSN_ADDI, "addi", "addi", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* addi.l $rm, $disp10, $rd */
  {
    SH_INSN_ADDIL, "addil", "addi.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* addz.l $rm, $rn, $rd */
  {
    SH_INSN_ADDZL, "addzl", "addz.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* alloco $rm, $disp6x32 */
  {
    SH_INSN_ALLOCO, "alloco", "alloco", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* and $rm, $rn, $rd */
  {
    SH_INSN_AND, "and", "and", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* andc $rm, $rn, $rd */
  {
    SH_INSN_ANDC, "andc", "andc", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* andi $rm, $disp10, $rd */
  {
    SH_INSN_ANDI, "andi", "andi", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* beq$likely $rm, $rn, $tra */
  {
    SH_INSN_BEQ, "beq", "beq", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* beqi$likely $rm, $imm6, $tra */
  {
    SH_INSN_BEQI, "beqi", "beqi", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* bge$likely $rm, $rn, $tra */
  {
    SH_INSN_BGE, "bge", "bge", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* bgeu$likely $rm, $rn, $tra */
  {
    SH_INSN_BGEU, "bgeu", "bgeu", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* bgt$likely $rm, $rn, $tra */
  {
    SH_INSN_BGT, "bgt", "bgt", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* bgtu$likely $rm, $rn, $tra */
  {
    SH_INSN_BGTU, "bgtu", "bgtu", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* blink $trb, $rd */
  {
    SH_INSN_BLINK, "blink", "blink", 32,
    { 0|A(UNCOND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* bne$likely $rm, $rn, $tra */
  {
    SH_INSN_BNE, "bne", "bne", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* bnei$likely $rm, $imm6, $tra */
  {
    SH_INSN_BNEI, "bnei", "bnei", 32,
    { 0|A(COND_CTI), { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* brk */
  {
    SH_INSN_BRK, "brk", "brk", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* byterev $rm, $rd */
  {
    SH_INSN_BYTEREV, "byterev", "byterev", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* cmpeq $rm, $rn, $rd */
  {
    SH_INSN_CMPEQ, "cmpeq", "cmpeq", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* cmpgt $rm, $rn, $rd */
  {
    SH_INSN_CMPGT, "cmpgt", "cmpgt", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* cmpgtu $rm,$rn, $rd */
  {
    SH_INSN_CMPGTU, "cmpgtu", "cmpgtu", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* cmveq $rm, $rn, $rd */
  {
    SH_INSN_CMVEQ, "cmveq", "cmveq", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* cmvne $rm, $rn, $rd */
  {
    SH_INSN_CMVNE, "cmvne", "cmvne", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fabs.d $drgh, $drf */
  {
    SH_INSN_FABSD, "fabsd", "fabs.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fabs.s $frgh, $frf */
  {
    SH_INSN_FABSS, "fabss", "fabs.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fadd.d $drg, $drh, $drf */
  {
    SH_INSN_FADDD, "faddd", "fadd.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fadd.s $frg, $frh, $frf */
  {
    SH_INSN_FADDS, "fadds", "fadd.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpeq.d $drg, $drh, $rd */
  {
    SH_INSN_FCMPEQD, "fcmpeqd", "fcmpeq.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpeq.s $frg, $frh, $rd */
  {
    SH_INSN_FCMPEQS, "fcmpeqs", "fcmpeq.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpge.d $drg, $drh, $rd */
  {
    SH_INSN_FCMPGED, "fcmpged", "fcmpge.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpge.s $frg, $frh, $rd */
  {
    SH_INSN_FCMPGES, "fcmpges", "fcmpge.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpgt.d $drg, $drh, $rd */
  {
    SH_INSN_FCMPGTD, "fcmpgtd", "fcmpgt.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpgt.s $frg, $frh, $rd */
  {
    SH_INSN_FCMPGTS, "fcmpgts", "fcmpgt.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpun.d $drg, $drh, $rd */
  {
    SH_INSN_FCMPUND, "fcmpund", "fcmpun.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcmpun.s $frg, $frh, $rd */
  {
    SH_INSN_FCMPUNS, "fcmpuns", "fcmpun.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcnv.ds $drgh, $frf */
  {
    SH_INSN_FCNVDS, "fcnvds", "fcnv.ds", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fcnv.sd $frgh, $drf */
  {
    SH_INSN_FCNVSD, "fcnvsd", "fcnv.sd", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fdiv.d $drg, $drh, $drf */
  {
    SH_INSN_FDIVD, "fdivd", "fdiv.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fdiv.s $frg, $frh, $frf */
  {
    SH_INSN_FDIVS, "fdivs", "fdiv.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fgetscr $frf */
  {
    SH_INSN_FGETSCR, "fgetscr", "fgetscr", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fipr.s $fvg, $fvh, $frf */
  {
    SH_INSN_FIPRS, "fiprs", "fipr.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fld.d $rm, $disp10x8, $drf */
  {
    SH_INSN_FLDD, "fldd", "fld.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fld.p $rm, $disp10x8, $fpf */
  {
    SH_INSN_FLDP, "fldp", "fld.p", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fld.s $rm, $disp10x4, $frf */
  {
    SH_INSN_FLDS, "flds", "fld.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fldx.d $rm, $rn, $drf */
  {
    SH_INSN_FLDXD, "fldxd", "fldx.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fldx.p $rm, $rn, $fpf */
  {
    SH_INSN_FLDXP, "fldxp", "fldx.p", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fldx.s $rm, $rn, $frf */
  {
    SH_INSN_FLDXS, "fldxs", "fldx.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* float.ld $frgh, $drf */
  {
    SH_INSN_FLOATLD, "floatld", "float.ld", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* float.ls $frgh, $frf */
  {
    SH_INSN_FLOATLS, "floatls", "float.ls", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* float.qd $drgh, $drf */
  {
    SH_INSN_FLOATQD, "floatqd", "float.qd", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* float.qs $drgh, $frf */
  {
    SH_INSN_FLOATQS, "floatqs", "float.qs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmac.s $frg, $frh, $frf */
  {
    SH_INSN_FMACS, "fmacs", "fmac.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmov.d $drgh, $drf */
  {
    SH_INSN_FMOVD, "fmovd", "fmov.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmov.dq $drgh, $rd */
  {
    SH_INSN_FMOVDQ, "fmovdq", "fmov.dq", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmov.ls $rm, $frf */
  {
    SH_INSN_FMOVLS, "fmovls", "fmov.ls", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmov.qd $rm, $drf */
  {
    SH_INSN_FMOVQD, "fmovqd", "fmov.qd", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmov.s $frgh, $frf */
  {
    SH_INSN_FMOVS, "fmovs", "fmov.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmov.sl $frgh, $rd */
  {
    SH_INSN_FMOVSL, "fmovsl", "fmov.sl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmul.d $drg, $drh, $drf */
  {
    SH_INSN_FMULD, "fmuld", "fmul.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fmul.s $frg, $frh, $frf */
  {
    SH_INSN_FMULS, "fmuls", "fmul.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fneg.d $drgh, $drf */
  {
    SH_INSN_FNEGD, "fnegd", "fneg.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fneg.s $frgh, $frf */
  {
    SH_INSN_FNEGS, "fnegs", "fneg.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fputscr $frgh */
  {
    SH_INSN_FPUTSCR, "fputscr", "fputscr", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fsqrt.d $drgh, $drf */
  {
    SH_INSN_FSQRTD, "fsqrtd", "fsqrt.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fsqrt.s $frgh, $frf */
  {
    SH_INSN_FSQRTS, "fsqrts", "fsqrt.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fst.d $rm, $disp10x8, $drf */
  {
    SH_INSN_FSTD, "fstd", "fst.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fst.p $rm, $disp10x8, $fpf */
  {
    SH_INSN_FSTP, "fstp", "fst.p", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fst.s $rm, $disp10x4, $frf */
  {
    SH_INSN_FSTS, "fsts", "fst.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fstx.d $rm, $rn, $drf */
  {
    SH_INSN_FSTXD, "fstxd", "fstx.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fstx.p $rm, $rn, $fpf */
  {
    SH_INSN_FSTXP, "fstxp", "fstx.p", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fstx.s $rm, $rn, $frf */
  {
    SH_INSN_FSTXS, "fstxs", "fstx.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fsub.d $drg, $drh, $drf */
  {
    SH_INSN_FSUBD, "fsubd", "fsub.d", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* fsub.s $frg, $frh, $frf */
  {
    SH_INSN_FSUBS, "fsubs", "fsub.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ftrc.dl $drgh, $frf */
  {
    SH_INSN_FTRCDL, "ftrcdl", "ftrc.dl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ftrc.sl $frgh, $frf */
  {
    SH_INSN_FTRCSL, "ftrcsl", "ftrc.sl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ftrc.dq $drgh, $drf */
  {
    SH_INSN_FTRCDQ, "ftrcdq", "ftrc.dq", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ftrc.sq $frgh, $drf */
  {
    SH_INSN_FTRCSQ, "ftrcsq", "ftrc.sq", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ftrv.s $mtrxg, $fvh, $fvf */
  {
    SH_INSN_FTRVS, "ftrvs", "ftrv.s", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* getcfg $rm, $disp6, $rd */
  {
    SH_INSN_GETCFG, "getcfg", "getcfg", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* getcon $crk, $rd */
  {
    SH_INSN_GETCON, "getcon", "getcon", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* gettr $trb, $rd */
  {
    SH_INSN_GETTR, "gettr", "gettr", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* icbi $rm, $disp6x32 */
  {
    SH_INSN_ICBI, "icbi", "icbi", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ld.b $rm, $disp10, $rd */
  {
    SH_INSN_LDB, "ldb", "ld.b", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ld.l $rm, $disp10x4, $rd */
  {
    SH_INSN_LDL, "ldl", "ld.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ld.q $rm, $disp10x8, $rd */
  {
    SH_INSN_LDQ, "ldq", "ld.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ld.ub $rm, $disp10, $rd */
  {
    SH_INSN_LDUB, "ldub", "ld.ub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ld.uw $rm, $disp10x2, $rd */
  {
    SH_INSN_LDUW, "lduw", "ld.uw", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ld.w $rm, $disp10x2, $rd */
  {
    SH_INSN_LDW, "ldw", "ld.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldhi.l $rm, $disp6, $rd */
  {
    SH_INSN_LDHIL, "ldhil", "ldhi.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldhi.q $rm, $disp6, $rd */
  {
    SH_INSN_LDHIQ, "ldhiq", "ldhi.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldlo.l $rm, $disp6, $rd */
  {
    SH_INSN_LDLOL, "ldlol", "ldlo.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldlo.q $rm, $disp6, $rd */
  {
    SH_INSN_LDLOQ, "ldloq", "ldlo.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldx.b $rm, $rn, $rd */
  {
    SH_INSN_LDXB, "ldxb", "ldx.b", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldx.l $rm, $rn, $rd */
  {
    SH_INSN_LDXL, "ldxl", "ldx.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldx.q $rm, $rn, $rd */
  {
    SH_INSN_LDXQ, "ldxq", "ldx.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldx.ub $rm, $rn, $rd */
  {
    SH_INSN_LDXUB, "ldxub", "ldx.ub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldx.uw $rm, $rn, $rd */
  {
    SH_INSN_LDXUW, "ldxuw", "ldx.uw", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ldx.w $rm, $rn, $rd */
  {
    SH_INSN_LDXW, "ldxw", "ldx.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mabs.l $rm, $rd */
  {
    SH_INSN_MABSL, "mabsl", "mabs.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mabs.w $rm, $rd */
  {
    SH_INSN_MABSW, "mabsw", "mabs.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* madd.l $rm, $rn, $rd */
  {
    SH_INSN_MADDL, "maddl", "madd.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* madd.w $rm, $rn, $rd */
  {
    SH_INSN_MADDW, "maddw", "madd.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* madds.l $rm, $rn, $rd */
  {
    SH_INSN_MADDSL, "maddsl", "madds.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* madds.ub $rm, $rn, $rd */
  {
    SH_INSN_MADDSUB, "maddsub", "madds.ub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* madds.w $rm, $rn, $rd */
  {
    SH_INSN_MADDSW, "maddsw", "madds.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcmpeq.b $rm, $rn, $rd */
  {
    SH_INSN_MCMPEQB, "mcmpeqb", "mcmpeq.b", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcmpeq.l $rm, $rn, $rd */
  {
    SH_INSN_MCMPEQL, "mcmpeql", "mcmpeq.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcmpeq.w $rm, $rn, $rd */
  {
    SH_INSN_MCMPEQW, "mcmpeqw", "mcmpeq.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcmpgt.l $rm, $rn, $rd */
  {
    SH_INSN_MCMPGTL, "mcmpgtl", "mcmpgt.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcmpgt.ub $rm, $rn, $rd */
  {
    SH_INSN_MCMPGTUB, "mcmpgtub", "mcmpgt.ub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcmpgt.w $rm, $rn, $rd */
  {
    SH_INSN_MCMPGTW, "mcmpgtw", "mcmpgt.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcmv $rm, $rn, $rd */
  {
    SH_INSN_MCMV, "mcmv", "mcmv", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcnvs.lw $rm, $rn, $rd */
  {
    SH_INSN_MCNVSLW, "mcnvslw", "mcnvs.lw", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcnvs.wb $rm, $rn, $rd */
  {
    SH_INSN_MCNVSWB, "mcnvswb", "mcnvs.wb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mcnvs.wub $rm, $rn, $rd */
  {
    SH_INSN_MCNVSWUB, "mcnvswub", "mcnvs.wub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mextr1 $rm, $rn, $rd */
  {
    SH_INSN_MEXTR1, "mextr1", "mextr1", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mextr2 $rm, $rn, $rd */
  {
    SH_INSN_MEXTR2, "mextr2", "mextr2", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mextr3 $rm, $rn, $rd */
  {
    SH_INSN_MEXTR3, "mextr3", "mextr3", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mextr4 $rm, $rn, $rd */
  {
    SH_INSN_MEXTR4, "mextr4", "mextr4", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mextr5 $rm, $rn, $rd */
  {
    SH_INSN_MEXTR5, "mextr5", "mextr5", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mextr6 $rm, $rn, $rd */
  {
    SH_INSN_MEXTR6, "mextr6", "mextr6", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mextr7 $rm, $rn, $rd */
  {
    SH_INSN_MEXTR7, "mextr7", "mextr7", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmacfx.wl $rm, $rn, $rd */
  {
    SH_INSN_MMACFXWL, "mmacfxwl", "mmacfx.wl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmacnfx.wl $rm, $rn, $rd */
  {
    SH_INSN_MMACNFX_WL, "mmacnfx.wl", "mmacnfx.wl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmul.l $rm, $rn, $rd */
  {
    SH_INSN_MMULL, "mmull", "mmul.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmul.w $rm, $rn, $rd */
  {
    SH_INSN_MMULW, "mmulw", "mmul.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmulfx.l $rm, $rn, $rd */
  {
    SH_INSN_MMULFXL, "mmulfxl", "mmulfx.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmulfx.w $rm, $rn, $rd */
  {
    SH_INSN_MMULFXW, "mmulfxw", "mmulfx.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmulfxrp.w $rm, $rn, $rd */
  {
    SH_INSN_MMULFXRPW, "mmulfxrpw", "mmulfxrp.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmulhi.wl $rm, $rn, $rd */
  {
    SH_INSN_MMULHIWL, "mmulhiwl", "mmulhi.wl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmullo.wl $rm, $rn, $rd */
  {
    SH_INSN_MMULLOWL, "mmullowl", "mmullo.wl", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mmulsum.wq $rm, $rn, $rd */
  {
    SH_INSN_MMULSUMWQ, "mmulsumwq", "mmulsum.wq", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* movi $imm16, $rd */
  {
    SH_INSN_MOVI, "movi", "movi", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mperm.w $rm, $rn, $rd */
  {
    SH_INSN_MPERMW, "mpermw", "mperm.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* msad.ubq $rm, $rn, $rd */
  {
    SH_INSN_MSADUBQ, "msadubq", "msad.ubq", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshalds.l $rm, $rn, $rd */
  {
    SH_INSN_MSHALDSL, "mshaldsl", "mshalds.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshalds.w $rm, $rn, $rd */
  {
    SH_INSN_MSHALDSW, "mshaldsw", "mshalds.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshard.l $rm, $rn, $rd */
  {
    SH_INSN_MSHARDL, "mshardl", "mshard.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshard.w $rm, $rn, $rd */
  {
    SH_INSN_MSHARDW, "mshardw", "mshard.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshards.q $rm, $rn, $rd */
  {
    SH_INSN_MSHARDSQ, "mshardsq", "mshards.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshfhi.b $rm, $rn, $rd */
  {
    SH_INSN_MSHFHIB, "mshfhib", "mshfhi.b", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshfhi.l $rm, $rn, $rd */
  {
    SH_INSN_MSHFHIL, "mshfhil", "mshfhi.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshfhi.w $rm, $rn, $rd */
  {
    SH_INSN_MSHFHIW, "mshfhiw", "mshfhi.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshflo.b $rm, $rn, $rd */
  {
    SH_INSN_MSHFLOB, "mshflob", "mshflo.b", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshflo.l $rm, $rn, $rd */
  {
    SH_INSN_MSHFLOL, "mshflol", "mshflo.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshflo.w $rm, $rn, $rd */
  {
    SH_INSN_MSHFLOW, "mshflow", "mshflo.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshlld.l $rm, $rn, $rd */
  {
    SH_INSN_MSHLLDL, "mshlldl", "mshlld.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshlld.w $rm, $rn, $rd */
  {
    SH_INSN_MSHLLDW, "mshlldw", "mshlld.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshlrd.l $rm, $rn, $rd */
  {
    SH_INSN_MSHLRDL, "mshlrdl", "mshlrd.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mshlrd.w $rm, $rn, $rd */
  {
    SH_INSN_MSHLRDW, "mshlrdw", "mshlrd.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* msub.l $rm, $rn, $rd */
  {
    SH_INSN_MSUBL, "msubl", "msub.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* msub.w $rm, $rn, $rd */
  {
    SH_INSN_MSUBW, "msubw", "msub.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* msubs.l $rm, $rn, $rd */
  {
    SH_INSN_MSUBSL, "msubsl", "msubs.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* msubs.ub $rm, $rn, $rd */
  {
    SH_INSN_MSUBSUB, "msubsub", "msubs.ub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* msubs.w $rm, $rn, $rd */
  {
    SH_INSN_MSUBSW, "msubsw", "msubs.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* muls.l $rm, $rn, $rd */
  {
    SH_INSN_MULSL, "mulsl", "muls.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* mulu.l $rm, $rn, $rd */
  {
    SH_INSN_MULUL, "mulul", "mulu.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* nop */
  {
    SH_INSN_NOP, "nop", "nop", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* nsb $rm, $rd */
  {
    SH_INSN_NSB, "nsb", "nsb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ocbi $rm, $disp6x32 */
  {
    SH_INSN_OCBI, "ocbi", "ocbi", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ocbp $rm, $disp6x32 */
  {
    SH_INSN_OCBP, "ocbp", "ocbp", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ocbwb $rm, $disp6x32 */
  {
    SH_INSN_OCBWB, "ocbwb", "ocbwb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* or $rm, $rn, $rd */
  {
    SH_INSN_OR, "or", "or", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ori $rm, $imm10, $rd */
  {
    SH_INSN_ORI, "ori", "ori", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* prefi $rm, $disp6x32 */
  {
    SH_INSN_PREFI, "prefi", "prefi", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* pta$likely $disp16, $tra */
  {
    SH_INSN_PTA, "pta", "pta", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ptabs$likely $rn, $tra */
  {
    SH_INSN_PTABS, "ptabs", "ptabs", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ptb$likely $disp16, $tra */
  {
    SH_INSN_PTB, "ptb", "ptb", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* ptrel$likely $rn, $tra */
  {
    SH_INSN_PTREL, "ptrel", "ptrel", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* putcfg $rm, $disp6, $rd */
  {
    SH_INSN_PUTCFG, "putcfg", "putcfg", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* putcon $rm, $crj */
  {
    SH_INSN_PUTCON, "putcon", "putcon", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* rte */
  {
    SH_INSN_RTE, "rte", "rte", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shard $rm, $rn, $rd */
  {
    SH_INSN_SHARD, "shard", "shard", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shard.l $rm, $rn, $rd */
  {
    SH_INSN_SHARDL, "shardl", "shard.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shari $rm, $uimm6, $rd */
  {
    SH_INSN_SHARI, "shari", "shari", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shari.l $rm, $uimm6, $rd */
  {
    SH_INSN_SHARIL, "sharil", "shari.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlld $rm, $rn, $rd */
  {
    SH_INSN_SHLLD, "shlld", "shlld", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlld.l $rm, $rn, $rd */
  {
    SH_INSN_SHLLDL, "shlldl", "shlld.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlli $rm, $uimm6, $rd */
  {
    SH_INSN_SHLLI, "shlli", "shlli", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlli.l $rm, $uimm6, $rd */
  {
    SH_INSN_SHLLIL, "shllil", "shlli.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlrd $rm, $rn, $rd */
  {
    SH_INSN_SHLRD, "shlrd", "shlrd", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlrd.l $rm, $rn, $rd */
  {
    SH_INSN_SHLRDL, "shlrdl", "shlrd.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlri $rm, $uimm6, $rd */
  {
    SH_INSN_SHLRI, "shlri", "shlri", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shlri.l $rm, $uimm6, $rd */
  {
    SH_INSN_SHLRIL, "shlril", "shlri.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* shori $uimm16, $rd */
  {
    SH_INSN_SHORI, "shori", "shori", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* sleep */
  {
    SH_INSN_SLEEP, "sleep", "sleep", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* st.b $rm, $disp10, $rd */
  {
    SH_INSN_STB, "stb", "st.b", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* st.l $rm, $disp10x4, $rd */
  {
    SH_INSN_STL, "stl", "st.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* st.q $rm, $disp10x8, $rd */
  {
    SH_INSN_STQ, "stq", "st.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* st.w $rm, $disp10x2, $rd */
  {
    SH_INSN_STW, "stw", "st.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* sthi.l $rm, $disp6, $rd */
  {
    SH_INSN_STHIL, "sthil", "sthi.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* sthi.q $rm, $disp6, $rd */
  {
    SH_INSN_STHIQ, "sthiq", "sthi.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stlo.l $rm, $disp6, $rd */
  {
    SH_INSN_STLOL, "stlol", "stlo.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stlo.q $rm, $disp6, $rd */
  {
    SH_INSN_STLOQ, "stloq", "stlo.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stx.b $rm, $rn, $rd */
  {
    SH_INSN_STXB, "stxb", "stx.b", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stx.l $rm, $rn, $rd */
  {
    SH_INSN_STXL, "stxl", "stx.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stx.q $rm, $rn, $rd */
  {
    SH_INSN_STXQ, "stxq", "stx.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* stx.w $rm, $rn, $rd */
  {
    SH_INSN_STXW, "stxw", "stx.w", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* sub $rm, $rn, $rd */
  {
    SH_INSN_SUB, "sub", "sub", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* sub.l $rm, $rn, $rd */
  {
    SH_INSN_SUBL, "subl", "sub.l", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* swap.q $rm, $rn, $rd */
  {
    SH_INSN_SWAPQ, "swapq", "swap.q", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* synci */
  {
    SH_INSN_SYNCI, "synci", "synci", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* synco */
  {
    SH_INSN_SYNCO, "synco", "synco", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* trapa $rm */
  {
    SH_INSN_TRAPA, "trapa", "trapa", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* xor $rm, $rn, $rd */
  {
    SH_INSN_XOR, "xor", "xor", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
/* xori $rm, $imm6, $rd */
  {
    SH_INSN_XORI, "xori", "xori", 32,
    { 0, { { { (1<<MACH_BASE), 0 } }, { { 1, "\x40" } }, { { SH4_GROUP_NONE, 0 } }, { { SH4A_GROUP_NONE, 0 } } } }
  },
};

#undef OP
#undef A

/* Initialize anything needed to be done once, before any cpu_open call.  */

static void
init_tables (void)
{
}

static const CGEN_MACH * lookup_mach_via_bfd_name (const CGEN_MACH *, const char *);
static void build_hw_table      (CGEN_CPU_TABLE *);
static void build_ifield_table  (CGEN_CPU_TABLE *);
static void build_operand_table (CGEN_CPU_TABLE *);
static void build_insn_table    (CGEN_CPU_TABLE *);
static void sh_cgen_rebuild_tables (CGEN_CPU_TABLE *);

/* Subroutine of sh_cgen_cpu_open to look up a mach via its bfd name.  */

static const CGEN_MACH *
lookup_mach_via_bfd_name (const CGEN_MACH *table, const char *name)
{
  while (table->name)
    {
      if (strcmp (name, table->bfd_name) == 0)
	return table;
      ++table;
    }
  abort ();
}

/* Subroutine of sh_cgen_cpu_open to build the hardware table.  */

static void
build_hw_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_HW_ENTRY *init = & sh_cgen_hw_table[0];
  /* MAX_HW is only an upper bound on the number of selected entries.
     However each entry is indexed by it's enum so there can be holes in
     the table.  */
  const CGEN_HW_ENTRY **selected =
    (const CGEN_HW_ENTRY **) xmalloc (MAX_HW * sizeof (CGEN_HW_ENTRY *));

  cd->hw_table.init_entries = init;
  cd->hw_table.entry_size = sizeof (CGEN_HW_ENTRY);
  memset (selected, 0, MAX_HW * sizeof (CGEN_HW_ENTRY *));
  /* ??? For now we just use machs to determine which ones we want.  */
  for (i = 0; init[i].name != NULL; ++i)
    if (CGEN_HW_ATTR_VALUE (&init[i], CGEN_HW_MACH)
	& machs)
      selected[init[i].type] = &init[i];
  cd->hw_table.entries = selected;
  cd->hw_table.num_entries = MAX_HW;
}

/* Subroutine of sh_cgen_cpu_open to build the hardware table.  */

static void
build_ifield_table (CGEN_CPU_TABLE *cd)
{
  cd->ifld_table = & sh_cgen_ifld_table[0];
}

/* Subroutine of sh_cgen_cpu_open to build the hardware table.  */

static void
build_operand_table (CGEN_CPU_TABLE *cd)
{
  int i;
  int machs = cd->machs;
  const CGEN_OPERAND *init = & sh_cgen_operand_table[0];
  /* MAX_OPERANDS is only an upper bound on the number of selected entries.
     However each entry is indexed by it's enum so there can be holes in
     the table.  */
  const CGEN_OPERAND **selected = xmalloc (MAX_OPERANDS * sizeof (* selected));

  cd->operand_table.init_entries = init;
  cd->operand_table.entry_size = sizeof (CGEN_OPERAND);
  memset (selected, 0, MAX_OPERANDS * sizeof (CGEN_OPERAND *));
  /* ??? For now we just use mach to determine which ones we want.  */
  for (i = 0; init[i].name != NULL; ++i)
    if (CGEN_OPERAND_ATTR_VALUE (&init[i], CGEN_OPERAND_MACH)
	& machs)
      selected[init[i].type] = &init[i];
  cd->operand_table.entries = selected;
  cd->operand_table.num_entries = MAX_OPERANDS;
}

/* Subroutine of sh_cgen_cpu_open to build the hardware table.
   ??? This could leave out insns not supported by the specified mach/isa,
   but that would cause errors like "foo only supported by bar" to become
   "unknown insn", so for now we include all insns and require the app to
   do the checking later.
   ??? On the other hand, parsing of such insns may require their hardware or
   operand elements to be in the table [which they mightn't be].  */

static void
build_insn_table (CGEN_CPU_TABLE *cd)
{
  int i;
  const CGEN_IBASE *ib = & sh_cgen_insn_table[0];
  CGEN_INSN *insns = xmalloc (MAX_INSNS * sizeof (CGEN_INSN));

  memset (insns, 0, MAX_INSNS * sizeof (CGEN_INSN));
  for (i = 0; i < MAX_INSNS; ++i)
    insns[i].base = &ib[i];
  cd->insn_table.init_entries = insns;
  cd->insn_table.entry_size = sizeof (CGEN_IBASE);
  cd->insn_table.num_init_entries = MAX_INSNS;
}

/* Subroutine of sh_cgen_cpu_open to rebuild the tables.  */

static void
sh_cgen_rebuild_tables (CGEN_CPU_TABLE *cd)
{
  int i;
  CGEN_BITSET *isas = cd->isas;
  unsigned int machs = cd->machs;

  cd->int_insn_p = CGEN_INT_INSN_P;

  /* Data derived from the isa spec.  */
#define UNSET (CGEN_SIZE_UNKNOWN + 1)
  cd->default_insn_bitsize = UNSET;
  cd->base_insn_bitsize = UNSET;
  cd->min_insn_bitsize = 65535; /* Some ridiculously big number.  */
  cd->max_insn_bitsize = 0;
  for (i = 0; i < MAX_ISAS; ++i)
    if (cgen_bitset_contains (isas, i))
      {
	const CGEN_ISA *isa = & sh_cgen_isa_table[i];

	/* Default insn sizes of all selected isas must be
	   equal or we set the result to 0, meaning "unknown".  */
	if (cd->default_insn_bitsize == UNSET)
	  cd->default_insn_bitsize = isa->default_insn_bitsize;
	else if (isa->default_insn_bitsize == cd->default_insn_bitsize)
	  ; /* This is ok.  */
	else
	  cd->default_insn_bitsize = CGEN_SIZE_UNKNOWN;

	/* Base insn sizes of all selected isas must be equal
	   or we set the result to 0, meaning "unknown".  */
	if (cd->base_insn_bitsize == UNSET)
	  cd->base_insn_bitsize = isa->base_insn_bitsize;
	else if (isa->base_insn_bitsize == cd->base_insn_bitsize)
	  ; /* This is ok.  */
	else
	  cd->base_insn_bitsize = CGEN_SIZE_UNKNOWN;

	/* Set min,max insn sizes.  */
	if (isa->min_insn_bitsize < cd->min_insn_bitsize)
	  cd->min_insn_bitsize = isa->min_insn_bitsize;
	if (isa->max_insn_bitsize > cd->max_insn_bitsize)
	  cd->max_insn_bitsize = isa->max_insn_bitsize;
      }

  /* Data derived from the mach spec.  */
  for (i = 0; i < MAX_MACHS; ++i)
    if (((1 << i) & machs) != 0)
      {
	const CGEN_MACH *mach = & sh_cgen_mach_table[i];

	if (mach->insn_chunk_bitsize != 0)
	{
	  if (cd->insn_chunk_bitsize != 0 && cd->insn_chunk_bitsize != mach->insn_chunk_bitsize)
	    {
	      fprintf (stderr, "sh_cgen_rebuild_tables: conflicting insn-chunk-bitsize values: `%d' vs. `%d'\n",
		       cd->insn_chunk_bitsize, mach->insn_chunk_bitsize);
	      abort ();
	    }

 	  cd->insn_chunk_bitsize = mach->insn_chunk_bitsize;
	}
      }

  /* Determine which hw elements are used by MACH.  */
  build_hw_table (cd);

  /* Build the ifield table.  */
  build_ifield_table (cd);

  /* Determine which operands are used by MACH/ISA.  */
  build_operand_table (cd);

  /* Build the instruction table.  */
  build_insn_table (cd);
}

/* Initialize a cpu table and return a descriptor.
   It's much like opening a file, and must be the first function called.
   The arguments are a set of (type/value) pairs, terminated with
   CGEN_CPU_OPEN_END.

   Currently supported values:
   CGEN_CPU_OPEN_ISAS:    bitmap of values in enum isa_attr
   CGEN_CPU_OPEN_MACHS:   bitmap of values in enum mach_attr
   CGEN_CPU_OPEN_BFDMACH: specify 1 mach using bfd name
   CGEN_CPU_OPEN_ENDIAN:  specify endian choice
   CGEN_CPU_OPEN_END:     terminates arguments

   ??? Simultaneous multiple isas might not make sense, but it's not (yet)
   precluded.

   ??? We only support ISO C stdargs here, not K&R.
   Laziness, plus experiment to see if anything requires K&R - eventually
   K&R will no longer be supported - e.g. GDB is currently trying this.  */

CGEN_CPU_DESC
sh_cgen_cpu_open (enum cgen_cpu_open_arg arg_type, ...)
{
  CGEN_CPU_TABLE *cd = (CGEN_CPU_TABLE *) xmalloc (sizeof (CGEN_CPU_TABLE));
  static int init_p;
  CGEN_BITSET *isas = 0;  /* 0 = "unspecified" */
  unsigned int machs = 0; /* 0 = "unspecified" */
  enum cgen_endian endian = CGEN_ENDIAN_UNKNOWN;
  va_list ap;

  if (! init_p)
    {
      init_tables ();
      init_p = 1;
    }

  memset (cd, 0, sizeof (*cd));

  va_start (ap, arg_type);
  while (arg_type != CGEN_CPU_OPEN_END)
    {
      switch (arg_type)
	{
	case CGEN_CPU_OPEN_ISAS :
	  isas = va_arg (ap, CGEN_BITSET *);
	  break;
	case CGEN_CPU_OPEN_MACHS :
	  machs = va_arg (ap, unsigned int);
	  break;
	case CGEN_CPU_OPEN_BFDMACH :
	  {
	    const char *name = va_arg (ap, const char *);
	    const CGEN_MACH *mach =
	      lookup_mach_via_bfd_name (sh_cgen_mach_table, name);

	    machs |= 1 << mach->num;
	    break;
	  }
	case CGEN_CPU_OPEN_ENDIAN :
	  endian = va_arg (ap, enum cgen_endian);
	  break;
	default :
	  fprintf (stderr, "sh_cgen_cpu_open: unsupported argument `%d'\n",
		   arg_type);
	  abort (); /* ??? return NULL? */
	}
      arg_type = va_arg (ap, enum cgen_cpu_open_arg);
    }
  va_end (ap);

  /* Mach unspecified means "all".  */
  if (machs == 0)
    machs = (1 << MAX_MACHS) - 1;
  /* Base mach is always selected.  */
  machs |= 1;
  if (endian == CGEN_ENDIAN_UNKNOWN)
    {
      /* ??? If target has only one, could have a default.  */
      fprintf (stderr, "sh_cgen_cpu_open: no endianness specified\n");
      abort ();
    }

  cd->isas = cgen_bitset_copy (isas);
  cd->machs = machs;
  cd->endian = endian;
  /* FIXME: for the sparc case we can determine insn-endianness statically.
     The worry here is where both data and insn endian can be independently
     chosen, in which case this function will need another argument.
     Actually, will want to allow for more arguments in the future anyway.  */
  cd->insn_endian = endian;

  /* Table (re)builder.  */
  cd->rebuild_tables = sh_cgen_rebuild_tables;
  sh_cgen_rebuild_tables (cd);

  /* Default to not allowing signed overflow.  */
  cd->signed_overflow_ok_p = 0;
  
  return (CGEN_CPU_DESC) cd;
}

/* Cover fn to sh_cgen_cpu_open to handle the simple case of 1 isa, 1 mach.
   MACH_NAME is the bfd name of the mach.  */

CGEN_CPU_DESC
sh_cgen_cpu_open_1 (const char *mach_name, enum cgen_endian endian)
{
  return sh_cgen_cpu_open (CGEN_CPU_OPEN_BFDMACH, mach_name,
			       CGEN_CPU_OPEN_ENDIAN, endian,
			       CGEN_CPU_OPEN_END);
}

/* Close a cpu table.
   ??? This can live in a machine independent file, but there's currently
   no place to put this file (there's no libcgen).  libopcodes is the wrong
   place as some simulator ports use this but they don't use libopcodes.  */

void
sh_cgen_cpu_close (CGEN_CPU_DESC cd)
{
  unsigned int i;
  const CGEN_INSN *insns;

  if (cd->macro_insn_table.init_entries)
    {
      insns = cd->macro_insn_table.init_entries;
      for (i = 0; i < cd->macro_insn_table.num_init_entries; ++i, ++insns)
	if (CGEN_INSN_RX ((insns)))
	  regfree (CGEN_INSN_RX (insns));
    }

  if (cd->insn_table.init_entries)
    {
      insns = cd->insn_table.init_entries;
      for (i = 0; i < cd->insn_table.num_init_entries; ++i, ++insns)
	if (CGEN_INSN_RX (insns))
	  regfree (CGEN_INSN_RX (insns));
    }  

  if (cd->macro_insn_table.init_entries)
    free ((CGEN_INSN *) cd->macro_insn_table.init_entries);

  if (cd->insn_table.init_entries)
    free ((CGEN_INSN *) cd->insn_table.init_entries);

  if (cd->hw_table.entries)
    free ((CGEN_HW_ENTRY *) cd->hw_table.entries);

  if (cd->operand_table.entries)
    free ((CGEN_HW_ENTRY *) cd->operand_table.entries);

  free (cd);
}

