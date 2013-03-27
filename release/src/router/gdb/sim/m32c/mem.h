/* mem.h --- interface to memory for M32C simulator.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

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
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


void init_mem (void);
void mem_usage_stats (void);

void mem_put_qi (int address, unsigned char value);
void mem_put_hi (int address, unsigned short value);
void mem_put_psi (int address, unsigned long value);
void mem_put_si (int address, unsigned long value);

void mem_put_blk (int address, void *bufptr, int nbytes);

unsigned char mem_get_pc ();

unsigned char mem_get_qi (int address);
unsigned short mem_get_hi (int address);
unsigned long mem_get_psi (int address);
unsigned long mem_get_si (int address);

void mem_get_blk (int address, void *bufptr, int nbytes);

int sign_ext (int v, int bits);
