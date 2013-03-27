/* Simulator instruction decoder for iq2000bf.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.

This file is part of the GNU simulators.

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

#define WANT_CPU iq2000bf
#define WANT_CPU_IQ2000BF

#include "sim-main.h"
#include "sim-assert.h"

/* The instruction descriptor array.
   This is computed at runtime.  Space for it is not malloc'd to save a
   teensy bit of cpu in the decoder.  Moving it to malloc space is trivial
   but won't be done until necessary (we don't currently support the runtime
   addition of instructions nor an SMP machine with different cpus).  */
static IDESC iq2000bf_insn_data[IQ2000BF_INSN_BMB + 1];

/* Commas between elements are contained in the macros.
   Some of these are conditionally compiled out.  */

static const struct insn_sem iq2000bf_insn_sem[] =
{
  { VIRTUAL_INSN_X_INVALID, IQ2000BF_INSN_X_INVALID, IQ2000BF_SFMT_EMPTY },
  { VIRTUAL_INSN_X_AFTER, IQ2000BF_INSN_X_AFTER, IQ2000BF_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEFORE, IQ2000BF_INSN_X_BEFORE, IQ2000BF_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CTI_CHAIN, IQ2000BF_INSN_X_CTI_CHAIN, IQ2000BF_SFMT_EMPTY },
  { VIRTUAL_INSN_X_CHAIN, IQ2000BF_INSN_X_CHAIN, IQ2000BF_SFMT_EMPTY },
  { VIRTUAL_INSN_X_BEGIN, IQ2000BF_INSN_X_BEGIN, IQ2000BF_SFMT_EMPTY },
  { IQ2000_INSN_ADD, IQ2000BF_INSN_ADD, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_ADDI, IQ2000BF_INSN_ADDI, IQ2000BF_SFMT_ADDI },
  { IQ2000_INSN_ADDIU, IQ2000BF_INSN_ADDIU, IQ2000BF_SFMT_ADDI },
  { IQ2000_INSN_ADDU, IQ2000BF_INSN_ADDU, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_ADO16, IQ2000BF_INSN_ADO16, IQ2000BF_SFMT_ADO16 },
  { IQ2000_INSN_AND, IQ2000BF_INSN_AND, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_ANDI, IQ2000BF_INSN_ANDI, IQ2000BF_SFMT_ADDI },
  { IQ2000_INSN_ANDOI, IQ2000BF_INSN_ANDOI, IQ2000BF_SFMT_ADDI },
  { IQ2000_INSN_NOR, IQ2000BF_INSN_NOR, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_OR, IQ2000BF_INSN_OR, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_ORI, IQ2000BF_INSN_ORI, IQ2000BF_SFMT_ADDI },
  { IQ2000_INSN_RAM, IQ2000BF_INSN_RAM, IQ2000BF_SFMT_RAM },
  { IQ2000_INSN_SLL, IQ2000BF_INSN_SLL, IQ2000BF_SFMT_SLL },
  { IQ2000_INSN_SLLV, IQ2000BF_INSN_SLLV, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_SLMV, IQ2000BF_INSN_SLMV, IQ2000BF_SFMT_SLMV },
  { IQ2000_INSN_SLT, IQ2000BF_INSN_SLT, IQ2000BF_SFMT_SLT },
  { IQ2000_INSN_SLTI, IQ2000BF_INSN_SLTI, IQ2000BF_SFMT_SLTI },
  { IQ2000_INSN_SLTIU, IQ2000BF_INSN_SLTIU, IQ2000BF_SFMT_SLTI },
  { IQ2000_INSN_SLTU, IQ2000BF_INSN_SLTU, IQ2000BF_SFMT_SLT },
  { IQ2000_INSN_SRA, IQ2000BF_INSN_SRA, IQ2000BF_SFMT_SLL },
  { IQ2000_INSN_SRAV, IQ2000BF_INSN_SRAV, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_SRL, IQ2000BF_INSN_SRL, IQ2000BF_SFMT_SLL },
  { IQ2000_INSN_SRLV, IQ2000BF_INSN_SRLV, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_SRMV, IQ2000BF_INSN_SRMV, IQ2000BF_SFMT_SLMV },
  { IQ2000_INSN_SUB, IQ2000BF_INSN_SUB, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_SUBU, IQ2000BF_INSN_SUBU, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_XOR, IQ2000BF_INSN_XOR, IQ2000BF_SFMT_ADD },
  { IQ2000_INSN_XORI, IQ2000BF_INSN_XORI, IQ2000BF_SFMT_ADDI },
  { IQ2000_INSN_BBI, IQ2000BF_INSN_BBI, IQ2000BF_SFMT_BBI },
  { IQ2000_INSN_BBIN, IQ2000BF_INSN_BBIN, IQ2000BF_SFMT_BBI },
  { IQ2000_INSN_BBV, IQ2000BF_INSN_BBV, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BBVN, IQ2000BF_INSN_BBVN, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BEQ, IQ2000BF_INSN_BEQ, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BEQL, IQ2000BF_INSN_BEQL, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BGEZ, IQ2000BF_INSN_BGEZ, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_BGEZAL, IQ2000BF_INSN_BGEZAL, IQ2000BF_SFMT_BGEZAL },
  { IQ2000_INSN_BGEZALL, IQ2000BF_INSN_BGEZALL, IQ2000BF_SFMT_BGEZAL },
  { IQ2000_INSN_BGEZL, IQ2000BF_INSN_BGEZL, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_BLTZ, IQ2000BF_INSN_BLTZ, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_BLTZL, IQ2000BF_INSN_BLTZL, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_BLTZAL, IQ2000BF_INSN_BLTZAL, IQ2000BF_SFMT_BGEZAL },
  { IQ2000_INSN_BLTZALL, IQ2000BF_INSN_BLTZALL, IQ2000BF_SFMT_BGEZAL },
  { IQ2000_INSN_BMB0, IQ2000BF_INSN_BMB0, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BMB1, IQ2000BF_INSN_BMB1, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BMB2, IQ2000BF_INSN_BMB2, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BMB3, IQ2000BF_INSN_BMB3, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BNE, IQ2000BF_INSN_BNE, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_BNEL, IQ2000BF_INSN_BNEL, IQ2000BF_SFMT_BBV },
  { IQ2000_INSN_JALR, IQ2000BF_INSN_JALR, IQ2000BF_SFMT_JALR },
  { IQ2000_INSN_JR, IQ2000BF_INSN_JR, IQ2000BF_SFMT_JR },
  { IQ2000_INSN_LB, IQ2000BF_INSN_LB, IQ2000BF_SFMT_LB },
  { IQ2000_INSN_LBU, IQ2000BF_INSN_LBU, IQ2000BF_SFMT_LB },
  { IQ2000_INSN_LH, IQ2000BF_INSN_LH, IQ2000BF_SFMT_LH },
  { IQ2000_INSN_LHU, IQ2000BF_INSN_LHU, IQ2000BF_SFMT_LH },
  { IQ2000_INSN_LUI, IQ2000BF_INSN_LUI, IQ2000BF_SFMT_LUI },
  { IQ2000_INSN_LW, IQ2000BF_INSN_LW, IQ2000BF_SFMT_LW },
  { IQ2000_INSN_SB, IQ2000BF_INSN_SB, IQ2000BF_SFMT_SB },
  { IQ2000_INSN_SH, IQ2000BF_INSN_SH, IQ2000BF_SFMT_SH },
  { IQ2000_INSN_SW, IQ2000BF_INSN_SW, IQ2000BF_SFMT_SW },
  { IQ2000_INSN_BREAK, IQ2000BF_INSN_BREAK, IQ2000BF_SFMT_BREAK },
  { IQ2000_INSN_SYSCALL, IQ2000BF_INSN_SYSCALL, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_ANDOUI, IQ2000BF_INSN_ANDOUI, IQ2000BF_SFMT_ANDOUI },
  { IQ2000_INSN_ORUI, IQ2000BF_INSN_ORUI, IQ2000BF_SFMT_ANDOUI },
  { IQ2000_INSN_BGTZ, IQ2000BF_INSN_BGTZ, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_BGTZL, IQ2000BF_INSN_BGTZL, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_BLEZ, IQ2000BF_INSN_BLEZ, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_BLEZL, IQ2000BF_INSN_BLEZL, IQ2000BF_SFMT_BGEZ },
  { IQ2000_INSN_MRGB, IQ2000BF_INSN_MRGB, IQ2000BF_SFMT_MRGB },
  { IQ2000_INSN_BCTXT, IQ2000BF_INSN_BCTXT, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC0F, IQ2000BF_INSN_BC0F, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC0FL, IQ2000BF_INSN_BC0FL, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC3F, IQ2000BF_INSN_BC3F, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC3FL, IQ2000BF_INSN_BC3FL, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC0T, IQ2000BF_INSN_BC0T, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC0TL, IQ2000BF_INSN_BC0TL, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC3T, IQ2000BF_INSN_BC3T, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_BC3TL, IQ2000BF_INSN_BC3TL, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_CFC0, IQ2000BF_INSN_CFC0, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CFC1, IQ2000BF_INSN_CFC1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CFC2, IQ2000BF_INSN_CFC2, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CFC3, IQ2000BF_INSN_CFC3, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CHKHDR, IQ2000BF_INSN_CHKHDR, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CTC0, IQ2000BF_INSN_CTC0, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CTC1, IQ2000BF_INSN_CTC1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CTC2, IQ2000BF_INSN_CTC2, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_CTC3, IQ2000BF_INSN_CTC3, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_JCR, IQ2000BF_INSN_JCR, IQ2000BF_SFMT_BCTXT },
  { IQ2000_INSN_LUC32, IQ2000BF_INSN_LUC32, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUC32L, IQ2000BF_INSN_LUC32L, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUC64, IQ2000BF_INSN_LUC64, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUC64L, IQ2000BF_INSN_LUC64L, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUK, IQ2000BF_INSN_LUK, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LULCK, IQ2000BF_INSN_LULCK, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUM32, IQ2000BF_INSN_LUM32, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUM32L, IQ2000BF_INSN_LUM32L, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUM64, IQ2000BF_INSN_LUM64, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUM64L, IQ2000BF_INSN_LUM64L, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUR, IQ2000BF_INSN_LUR, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LURL, IQ2000BF_INSN_LURL, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LUULCK, IQ2000BF_INSN_LUULCK, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MFC0, IQ2000BF_INSN_MFC0, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MFC1, IQ2000BF_INSN_MFC1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MFC2, IQ2000BF_INSN_MFC2, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MFC3, IQ2000BF_INSN_MFC3, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MTC0, IQ2000BF_INSN_MTC0, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MTC1, IQ2000BF_INSN_MTC1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MTC2, IQ2000BF_INSN_MTC2, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_MTC3, IQ2000BF_INSN_MTC3, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_PKRL, IQ2000BF_INSN_PKRL, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_PKRLR1, IQ2000BF_INSN_PKRLR1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_PKRLR30, IQ2000BF_INSN_PKRLR30, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_RB, IQ2000BF_INSN_RB, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_RBR1, IQ2000BF_INSN_RBR1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_RBR30, IQ2000BF_INSN_RBR30, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_RFE, IQ2000BF_INSN_RFE, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_RX, IQ2000BF_INSN_RX, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_RXR1, IQ2000BF_INSN_RXR1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_RXR30, IQ2000BF_INSN_RXR30, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_SLEEP, IQ2000BF_INSN_SLEEP, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_SRRD, IQ2000BF_INSN_SRRD, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_SRRDL, IQ2000BF_INSN_SRRDL, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_SRULCK, IQ2000BF_INSN_SRULCK, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_SRWR, IQ2000BF_INSN_SRWR, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_SRWRU, IQ2000BF_INSN_SRWRU, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_TRAPQFL, IQ2000BF_INSN_TRAPQFL, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_TRAPQNE, IQ2000BF_INSN_TRAPQNE, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_TRAPREL, IQ2000BF_INSN_TRAPREL, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WB, IQ2000BF_INSN_WB, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WBU, IQ2000BF_INSN_WBU, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WBR1, IQ2000BF_INSN_WBR1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WBR1U, IQ2000BF_INSN_WBR1U, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WBR30, IQ2000BF_INSN_WBR30, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WBR30U, IQ2000BF_INSN_WBR30U, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WX, IQ2000BF_INSN_WX, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WXU, IQ2000BF_INSN_WXU, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WXR1, IQ2000BF_INSN_WXR1, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WXR1U, IQ2000BF_INSN_WXR1U, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WXR30, IQ2000BF_INSN_WXR30, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_WXR30U, IQ2000BF_INSN_WXR30U, IQ2000BF_SFMT_SYSCALL },
  { IQ2000_INSN_LDW, IQ2000BF_INSN_LDW, IQ2000BF_SFMT_LDW },
  { IQ2000_INSN_SDW, IQ2000BF_INSN_SDW, IQ2000BF_SFMT_SDW },
  { IQ2000_INSN_J, IQ2000BF_INSN_J, IQ2000BF_SFMT_J },
  { IQ2000_INSN_JAL, IQ2000BF_INSN_JAL, IQ2000BF_SFMT_JAL },
  { IQ2000_INSN_BMB, IQ2000BF_INSN_BMB, IQ2000BF_SFMT_BBV },
};

static const struct insn_sem iq2000bf_insn_sem_invalid = {
  VIRTUAL_INSN_X_INVALID, IQ2000BF_INSN_X_INVALID, IQ2000BF_SFMT_EMPTY
};

/* Initialize an IDESC from the compile-time computable parts.  */

static INLINE void
init_idesc (SIM_CPU *cpu, IDESC *id, const struct insn_sem *t)
{
  const CGEN_INSN *insn_table = CGEN_CPU_INSN_TABLE (CPU_CPU_DESC (cpu))->init_entries;

  id->num = t->index;
  id->sfmt = t->sfmt;
  if ((int) t->type <= 0)
    id->idata = & cgen_virtual_insn_table[- (int) t->type];
  else
    id->idata = & insn_table[t->type];
  id->attrs = CGEN_INSN_ATTRS (id->idata);
  /* Oh my god, a magic number.  */
  id->length = CGEN_INSN_BITSIZE (id->idata) / 8;

#if WITH_PROFILE_MODEL_P
  id->timing = & MODEL_TIMING (CPU_MODEL (cpu)) [t->index];
  {
    SIM_DESC sd = CPU_STATE (cpu);
    SIM_ASSERT (t->index == id->timing->num);
  }
#endif

  /* Semantic pointers are initialized elsewhere.  */
}

/* Initialize the instruction descriptor table.  */

void
iq2000bf_init_idesc_table (SIM_CPU *cpu)
{
  IDESC *id,*tabend;
  const struct insn_sem *t,*tend;
  int tabsize = sizeof (iq2000bf_insn_data) / sizeof (IDESC);
  IDESC *table = iq2000bf_insn_data;

  memset (table, 0, tabsize * sizeof (IDESC));

  /* First set all entries to the `invalid insn'.  */
  t = & iq2000bf_insn_sem_invalid;
  for (id = table, tabend = table + tabsize; id < tabend; ++id)
    init_idesc (cpu, id, t);

  /* Now fill in the values for the chosen cpu.  */
  for (t = iq2000bf_insn_sem, tend = t + sizeof (iq2000bf_insn_sem) / sizeof (*t);
       t != tend; ++t)
    {
      init_idesc (cpu, & table[t->index], t);
    }

  /* Link the IDESC table into the cpu.  */
  CPU_IDESC (cpu) = table;
}

/* Given an instruction, return a pointer to its IDESC entry.  */

const IDESC *
iq2000bf_decode (SIM_CPU *current_cpu, IADDR pc,
              CGEN_INSN_INT base_insn, CGEN_INSN_INT entire_insn,
              ARGBUF *abuf)
{
  /* Result of decoder.  */
  IQ2000BF_INSN_TYPE itype;

  {
    CGEN_INSN_INT insn = base_insn;

    {
      unsigned int val = (((insn >> 26) & (63 << 0)));
      switch (val)
      {
      case 0 :
        {
          unsigned int val = (((insn >> 1) & (1 << 4)) | ((insn >> 0) & (15 << 0)));
          switch (val)
          {
          case 0 : itype = IQ2000BF_INSN_SLL;goto extract_sfmt_sll;
          case 1 : itype = IQ2000BF_INSN_SLMV;goto extract_sfmt_slmv;
          case 2 : itype = IQ2000BF_INSN_SRL;goto extract_sfmt_sll;
          case 3 : itype = IQ2000BF_INSN_SRA;goto extract_sfmt_sll;
          case 4 : itype = IQ2000BF_INSN_SLLV;goto extract_sfmt_add;
          case 5 : itype = IQ2000BF_INSN_SRMV;goto extract_sfmt_slmv;
          case 6 : itype = IQ2000BF_INSN_SRLV;goto extract_sfmt_add;
          case 7 : itype = IQ2000BF_INSN_SRAV;goto extract_sfmt_add;
          case 8 : itype = IQ2000BF_INSN_JR;goto extract_sfmt_jr;
          case 9 : itype = IQ2000BF_INSN_JALR;goto extract_sfmt_jalr;
          case 10 : itype = IQ2000BF_INSN_JCR;goto extract_sfmt_bctxt;
          case 12 : itype = IQ2000BF_INSN_SYSCALL;goto extract_sfmt_syscall;
          case 13 : itype = IQ2000BF_INSN_BREAK;goto extract_sfmt_break;
          case 14 : itype = IQ2000BF_INSN_SLEEP;goto extract_sfmt_syscall;
          case 16 : itype = IQ2000BF_INSN_ADD;goto extract_sfmt_add;
          case 17 : itype = IQ2000BF_INSN_ADDU;goto extract_sfmt_add;
          case 18 : itype = IQ2000BF_INSN_SUB;goto extract_sfmt_add;
          case 19 : itype = IQ2000BF_INSN_SUBU;goto extract_sfmt_add;
          case 20 : itype = IQ2000BF_INSN_AND;goto extract_sfmt_add;
          case 21 : itype = IQ2000BF_INSN_OR;goto extract_sfmt_add;
          case 22 : itype = IQ2000BF_INSN_XOR;goto extract_sfmt_add;
          case 23 : itype = IQ2000BF_INSN_NOR;goto extract_sfmt_add;
          case 25 : itype = IQ2000BF_INSN_ADO16;goto extract_sfmt_ado16;
          case 26 : itype = IQ2000BF_INSN_SLT;goto extract_sfmt_slt;
          case 27 : itype = IQ2000BF_INSN_SLTU;goto extract_sfmt_slt;
          case 29 : itype = IQ2000BF_INSN_MRGB;goto extract_sfmt_mrgb;
          default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 1 :
        {
          unsigned int val = (((insn >> 17) & (1 << 3)) | ((insn >> 16) & (7 << 0)));
          switch (val)
          {
          case 0 : itype = IQ2000BF_INSN_BLTZ;goto extract_sfmt_bgez;
          case 1 : itype = IQ2000BF_INSN_BGEZ;goto extract_sfmt_bgez;
          case 2 : itype = IQ2000BF_INSN_BLTZL;goto extract_sfmt_bgez;
          case 3 : itype = IQ2000BF_INSN_BGEZL;goto extract_sfmt_bgez;
          case 6 : itype = IQ2000BF_INSN_BCTXT;goto extract_sfmt_bctxt;
          case 8 : itype = IQ2000BF_INSN_BLTZAL;goto extract_sfmt_bgezal;
          case 9 : itype = IQ2000BF_INSN_BGEZAL;goto extract_sfmt_bgezal;
          case 10 : itype = IQ2000BF_INSN_BLTZALL;goto extract_sfmt_bgezal;
          case 11 : itype = IQ2000BF_INSN_BGEZALL;goto extract_sfmt_bgezal;
          default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 2 : itype = IQ2000BF_INSN_J;goto extract_sfmt_j;
      case 3 : itype = IQ2000BF_INSN_JAL;goto extract_sfmt_jal;
      case 4 : itype = IQ2000BF_INSN_BEQ;goto extract_sfmt_bbv;
      case 5 : itype = IQ2000BF_INSN_BNE;goto extract_sfmt_bbv;
      case 6 : itype = IQ2000BF_INSN_BLEZ;goto extract_sfmt_bgez;
      case 7 : itype = IQ2000BF_INSN_BGTZ;goto extract_sfmt_bgez;
      case 8 : itype = IQ2000BF_INSN_ADDI;goto extract_sfmt_addi;
      case 9 : itype = IQ2000BF_INSN_ADDIU;goto extract_sfmt_addi;
      case 10 : itype = IQ2000BF_INSN_SLTI;goto extract_sfmt_slti;
      case 11 : itype = IQ2000BF_INSN_SLTIU;goto extract_sfmt_slti;
      case 12 : itype = IQ2000BF_INSN_ANDI;goto extract_sfmt_addi;
      case 13 : itype = IQ2000BF_INSN_ORI;goto extract_sfmt_addi;
      case 14 : itype = IQ2000BF_INSN_XORI;goto extract_sfmt_addi;
      case 15 : itype = IQ2000BF_INSN_LUI;goto extract_sfmt_lui;
      case 16 :
        {
          unsigned int val = (((insn >> 19) & (15 << 3)) | ((insn >> 15) & (3 << 1)) | ((insn >> 4) & (1 << 0)));
          switch (val)
          {
          case 0 : /* fall through */
          case 2 : /* fall through */
          case 4 : /* fall through */
          case 6 : itype = IQ2000BF_INSN_MFC0;goto extract_sfmt_syscall;
          case 8 : /* fall through */
          case 10 : /* fall through */
          case 12 : /* fall through */
          case 14 : itype = IQ2000BF_INSN_CFC0;goto extract_sfmt_syscall;
          case 16 : /* fall through */
          case 18 : /* fall through */
          case 20 : /* fall through */
          case 22 : itype = IQ2000BF_INSN_MTC0;goto extract_sfmt_syscall;
          case 24 : /* fall through */
          case 26 : /* fall through */
          case 28 : /* fall through */
          case 30 : itype = IQ2000BF_INSN_CTC0;goto extract_sfmt_syscall;
          case 32 : /* fall through */
          case 33 : itype = IQ2000BF_INSN_BC0F;goto extract_sfmt_bctxt;
          case 34 : /* fall through */
          case 35 : itype = IQ2000BF_INSN_BC0T;goto extract_sfmt_bctxt;
          case 36 : /* fall through */
          case 37 : itype = IQ2000BF_INSN_BC0FL;goto extract_sfmt_bctxt;
          case 38 : /* fall through */
          case 39 : itype = IQ2000BF_INSN_BC0TL;goto extract_sfmt_bctxt;
          case 65 : itype = IQ2000BF_INSN_RFE;goto extract_sfmt_syscall;
          default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 17 :
        {
          unsigned int val = (((insn >> 22) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = IQ2000BF_INSN_MFC1;goto extract_sfmt_syscall;
          case 1 : itype = IQ2000BF_INSN_CFC1;goto extract_sfmt_syscall;
          case 2 : itype = IQ2000BF_INSN_MTC1;goto extract_sfmt_syscall;
          case 3 : itype = IQ2000BF_INSN_CTC1;goto extract_sfmt_syscall;
          default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 18 :
        {
          unsigned int val = (((insn >> 16) & (3 << 5)) | ((insn >> 0) & (31 << 0)));
          switch (val)
          {
          case 0 :
            {
              unsigned int val = (((insn >> 23) & (1 << 0)));
              switch (val)
              {
              case 0 : itype = IQ2000BF_INSN_MFC2;goto extract_sfmt_syscall;
              case 1 : itype = IQ2000BF_INSN_MTC2;goto extract_sfmt_syscall;
              default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 32 : itype = IQ2000BF_INSN_LUULCK;goto extract_sfmt_syscall;
          case 33 : itype = IQ2000BF_INSN_LUR;goto extract_sfmt_syscall;
          case 34 : itype = IQ2000BF_INSN_LUM32;goto extract_sfmt_syscall;
          case 35 : itype = IQ2000BF_INSN_LUC32;goto extract_sfmt_syscall;
          case 36 : itype = IQ2000BF_INSN_LULCK;goto extract_sfmt_syscall;
          case 37 : itype = IQ2000BF_INSN_LURL;goto extract_sfmt_syscall;
          case 38 : itype = IQ2000BF_INSN_LUM32L;goto extract_sfmt_syscall;
          case 39 : itype = IQ2000BF_INSN_LUC32L;goto extract_sfmt_syscall;
          case 40 : itype = IQ2000BF_INSN_LUK;goto extract_sfmt_syscall;
          case 42 : itype = IQ2000BF_INSN_LUM64;goto extract_sfmt_syscall;
          case 43 : itype = IQ2000BF_INSN_LUC64;goto extract_sfmt_syscall;
          case 46 : itype = IQ2000BF_INSN_LUM64L;goto extract_sfmt_syscall;
          case 47 : itype = IQ2000BF_INSN_LUC64L;goto extract_sfmt_syscall;
          case 48 : itype = IQ2000BF_INSN_SRRD;goto extract_sfmt_syscall;
          case 49 : itype = IQ2000BF_INSN_SRWR;goto extract_sfmt_syscall;
          case 52 : itype = IQ2000BF_INSN_SRRDL;goto extract_sfmt_syscall;
          case 53 : itype = IQ2000BF_INSN_SRWRU;goto extract_sfmt_syscall;
          case 54 : itype = IQ2000BF_INSN_SRULCK;goto extract_sfmt_syscall;
          case 64 :
            {
              unsigned int val = (((insn >> 23) & (1 << 0)));
              switch (val)
              {
              case 0 : itype = IQ2000BF_INSN_CFC2;goto extract_sfmt_syscall;
              case 1 : itype = IQ2000BF_INSN_CTC2;goto extract_sfmt_syscall;
              default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 19 :
        {
          unsigned int val = (((insn >> 19) & (31 << 2)) | ((insn >> 0) & (3 << 0)));
          switch (val)
          {
          case 0 : itype = IQ2000BF_INSN_MFC3;goto extract_sfmt_syscall;
          case 4 :
            {
              unsigned int val = (((insn >> 2) & (3 << 0)));
              switch (val)
              {
              case 0 : itype = IQ2000BF_INSN_WB;goto extract_sfmt_syscall;
              case 1 : itype = IQ2000BF_INSN_RB;goto extract_sfmt_syscall;
              case 2 : itype = IQ2000BF_INSN_TRAPQFL;goto extract_sfmt_syscall;
              default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 5 :
            {
              unsigned int val = (((insn >> 3) & (1 << 0)));
              switch (val)
              {
              case 0 : itype = IQ2000BF_INSN_WBU;goto extract_sfmt_syscall;
              case 1 : itype = IQ2000BF_INSN_TRAPQNE;goto extract_sfmt_syscall;
              default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 6 :
            {
              unsigned int val = (((insn >> 2) & (3 << 0)));
              switch (val)
              {
              case 0 : itype = IQ2000BF_INSN_WX;goto extract_sfmt_syscall;
              case 1 : itype = IQ2000BF_INSN_RX;goto extract_sfmt_syscall;
              case 2 : itype = IQ2000BF_INSN_TRAPREL;goto extract_sfmt_syscall;
              default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 7 :
            {
              unsigned int val = (((insn >> 2) & (1 << 0)));
              switch (val)
              {
              case 0 : itype = IQ2000BF_INSN_WXU;goto extract_sfmt_syscall;
              case 1 : itype = IQ2000BF_INSN_PKRL;goto extract_sfmt_syscall;
              default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 8 : itype = IQ2000BF_INSN_CFC3;goto extract_sfmt_syscall;
          case 16 : itype = IQ2000BF_INSN_MTC3;goto extract_sfmt_syscall;
          case 24 : itype = IQ2000BF_INSN_CTC3;goto extract_sfmt_syscall;
          case 32 : /* fall through */
          case 33 : /* fall through */
          case 34 : /* fall through */
          case 35 :
            {
              unsigned int val = (((insn >> 16) & (3 << 0)));
              switch (val)
              {
              case 0 : itype = IQ2000BF_INSN_BC3F;goto extract_sfmt_bctxt;
              case 1 : itype = IQ2000BF_INSN_BC3T;goto extract_sfmt_bctxt;
              case 2 : itype = IQ2000BF_INSN_BC3FL;goto extract_sfmt_bctxt;
              case 3 : itype = IQ2000BF_INSN_BC3TL;goto extract_sfmt_bctxt;
              default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
              }
            }
          case 36 : itype = IQ2000BF_INSN_CHKHDR;goto extract_sfmt_syscall;
          case 64 : /* fall through */
          case 65 : /* fall through */
          case 66 : /* fall through */
          case 67 : itype = IQ2000BF_INSN_WBR1;goto extract_sfmt_syscall;
          case 68 : /* fall through */
          case 69 : /* fall through */
          case 70 : /* fall through */
          case 71 : itype = IQ2000BF_INSN_WBR1U;goto extract_sfmt_syscall;
          case 72 : /* fall through */
          case 73 : /* fall through */
          case 74 : /* fall through */
          case 75 : itype = IQ2000BF_INSN_WBR30;goto extract_sfmt_syscall;
          case 76 : /* fall through */
          case 77 : /* fall through */
          case 78 : /* fall through */
          case 79 : itype = IQ2000BF_INSN_WBR30U;goto extract_sfmt_syscall;
          case 80 : /* fall through */
          case 81 : /* fall through */
          case 82 : /* fall through */
          case 83 : itype = IQ2000BF_INSN_WXR1;goto extract_sfmt_syscall;
          case 84 : /* fall through */
          case 85 : /* fall through */
          case 86 : /* fall through */
          case 87 : itype = IQ2000BF_INSN_WXR1U;goto extract_sfmt_syscall;
          case 88 : /* fall through */
          case 89 : /* fall through */
          case 90 : /* fall through */
          case 91 : itype = IQ2000BF_INSN_WXR30;goto extract_sfmt_syscall;
          case 92 : /* fall through */
          case 93 : /* fall through */
          case 94 : /* fall through */
          case 95 : itype = IQ2000BF_INSN_WXR30U;goto extract_sfmt_syscall;
          case 96 : /* fall through */
          case 97 : /* fall through */
          case 98 : /* fall through */
          case 99 : itype = IQ2000BF_INSN_RBR1;goto extract_sfmt_syscall;
          case 104 : /* fall through */
          case 105 : /* fall through */
          case 106 : /* fall through */
          case 107 : itype = IQ2000BF_INSN_RBR30;goto extract_sfmt_syscall;
          case 112 : /* fall through */
          case 113 : /* fall through */
          case 114 : /* fall through */
          case 115 : itype = IQ2000BF_INSN_RXR1;goto extract_sfmt_syscall;
          case 116 : /* fall through */
          case 117 : /* fall through */
          case 118 : /* fall through */
          case 119 : itype = IQ2000BF_INSN_PKRLR1;goto extract_sfmt_syscall;
          case 120 : /* fall through */
          case 121 : /* fall through */
          case 122 : /* fall through */
          case 123 : itype = IQ2000BF_INSN_RXR30;goto extract_sfmt_syscall;
          case 124 : /* fall through */
          case 125 : /* fall through */
          case 126 : /* fall through */
          case 127 : itype = IQ2000BF_INSN_PKRLR30;goto extract_sfmt_syscall;
          default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
          }
        }
      case 20 : itype = IQ2000BF_INSN_BEQL;goto extract_sfmt_bbv;
      case 21 : itype = IQ2000BF_INSN_BNEL;goto extract_sfmt_bbv;
      case 22 : itype = IQ2000BF_INSN_BLEZL;goto extract_sfmt_bgez;
      case 23 : itype = IQ2000BF_INSN_BGTZL;goto extract_sfmt_bgez;
      case 24 : itype = IQ2000BF_INSN_BMB0;goto extract_sfmt_bbv;
      case 25 : itype = IQ2000BF_INSN_BMB1;goto extract_sfmt_bbv;
      case 26 : itype = IQ2000BF_INSN_BMB2;goto extract_sfmt_bbv;
      case 27 : itype = IQ2000BF_INSN_BMB3;goto extract_sfmt_bbv;
      case 28 : itype = IQ2000BF_INSN_BBI;goto extract_sfmt_bbi;
      case 29 : itype = IQ2000BF_INSN_BBV;goto extract_sfmt_bbv;
      case 30 : itype = IQ2000BF_INSN_BBIN;goto extract_sfmt_bbi;
      case 31 : itype = IQ2000BF_INSN_BBVN;goto extract_sfmt_bbv;
      case 32 : itype = IQ2000BF_INSN_LB;goto extract_sfmt_lb;
      case 33 : itype = IQ2000BF_INSN_LH;goto extract_sfmt_lh;
      case 35 : itype = IQ2000BF_INSN_LW;goto extract_sfmt_lw;
      case 36 : itype = IQ2000BF_INSN_LBU;goto extract_sfmt_lb;
      case 37 : itype = IQ2000BF_INSN_LHU;goto extract_sfmt_lh;
      case 39 : itype = IQ2000BF_INSN_RAM;goto extract_sfmt_ram;
      case 40 : itype = IQ2000BF_INSN_SB;goto extract_sfmt_sb;
      case 41 : itype = IQ2000BF_INSN_SH;goto extract_sfmt_sh;
      case 43 : itype = IQ2000BF_INSN_SW;goto extract_sfmt_sw;
      case 44 : itype = IQ2000BF_INSN_ANDOI;goto extract_sfmt_addi;
      case 45 : itype = IQ2000BF_INSN_BMB;goto extract_sfmt_bbv;
      case 47 : itype = IQ2000BF_INSN_ORUI;goto extract_sfmt_andoui;
      case 48 : itype = IQ2000BF_INSN_LDW;goto extract_sfmt_ldw;
      case 56 : itype = IQ2000BF_INSN_SDW;goto extract_sfmt_sdw;
      case 63 : itype = IQ2000BF_INSN_ANDOUI;goto extract_sfmt_andoui;
      default : itype = IQ2000BF_INSN_X_INVALID; goto extract_sfmt_empty;
      }
    }
  }

  /* The instruction has been decoded, now extract the fields.  */

 extract_sfmt_empty:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_empty", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_add:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_mrgb.f
    UINT f_rs;
    UINT f_rt;
    UINT f_rd;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (f_rd) = f_rd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_add", "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "f_rd 0x%x", 'x', f_rd, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_addi:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm) = f_imm;
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_addi", "f_imm 0x%x", 'x', f_imm, "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_ado16:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_mrgb.f
    UINT f_rs;
    UINT f_rt;
    UINT f_rd;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (f_rd) = f_rd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ado16", "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "f_rd 0x%x", 'x', f_rd, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_ram:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ram.f
    UINT f_rs;
    UINT f_rt;
    UINT f_rd;
    UINT f_shamt;
    UINT f_maskl;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);
    f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5);
    f_maskl = EXTRACT_LSB0_UINT (insn, 32, 4, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_maskl) = f_maskl;
  FLD (f_rs) = f_rs;
  FLD (f_rd) = f_rd;
  FLD (f_rt) = f_rt;
  FLD (f_shamt) = f_shamt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ram", "f_maskl 0x%x", 'x', f_maskl, "f_rs 0x%x", 'x', f_rs, "f_rd 0x%x", 'x', f_rd, "f_rt 0x%x", 'x', f_rt, "f_shamt 0x%x", 'x', f_shamt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_sll:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ram.f
    UINT f_rt;
    UINT f_rd;
    UINT f_shamt;

    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);
    f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_rt) = f_rt;
  FLD (f_shamt) = f_shamt;
  FLD (f_rd) = f_rd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sll", "f_rt 0x%x", 'x', f_rt, "f_shamt 0x%x", 'x', f_shamt, "f_rd 0x%x", 'x', f_rd, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_slmv:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_ram.f
    UINT f_rs;
    UINT f_rt;
    UINT f_rd;
    UINT f_shamt;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);
    f_shamt = EXTRACT_LSB0_UINT (insn, 32, 10, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (f_shamt) = f_shamt;
  FLD (f_rd) = f_rd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_slmv", "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "f_shamt 0x%x", 'x', f_shamt, "f_rd 0x%x", 'x', f_rd, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_slt:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_mrgb.f
    UINT f_rs;
    UINT f_rt;
    UINT f_rd;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (f_rd) = f_rd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_slt", "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "f_rd 0x%x", 'x', f_rd, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_slti:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm) = f_imm;
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_slti", "f_imm 0x%x", 'x', f_imm, "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_bbi:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bbi.f
    UINT f_rs;
    UINT f_rt;
    SI f_offset;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (f_rt) = f_rt;
  FLD (f_rs) = f_rs;
  FLD (i_offset) = f_offset;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bbi", "f_rt 0x%x", 'x', f_rt, "f_rs 0x%x", 'x', f_rs, "offset 0x%x", 'x', f_offset, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bbv:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bbi.f
    UINT f_rs;
    UINT f_rt;
    SI f_offset;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (i_offset) = f_offset;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bbv", "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "offset 0x%x", 'x', f_offset, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bgez:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bbi.f
    UINT f_rs;
    SI f_offset;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (i_offset) = f_offset;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bgez", "f_rs 0x%x", 'x', f_rs, "offset 0x%x", 'x', f_offset, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_bgezal:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bbi.f
    UINT f_rs;
    SI f_offset;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_offset = ((((EXTRACT_LSB0_INT (insn, 32, 15, 16)) << (2))) + (((pc) + (4))));

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (i_offset) = f_offset;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bgezal", "f_rs 0x%x", 'x', f_rs, "offset 0x%x", 'x', f_offset, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jalr:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_mrgb.f
    UINT f_rs;
    UINT f_rd;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rd) = f_rd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jalr", "f_rs 0x%x", 'x', f_rs, "f_rd 0x%x", 'x', f_rd, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jr:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_bbi.f
    UINT f_rs;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jr", "f_rs 0x%x", 'x', f_rs, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_lb:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_imm) = f_imm;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lb", "f_rs 0x%x", 'x', f_rs, "f_imm 0x%x", 'x', f_imm, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_lh:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_imm) = f_imm;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lh", "f_rs 0x%x", 'x', f_rs, "f_imm 0x%x", 'x', f_imm, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_lui:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rt;
    UINT f_imm;

    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm) = f_imm;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lui", "f_imm 0x%x", 'x', f_imm, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_lw:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_imm) = f_imm;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_lw", "f_rs 0x%x", 'x', f_rs, "f_imm 0x%x", 'x', f_imm, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_sb:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_imm) = f_imm;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sb", "f_rs 0x%x", 'x', f_rs, "f_imm 0x%x", 'x', f_imm, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_sh:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_imm) = f_imm;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sh", "f_rs 0x%x", 'x', f_rs, "f_imm 0x%x", 'x', f_imm, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_sw:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_imm) = f_imm;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sw", "f_rs 0x%x", 'x', f_rs, "f_imm 0x%x", 'x', f_imm, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_break:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_break", (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_syscall:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_syscall", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_andoui:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_imm) = f_imm;
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_andoui", "f_imm 0x%x", 'x', f_imm, "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_mrgb:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_mrgb.f
    UINT f_rs;
    UINT f_rt;
    UINT f_rd;
    UINT f_mask;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_rd = EXTRACT_LSB0_UINT (insn, 32, 15, 5);
    f_mask = EXTRACT_LSB0_UINT (insn, 32, 9, 4);

  /* Record the fields for the semantic handler.  */
  FLD (f_mask) = f_mask;
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (f_rd) = f_rd;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_mrgb", "f_mask 0x%x", 'x', f_mask, "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "f_rd 0x%x", 'x', f_rd, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_bctxt:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
#define FLD(f) abuf->fields.fmt_empty.f


  /* Record the fields for the semantic handler.  */
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_bctxt", (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_ldw:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (f_imm) = f_imm;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_ldw", "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "f_imm 0x%x", 'x', f_imm, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_sdw:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_addi.f
    UINT f_rs;
    UINT f_rt;
    UINT f_imm;

    f_rs = EXTRACT_LSB0_UINT (insn, 32, 25, 5);
    f_rt = EXTRACT_LSB0_UINT (insn, 32, 20, 5);
    f_imm = EXTRACT_LSB0_UINT (insn, 32, 15, 16);

  /* Record the fields for the semantic handler.  */
  FLD (f_rs) = f_rs;
  FLD (f_rt) = f_rt;
  FLD (f_imm) = f_imm;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_sdw", "f_rs 0x%x", 'x', f_rs, "f_rt 0x%x", 'x', f_rt, "f_imm 0x%x", 'x', f_imm, (char *) 0));

#undef FLD
    return idesc;
  }

 extract_sfmt_j:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_j.f
    USI f_jtarg;

    f_jtarg = ((((pc) & (0xf0000000))) | (((EXTRACT_LSB0_UINT (insn, 32, 15, 16)) << (2))));

  /* Record the fields for the semantic handler.  */
  FLD (i_jmptarg) = f_jtarg;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_j", "jmptarg 0x%x", 'x', f_jtarg, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

 extract_sfmt_jal:
  {
    const IDESC *idesc = &iq2000bf_insn_data[itype];
    CGEN_INSN_INT insn = entire_insn;
#define FLD(f) abuf->fields.sfmt_j.f
    USI f_jtarg;

    f_jtarg = ((((pc) & (0xf0000000))) | (((EXTRACT_LSB0_UINT (insn, 32, 15, 16)) << (2))));

  /* Record the fields for the semantic handler.  */
  FLD (i_jmptarg) = f_jtarg;
  TRACE_EXTRACT (current_cpu, abuf, (current_cpu, pc, "sfmt_jal", "jmptarg 0x%x", 'x', f_jtarg, (char *) 0));

#if WITH_PROFILE_MODEL_P
  /* Record the fields for profiling.  */
  if (PROFILE_MODEL_P (current_cpu))
    {
    }
#endif
#undef FLD
    return idesc;
  }

}
