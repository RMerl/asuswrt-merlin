/* ISA definitions header for media.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

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

#ifndef DEFS_SH64_MEDIA_H
#define DEFS_SH64_MEDIA_H

/* Instruction argument buffer.  */

union sem_fields {
  struct { /* no operands */
    int empty;
  } fmt_empty;
  struct { /*  */
    UINT f_dest;
    UINT f_uimm16;
  } sfmt_shori;
  struct { /*  */
    DI f_disp16;
    UINT f_tra;
  } sfmt_pta;
  struct { /*  */
    INT f_imm16;
    UINT f_dest;
  } sfmt_movi;
  struct { /*  */
    UINT f_dest;
    UINT f_left_right;
  } sfmt_fabsd;
  struct { /*  */
    UINT f_dest;
    UINT f_trb;
  } sfmt_blink;
  struct { /*  */
    INT f_imm6;
    UINT f_dest;
    UINT f_left;
  } sfmt_xori;
  struct { /*  */
    UINT f_dest;
    UINT f_left;
    UINT f_uimm6;
  } sfmt_shari;
  struct { /*  */
    INT f_imm10;
    UINT f_dest;
    UINT f_left;
  } sfmt_ori;
  struct { /*  */
    SI f_disp10x2;
    UINT f_dest;
    UINT f_left;
  } sfmt_lduw;
  struct { /*  */
    INT f_disp6;
    UINT f_dest;
    UINT f_left;
  } sfmt_getcfg;
  struct { /*  */
    SI f_disp10x4;
    UINT f_dest;
    UINT f_left;
  } sfmt_flds;
  struct { /*  */
    SI f_disp10x8;
    UINT f_dest;
    UINT f_left;
  } sfmt_fldd;
  struct { /*  */
    INT f_imm6;
    UINT f_left;
    UINT f_tra;
  } sfmt_beqi;
  struct { /*  */
    UINT f_left;
    UINT f_right;
    UINT f_tra;
  } sfmt_beq;
  struct { /*  */
    INT f_disp10;
    UINT f_dest;
    UINT f_left;
  } sfmt_addi;
  struct { /*  */
    UINT f_dest;
    UINT f_left;
    UINT f_right;
  } sfmt_add;
#if WITH_SCACHE_PBB
  /* Writeback handler.  */
  struct {
    /* Pointer to argbuf entry for insn whose results need writing back.  */
    const struct argbuf *abuf;
  } write;
  /* x-before handler */
  struct {
    /*const SCACHE *insns[MAX_PARALLEL_INSNS];*/
    int first_p;
  } before;
  /* x-after handler */
  struct {
    int empty;
  } after;
  /* This entry is used to terminate each pbb.  */
  struct {
    /* Number of insns in pbb.  */
    int insn_count;
    /* Next pbb to execute.  */
    SCACHE *next;
    SCACHE *branch_target;
  } chain;
#endif
};

/* The ARGBUF struct.  */
struct argbuf {
  /* These are the baseclass definitions.  */
  IADDR addr;
  const IDESC *idesc;
  char trace_p;
  char profile_p;
  /* ??? Temporary hack for skip insns.  */
  char skip_count;
  char unused;
  /* cpu specific data follows */
  union sem semantic;
  int written;
  union sem_fields fields;
};

/* A cached insn.

   ??? SCACHE used to contain more than just argbuf.  We could delete the
   type entirely and always just use ARGBUF, but for future concerns and as
   a level of abstraction it is left in.  */

struct scache {
  struct argbuf argbuf;
};

/* Macros to simplify extraction, reading and semantic code.
   These define and assign the local vars that contain the insn's fields.  */

#define EXTRACT_IFMT_EMPTY_VARS \
  unsigned int length;
#define EXTRACT_IFMT_EMPTY_CODE \
  length = 0; \

#define EXTRACT_IFMT_ADD_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_ADD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_ADDI_VARS \
  UINT f_op; \
  UINT f_left; \
  INT f_disp10; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_ADDI_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_disp10 = EXTRACT_MSB0_INT (insn, 32, 12, 10); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_ALLOCO_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  SI f_disp6x32; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_ALLOCO_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_disp6x32 = ((EXTRACT_MSB0_INT (insn, 32, 16, 6)) << (5)); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_BEQ_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_likely; \
  UINT f_23_2; \
  UINT f_tra; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_BEQ_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_likely = EXTRACT_MSB0_UINT (insn, 32, 22, 1); \
  f_23_2 = EXTRACT_MSB0_UINT (insn, 32, 23, 2); \
  f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_BEQI_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  INT f_imm6; \
  UINT f_likely; \
  UINT f_23_2; \
  UINT f_tra; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_BEQI_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_imm6 = EXTRACT_MSB0_INT (insn, 32, 16, 6); \
  f_likely = EXTRACT_MSB0_UINT (insn, 32, 22, 1); \
  f_23_2 = EXTRACT_MSB0_UINT (insn, 32, 23, 2); \
  f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_BLINK_VARS \
  UINT f_op; \
  UINT f_6_3; \
  UINT f_trb; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_BLINK_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_6_3 = EXTRACT_MSB0_UINT (insn, 32, 6, 3); \
  f_trb = EXTRACT_MSB0_UINT (insn, 32, 9, 3); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_BRK_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_BRK_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_BYTEREV_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_BYTEREV_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FABSD_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_right; \
  UINT f_left_right; \
  UINT f_ext; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FABSD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_left_right = f_left;\
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FABSS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_right; \
  UINT f_left_right; \
  UINT f_ext; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FABSS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_left_right = f_left;\
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FADDD_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FADDD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FADDS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FADDS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FCMPEQD_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FCMPEQD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FCMPEQS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FCMPEQS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FCNVDS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_right; \
  UINT f_left_right; \
  UINT f_ext; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FCNVDS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_left_right = f_left;\
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FCNVSD_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_right; \
  UINT f_left_right; \
  UINT f_ext; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FCNVSD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_left_right = f_left;\
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FGETSCR_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FGETSCR_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FIPRS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FIPRS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FLDD_VARS \
  UINT f_op; \
  UINT f_left; \
  SI f_disp10x8; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FLDD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3)); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FLDP_VARS \
  UINT f_op; \
  UINT f_left; \
  SI f_disp10x8; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FLDP_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3)); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FLDS_VARS \
  UINT f_op; \
  UINT f_left; \
  SI f_disp10x4; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FLDS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_disp10x4 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (2)); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FLDXD_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FLDXD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FLDXP_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FLDXP_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FMOVDQ_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_right; \
  UINT f_left_right; \
  UINT f_ext; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FMOVDQ_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_left_right = f_left;\
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FMOVLS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FMOVLS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FMOVSL_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_right; \
  UINT f_left_right; \
  UINT f_ext; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FMOVSL_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_left_right = f_left;\
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FPUTSCR_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_right; \
  UINT f_left_right; \
  UINT f_ext; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FPUTSCR_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_left_right = f_left;\
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FSTXD_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FSTXD_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_FTRVS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_FTRVS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_GETCFG_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  INT f_disp6; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_GETCFG_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_disp6 = EXTRACT_MSB0_INT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_GETCON_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_GETCON_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_LDL_VARS \
  UINT f_op; \
  UINT f_left; \
  SI f_disp10x4; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_LDL_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_disp10x4 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (2)); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_LDQ_VARS \
  UINT f_op; \
  UINT f_left; \
  SI f_disp10x8; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_LDQ_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_disp10x8 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (3)); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_MMACNFX_WL_VARS \
  UINT f_op; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_MMACNFX_WL_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_MOVI_VARS \
  UINT f_op; \
  INT f_imm16; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_MOVI_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_imm16 = EXTRACT_MSB0_INT (insn, 32, 6, 16); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_ORI_VARS \
  UINT f_op; \
  UINT f_left; \
  INT f_imm10; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_ORI_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_imm10 = EXTRACT_MSB0_INT (insn, 32, 12, 10); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_PREFI_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  SI f_disp6x32; \
  UINT f_right; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_PREFI_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_disp6x32 = ((EXTRACT_MSB0_INT (insn, 32, 16, 6)) << (5)); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_PTA_VARS \
  UINT f_op; \
  DI f_disp16; \
  UINT f_likely; \
  UINT f_23_2; \
  UINT f_tra; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_PTA_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_disp16 = ((((EXTRACT_MSB0_INT (insn, 32, 6, 16)) << (2))) + (pc)); \
  f_likely = EXTRACT_MSB0_UINT (insn, 32, 22, 1); \
  f_23_2 = EXTRACT_MSB0_UINT (insn, 32, 23, 2); \
  f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_PTABS_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_likely; \
  UINT f_23_2; \
  UINT f_tra; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_PTABS_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_likely = EXTRACT_MSB0_UINT (insn, 32, 22, 1); \
  f_23_2 = EXTRACT_MSB0_UINT (insn, 32, 23, 2); \
  f_tra = EXTRACT_MSB0_UINT (insn, 32, 25, 3); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_PUTCON_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_PUTCON_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_SHARI_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_uimm6; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_SHARI_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_uimm6 = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_SHORI_VARS \
  UINT f_op; \
  UINT f_uimm16; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_SHORI_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_uimm16 = EXTRACT_MSB0_UINT (insn, 32, 6, 16); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_STW_VARS \
  UINT f_op; \
  UINT f_left; \
  SI f_disp10x2; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_STW_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_disp10x2 = ((EXTRACT_MSB0_INT (insn, 32, 12, 10)) << (1)); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#define EXTRACT_IFMT_TRAPA_VARS \
  UINT f_op; \
  UINT f_left; \
  UINT f_ext; \
  UINT f_right; \
  UINT f_dest; \
  UINT f_rsvd; \
  unsigned int length;
#define EXTRACT_IFMT_TRAPA_CODE \
  length = 4; \
  f_op = EXTRACT_MSB0_UINT (insn, 32, 0, 6); \
  f_left = EXTRACT_MSB0_UINT (insn, 32, 6, 6); \
  f_ext = EXTRACT_MSB0_UINT (insn, 32, 12, 4); \
  f_right = EXTRACT_MSB0_UINT (insn, 32, 16, 6); \
  f_dest = EXTRACT_MSB0_UINT (insn, 32, 22, 6); \
  f_rsvd = EXTRACT_MSB0_UINT (insn, 32, 28, 4); \

#endif /* DEFS_SH64_MEDIA_H */
