/* Target-dependent code for the S+core architecture, for GDB,
   the GNU Debugger.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   Contributed by Qinwei (qinwei@sunnorth.com.cn)
   Contributed by Ching-Peng Lin (cplin@sunplus.com)

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

#ifndef SCORE_TDEP_H
#define SCORE_TDEP_H
#include "math.h"

enum gdb_regnum
{
  SCORE_SP_REGNUM = 0,
  SCORE_FP_REGNUM = 2,
  SCORE_RA_REGNUM = 3,
  SCORE_A0_REGNUM = 4,
  SCORE_AL_REGNUM = 7,
  SCORE_PC_REGNUM = 49,
};

#define SCORE_A0_REGNUM        4
#define SCORE_A1_REGNUM        5
#define SCORE_REGSIZE          4
#define SCORE_NUM_REGS         56
#define SCORE_BEGIN_ARG_REGNUM 4
#define SCORE_LAST_ARG_REGNUM  7

#define SCORE_INSTLEN          4
#define SCORE16_INSTLEN        2

#endif /* SCORE_TDEP_H */
