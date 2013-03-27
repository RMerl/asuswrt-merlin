/* collection of junk waiting time to sort out
   Copyright (C) 2000, 2006 Free Software Foundation, Inc.
   Contributed by Red Hat, Inc.

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

#ifndef SH64_SIM_H
#define SH64_SIM_H

#define GETTWI GETTSI
#define SETTWI SETTSI


enum {
  ISM_COMPACT, ISM_MEDIA
};

/* Hardware/device support.  */
extern device sh5_devices;

/* FIXME: Temporary, until device support ready.  */
struct _device { int foo; };

extern IDESC * sh64_idesc_media;
extern IDESC * sh64_idesc_compact;

/* Function prototypes from sh64.c.  */

BI sh64_endian (SIM_CPU *);
VOID sh64_break (SIM_CPU *, PCADDR);
SI sh64_movua (SIM_CPU *, PCADDR, SI);
VOID sh64_trapa (SIM_CPU *, DI, PCADDR);
VOID sh64_compact_trapa (SIM_CPU *, UQI, PCADDR);

SF sh64_fldi0 (SIM_CPU *);
SF sh64_fldi1 (SIM_CPU *);
DF sh64_fcnvsd (SIM_CPU *, SF);
SF sh64_fcnvds (SIM_CPU *, DF);

DF sh64_fabsd (SIM_CPU *, DF);
SF sh64_fabss (SIM_CPU *, SF);
DF sh64_faddd (SIM_CPU *, DF, DF);
SF sh64_fadds (SIM_CPU *, SF, SF);
DF sh64_fdivd (SIM_CPU *, DF, DF);
SF sh64_fdivs (SIM_CPU *, SF, SF);
DF sh64_floatld (SIM_CPU *, SF);
SF sh64_floatls (SIM_CPU *, SF);
DF sh64_floatqd (SIM_CPU *, DF);
SF sh64_floatqs (SIM_CPU *, DF);
SF sh64_fmacs(SIM_CPU *, SF, SF, SF);
DF sh64_fmuld (SIM_CPU *, DF, DF);
SF sh64_fmuls (SIM_CPU *, SF, SF);
DF sh64_fnegd (SIM_CPU *, DF);
SF sh64_fnegs (SIM_CPU *, SF);
DF sh64_fsqrtd (SIM_CPU *, DF);
SF sh64_fsqrts (SIM_CPU *, SF);
DF sh64_fsubd (SIM_CPU *, DF, DF);
SF sh64_fsubs (SIM_CPU *, SF, SF);
SF sh64_ftrcdl (SIM_CPU *, DF);
DF sh64_ftrcdq (SIM_CPU *, DF);
SF sh64_ftrcsl (SIM_CPU *, SF);
DF sh64_ftrcsq (SIM_CPU *, SF);
VOID sh64_ftrvs (SIM_CPU *, unsigned, unsigned, unsigned);
VOID sh64_fipr (SIM_CPU *cpu, unsigned m, unsigned n);
SF sh64_fiprs (SIM_CPU *cpu, unsigned g, unsigned h);
VOID sh64_fldp (SIM_CPU *cpu, PCADDR pc, DI rm, DI rn, unsigned f);
VOID sh64_fstp (SIM_CPU *cpu, PCADDR pc, DI rm, DI rn, unsigned f);
VOID sh64_ftrv (SIM_CPU *cpu, UINT ignored);
VOID sh64_pref (SIM_CPU *cpu, SI addr);
BI sh64_fcmpeqs (SIM_CPU *, SF, SF);
BI sh64_fcmpeqd (SIM_CPU *, DF, DF);
BI sh64_fcmpges (SIM_CPU *, SF, SF);
BI sh64_fcmpged (SIM_CPU *, DF, DF);
BI sh64_fcmpgts (SIM_CPU *, SF, SF);
BI sh64_fcmpgtd (SIM_CPU *, DF, DF);
BI sh64_fcmpund (SIM_CPU *, DF, DF);
BI sh64_fcmpuns (SIM_CPU *, SF, SF);

DI sh64_nsb (SIM_CPU *, DI);

#endif /* SH64_SIM_H */
