/* Register definitions for the FRV simulator
   Copyright (C) 2000, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

This file is part of the GNU Simulators.

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

#ifndef REGISTERS_H
#define REGISTERS_H

#define FRV_MAX_GR  64
#define FRV_MAX_FR  64
#define FRV_MAX_CPR 64
#define FRV_MAX_SPR 4096

/* Register init, reset values and read_only masks.  */
typedef struct
{
  USI init_value;     /* initial value */
  USI reset_value;    /* value for software reset */
  USI reset_mask;     /* bits which are reset */
  USI read_only_mask; /* bits which are read-only */
  char implemented;   /* 1==register is implemented */
  char supervisor;    /* 1==register is supervisor-only */
} FRV_SPR_CONTROL_INFO;

typedef struct
{
  int fr;                    /* FR registers implemented */
  int cpr;                   /* coprocessor registers implemented */
  FRV_SPR_CONTROL_INFO *spr; /* SPR implementation details */
} FRV_REGISTER_CONTROL;

void frv_register_control_init (SIM_CPU *);
void frv_initialize_spr (SIM_CPU *);
void frv_reset_spr (SIM_CPU *);

void frv_check_spr_access (SIM_CPU *, UINT);

void frv_fr_registers_available (SIM_CPU *, int *, int *);
void frv_gr_registers_available (SIM_CPU *, int *, int *);
int  frv_check_register_access (SIM_CPU *, SI, int, int);
int  frv_check_gr_access (SIM_CPU *, SI);
int  frv_check_fr_access (SIM_CPU *, SI);

#endif /* REGISTERS_H */
