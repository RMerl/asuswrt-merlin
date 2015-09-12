/*
 * Recursive Nexthop Iterator test.
 * This tests the ALL_NEXTHOPS_RO macro.
 *
 * Copyright (C) 2012 by Open Source Routing.
 * Copyright (C) 2012 by Internet Systems Consortium, Inc. ("ISC")
 *
 * This file is part of Quagga
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>
#include "zebra/rib.h"
#include "prng.h"

struct thread_master *master;
static int verbose;

static void
str_append(char **buf, const char *repr)
{
  if (*buf)
    {
      *buf = realloc(*buf, strlen(*buf) + strlen(repr) + 1);
      assert(*buf);
      strncpy((*buf) + strlen(*buf), repr, strlen(repr) + 1);
    }
  else
    {
      *buf = strdup(repr);
      assert(*buf);
    }
}

static void
str_appendf(char **buf, const char *format, ...)
{
  va_list ap;
  int rv;
  char *pbuf;

  va_start(ap, format);
  rv = vasprintf(&pbuf, format, ap);
  va_end(ap);
  assert(rv >= 0);

  str_append(buf, pbuf);
  free(pbuf);
}

/* This structure contains a nexthop chain
 * and its expected representation */
struct nexthop_chain
{
  /* Head of the chain */
  struct nexthop *head;
  /* Last nexthop in top chain */
  struct nexthop *current_top;
  /* Last nexthop in current recursive chain */
  struct nexthop *current_recursive;
  /* Expected string representation. */
  char *repr;
};

static struct nexthop_chain*
nexthop_chain_new(void)
{
  struct nexthop_chain *rv;

  rv = calloc(sizeof(*rv), 1);
  assert(rv);
  return rv;
}

static void
nexthop_chain_add_top(struct nexthop_chain *nc)
{
  struct nexthop *nh;

  nh = calloc(sizeof(*nh), 1);
  assert(nh);

  if (nc->head)
    {
      nc->current_top->next = nh;
      nh->prev = nc->current_top;
      nc->current_top = nh;
    }
  else
    {
      nc->head = nc->current_top = nh;
    }
  nc->current_recursive = NULL;
  str_appendf(&nc->repr, "%p\n", nh);
}

static void
nexthop_chain_add_recursive(struct nexthop_chain *nc)
{
  struct nexthop *nh;

  nh = calloc(sizeof(*nh), 1);
  assert(nh);

  assert(nc->current_top);
  if (nc->current_recursive)
    {
      nc->current_recursive->next = nh;
      nh->prev = nc->current_recursive;
      nc->current_recursive = nh;
    }
  else
    {
      SET_FLAG(nc->current_top->flags, NEXTHOP_FLAG_RECURSIVE);
      nc->current_top->resolved = nh;
      nc->current_recursive = nh;
    }
  str_appendf(&nc->repr, "  %p\n", nh);
}

static void
nexthop_chain_clear(struct nexthop_chain *nc)
{
  struct nexthop *tcur, *tnext;

  for (tcur = nc->head; tcur; tcur = tnext)
    {
      tnext = tcur->next;
      if (CHECK_FLAG(tcur->flags, NEXTHOP_FLAG_RECURSIVE))
        {
          struct nexthop *rcur, *rnext;
          for (rcur = tcur->resolved; rcur; rcur = rnext)
            {
              rnext = rcur->next;
              free(rcur);
            }
        }
      free(tcur);
    }
  nc->head = nc->current_top = nc->current_recursive = NULL;
  free(nc->repr);
  nc->repr = NULL;
}

static void
nexthop_chain_free(struct nexthop_chain *nc)
{
  if (!nc)
    return;
  nexthop_chain_clear(nc);
  free(nc);
}

/* This function builds a string representation of
 * the nexthop chain using the ALL_NEXTHOPS_RO macro.
 * It verifies that the ALL_NEXTHOPS_RO macro iterated
 * correctly over the nexthop chain by comparing the
 * generated representation with the expected representation.
 */
static void
nexthop_chain_verify_iter(struct nexthop_chain *nc)
{
  struct nexthop *nh, *tnh;
  int recursing;
  char *repr = NULL;

  for (ALL_NEXTHOPS_RO(nc->head, nh, tnh, recursing))
    {
      if (recursing)
        str_appendf(&repr, "  %p\n", nh);
      else
        str_appendf(&repr, "%p\n", nh);
    }

  if (repr && verbose)
    printf("===\n%s", repr);
  assert((!repr && !nc->repr) || (repr && nc->repr && !strcmp(repr, nc->repr)));
  free(repr);
}

/* This test run builds a simple nexthop chain
 * with some recursive nexthops and verifies that
 * the iterator works correctly in each stage along
 * the way.
 */
static void
test_run_first(void)
{
  struct nexthop_chain *nc;

  nc = nexthop_chain_new();
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_top(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_top(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_recursive(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_recursive(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_top(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_top(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_top(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_recursive(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_recursive(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_add_recursive(nc);
  nexthop_chain_verify_iter(nc);

  nexthop_chain_free(nc);
}

/* This test run builds numerous random
 * nexthop chain configurations and verifies
 * that the iterator correctly progresses
 * through each. */
static void
test_run_prng(void)
{
  struct nexthop_chain *nc;
  struct prng *prng;
  int i;

  nc = nexthop_chain_new();
  prng = prng_new(0);

  for (i = 0; i < 1000000; i++)
    {
      switch (prng_rand(prng) % 10)
        {
        case 0:
          nexthop_chain_clear(nc);
          break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
          nexthop_chain_add_top(nc);
          break;
        case 6:
        case 7:
        case 8:
        case 9:
          if (nc->current_top)
            nexthop_chain_add_recursive(nc);
          break;
        }
      nexthop_chain_verify_iter(nc);
    }
  nexthop_chain_free(nc);
  prng_free(prng);
}

int main(int argc, char **argv)
{
  if (argc >= 2 && !strcmp("-v", argv[1]))
    verbose = 1;
  test_run_first();
  printf("Simple test passed.\n");
  test_run_prng();
  printf("PRNG test passed.\n");
}
