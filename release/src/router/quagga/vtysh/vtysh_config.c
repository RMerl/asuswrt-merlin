/* Configuration generator.
   Copyright (C) 2000 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "command.h"
#include "linklist.h"
#include "memory.h"

#include "vtysh/vtysh.h"

vector configvec;

extern int vtysh_writeconfig_integrated;

struct config
{
  /* Configuration node name. */
  char *name;

  /* Configuration string line. */
  struct list *line;

  /* Configuration can be nest. */
  struct config *config;

  /* Index of this config. */
  u_int32_t index;
};

struct list *config_top;

int
line_cmp (char *c1, char *c2)
{
  return strcmp (c1, c2);
}

void
line_del (char *line)
{
  XFREE (MTYPE_VTYSH_CONFIG_LINE, line);
}

struct config *
config_new ()
{
  struct config *config;
  config = XCALLOC (MTYPE_VTYSH_CONFIG, sizeof (struct config));
  return config;
}

int
config_cmp (struct config *c1, struct config *c2)
{
  return strcmp (c1->name, c2->name);
}

void
config_del (struct config* config)
{
  list_delete (config->line);
  if (config->name)
    XFREE (MTYPE_VTYSH_CONFIG_LINE, config->name);
  XFREE (MTYPE_VTYSH_CONFIG, config);
}

struct config *
config_get (int index, const char *line)
{
  struct config *config;
  struct config *config_loop;
  struct list *master;
  struct listnode *node, *nnode;

  config = config_loop = NULL;

  master = vector_lookup_ensure (configvec, index);

  if (! master)
    {
      master = list_new ();
      master->del = (void (*) (void *))config_del;
      master->cmp = (int (*)(void *, void *)) config_cmp;
      vector_set_index (configvec, index, master);
    }
  
  for (ALL_LIST_ELEMENTS (master, node, nnode, config_loop))
    {
      if (strcmp (config_loop->name, line) == 0)
	config = config_loop;
    }

  if (! config)
    {
      config = config_new ();
      config->line = list_new ();
      config->line->del = (void (*) (void *))line_del;
      config->line->cmp = (int (*)(void *, void *)) line_cmp;
      config->name = XSTRDUP (MTYPE_VTYSH_CONFIG_LINE, line);
      config->index = index;
      listnode_add (master, config);
    }
  return config;
}

void
config_add_line (struct list *config, const char *line)
{
  listnode_add (config, XSTRDUP (MTYPE_VTYSH_CONFIG_LINE, line));
}

void
config_add_line_uniq (struct list *config, const char *line)
{
  struct listnode *node, *nnode;
  char *pnt;

  for (ALL_LIST_ELEMENTS (config, node, nnode, pnt))
    {
      if (strcmp (pnt, line) == 0)
	return;
    }
  listnode_add_sort (config, XSTRDUP (MTYPE_VTYSH_CONFIG_LINE, line));
}

void
vtysh_config_parse_line (const char *line)
{
  char c;
  static struct config *config = NULL;

  if (! line)
    return;

  c = line[0];

  if (c == '\0')
    return;

  /* printf ("[%s]\n", line); */

  switch (c)
    {
    case '!':
    case '#':
      break;
    case ' ':
      /* Store line to current configuration. */
      if (config)
	{
	  if (strncmp (line, " address-family vpnv4",
	      strlen (" address-family vpnv4")) == 0)
	    config = config_get (BGP_VPNV4_NODE, line);
	  else if (strncmp (line, " address-family ipv4 multicast",
		   strlen (" address-family ipv4 multicast")) == 0)
	    config = config_get (BGP_IPV4M_NODE, line);
	  else if (strncmp (line, " address-family ipv6",
		   strlen (" address-family ipv6")) == 0)
	    config = config_get (BGP_IPV6_NODE, line);
	  else if (config->index == RMAP_NODE ||
	           config->index == INTERFACE_NODE ||
		   config->index == VTY_NODE)
	    config_add_line_uniq (config->line, line);
	  else
	    config_add_line (config->line, line);
	}
      else
	config_add_line (config_top, line);
      break;
    default:
      if (strncmp (line, "interface", strlen ("interface")) == 0)
	config = config_get (INTERFACE_NODE, line);
      else if (strncmp (line, "router-id", strlen ("router-id")) == 0)
	config = config_get (ZEBRA_NODE, line);
      else if (strncmp (line, "router rip", strlen ("router rip")) == 0)
	config = config_get (RIP_NODE, line);
      else if (strncmp (line, "router ripng", strlen ("router ripng")) == 0)
	config = config_get (RIPNG_NODE, line);
      else if (strncmp (line, "router ospf", strlen ("router ospf")) == 0)
	config = config_get (OSPF_NODE, line);
      else if (strncmp (line, "router ospf6", strlen ("router ospf6")) == 0)
	config = config_get (OSPF6_NODE, line);
      else if (strncmp (line, "router babel", strlen ("router babel")) == 0)
	config = config_get (BABEL_NODE, line);
      else if (strncmp (line, "router bgp", strlen ("router bgp")) == 0)
	config = config_get (BGP_NODE, line);
      else if (strncmp (line, "router isis", strlen ("router isis")) == 0)
  	config = config_get (ISIS_NODE, line);
      else if (strncmp (line, "router bgp", strlen ("router bgp")) == 0)
	config = config_get (BGP_NODE, line);
      else if (strncmp (line, "route-map", strlen ("route-map")) == 0)
	config = config_get (RMAP_NODE, line);
      else if (strncmp (line, "access-list", strlen ("access-list")) == 0)
	config = config_get (ACCESS_NODE, line);
      else if (strncmp (line, "ipv6 access-list",
	       strlen ("ipv6 access-list")) == 0)
	config = config_get (ACCESS_IPV6_NODE, line);
      else if (strncmp (line, "ip prefix-list",
	       strlen ("ip prefix-list")) == 0)
	config = config_get (PREFIX_NODE, line);
      else if (strncmp (line, "ipv6 prefix-list",
	       strlen ("ipv6 prefix-list")) == 0)
	config = config_get (PREFIX_IPV6_NODE, line);
      else if (strncmp (line, "ip as-path access-list",
	       strlen ("ip as-path access-list")) == 0)
	config = config_get (AS_LIST_NODE, line);
      else if (strncmp (line, "ip community-list",
	       strlen ("ip community-list")) == 0)
	config = config_get (COMMUNITY_LIST_NODE, line);
      else if (strncmp (line, "ip route", strlen ("ip route")) == 0)
	config = config_get (IP_NODE, line);
      else if (strncmp (line, "ipv6 route", strlen ("ipv6 route")) == 0)
   	config = config_get (IP_NODE, line);
      else if (strncmp (line, "key", strlen ("key")) == 0)
	config = config_get (KEYCHAIN_NODE, line);
      else if (strncmp (line, "line", strlen ("line")) == 0)
	config = config_get (VTY_NODE, line);
      else if ( (strncmp (line, "ipv6 forwarding",
		 strlen ("ipv6 forwarding")) == 0)
	       || (strncmp (line, "ip forwarding",
		   strlen ("ip forwarding")) == 0) )
	config = config_get (FORWARDING_NODE, line);
      else if (strncmp (line, "service", strlen ("service")) == 0)
	config = config_get (SERVICE_NODE, line);
      else if (strncmp (line, "debug", strlen ("debug")) == 0)
	config = config_get (DEBUG_NODE, line);
      else if (strncmp (line, "password", strlen ("password")) == 0
	       || strncmp (line, "enable password",
			   strlen ("enable password")) == 0)
	config = config_get (AAA_NODE, line);
      else if (strncmp (line, "ip protocol", strlen ("ip protocol")) == 0)
	config = config_get (PROTOCOL_NODE, line);
      else
	{
	  if (strncmp (line, "log", strlen ("log")) == 0
	      || strncmp (line, "hostname", strlen ("hostname")) == 0
	     )
	    config_add_line_uniq (config_top, line);
	  else
	    config_add_line (config_top, line);
	  config = NULL;
	}
      break;
    }
}

void
vtysh_config_parse (char *line)
{
  char *begin;
  char *pnt;
  
  begin = pnt = line;

  while (*pnt != '\0')
    {
      if (*pnt == '\n')
	{
	  *pnt++ = '\0';
	  vtysh_config_parse_line (begin);
	  begin = pnt;
	}
      else
	{
	  pnt++;
	}
    }
}

/* Macro to check delimiter is needed between each configuration line
 * or not. */
#define NO_DELIMITER(I)  \
  ((I) == ACCESS_NODE || (I) == PREFIX_NODE || (I) == IP_NODE \
   || (I) == AS_LIST_NODE || (I) == COMMUNITY_LIST_NODE || \
   (I) == ACCESS_IPV6_NODE || (I) == PREFIX_IPV6_NODE \
   || (I) == SERVICE_NODE || (I) == FORWARDING_NODE || (I) == DEBUG_NODE \
   || (I) == AAA_NODE)

/* Display configuration to file pointer. */
void
vtysh_config_dump (FILE *fp)
{
  struct listnode *node, *nnode;
  struct listnode *mnode, *mnnode;
  struct config *config;
  struct list *master;
  char *line;
  unsigned int i;

  for (ALL_LIST_ELEMENTS (config_top, node, nnode, line))
    {
      fprintf (fp, "%s\n", line);
      fflush (fp);
    }
  fprintf (fp, "!\n");
  fflush (fp);

  for (i = 0; i < vector_active (configvec); i++)
    if ((master = vector_slot (configvec, i)) != NULL)
      {
	for (ALL_LIST_ELEMENTS (master, node, nnode, config))
	  {
	    fprintf (fp, "%s\n", config->name);
	    fflush (fp);

	    for (ALL_LIST_ELEMENTS (config->line, mnode, mnnode, line))
	      {
		fprintf  (fp, "%s\n", line);
		fflush (fp);
	      }
	    if (! NO_DELIMITER (i))
	      {
		fprintf (fp, "!\n");
		fflush (fp);
	      }
	  }
	if (NO_DELIMITER (i))
	  {
	    fprintf (fp, "!\n");
	    fflush (fp);
	  }
      }

  for (i = 0; i < vector_active (configvec); i++)
    if ((master = vector_slot (configvec, i)) != NULL)
      {
	list_delete (master);
	vector_slot (configvec, i) = NULL;
      }
  list_delete_all_node (config_top);
}

/* Read up configuration file from file_name. */
static void
vtysh_read_file (FILE *confp)
{
  int ret;
  struct vty *vty;

  vty = vty_new ();
  vty->fd = 0;			/* stdout */
  vty->type = VTY_TERM;
  vty->node = CONFIG_NODE;
  
  vtysh_execute_no_pager ("enable");
  vtysh_execute_no_pager ("configure terminal");

  /* Execute configuration file. */
  ret = vtysh_config_from_file (vty, confp);

  vtysh_execute_no_pager ("end");
  vtysh_execute_no_pager ("disable");

  vty_close (vty);

  if (ret != CMD_SUCCESS) 
    {
      switch (ret)
	{
	case CMD_ERR_AMBIGUOUS:
	  fprintf (stderr, "Ambiguous command.\n");
	  break;
	case CMD_ERR_NO_MATCH:
	  fprintf (stderr, "There is no such command.\n");
	  break;
	}
      fprintf (stderr, "Error occured during reading below line.\n%s\n", 
	       vty->buf);
      exit (1);
    }
}

/* Read up configuration file from config_default_dir. */
int
vtysh_read_config (char *config_default_dir)
{
  FILE *confp = NULL;

  confp = fopen (config_default_dir, "r");
  if (confp == NULL)
    return (1);

  vtysh_read_file (confp);
  fclose (confp);
  host_config_set (config_default_dir);

  return (0);
}

/* We don't write vtysh specific into file from vtysh. vtysh.conf should
 * be edited by hand. So, we handle only "write terminal" case here and
 * integrate vtysh specific conf with conf from daemons.
 */
void
vtysh_config_write ()
{
  char line[81];
  extern struct host host;

  if (host.name)
    {
      sprintf (line, "hostname %s", host.name);
      vtysh_config_parse_line(line);
    }
  if (vtysh_writeconfig_integrated)
    vtysh_config_parse_line ("service integrated-vtysh-config");
}

void
vtysh_config_init ()
{
  config_top = list_new ();
  config_top->del = (void (*) (void *))line_del;
  configvec = vector_init (1);
}
