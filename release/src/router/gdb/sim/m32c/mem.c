/* mem.c --- memory for M32C simulator.

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "cpu.h"
#include "syscalls.h"
#include "misc.h"

#define L1_BITS  (10)
#define L2_BITS  (10)
#define OFF_BITS (12)

#define L1_LEN  (1 << L1_BITS)
#define L2_LEN  (1 << L2_BITS)
#define OFF_LEN (1 << OFF_BITS)

static unsigned char **pt[L1_LEN];

/* [ get=0/put=1 ][ byte size ] */
static unsigned int mem_counters[2][4];

#define COUNT(isput,bytes)                                      \
  if (verbose && enable_counting) mem_counters[isput][bytes]++

void
init_mem (void)
{
  int i, j;

  for (i = 0; i < L1_LEN; i++)
    if (pt[i])
      {
	for (j = 0; j < L2_LEN; j++)
	  if (pt[i][j])
	    free (pt[i][j]);
	free (pt[i]);
      }
  memset (pt, 0, sizeof (pt));
  memset (mem_counters, 0, sizeof (mem_counters));
}

static unsigned char *
mem_ptr (address)
{
  int pt1 = (address >> (L2_BITS + OFF_BITS)) & ((1 << L1_BITS) - 1);
  int pt2 = (address >> OFF_BITS) & ((1 << L2_BITS) - 1);
  int pto = address & ((1 << OFF_BITS) - 1);

  if (address == 0)
    {
      printf ("NULL pointer dereference\n");
      exit (1);
    }

  if (pt[pt1] == 0)
    pt[pt1] = (unsigned char **) calloc (L2_LEN, sizeof (char **));
  if (pt[pt1][pt2] == 0)
    {
      pt[pt1][pt2] = (unsigned char *) malloc (OFF_LEN);
      memset (pt[pt1][pt2], 0, OFF_LEN);
    }

  return pt[pt1][pt2] + pto;
}

static void
used (int rstart, int i, int j)
{
  int rend = i << (L2_BITS + OFF_BITS);
  rend += j << OFF_BITS;
  if (rstart == 0xe0000 && rend == 0xe1000)
    return;
  printf ("mem:   %08x - %08x (%dk bytes)\n", rstart, rend - 1,
	  (rend - rstart) / 1024);
}

static char *
mcs (int isput, int bytes)
{
  return comma (mem_counters[isput][bytes]);
}

void
mem_usage_stats ()
{
  int i, j;
  int rstart = 0;
  int pending = 0;

  for (i = 0; i < L1_LEN; i++)
    if (pt[i])
      {
	for (j = 0; j < L2_LEN; j++)
	  if (pt[i][j])
	    {
	      if (!pending)
		{
		  pending = 1;
		  rstart = (i << (L2_BITS + OFF_BITS)) + (j << OFF_BITS);
		}
	    }
	  else if (pending)
	    {
	      pending = 0;
	      used (rstart, i, j);
	    }
      }
    else
      {
	if (pending)
	  {
	    pending = 0;
	    used (rstart, i, 0);
	  }
      }
  /*       mem foo: 123456789012 123456789012 123456789012 123456789012
            123456789012 */
  printf ("                 byte        short      pointer         long"
          "        fetch\n");
  printf ("mem get: %12s %12s %12s %12s %12s\n", mcs (0, 1), mcs (0, 2),
	  mcs (0, 3), mcs (0, 4), mcs (0, 0));
  printf ("mem put: %12s %12s %12s %12s\n", mcs (1, 1), mcs (1, 2),
	  mcs (1, 3), mcs (1, 4));
}

static int tpr = 0;
static void
s (int address, char *dir)
{
  if (tpr == 0)
    printf ("MEM[%0*x] %s", membus_mask == 0xfffff ? 5 : 6, address, dir);
  tpr++;
}

#define S(d) if (trace) s(address, d)
static void
e ()
{
  if (!trace)
    return;
  tpr--;
  if (tpr == 0)
    printf ("\n");
}

#define E() if (trace) e()

void
mem_put_byte (int address, unsigned char value)
{
  unsigned char *m;
  address &= membus_mask;
  m = mem_ptr (address);
  if (trace)
    printf (" %02x", value);
  *m = value;
  switch (address)
    {
    case 0x00e1:
      {
	static int old_led = -1;
	static char *led_on[] =
	  { "\033[31m O ", "\033[32m O ", "\033[34m O " };
	static char *led_off[] = { "\033[0m · ", "\033[0m · ", "\033[0m · " };
	int i;
	if (old_led != value)
	  {
	    fputs ("  ", stdout);
	    for (i = 0; i < 3; i++)
	      if (value & (1 << i))
		fputs (led_off[i], stdout);
	      else
		fputs (led_on[i], stdout);
	    fputs ("\033[0m\r", stdout);
	    fflush (stdout);
	    old_led = value;
	  }
      }
      break;

    case 0x3aa: /* uart1tx */
      {
	static int pending_exit = 0;
	if (value == 0)
	  {
	    if (pending_exit)
	      {
		step_result = M32C_MAKE_EXITED(value);
		return;
	      }
	    pending_exit = 1;
	  }
	else
	  putchar(value);
      }
      break;

    case 0x400:
      m32c_syscall (value);
      break;

    case 0x401:
      putchar (value);
      break;

    case 0x402:
      printf ("SimTrace: %06lx %02x\n", regs.r_pc, value);
      break;

    case 0x403:
      printf ("SimTrap: %06lx %02x\n", regs.r_pc, value);
      abort ();
    }
}

void
mem_put_qi (int address, unsigned char value)
{
  S ("<=");
  mem_put_byte (address, value & 0xff);
  E ();
  COUNT (1, 1);
}

void
mem_put_hi (int address, unsigned short value)
{
  if (address == 0x402)
    {
      printf ("SimTrace: %06lx %04x\n", regs.r_pc, value);
      return;
    }
  S ("<=");
  mem_put_byte (address, value & 0xff);
  mem_put_byte (address + 1, value >> 8);
  E ();
  COUNT (1, 2);
}

void
mem_put_psi (int address, unsigned long value)
{
  S ("<=");
  mem_put_byte (address, value & 0xff);
  mem_put_byte (address + 1, (value >> 8) & 0xff);
  mem_put_byte (address + 2, value >> 16);
  E ();
  COUNT (1, 3);
}

void
mem_put_si (int address, unsigned long value)
{
  S ("<=");
  mem_put_byte (address, value & 0xff);
  mem_put_byte (address + 1, (value >> 8) & 0xff);
  mem_put_byte (address + 2, (value >> 16) & 0xff);
  mem_put_byte (address + 3, (value >> 24) & 0xff);
  E ();
  COUNT (1, 4);
}

void
mem_put_blk (int address, void *bufptr, int nbytes)
{
  S ("<=");
  if (enable_counting)
    mem_counters[1][1] += nbytes;
  while (nbytes--)
    mem_put_byte (address++, *(unsigned char *) bufptr++);
  E ();
}

unsigned char
mem_get_pc ()
{
  unsigned char *m = mem_ptr (regs.r_pc & membus_mask);
  COUNT (0, 0);
  return *m;
}

static unsigned char
mem_get_byte (int address)
{
  unsigned char *m;
  address &= membus_mask;
  S ("=>");
  m = mem_ptr (address);
  switch (address)
    {
    case 0x3ad: /* uart1c1 */
      E();
      return 2; /* transmitter empty */
      break;
    default: 
      if (trace)
	printf (" %02x", *m);
      break;
    }
  E ();
  return *m;
}

unsigned char
mem_get_qi (int address)
{
  unsigned char rv;
  S ("=>");
  rv = mem_get_byte (address);
  COUNT (0, 1);
  E ();
  return rv;
}

unsigned short
mem_get_hi (int address)
{
  unsigned short rv;
  S ("=>");
  rv = mem_get_byte (address);
  rv |= mem_get_byte (address + 1) * 256;
  COUNT (0, 2);
  E ();
  return rv;
}

unsigned long
mem_get_psi (int address)
{
  unsigned long rv;
  S ("=>");
  rv = mem_get_byte (address);
  rv |= mem_get_byte (address + 1) * 256;
  rv |= mem_get_byte (address + 2) * 65536;
  COUNT (0, 3);
  E ();
  return rv;
}

unsigned long
mem_get_si (int address)
{
  unsigned long rv;
  S ("=>");
  rv = mem_get_byte (address);
  rv |= mem_get_byte (address + 1) << 8;
  rv |= mem_get_byte (address + 2) << 16;
  rv |= mem_get_byte (address + 3) << 24;
  COUNT (0, 4);
  E ();
  return rv;
}

void
mem_get_blk (int address, void *bufptr, int nbytes)
{
  S ("=>");
  if (enable_counting)
    mem_counters[0][1] += nbytes;
  while (nbytes--)
    *(char *) bufptr++ = mem_get_byte (address++);
  E ();
}

int
sign_ext (int v, int bits)
{
  if (bits < 32)
    {
      v &= (1 << bits) - 1;
      if (v & (1 << (bits - 1)))
	v -= (1 << bits);
    }
  return v;
}
