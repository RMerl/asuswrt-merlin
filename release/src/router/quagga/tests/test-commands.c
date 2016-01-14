/*
 * Test code for lib/command.c
 *
 * Copyright (C) 2013 by Open Source Routing.
 * Copyright (C) 2013 by Internet Systems Consortium, Inc. ("ISC")
 *
 * This program reads in a list of commandlines from stdin
 * and calls all the public functions of lib/command.c for
 * both the given command lines and fuzzed versions thereof.
 *
 * The output is currently not validated but only logged. It can
 * be diffed to find regressions between versions.
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

#define REALLY_NEED_PLAIN_GETOPT 1

#include <zebra.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"
#include "memory.h"
#include "vector.h"
#include "prng.h"

extern vector cmdvec;
extern struct cmd_node vty_node;
extern void test_init_cmd(void); /* provided in test-commands-defun.c */

struct thread_master *master; /* dummy for libzebra*/

static vector test_cmds;
static char test_buf[32768];

static struct cmd_node bgp_node =
{
  BGP_NODE,
  "%s(config-router)# ",
};

static struct cmd_node rip_node =
{
  RIP_NODE,
  "%s(config-router)# ",
};

static struct cmd_node isis_node =
{
  ISIS_NODE,
  "%s(config-router)# ",
};

static struct cmd_node interface_node =
{
  INTERFACE_NODE,
  "%s(config-if)# ",
};

static struct cmd_node rmap_node =
{
  RMAP_NODE,
  "%s(config-route-map)# "
};

static struct cmd_node zebra_node =
{
  ZEBRA_NODE,
  "%s(config-router)# "
};

static struct cmd_node bgp_vpnv4_node =
{
  BGP_VPNV4_NODE,
  "%s(config-router-af)# "
};

static struct cmd_node bgp_ipv4_node =
{
  BGP_IPV4_NODE,
  "%s(config-router-af)# "
};

static struct cmd_node bgp_ipv4m_node =
{
  BGP_IPV4M_NODE,
  "%s(config-router-af)# "
};

static struct cmd_node bgp_ipv6_node =
{
  BGP_IPV6_NODE,
  "%s(config-router-af)# "
};

static struct cmd_node bgp_ipv6m_node =
{
  BGP_IPV6M_NODE,
  "%s(config-router-af)# "
};

static struct cmd_node ospf_node =
{
  OSPF_NODE,
  "%s(config-router)# "
};

static struct cmd_node ripng_node =
{
  RIPNG_NODE,
  "%s(config-router)# "
};

static struct cmd_node ospf6_node =
{
  OSPF6_NODE,
  "%s(config-ospf6)# "
};

static struct cmd_node babel_node =
{
  BABEL_NODE,
  "%s(config-babel)# "
};

static struct cmd_node keychain_node =
{
  KEYCHAIN_NODE,
  "%s(config-keychain)# "
};

static struct cmd_node keychain_key_node =
{
  KEYCHAIN_KEY_NODE,
  "%s(config-keychain-key)# "
};

static int
test_callback(struct cmd_element *cmd, struct vty *vty, int argc, const char *argv[])
{
  int offset;
  int rv;
  int i;

  offset = 0;
  rv = snprintf(test_buf, sizeof(test_buf), "'%s'", cmd->string);
  if (rv < 0)
    abort();

  offset += rv;

  for (i = 0; i < argc; i++)
    {
      rv = snprintf(test_buf + offset, sizeof(test_buf) - offset, "%s'%s'",
                    (i == 0) ? ": " : ", ", argv[i]);
      if (rv < 0)
        abort();
      offset += rv;
    }

  return CMD_SUCCESS;
}

static void
test_load(void)
{
  char line[4096];

  test_cmds = vector_init(VECTOR_MIN_SIZE);

  while (fgets(line, sizeof(line), stdin) != NULL)
    {
      if (strlen(line))
        line[strlen(line) - 1] = '\0';
      if (line[0] == '#')
        continue;
      vector_set(test_cmds, XSTRDUP(MTYPE_STRVEC, line));
    }
}

static void
test_init(void)
{
  unsigned int node;
  unsigned int i;
  struct cmd_node *cnode;
  struct cmd_element *cmd;

  cmd_init(1);

  install_node (&bgp_node, NULL);
  install_node (&rip_node, NULL);
  install_node (&interface_node, NULL);
  install_node (&rmap_node, NULL);
  install_node (&zebra_node, NULL);
  install_node (&bgp_vpnv4_node, NULL);
  install_node (&bgp_ipv4_node, NULL);
  install_node (&bgp_ipv4m_node, NULL);
  install_node (&bgp_ipv6_node, NULL);
  install_node (&bgp_ipv6m_node, NULL);
  install_node (&ospf_node, NULL);
  install_node (&ripng_node, NULL);
  install_node (&ospf6_node, NULL);
  install_node (&babel_node, NULL);
  install_node (&keychain_node, NULL);
  install_node (&keychain_key_node, NULL);
  install_node (&isis_node, NULL);
  install_node (&vty_node, NULL);

  test_init_cmd();

  for (node = 0; node < vector_active(cmdvec); node++)
    if ((cnode = vector_slot(cmdvec, node)) != NULL)
      for (i = 0; i < vector_active(cnode->cmd_vector); i++)
        if ((cmd = vector_slot(cnode->cmd_vector, i)) != NULL)
          {
            cmd->daemon = 0;
            cmd->func = test_callback;
          }
  test_load();
  vty_init_vtysh();
}

static void
test_terminate(void)
{
  unsigned int i;

  vty_terminate();
  for (i = 0; i < vector_active(test_cmds); i++)
    XFREE(MTYPE_STRVEC, vector_slot(test_cmds, i));
  vector_free(test_cmds);
  cmd_terminate();
}

static void
test_run(struct prng *prng, struct vty *vty, const char *cmd, unsigned int edit_dist, unsigned int node_index, int verbose)
{
  const char *test_str;
  vector vline;
  int ret;
  unsigned int i;
  char **completions;
  unsigned int j;
  struct cmd_node *cnode;
  vector descriptions;
  int appended_null;
  int no_match;

  test_str = prng_fuzz(prng, cmd, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_:. /", edit_dist);
  vline = cmd_make_strvec(test_str);

  if (vline == NULL)
    return;

  appended_null = 0;
  for (i = 0; i < vector_active(cmdvec); i++)
    if ((cnode = vector_slot(cmdvec, i)) != NULL)
      {
        if (node_index != (unsigned int)-1 && i != node_index)
          continue;

        if (appended_null)
          {
            vector_unset(vline, vector_active(vline) - 1);
            appended_null = 0;
          }
        vty->node = cnode->node;
        test_buf[0] = '\0';
        ret = cmd_execute_command(vline, vty, NULL, 0);
        no_match = (ret == CMD_ERR_NO_MATCH);
        if (verbose || !no_match)
          printf("execute relaxed '%s'@%d: rv==%d%s%s\n",
                 test_str,
                 cnode->node,
                 ret,
                 (test_buf[0] != '\0') ? ", " : "",
                 test_buf);

        vty->node = cnode->node;
        test_buf[0] = '\0';
        ret = cmd_execute_command_strict(vline, vty, NULL);
        if (verbose || !no_match)
          printf("execute strict '%s'@%d: rv==%d%s%s\n",
                 test_str,
                 cnode->node,
                 ret,
                 (test_buf[0] != '\0') ? ", " : "",
                 test_buf);

        if (isspace((int) test_str[strlen(test_str) - 1]))
          {
            vector_set (vline, NULL);
            appended_null = 1;
          }

        vty->node = cnode->node;
        completions = cmd_complete_command(vline, vty, &ret);
        if (verbose || !no_match)
          printf("complete '%s'@%d: rv==%d\n",
                 test_str,
                 cnode->node,
                 ret);
        if (completions != NULL)
          {
            for (j = 0; completions[j] != NULL; j++)
              {
                printf("  '%s'\n", completions[j]);
                XFREE(MTYPE_TMP, completions[j]);
              }
            XFREE(MTYPE_VECTOR_INDEX, completions);
          }

        vty->node = cnode->node;
        descriptions = cmd_describe_command(vline, vty, &ret);
        if (verbose || !no_match)
          printf("describe '%s'@%d: rv==%d\n",
                 test_str,
                 cnode->node,
                 ret);
        if (descriptions != NULL)
          {
            for (j = 0; j < vector_active(descriptions); j++)
              {
                struct cmd_token *cmd = vector_slot(descriptions, j);
                printf("  '%s' '%s'\n", cmd->cmd, cmd->desc);
              }
            vector_free(descriptions);
          }
      }
  cmd_free_strvec(vline);
}

int
main(int argc, char **argv)
{
  int opt;
  struct prng *prng;
  struct vty *vty;
  unsigned int edit_distance;
  unsigned int max_edit_distance;
  unsigned int node_index;
  int verbose;
  unsigned int test_cmd;
  unsigned int iteration;
  unsigned int num_iterations;

  max_edit_distance = 3;
  node_index = -1;
  verbose = 0;

  while ((opt = getopt(argc, argv, "e:n:v")) != -1)
    {
      switch (opt)
        {
        case 'e':
          max_edit_distance = atoi(optarg);
          break;
        case 'n':
          node_index = atoi(optarg);
          break;
        case 'v':
          verbose++;
          break;
        default:
          fprintf(stderr, "Usage: %s [-e <edit_dist>] [-n <node_idx>] [-v]\n", argv[0]);
          exit(1);
          break;
        }
    }

  test_init();
  prng = prng_new(0);

  vty = vty_new();
  vty->type = VTY_TERM;

  fprintf(stderr, "Progress:\n0/%u", vector_active(test_cmds));
  for (test_cmd = 0; test_cmd < vector_active(test_cmds); test_cmd++)
    {
      for (edit_distance = 0;
           edit_distance <= max_edit_distance;
           edit_distance++)
        {
          num_iterations = 1 << edit_distance;
          num_iterations *= num_iterations * num_iterations;

          for (iteration = 0; iteration < num_iterations; iteration++)
            test_run(prng, vty, vector_slot(test_cmds, test_cmd), edit_distance, node_index, verbose);
        }
      fprintf(stderr, "\r%u/%u", test_cmd + 1, vector_active(test_cmds));
    }
  fprintf(stderr, "\nDone.\n");

  vty_close(vty);
  prng_free(prng);
  test_terminate();
  return 0;
}
