/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include "pim_rand.h"
#include "pim_time.h"

/* Quick and dirty random number generator from NUMERICAL RECIPES IN C:
   THE ART OF SCIENTIFIC COMPUTING (ISBN 0-521-43108-5). */
/* BEWARE: '_qseed_' is assigned! */
#define QRANDOM(_qseed_)  ((_qseed_) = (((_qseed_) * 1664525L) + 1013904223L))

static long qpim_rand_seed;

void pim_rand_init()
{
  qpim_rand_seed = pim_time_monotonic_sec() ^ getpid();
}

long pim_rand()
{
  return QRANDOM(qpim_rand_seed);
}

int pim_rand_next(int min, int max)
{
  long rand;

  assert(min <= max);

  /* FIXME better random generator ? */

  rand = QRANDOM(qpim_rand_seed); 
  if (rand < 0)
    rand = -rand;
  rand = rand % (1 + max - min) + min;

  assert(rand >= min);
  assert(rand <= max);

  return rand;
}
