/* $QuaggaId: Format:%an, %ai, %h$ $
 *
 * Routing table test
 * Copyright (C) 2012 OSR.
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

#include "prefix.h"
#include "table.h"

/*
 * test_node_t
 *
 * Information that is kept for each node in the radix tree.
 */
typedef struct test_node_t_
{

  /*
   * Human readable representation of the string. Allocated using
   * malloc()/dup().
   */
  char *prefix_str;
} test_node_t;

struct thread_master *master;

/*
 * add_node
 *
 * Add the given prefix (passed in as a string) to the given table.
 */
static void
add_node (struct route_table *table, const char *prefix_str)
{
  struct prefix_ipv4 p;
  test_node_t *node;
  struct route_node *rn;

  assert (prefix_str);

  if (str2prefix_ipv4 (prefix_str, &p) <= 0)
    {
      assert (0);
    }

  rn = route_node_get (table, (struct prefix *) &p);
  if (rn->info)
    {
      assert (0);
      return;
    }

  node = malloc (sizeof (test_node_t));
  assert (node);
  node->prefix_str = strdup (prefix_str);
  assert (node->prefix_str);
  rn->info = node;
}

/*
 * add_nodes
 *
 * Convenience function to add a bunch of nodes together.
 *
 * The arguments must be prefixes in string format, with a NULL as the
 * last argument.
 */
static void
add_nodes (struct route_table *table, ...)
{
  va_list arglist;
  char *prefix;

  va_start (arglist, table);

  prefix = va_arg (arglist, char *);
  while (prefix)
    {
      add_node (table, prefix);
      prefix = va_arg (arglist, char *);
    }

  va_end (arglist);
}

/*
 * print_subtree
 *
 * Recursive function to print a route node and its children.
 *
 * @see print_table
 */
static void
print_subtree (struct route_node *rn, const char *legend, int indent_level)
{
  char buf[INET_ADDRSTRLEN + 4];
  int i;

  /*
   * Print this node first.
   */
  for (i = 0; i < indent_level; i++)
    {
      printf ("  ");
    }

  prefix2str (&rn->p, buf, sizeof (buf));
  printf ("%s: %s", legend, buf);
  if (!rn->info)
    {
      printf (" (internal)");
    }
  printf ("\n");
  if (rn->l_left)
    {
      print_subtree (rn->l_left, "Left", indent_level + 1);
    }
  if (rn->l_right)
    {
      print_subtree (rn->l_right, "Right", indent_level + 1);
    }
}

/*
 * print_table
 *
 * Function that prints out the internal structure of a route table.
 */
static void
print_table (struct route_table *table)
{
  struct route_node *rn;

  rn = table->top;

  if (!rn)
    {
      printf ("<Empty Table>\n");
      return;
    }

  print_subtree (rn, "Top", 0);
}

/*
 * clear_table
 *
 * Remove all nodes from the given table.
 */
static void
clear_table (struct route_table *table)
{
  route_table_iter_t iter;
  struct route_node *rn;
  test_node_t *node;

  route_table_iter_init (&iter, table);

  while ((rn = route_table_iter_next (&iter)))
    {
      node = rn->info;
      if (!node)
	{
	  continue;
	}
      rn->info = NULL;
      route_unlock_node (rn);
      free (node->prefix_str);
      free (node);
    }

  route_table_iter_cleanup (&iter);

  assert (table->top == NULL);
}

/*
 * verify_next_by_iterating
 *
 * Iterate over the tree to make sure that the first prefix after
 * target_pfx is the expected one. Note that target_pfx may not be
 * present in the tree.
 */
static void
verify_next_by_iterating (struct route_table *table,
			  struct prefix *target_pfx, struct prefix *next_pfx)
{
  route_table_iter_t iter;
  struct route_node *rn;

  route_table_iter_init (&iter, table);
  while ((rn = route_table_iter_next (&iter)))
    {
      if (route_table_prefix_iter_cmp (&rn->p, target_pfx) > 0)
	{
	  assert (!prefix_cmp (&rn->p, next_pfx));
	  break;
	}
    }

  if (!rn)
    {
      assert (!next_pfx);
    }

  route_table_iter_cleanup (&iter);
}

/*
 * verify_next
 *
 * Verifies that route_table_get_next() returns the expected result
 * (result) for the prefix string 'target'.
 */
static void
verify_next (struct route_table *table, const char *target, const char *next)
{
  struct prefix_ipv4 target_pfx, next_pfx;
  struct route_node *rn;
  char result_buf[INET_ADDRSTRLEN + 4];

  if (str2prefix_ipv4 (target, &target_pfx) <= 0)
    {
      assert (0);
    }

  rn = route_table_get_next (table, (struct prefix *) &target_pfx);
  if (rn)
    {
      prefix2str (&rn->p, result_buf, sizeof (result_buf));
    }
  else
    {
      snprintf (result_buf, sizeof (result_buf), "(Null)");
    }

  printf ("\n");
  print_table (table);
  printf ("Verifying successor of %s. Expected: %s, Result: %s\n", target,
	  next ? next : "(Null)", result_buf);

  if (!rn)
    {
      assert (!next);
      verify_next_by_iterating (table, (struct prefix *) &target_pfx, NULL);
      return;
    }

  assert (next);

  if (str2prefix_ipv4 (next, &next_pfx) <= 0)
    {
      assert (0);
    }

  if (prefix_cmp (&rn->p, (struct prefix *) &next_pfx))
    {
      assert (0);
    }
  route_unlock_node (rn);

  verify_next_by_iterating (table, (struct prefix *) &target_pfx,
			    (struct prefix *) &next_pfx);
}

/*
 * test_get_next
 */
static void
test_get_next (void)
{
  struct route_table *table;

  printf ("\n\nTesting route_table_get_next()\n");
  table = route_table_init ();

  /*
   * Target exists in tree, but has no successor.
   */
  add_nodes (table, "1.0.1.0/24", NULL);
  verify_next (table, "1.0.1.0/24", NULL);
  clear_table (table);

  /*
   * Target exists in tree, and there is a node in its left subtree.
   */
  add_nodes (table, "1.0.1.0/24", "1.0.1.0/25", NULL);
  verify_next (table, "1.0.1.0/24", "1.0.1.0/25");
  clear_table (table);

  /*
   * Target exists in tree, and there is a node in its right subtree.
   */
  add_nodes (table, "1.0.1.0/24", "1.0.1.128/25", NULL);
  verify_next (table, "1.0.1.0/24", "1.0.1.128/25");
  clear_table (table);

  /*
   * Target exists in the tree, next node is outside subtree.
   */
  add_nodes (table, "1.0.1.0/24", "1.1.0.0/16", NULL);
  verify_next (table, "1.0.1.0/24", "1.1.0.0/16");
  clear_table (table);

  /*
   * The target node does not exist in the tree for all the test cases
   * below this point.
   */

  /*
   * There is no successor in the tree.
   */
  add_nodes (table, "1.0.0.0/16", NULL);
  verify_next (table, "1.0.1.0/24", NULL);
  clear_table (table);

  /*
   * There exists a node that would be in the target's left subtree.
   */
  add_nodes (table, "1.0.0.0/16", "1.0.1.0/25", NULL);
  verify_next (table, "1.0.1.0/24", "1.0.1.0/25");
  clear_table (table);

  /*
   * There exists a node would be in the target's right subtree.
   */
  add_nodes (table, "1.0.0.0/16", "1.0.1.128/25", NULL);
  verify_next (table, "1.0.1.0/24", "1.0.1.128/25");
  clear_table (table);

  /*
   * A search for the target reaches a node where there are no child
   * nodes in the direction of the target (left), but the node has a
   * right child.
   */
  add_nodes (table, "1.0.0.0/16", "1.0.128.0/17", NULL);
  verify_next (table, "1.0.0.0/17", "1.0.128.0/17");
  clear_table (table);

  /*
   * A search for the target reaches a node with no children. We have
   * to go upwards in the tree to find a successor.
   */
  add_nodes (table, "1.0.0.0/16", "1.0.0.0/24", "1.0.1.0/24",
	     "1.0.128.0/17", NULL);
  verify_next (table, "1.0.1.0/25", "1.0.128.0/17");
  clear_table (table);

  /*
   * A search for the target reaches a node where neither the node nor
   * the target prefix contain each other.
   *
   * In first case below the node succeeds the target.
   *
   * In the second case, the node comes before the target, so we have
   * to go up the tree looking for a successor.
   */
  add_nodes (table, "1.0.0.0/16", "1.0.1.0/24", NULL);
  verify_next (table, "1.0.0.0/24", "1.0.1.0/24");
  clear_table (table);

  add_nodes (table, "1.0.0.0/16", "1.0.0.0/24", "1.0.1.0/25",
	     "1.0.128.0/17", NULL);
  verify_next (table, "1.0.1.128/25", "1.0.128.0/17");
  clear_table (table);

  route_table_finish (table);
}

/*
 * verify_prefix_iter_cmp
 */
static void
verify_prefix_iter_cmp (const char *p1, const char *p2, int exp_result)
{
  struct prefix_ipv4 p1_pfx, p2_pfx;
  int result;

  if (str2prefix_ipv4 (p1, &p1_pfx) <= 0)
    {
      assert (0);
    }

  if (str2prefix_ipv4 (p2, &p2_pfx) <= 0)
    {
      assert (0);
    }

  result = route_table_prefix_iter_cmp ((struct prefix *) &p1_pfx,
					(struct prefix *) &p2_pfx);

  printf ("Verifying cmp(%s, %s) returns %d\n", p1, p2, exp_result);

  assert (exp_result == result);

  /*
   * Also check the reverse comparision.
   */
  result = route_table_prefix_iter_cmp ((struct prefix *) &p2_pfx,
					(struct prefix *) &p1_pfx);

  if (exp_result)
    {
      exp_result = -exp_result;
    }

  printf ("Verifying cmp(%s, %s) returns %d\n", p1, p2, exp_result);
  assert (result == exp_result);
}

/*
 * test_prefix_iter_cmp
 *
 * Tests comparision of prefixes according to order of iteration.
 */
static void
test_prefix_iter_cmp ()
{
  printf ("\n\nTesting route_table_prefix_iter_cmp()\n");

  verify_prefix_iter_cmp ("1.0.0.0/8", "1.0.0.0/8", 0);

  verify_prefix_iter_cmp ("1.0.0.0/8", "1.0.0.0/16", -1);

  verify_prefix_iter_cmp ("1.0.0.0/16", "1.128.0.0/16", -1);
}

/*
 * verify_iter_with_pause
 *
 * Iterates over a tree using two methods: 'normal' iteration, and an
 * iterator that pauses at each node. Verifies that the two methods
 * yield the same results.
 */
static void
verify_iter_with_pause (struct route_table *table)
{
  unsigned long num_nodes;
  struct route_node *rn, *iter_rn;
  route_table_iter_t iter_space;
  route_table_iter_t *iter = &iter_space;

  route_table_iter_init (iter, table);
  num_nodes = 0;

  for (rn = route_top (table); rn; rn = route_next (rn))
    {
      num_nodes++;
      route_table_iter_pause (iter);

      assert (iter->current == NULL);
      if (route_table_iter_started (iter))
	{
	  assert (iter->state == RT_ITER_STATE_PAUSED);
	}
      else
	{
	  assert (rn == table->top);
	  assert (iter->state == RT_ITER_STATE_INIT);
	}

      iter_rn = route_table_iter_next (iter);

      /*
       * Make sure both iterations return the same node.
       */
      assert (rn == iter_rn);
    }

  assert (num_nodes == route_table_count (table));

  route_table_iter_pause (iter);
  iter_rn = route_table_iter_next (iter);

  assert (iter_rn == NULL);
  assert (iter->state == RT_ITER_STATE_DONE);

  assert (route_table_iter_next (iter) == NULL);
  assert (iter->state == RT_ITER_STATE_DONE);

  route_table_iter_cleanup (iter);

  print_table (table);
  printf ("Verified pausing iteration on tree with %lu nodes\n", num_nodes);
}

/*
 * test_iter_pause
 */
static void
test_iter_pause (void)
{
  struct route_table *table;
  int i, num_prefixes;
  const char *prefixes[] = {
    "1.0.1.0/24",
    "1.0.1.0/25",
    "1.0.1.128/25",
    "1.0.2.0/24",
    "2.0.0.0/8"
  };

  num_prefixes = sizeof (prefixes) / sizeof (prefixes[0]);

  printf ("\n\nTesting that route_table_iter_pause() works as expected\n");
  table = route_table_init ();
  for (i = 0; i < num_prefixes; i++)
    {
      add_nodes (table, prefixes[i], NULL);
    }

  verify_iter_with_pause (table);

  clear_table (table);
  route_table_finish (table);
}

/*
 * run_tests
 */
static void
run_tests (void)
{
  test_prefix_iter_cmp ();
  test_get_next ();
  test_iter_pause ();
}

/*
 * main
 */
int
main (void)
{
  run_tests ();
}
