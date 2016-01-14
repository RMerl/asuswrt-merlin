/* Virtual terminal interface shell.
 * Copyright (C) 2000 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include <sys/un.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "command.h"
#include "memory.h"
#include "vtysh/vtysh.h"
#include "log.h"
#include "bgpd/bgp_vty.h"

/* Struct VTY. */
struct vty *vty;

/* VTY shell pager name. */
char *vtysh_pager_name = NULL;

/* VTY shell client structure. */
struct vtysh_client
{
  int fd;
  const char *name;
  int flag;
  const char *path;
} vtysh_client[] =
{
  { .fd = -1, .name = "zebra", .flag = VTYSH_ZEBRA, .path = ZEBRA_VTYSH_PATH},
  { .fd = -1, .name = "ripd", .flag = VTYSH_RIPD, .path = RIP_VTYSH_PATH},
  { .fd = -1, .name = "ripngd", .flag = VTYSH_RIPNGD, .path = RIPNG_VTYSH_PATH},
  { .fd = -1, .name = "ospfd", .flag = VTYSH_OSPFD, .path = OSPF_VTYSH_PATH},
  { .fd = -1, .name = "ospf6d", .flag = VTYSH_OSPF6D, .path = OSPF6_VTYSH_PATH},
  { .fd = -1, .name = "bgpd", .flag = VTYSH_BGPD, .path = BGP_VTYSH_PATH},
  { .fd = -1, .name = "isisd", .flag = VTYSH_ISISD, .path = ISIS_VTYSH_PATH},
  { .fd = -1, .name = "babeld", .flag = VTYSH_BABELD, .path = BABEL_VTYSH_PATH},
  { .fd = -1, .name = "pimd", .flag = VTYSH_PIMD, .path = PIM_VTYSH_PATH},
};


/* We need direct access to ripd to implement vtysh_exit_ripd_only. */
static struct vtysh_client *ripd_client = NULL;
 

/* Using integrated config from Quagga.conf. Default is no. */
int vtysh_writeconfig_integrated = 0;

extern char config_default[];

static void
vclient_close (struct vtysh_client *vclient)
{
  if (vclient->fd >= 0)
    {
      fprintf(stderr,
	      "Warning: closing connection to %s because of an I/O error!\n",
	      vclient->name);
      close (vclient->fd);
      vclient->fd = -1;
    }
}

/* Following filled with debug code to trace a problematic condition
 * under load - it SHOULD handle it. */
#define ERR_WHERE_STRING "vtysh(): vtysh_client_config(): "
static int
vtysh_client_config (struct vtysh_client *vclient, char *line)
{
  int ret;
  char *buf;
  size_t bufsz;
  char *pbuf;
  size_t left;
  char *eoln;
  int nbytes;
  int i;
  int readln;

  if (vclient->fd < 0)
    return CMD_SUCCESS;

  ret = write (vclient->fd, line, strlen (line) + 1);
  if (ret <= 0)
    {
      vclient_close (vclient);
      return CMD_SUCCESS;
    }
	
  /* Allow enough room for buffer to read more than a few pages from socket. */
  bufsz = 5 * getpagesize() + 1;
  buf = XMALLOC(MTYPE_TMP, bufsz);
  memset(buf, 0, bufsz);
  pbuf = buf;

  while (1)
    {
      if (pbuf >= ((buf + bufsz) -1))
	{
	  fprintf (stderr, ERR_WHERE_STRING \
		   "warning - pbuf beyond buffer end.\n");
	  return CMD_WARNING;
	}

      readln = (buf + bufsz) - pbuf - 1;
      nbytes = read (vclient->fd, pbuf, readln);

      if (nbytes <= 0)
	{

	  if (errno == EINTR)
	    continue;

	  fprintf(stderr, ERR_WHERE_STRING "(%u)", errno);
	  perror("");

	  if (errno == EAGAIN || errno == EIO)
	    continue;

	  vclient_close (vclient);
	  XFREE(MTYPE_TMP, buf);
	  return CMD_SUCCESS;
	}

      pbuf[nbytes] = '\0';

      if (nbytes >= 4)
	{
	  i = nbytes - 4;
	  if (pbuf[i] == '\0' && pbuf[i + 1] == '\0' && pbuf[i + 2] == '\0')
	    {
	      ret = pbuf[i + 3];
	      break;
	    }
	}
      pbuf += nbytes;

      /* See if a line exists in buffer, if so parse and consume it, and
       * reset read position. */
      if ((eoln = strrchr(buf, '\n')) == NULL)
	continue;

      if (eoln >= ((buf + bufsz) - 1))
	{
	  fprintf (stderr, ERR_WHERE_STRING \
		   "warning - eoln beyond buffer end.\n");
	}
      vtysh_config_parse(buf);

      eoln++;
      left = (size_t)(buf + bufsz - eoln);
      memmove(buf, eoln, left);
      buf[bufsz-1] = '\0';
      pbuf = buf + strlen(buf);
    }

  /* Parse anything left in the buffer. */

  vtysh_config_parse (buf);

  XFREE(MTYPE_TMP, buf);
  return ret;
}

static int
vtysh_client_execute (struct vtysh_client *vclient, const char *line, FILE *fp)
{
  int ret;
  char buf[1001];
  int nbytes;
  int i; 
  int numnulls = 0;

  if (vclient->fd < 0)
    return CMD_SUCCESS;

  ret = write (vclient->fd, line, strlen (line) + 1);
  if (ret <= 0)
    {
      vclient_close (vclient);
      return CMD_SUCCESS;
    }
	
  while (1)
    {
      nbytes = read (vclient->fd, buf, sizeof(buf)-1);

      if (nbytes <= 0 && errno != EINTR)
	{
	  vclient_close (vclient);
	  return CMD_SUCCESS;
	}

      if (nbytes > 0)
	{
	  if ((numnulls == 3) && (nbytes == 1))
	    return buf[0];

	  buf[nbytes] = '\0';
	  fputs (buf, fp);
	  fflush (fp);
	  
	  /* check for trailling \0\0\0<ret code>, 
	   * even if split across reads 
	   * (see lib/vty.c::vtysh_read)
	   */
          if (nbytes >= 4) 
            {
              i = nbytes-4;
              numnulls = 0;
            }
          else
            i = 0;
          
          while (i < nbytes && numnulls < 3)
            {
              if (buf[i++] == '\0')
                numnulls++;
              else
                numnulls = 0;
            }

          /* got 3 or more trailing NULs? */
          if ((numnulls >= 3) && (i < nbytes))
            return (buf[nbytes-1]);
	}
    }
}

void
vtysh_exit_ripd_only (void)
{
  if (ripd_client)
    vtysh_client_execute (ripd_client, "exit", stdout);
}


void
vtysh_pager_init (void)
{
  char *pager_defined;

  pager_defined = getenv ("VTYSH_PAGER");

  if (pager_defined)
    vtysh_pager_name = strdup (pager_defined);
  else
    vtysh_pager_name = strdup ("more");
}

/* Command execution over the vty interface. */
static int
vtysh_execute_func (const char *line, int pager)
{
  int ret, cmd_stat;
  u_int i;
  vector vline;
  struct cmd_element *cmd;
  FILE *fp = NULL;
  int closepager = 0;
  int tried = 0;
  int saved_ret, saved_node;

  /* Split readline string up into the vector. */
  vline = cmd_make_strvec (line);

  if (vline == NULL)
    return CMD_SUCCESS;

  saved_ret = ret = cmd_execute_command (vline, vty, &cmd, 1);
  saved_node = vty->node;

  /* If command doesn't succeeded in current node, try to walk up in node tree.
   * Changing vty->node is enough to try it just out without actual walkup in
   * the vtysh. */
  while (ret != CMD_SUCCESS && ret != CMD_SUCCESS_DAEMON && ret != CMD_WARNING
	 && vty->node > CONFIG_NODE)
    {
      vty->node = node_parent(vty->node);
      ret = cmd_execute_command (vline, vty, &cmd, 1);
      tried++;
    }

  vty->node = saved_node;

  /* If command succeeded in any other node than current (tried > 0) we have
   * to move into node in the vtysh where it succeeded. */
  if (ret == CMD_SUCCESS || ret == CMD_SUCCESS_DAEMON || ret == CMD_WARNING)
    {
      if ((saved_node == BGP_VPNV4_NODE || saved_node == BGP_IPV4_NODE
	   || saved_node == BGP_IPV6_NODE || saved_node == BGP_IPV4M_NODE
	   || saved_node == BGP_IPV6M_NODE)
	  && (tried == 1))
	{
	  vtysh_execute("exit-address-family");
	}
      else if ((saved_node == KEYCHAIN_KEY_NODE) && (tried == 1))
	{
	  vtysh_execute("exit");
	}
      else if (tried)
	{
	  vtysh_execute ("end");
	  vtysh_execute ("configure terminal");
	}
    }
  /* If command didn't succeed in any node, continue with return value from
   * first try. */
  else if (tried)
    {
      ret = saved_ret;
    }

  cmd_free_strvec (vline);

  cmd_stat = ret;
  switch (ret)
    {
    case CMD_WARNING:
      if (vty->type == VTY_FILE)
	fprintf (stdout,"Warning...\n");
      break;
    case CMD_ERR_AMBIGUOUS:
      fprintf (stdout,"%% Ambiguous command.\n");
      break;
    case CMD_ERR_NO_MATCH:
      fprintf (stdout,"%% Unknown command.\n");
      break;
    case CMD_ERR_INCOMPLETE:
      fprintf (stdout,"%% Command incomplete.\n");
      break;
    case CMD_SUCCESS_DAEMON:
      {
	/* FIXME: Don't open pager for exit commands. popen() causes problems
	 * if exited from vtysh at all. This hack shouldn't cause any problem
	 * but is really ugly. */
	if (pager && vtysh_pager_name && (strncmp(line, "exit", 4) != 0))
	  {
	    fp = popen (vtysh_pager_name, "w");
	    if (fp == NULL)
	      {
		perror ("popen failed for pager");
		fp = stdout;
	      }
	    else
	      closepager=1;
	  }
	else
	  fp = stdout;

	if (! strcmp(cmd->string,"configure terminal"))
	  {
	    for (i = 0; i < array_size(vtysh_client); i++)
	      {
	        cmd_stat = vtysh_client_execute(&vtysh_client[i], line, fp);
		if (cmd_stat == CMD_WARNING)
		  break;
	      }

	    if (cmd_stat)
	      {
		line = "end";
		vline = cmd_make_strvec (line);

		if (vline == NULL)
		  {
		    if (pager && vtysh_pager_name && fp && closepager)
		      {
			if (pclose (fp) == -1)
			  {
			    perror ("pclose failed for pager");
			  }
			fp = NULL;
		      }
		    return CMD_SUCCESS;
		  }

		ret = cmd_execute_command (vline, vty, &cmd, 1);
		cmd_free_strvec (vline);
		if (ret != CMD_SUCCESS_DAEMON)
		  break;
	      }
	    else
	      if (cmd->func)
		{
		  (*cmd->func) (cmd, vty, 0, NULL);
		  break;
		}
	  }

	cmd_stat = CMD_SUCCESS;
	for (i = 0; i < array_size(vtysh_client); i++)
	  {
	    if (cmd->daemon & vtysh_client[i].flag)
	      {
	        cmd_stat = vtysh_client_execute(&vtysh_client[i], line, fp);
		if (cmd_stat != CMD_SUCCESS)
		  break;
	      }
	  }
	if (cmd_stat != CMD_SUCCESS)
	  break;

	if (cmd->func)
	  (*cmd->func) (cmd, vty, 0, NULL);
      }
    }
  if (pager && vtysh_pager_name && fp && closepager)
    {
      if (pclose (fp) == -1)
	{
	  perror ("pclose failed for pager");
	}
      fp = NULL;
    }
  return cmd_stat;
}

int
vtysh_execute_no_pager (const char *line)
{
  return vtysh_execute_func (line, 0);
}

int
vtysh_execute (const char *line)
{
  return vtysh_execute_func (line, 1);
}

/* Configration make from file. */
int
vtysh_config_from_file (struct vty *vty, FILE *fp)
{
  int ret;
  vector vline;
  struct cmd_element *cmd;

  while (fgets (vty->buf, VTY_BUFSIZ, fp))
    {
      if (vty->buf[0] == '!' || vty->buf[1] == '#')
	continue;

      vline = cmd_make_strvec (vty->buf);

      /* In case of comment line. */
      if (vline == NULL)
	continue;

      /* Execute configuration command : this is strict match. */
      ret = cmd_execute_command_strict (vline, vty, &cmd);

      /* Try again with setting node to CONFIG_NODE. */
      if (ret != CMD_SUCCESS 
	  && ret != CMD_SUCCESS_DAEMON
	  && ret != CMD_WARNING)
	{
	  if (vty->node == KEYCHAIN_KEY_NODE)
	    {
	      vty->node = KEYCHAIN_NODE;
	      vtysh_exit_ripd_only ();
	      ret = cmd_execute_command_strict (vline, vty, &cmd);

	      if (ret != CMD_SUCCESS 
		  && ret != CMD_SUCCESS_DAEMON 
		  && ret != CMD_WARNING)
		{
		  vtysh_exit_ripd_only ();
		  vty->node = CONFIG_NODE;
		  ret = cmd_execute_command_strict (vline, vty, &cmd);
		}
	    }
	  else
	    {
	      vtysh_execute ("end");
	      vtysh_execute ("configure terminal");
	      vty->node = CONFIG_NODE;
	      ret = cmd_execute_command_strict (vline, vty, &cmd);
	    }
	}	  

      cmd_free_strvec (vline);

      switch (ret)
	{
	case CMD_WARNING:
	  if (vty->type == VTY_FILE)
	    fprintf (stdout,"Warning...\n");
	  break;
	case CMD_ERR_AMBIGUOUS:
	  fprintf (stdout,"%% Ambiguous command.\n");
	  break;
	case CMD_ERR_NO_MATCH:
	  fprintf (stdout,"%% Unknown command: %s", vty->buf);
	  break;
	case CMD_ERR_INCOMPLETE:
	  fprintf (stdout,"%% Command incomplete.\n");
	  break;
	case CMD_SUCCESS_DAEMON:
	  {
	    u_int i;
	    int cmd_stat = CMD_SUCCESS;

	    for (i = 0; i < array_size(vtysh_client); i++)
	      {
	        if (cmd->daemon & vtysh_client[i].flag)
		  {
		    cmd_stat = vtysh_client_execute (&vtysh_client[i],
						     vty->buf, stdout);
		    if (cmd_stat != CMD_SUCCESS)
		      break;
		  }
	      }
	    if (cmd_stat != CMD_SUCCESS)
	      break;

	    if (cmd->func)
	      (*cmd->func) (cmd, vty, 0, NULL);
	  }
	}
    }
  return CMD_SUCCESS;
}

/* We don't care about the point of the cursor when '?' is typed. */
int
vtysh_rl_describe (void)
{
  int ret;
  unsigned int i;
  vector vline;
  vector describe;
  int width;
  struct cmd_token *token;

  vline = cmd_make_strvec (rl_line_buffer);

  /* In case of '> ?'. */
  if (vline == NULL)
    {
      vline = vector_init (1);
      vector_set (vline, '\0');
    }
  else 
    if (rl_end && isspace ((int) rl_line_buffer[rl_end - 1]))
      vector_set (vline, '\0');

  describe = cmd_describe_command (vline, vty, &ret);

  fprintf (stdout,"\n");

  /* Ambiguous and no match error. */
  switch (ret)
    {
    case CMD_ERR_AMBIGUOUS:
      cmd_free_strvec (vline);
      fprintf (stdout,"%% Ambiguous command.\n");
      rl_on_new_line ();
      return 0;
      break;
    case CMD_ERR_NO_MATCH:
      cmd_free_strvec (vline);
      fprintf (stdout,"%% There is no matched command.\n");
      rl_on_new_line ();
      return 0;
      break;
    }  

  /* Get width of command string. */
  width = 0;
  for (i = 0; i < vector_active (describe); i++)
    if ((token = vector_slot (describe, i)) != NULL)
      {
	int len;

	if (token->cmd[0] == '\0')
	  continue;

	len = strlen (token->cmd);
	if (token->cmd[0] == '.')
	  len--;

	if (width < len)
	  width = len;
      }

  for (i = 0; i < vector_active (describe); i++)
    if ((token = vector_slot (describe, i)) != NULL)
      {
	if (token->cmd[0] == '\0')
	  continue;

	if (! token->desc)
	  fprintf (stdout,"  %-s\n",
		   token->cmd[0] == '.' ? token->cmd + 1 : token->cmd);
	else
	  fprintf (stdout,"  %-*s  %s\n",
		   width,
		   token->cmd[0] == '.' ? token->cmd + 1 : token->cmd,
		   token->desc);
      }

  cmd_free_strvec (vline);
  vector_free (describe);

  rl_on_new_line();

  return 0;
}

/* Result of cmd_complete_command() call will be stored here
 * and used in new_completion() in order to put the space in
 * correct places only. */
int complete_status;

static char *
command_generator (const char *text, int state)
{
  vector vline;
  static char **matched = NULL;
  static int index = 0;

  /* First call. */
  if (! state)
    {
      index = 0;

      if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
	return NULL;

      vline = cmd_make_strvec (rl_line_buffer);
      if (vline == NULL)
	return NULL;

      if (rl_end && isspace ((int) rl_line_buffer[rl_end - 1]))
	vector_set (vline, '\0');

      matched = cmd_complete_command (vline, vty, &complete_status);
    }

  if (matched && matched[index])
    return matched[index++];

  return NULL;
}

static char **
new_completion (char *text, int start, int end)
{
  char **matches;

  matches = rl_completion_matches (text, command_generator);

  if (matches)
    {
      rl_point = rl_end;
      if (complete_status != CMD_COMPLETE_FULL_MATCH)
        /* only append a space on full match */
        rl_completion_append_character = '\0';
    }

  return matches;
}

#if 0
/* This function is not actually being used. */
static char **
vtysh_completion (char *text, int start, int end)
{
  int ret;
  vector vline;
  char **matched = NULL;

  if (vty->node == AUTH_NODE || vty->node == AUTH_ENABLE_NODE)
    return NULL;

  vline = cmd_make_strvec (rl_line_buffer);
  if (vline == NULL)
    return NULL;

  /* In case of 'help \t'. */
  if (rl_end && isspace ((int) rl_line_buffer[rl_end - 1]))
    vector_set (vline, '\0');

  matched = cmd_complete_command (vline, vty, &ret);

  cmd_free_strvec (vline);

  return (char **) matched;
}
#endif

/* Vty node structures. */
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

/* Defined in lib/vty.c */
extern struct cmd_node vty_node;

/* When '^Z' is received from vty, move down to the enable mode. */
int
vtysh_end (void)
{
  switch (vty->node)
    {
    case VIEW_NODE:
    case ENABLE_NODE:
      /* Nothing to do. */
      break;
    default:
      vty->node = ENABLE_NODE;
      break;
    }
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_end_all,
	 vtysh_end_all_cmd,
	 "end",
	 "End current mode and change to enable mode\n")
{
  return vtysh_end ();
}

DEFUNSH (VTYSH_BGPD,
	 router_bgp,
	 router_bgp_cmd,
	 "router bgp " CMD_AS_RANGE,
	 ROUTER_STR
	 BGP_STR
	 AS_STR)
{
  vty->node = BGP_NODE;
  return CMD_SUCCESS;
}

ALIAS_SH (VTYSH_BGPD,
	  router_bgp,
	  router_bgp_view_cmd,
	  "router bgp " CMD_AS_RANGE " view WORD",
	  ROUTER_STR
	  BGP_STR
	  AS_STR
	  "BGP view\n"
	  "view name\n")

DEFUNSH (VTYSH_BGPD,
	 address_family_vpnv4,
	 address_family_vpnv4_cmd,
	 "address-family vpnv4",
	 "Enter Address Family command mode\n"
	 "Address family\n")
{
  vty->node = BGP_VPNV4_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_BGPD,
	 address_family_vpnv4_unicast,
	 address_family_vpnv4_unicast_cmd,
	 "address-family vpnv4 unicast",
	 "Enter Address Family command mode\n"
	 "Address family\n"
	 "Address Family Modifier\n")
{
  vty->node = BGP_VPNV4_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_BGPD,
	 address_family_ipv4_unicast,
	 address_family_ipv4_unicast_cmd,
	 "address-family ipv4 unicast",
	 "Enter Address Family command mode\n"
	 "Address family\n"
	 "Address Family Modifier\n")
{
  vty->node = BGP_IPV4_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_BGPD,
	 address_family_ipv4_multicast,
	 address_family_ipv4_multicast_cmd,
	 "address-family ipv4 multicast",
	 "Enter Address Family command mode\n"
	 "Address family\n"
	 "Address Family Modifier\n")
{
  vty->node = BGP_IPV4M_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_BGPD,
	 address_family_ipv6,
	 address_family_ipv6_cmd,
	 "address-family ipv6",
	 "Enter Address Family command mode\n"
	 "Address family\n")
{
  vty->node = BGP_IPV6_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_BGPD,
	 address_family_ipv6_unicast,
	 address_family_ipv6_unicast_cmd,
	 "address-family ipv6 unicast",
	 "Enter Address Family command mode\n"
	 "Address family\n"
	 "Address Family Modifier\n")
{
  vty->node = BGP_IPV6_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_BGPD,
	 address_family_ipv6_multicast,
	 address_family_ipv6_multicast_cmd,
	 "address-family ipv6 multicast",
	 "Enter Address Family command mode\n"
	 "Address family\n"
	 "Address Family Modifier\n")
{
  vty->node = BGP_IPV6M_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_RIPD,
	 key_chain,
	 key_chain_cmd,
	 "key chain WORD",
	 "Authentication key management\n"
	 "Key-chain management\n"
	 "Key-chain name\n")
{
  vty->node = KEYCHAIN_NODE;
  return CMD_SUCCESS;
}	 

DEFUNSH (VTYSH_RIPD,
	 key,
	 key_cmd,
	 "key <0-2147483647>",
	 "Configure a key\n"
	 "Key identifier number\n")
{
  vty->node = KEYCHAIN_KEY_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_RIPD,
	 router_rip,
	 router_rip_cmd,
	 "router rip",
	 ROUTER_STR
	 "RIP")
{
  vty->node = RIP_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_RIPNGD,
	 router_ripng,
	 router_ripng_cmd,
	 "router ripng",
	 ROUTER_STR
	 "RIPng")
{
  vty->node = RIPNG_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_OSPFD,
	 router_ospf,
	 router_ospf_cmd,
	 "router ospf",
	 "Enable a routing process\n"
	 "Start OSPF configuration\n")
{
  vty->node = OSPF_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_OSPF6D,
	 router_ospf6,
	 router_ospf6_cmd,
	 "router ospf6",
	 OSPF6_ROUTER_STR
	 OSPF6_STR)
{
  vty->node = OSPF6_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_BABELD,
	 router_babel,
	 router_babel_cmd,
	 "router babel",
	 ROUTER_STR
	 "Babel")
{
  vty->node = BABEL_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ISISD,
	 router_isis,
	 router_isis_cmd,
	 "router isis WORD",
	 ROUTER_STR
	 "ISO IS-IS\n"
	 "ISO Routing area tag")
{
  vty->node = ISIS_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_RMAP,
	 route_map,
	 route_map_cmd,
	 "route-map WORD (deny|permit) <1-65535>",
	 "Create route-map or enter route-map command mode\n"
	 "Route map tag\n"
	 "Route map denies set operations\n"
	 "Route map permits set operations\n"
	 "Sequence to insert to/delete from existing route-map entry\n")
{
  vty->node = RMAP_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_line_vty,
	 vtysh_line_vty_cmd,
	 "line vty",
	 "Configure a terminal line\n"
	 "Virtual terminal\n")
{
  vty->node = VTY_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_enable, 
	 vtysh_enable_cmd,
	 "enable",
	 "Turn on privileged mode command\n")
{
  vty->node = ENABLE_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_disable, 
	 vtysh_disable_cmd,
	 "disable",
	 "Turn off privileged mode command\n")
{
  if (vty->node == ENABLE_NODE)
    vty->node = VIEW_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_config_terminal,
	 vtysh_config_terminal_cmd,
	 "configure terminal",
	 "Configuration from vty interface\n"
	 "Configuration terminal\n")
{
  vty->node = CONFIG_NODE;
  return CMD_SUCCESS;
}

static int
vtysh_exit (struct vty *vty)
{
  switch (vty->node)
    {
    case VIEW_NODE:
    case ENABLE_NODE:
      exit (0);
      break;
    case CONFIG_NODE:
      vty->node = ENABLE_NODE;
      break;
    case INTERFACE_NODE:
    case ZEBRA_NODE:
    case BGP_NODE:
    case RIP_NODE:
    case RIPNG_NODE:
    case OSPF_NODE:
    case OSPF6_NODE:
    case BABEL_NODE:
    case ISIS_NODE:
    case MASC_NODE:
    case RMAP_NODE:
    case VTY_NODE:
    case KEYCHAIN_NODE:
      vtysh_execute("end");
      vtysh_execute("configure terminal");
      vty->node = CONFIG_NODE;
      break;
    case BGP_VPNV4_NODE:
    case BGP_IPV4_NODE:
    case BGP_IPV4M_NODE:
    case BGP_IPV6_NODE:
    case BGP_IPV6M_NODE:
      vty->node = BGP_NODE;
      break;
    case KEYCHAIN_KEY_NODE:
      vty->node = KEYCHAIN_NODE;
      break;
    default:
      break;
    }
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_exit_all,
	 vtysh_exit_all_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_all,
       vtysh_quit_all_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_BGPD,
	 exit_address_family,
	 exit_address_family_cmd,
	 "exit-address-family",
	 "Exit from Address Family configuration mode\n")
{
  if (vty->node == BGP_IPV4_NODE
      || vty->node == BGP_IPV4M_NODE
      || vty->node == BGP_VPNV4_NODE
      || vty->node == BGP_IPV6_NODE
      || vty->node == BGP_IPV6M_NODE)
    vty->node = BGP_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ZEBRA,
	 vtysh_exit_zebra,
	 vtysh_exit_zebra_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_zebra,
       vtysh_quit_zebra_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_RIPD,
	 vtysh_exit_ripd,
	 vtysh_exit_ripd_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_ripd,
       vtysh_quit_ripd_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_RIPNGD,
	 vtysh_exit_ripngd,
	 vtysh_exit_ripngd_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_ripngd,
       vtysh_quit_ripngd_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_RMAP,
	 vtysh_exit_rmap,
	 vtysh_exit_rmap_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_rmap,
       vtysh_quit_rmap_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_BGPD,
	 vtysh_exit_bgpd,
	 vtysh_exit_bgpd_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_bgpd,
       vtysh_quit_bgpd_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_OSPFD,
	 vtysh_exit_ospfd,
	 vtysh_exit_ospfd_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_ospfd,
       vtysh_quit_ospfd_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_OSPF6D,
	 vtysh_exit_ospf6d,
	 vtysh_exit_ospf6d_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_ospf6d,
       vtysh_quit_ospf6d_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_ISISD,
	 vtysh_exit_isisd,
	 vtysh_exit_isisd_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_isisd,
       vtysh_quit_isisd_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_ALL,
         vtysh_exit_line_vty,
         vtysh_exit_line_vty_cmd,
         "exit",
         "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_line_vty,
       vtysh_quit_line_vty_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

DEFUNSH (VTYSH_INTERFACE,
	 vtysh_interface,
	 vtysh_interface_cmd,
	 "interface IFNAME",
	 "Select an interface to configure\n"
	 "Interface's name\n")
{
  vty->node = INTERFACE_NODE;
  return CMD_SUCCESS;
}

/* TODO Implement "no interface command in isisd. */
DEFSH (VTYSH_ZEBRA|VTYSH_RIPD|VTYSH_RIPNGD|VTYSH_OSPFD|VTYSH_OSPF6D,
       vtysh_no_interface_cmd,
       "no interface IFNAME",
       NO_STR
       "Delete a pseudo interface's configuration\n"
       "Interface's name\n")

/* TODO Implement interface description commands in ripngd, ospf6d
 * and isisd. */
DEFSH (VTYSH_ZEBRA|VTYSH_RIPD|VTYSH_OSPFD,
       interface_desc_cmd,
       "description .LINE",
       "Interface specific description\n"
       "Characters describing this interface\n")
       
DEFSH (VTYSH_ZEBRA|VTYSH_RIPD|VTYSH_OSPFD,
       no_interface_desc_cmd,
       "no description",
       NO_STR
       "Interface specific description\n")

DEFUNSH (VTYSH_INTERFACE,
	 vtysh_exit_interface,
	 vtysh_exit_interface_cmd,
	 "exit",
	 "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

ALIAS (vtysh_exit_interface,
       vtysh_quit_interface_cmd,
       "quit",
       "Exit current mode and down to previous mode\n")

/* Memory */
DEFUN (vtysh_show_memory,
       vtysh_show_memory_cmd,
       "show memory",
       SHOW_STR
       "Memory statistics\n")
{
  unsigned int i;
  int ret = CMD_SUCCESS;
  char line[] = "show memory\n";
  
  for (i = 0; i < array_size(vtysh_client); i++)
    if ( vtysh_client[i].fd >= 0 )
      {
        fprintf (stdout, "Memory statistics for %s:\n", 
                 vtysh_client[i].name);
        ret = vtysh_client_execute (&vtysh_client[i], line, stdout);
        fprintf (stdout,"\n");
      }
  
  return ret;
}

/* Logging commands. */
DEFUN (vtysh_show_logging,
       vtysh_show_logging_cmd,
       "show logging",
       SHOW_STR
       "Show current logging configuration\n")
{
  unsigned int i;
  int ret = CMD_SUCCESS;
  char line[] = "show logging\n";
  
  for (i = 0; i < array_size(vtysh_client); i++)
    if ( vtysh_client[i].fd >= 0 )
      {
        fprintf (stdout,"Logging configuration for %s:\n", 
                 vtysh_client[i].name);
        ret = vtysh_client_execute (&vtysh_client[i], line, stdout);
        fprintf (stdout,"\n");
      }
  
  return ret;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_stdout,
	 vtysh_log_stdout_cmd,
	 "log stdout",
	 "Logging control\n"
	 "Set stdout logging level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_stdout_level,
	 vtysh_log_stdout_level_cmd,
	 "log stdout "LOG_LEVELS,
	 "Logging control\n"
	 "Set stdout logging level\n"
	 LOG_LEVEL_DESC)
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_stdout,
	 no_vtysh_log_stdout_cmd,
	 "no log stdout [LEVEL]",
	 NO_STR
	 "Logging control\n"
	 "Cancel logging to stdout\n"
	 "Logging level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_file,
	 vtysh_log_file_cmd,
	 "log file FILENAME",
	 "Logging control\n"
	 "Logging to file\n"
	 "Logging filename\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_file_level,
	 vtysh_log_file_level_cmd,
	 "log file FILENAME "LOG_LEVELS,
	 "Logging control\n"
	 "Logging to file\n"
	 "Logging filename\n"
	 LOG_LEVEL_DESC)
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_file,
	 no_vtysh_log_file_cmd,
	 "no log file [FILENAME]",
	 NO_STR
	 "Logging control\n"
	 "Cancel logging to file\n"
	 "Logging file name\n")
{
  return CMD_SUCCESS;
}

ALIAS_SH (VTYSH_ALL,
	  no_vtysh_log_file,
	  no_vtysh_log_file_level_cmd,
	  "no log file FILENAME LEVEL",
	  NO_STR
	  "Logging control\n"
	  "Cancel logging to file\n"
	  "Logging file name\n"
	  "Logging level\n")

DEFUNSH (VTYSH_ALL,
	 vtysh_log_monitor,
	 vtysh_log_monitor_cmd,
	 "log monitor",
	 "Logging control\n"
	 "Set terminal line (monitor) logging level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_monitor_level,
	 vtysh_log_monitor_level_cmd,
	 "log monitor "LOG_LEVELS,
	 "Logging control\n"
	 "Set terminal line (monitor) logging level\n"
	 LOG_LEVEL_DESC)
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_monitor,
	 no_vtysh_log_monitor_cmd,
	 "no log monitor [LEVEL]",
	 NO_STR
	 "Logging control\n"
	 "Disable terminal line (monitor) logging\n"
	 "Logging level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_syslog,
	 vtysh_log_syslog_cmd,
	 "log syslog",
	 "Logging control\n"
	 "Set syslog logging level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_syslog_level,
	 vtysh_log_syslog_level_cmd,
	 "log syslog "LOG_LEVELS,
	 "Logging control\n"
	 "Set syslog logging level\n"
	 LOG_LEVEL_DESC)
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_syslog,
	 no_vtysh_log_syslog_cmd,
	 "no log syslog [LEVEL]",
	 NO_STR
	 "Logging control\n"
	 "Cancel logging to syslog\n"
	 "Logging level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_facility,
	 vtysh_log_facility_cmd,
	 "log facility "LOG_FACILITIES,
	 "Logging control\n"
	 "Facility parameter for syslog messages\n"
	 LOG_FACILITY_DESC)

{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_facility,
	 no_vtysh_log_facility_cmd,
	 "no log facility [FACILITY]",
	 NO_STR
	 "Logging control\n"
	 "Reset syslog facility to default (daemon)\n"
	 "Syslog facility\n")

{
  return CMD_SUCCESS;
}

DEFUNSH_DEPRECATED (VTYSH_ALL,
		    vtysh_log_trap,
		    vtysh_log_trap_cmd,
		    "log trap "LOG_LEVELS,
		    "Logging control\n"
		    "(Deprecated) Set logging level and default for all destinations\n"
		    LOG_LEVEL_DESC)

{
  return CMD_SUCCESS;
}

DEFUNSH_DEPRECATED (VTYSH_ALL,
		    no_vtysh_log_trap,
		    no_vtysh_log_trap_cmd,
		    "no log trap [LEVEL]",
		    NO_STR
		    "Logging control\n"
		    "Permit all logging information\n"
		    "Logging level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_record_priority,
	 vtysh_log_record_priority_cmd,
	 "log record-priority",
	 "Logging control\n"
	 "Log the priority of the message within the message\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_record_priority,
	 no_vtysh_log_record_priority_cmd,
	 "no log record-priority",
	 NO_STR
	 "Logging control\n"
	 "Do not log the priority of the message within the message\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_timestamp_precision,
	 vtysh_log_timestamp_precision_cmd,
	 "log timestamp precision <0-6>",
	 "Logging control\n"
	 "Timestamp configuration\n"
	 "Set the timestamp precision\n"
	 "Number of subsecond digits\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_timestamp_precision,
	 no_vtysh_log_timestamp_precision_cmd,
	 "no log timestamp precision",
	 NO_STR
	 "Logging control\n"
	 "Timestamp configuration\n"
	 "Reset the timestamp precision to the default value of 0\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_service_password_encrypt,
	 vtysh_service_password_encrypt_cmd,
	 "service password-encryption",
	 "Set up miscellaneous service\n"
	 "Enable encrypted passwords\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_service_password_encrypt,
	 no_vtysh_service_password_encrypt_cmd,
	 "no service password-encryption",
	 NO_STR
	 "Set up miscellaneous service\n"
	 "Enable encrypted passwords\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_config_password,
	 vtysh_password_cmd,
	 "password (8|) WORD",
	 "Assign the terminal connection password\n"
	 "Specifies a HIDDEN password will follow\n"
	 "dummy string \n"
	 "The HIDDEN line password string\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_password_text,
	 vtysh_password_text_cmd,
	 "password LINE",
	 "Assign the terminal connection password\n"
	 "The UNENCRYPTED (cleartext) line password\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_config_enable_password,
	 vtysh_enable_password_cmd,
	 "enable password (8|) WORD",
	 "Modify enable password parameters\n"
	 "Assign the privileged level password\n"
	 "Specifies a HIDDEN password will follow\n"
	 "dummy string \n"
	 "The HIDDEN 'enable' password string\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_enable_password_text,
	 vtysh_enable_password_text_cmd,
	 "enable password LINE",
	 "Modify enable password parameters\n"
	 "Assign the privileged level password\n"
	 "The UNENCRYPTED (cleartext) 'enable' password\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_config_enable_password,
	 no_vtysh_enable_password_cmd,
	 "no enable password",
	 NO_STR
	 "Modify enable password parameters\n"
	 "Assign the privileged level password\n")
{
  return CMD_SUCCESS;
}

DEFUN (vtysh_write_terminal,
       vtysh_write_terminal_cmd,
       "write terminal",
       "Write running configuration to memory, network, or terminal\n"
       "Write to terminal\n")
{
  u_int i;
  int ret;
  char line[] = "write terminal\n";
  FILE *fp = NULL;

  if (vtysh_pager_name)
    {
      fp = popen (vtysh_pager_name, "w");
      if (fp == NULL)
	{
	  perror ("popen");
	  exit (1);
	}
    }
  else
    fp = stdout;

  vty_out (vty, "Building configuration...%s", VTY_NEWLINE);
  vty_out (vty, "%sCurrent configuration:%s", VTY_NEWLINE,
	   VTY_NEWLINE);
  vty_out (vty, "!%s", VTY_NEWLINE);

  for (i = 0; i < array_size(vtysh_client); i++)
    ret = vtysh_client_config (&vtysh_client[i], line);

  /* Integrate vtysh specific configuration. */
  vtysh_config_write ();

  vtysh_config_dump (fp);

  if (vtysh_pager_name && fp)
    {
      fflush (fp);
      if (pclose (fp) == -1)
	{
	  perror ("pclose");
	  exit (1);
	}
      fp = NULL;
    }

  vty_out (vty, "end%s", VTY_NEWLINE);
  
  return CMD_SUCCESS;
}

DEFUN (vtysh_integrated_config,
       vtysh_integrated_config_cmd,
       "service integrated-vtysh-config",
       "Set up miscellaneous service\n"
       "Write configuration into integrated file\n")
{
  vtysh_writeconfig_integrated = 1;
  return CMD_SUCCESS;
}

DEFUN (no_vtysh_integrated_config,
       no_vtysh_integrated_config_cmd,
       "no service integrated-vtysh-config",
       NO_STR
       "Set up miscellaneous service\n"
       "Write configuration into integrated file\n")
{
  vtysh_writeconfig_integrated = 0;
  return CMD_SUCCESS;
}

static int
write_config_integrated(void)
{
  u_int i;
  int ret;
  char line[] = "write terminal\n";
  FILE *fp;
  char *integrate_sav = NULL;

  integrate_sav = malloc (strlen (integrate_default) +
			  strlen (CONF_BACKUP_EXT) + 1);
  strcpy (integrate_sav, integrate_default);
  strcat (integrate_sav, CONF_BACKUP_EXT);

  fprintf (stdout,"Building Configuration...\n");

  /* Move current configuration file to backup config file. */
  unlink (integrate_sav);
  rename (integrate_default, integrate_sav);
  free (integrate_sav);
 
  fp = fopen (integrate_default, "w");
  if (fp == NULL)
    {
      fprintf (stdout,"%% Can't open configuration file %s.\n",
	       integrate_default);
      return CMD_SUCCESS;
    }

  for (i = 0; i < array_size(vtysh_client); i++)
    ret = vtysh_client_config (&vtysh_client[i], line);

  vtysh_config_dump (fp);

  fclose (fp);

  if (chmod (integrate_default, CONFIGFILE_MASK) != 0)
    {
      fprintf (stdout,"%% Can't chmod configuration file %s: %s (%d)\n", 
	integrate_default, safe_strerror(errno), errno);
      return CMD_WARNING;
    }

  fprintf(stdout,"Integrated configuration saved to %s\n",integrate_default);

  fprintf (stdout,"[OK]\n");

  return CMD_SUCCESS;
}

DEFUN (vtysh_write_memory,
       vtysh_write_memory_cmd,
       "write memory",
       "Write running configuration to memory, network, or terminal\n"
       "Write configuration to the file (same as write file)\n")
{
  int ret = CMD_SUCCESS;
  char line[] = "write memory\n";
  u_int i;
  
  /* If integrated Quagga.conf explicitely set. */
  if (vtysh_writeconfig_integrated)
    return write_config_integrated();

  fprintf (stdout,"Building Configuration...\n");
	  
  for (i = 0; i < array_size(vtysh_client); i++)
    ret = vtysh_client_execute (&vtysh_client[i], line, stdout);
  
  fprintf (stdout,"[OK]\n");

  return ret;
}

ALIAS (vtysh_write_memory,
       vtysh_copy_runningconfig_startupconfig_cmd,
       "copy running-config startup-config",  
       "Copy from one file to another\n"
       "Copy from current system configuration\n"
       "Copy to startup configuration\n")

ALIAS (vtysh_write_memory,
       vtysh_write_file_cmd,
       "write file",
       "Write running configuration to memory, network, or terminal\n"
       "Write configuration to the file (same as write memory)\n")

ALIAS (vtysh_write_memory,
       vtysh_write_cmd,
       "write",
       "Write running configuration to memory, network, or terminal\n")

ALIAS (vtysh_write_terminal,
       vtysh_show_running_config_cmd,
       "show running-config",
       SHOW_STR
       "Current operating configuration\n")

DEFUN (vtysh_terminal_length,
       vtysh_terminal_length_cmd,
       "terminal length <0-512>",
       "Set terminal line parameters\n"
       "Set number of lines on a screen\n"
       "Number of lines on screen (0 for no pausing)\n")
{
  int lines;
  char *endptr = NULL;
  char default_pager[10];

  lines = strtol (argv[0], &endptr, 10);
  if (lines < 0 || lines > 512 || *endptr != '\0')
    {
      vty_out (vty, "length is malformed%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (vtysh_pager_name)
    {
      free (vtysh_pager_name);
      vtysh_pager_name = NULL;
    }

  if (lines != 0)
    {
      snprintf(default_pager, 10, "more -%i", lines);
      vtysh_pager_name = strdup (default_pager);
    }

  return CMD_SUCCESS;
}

DEFUN (vtysh_terminal_no_length,
       vtysh_terminal_no_length_cmd,
       "terminal no length",
       "Set terminal line parameters\n"
       NO_STR
       "Set number of lines on a screen\n")
{
  if (vtysh_pager_name)
    {
      free (vtysh_pager_name);
      vtysh_pager_name = NULL;
    }

  vtysh_pager_init();
  return CMD_SUCCESS;
}

DEFUN (vtysh_show_daemons,
       vtysh_show_daemons_cmd,
       "show daemons",
       SHOW_STR
       "Show list of running daemons\n")
{
  u_int i;

  for (i = 0; i < array_size(vtysh_client); i++)
    if ( vtysh_client[i].fd >= 0 )
      vty_out(vty, " %s", vtysh_client[i].name);
  vty_out(vty, "%s", VTY_NEWLINE);

  return CMD_SUCCESS;
}

/* Execute command in child process. */
static int
execute_command (const char *command, int argc, const char *arg1,
		 const char *arg2)
{
  int ret;
  pid_t pid;
  int status;

  /* Call fork(). */
  pid = fork ();

  if (pid < 0)
    {
      /* Failure of fork(). */
      fprintf (stderr, "Can't fork: %s\n", safe_strerror (errno));
      exit (1);
    }
  else if (pid == 0)
    {
      /* This is child process. */
      switch (argc)
	{
	case 0:
	  ret = execlp (command, command, (const char *)NULL);
	  break;
	case 1:
	  ret = execlp (command, command, arg1, (const char *)NULL);
	  break;
	case 2:
	  ret = execlp (command, command, arg1, arg2, (const char *)NULL);
	  break;
	}

      /* When execlp suceed, this part is not executed. */
      fprintf (stderr, "Can't execute %s: %s\n", command, safe_strerror (errno));
      exit (1);
    }
  else
    {
      /* This is parent. */
      execute_flag = 1;
      ret = wait4 (pid, &status, 0, NULL);
      execute_flag = 0;
    }
  return 0;
}

DEFUN (vtysh_ping,
       vtysh_ping_cmd,
       "ping WORD",
       "Send echo messages\n"
       "Ping destination address or hostname\n")
{
  execute_command ("ping", 1, argv[0], NULL);
  return CMD_SUCCESS;
}

ALIAS (vtysh_ping,
       vtysh_ping_ip_cmd,
       "ping ip WORD",
       "Send echo messages\n"
       "IP echo\n"
       "Ping destination address or hostname\n")

DEFUN (vtysh_traceroute,
       vtysh_traceroute_cmd,
       "traceroute WORD",
       "Trace route to destination\n"
       "Trace route to destination address or hostname\n")
{
  execute_command ("traceroute", 1, argv[0], NULL);
  return CMD_SUCCESS;
}

ALIAS (vtysh_traceroute,
       vtysh_traceroute_ip_cmd,
       "traceroute ip WORD",
       "Trace route to destination\n"
       "IP trace\n"
       "Trace route to destination address or hostname\n")

#ifdef HAVE_IPV6
DEFUN (vtysh_ping6,
       vtysh_ping6_cmd,
       "ping ipv6 WORD",
       "Send echo messages\n"
       "IPv6 echo\n"
       "Ping destination address or hostname\n")
{
  execute_command ("ping6", 1, argv[0], NULL);
  return CMD_SUCCESS;
}

DEFUN (vtysh_traceroute6,
       vtysh_traceroute6_cmd,
       "traceroute ipv6 WORD",
       "Trace route to destination\n"
       "IPv6 trace\n"
       "Trace route to destination address or hostname\n")
{
  execute_command ("traceroute6", 1, argv[0], NULL);
  return CMD_SUCCESS;
}
#endif

DEFUN (vtysh_telnet,
       vtysh_telnet_cmd,
       "telnet WORD",
       "Open a telnet connection\n"
       "IP address or hostname of a remote system\n")
{
  execute_command ("telnet", 1, argv[0], NULL);
  return CMD_SUCCESS;
}

DEFUN (vtysh_telnet_port,
       vtysh_telnet_port_cmd,
       "telnet WORD PORT",
       "Open a telnet connection\n"
       "IP address or hostname of a remote system\n"
       "TCP Port number\n")
{
  execute_command ("telnet", 2, argv[0], argv[1]);
  return CMD_SUCCESS;
}

DEFUN (vtysh_ssh,
       vtysh_ssh_cmd,
       "ssh WORD",
       "Open an ssh connection\n"
       "[user@]host\n")
{
  execute_command ("ssh", 1, argv[0], NULL);
  return CMD_SUCCESS;
}

DEFUN (vtysh_start_shell,
       vtysh_start_shell_cmd,
       "start-shell",
       "Start UNIX shell\n")
{
  execute_command ("sh", 0, NULL, NULL);
  return CMD_SUCCESS;
}

DEFUN (vtysh_start_bash,
       vtysh_start_bash_cmd,
       "start-shell bash",
       "Start UNIX shell\n"
       "Start bash\n")
{
  execute_command ("bash", 0, NULL, NULL);
  return CMD_SUCCESS;
}

DEFUN (vtysh_start_zsh,
       vtysh_start_zsh_cmd,
       "start-shell zsh",
       "Start UNIX shell\n"
       "Start Z shell\n")
{
  execute_command ("zsh", 0, NULL, NULL);
  return CMD_SUCCESS;
}

static void
vtysh_install_default (enum node_type node)
{
  install_element (node, &config_list_cmd);
}

/* Making connection to protocol daemon. */
static int
vtysh_connect (struct vtysh_client *vclient)
{
  int ret;
  int sock, len;
  struct sockaddr_un addr;
  struct stat s_stat;

  /* Stat socket to see if we have permission to access it. */
  ret = stat (vclient->path, &s_stat);
  if (ret < 0 && errno != ENOENT)
    {
      fprintf  (stderr, "vtysh_connect(%s): stat = %s\n", 
		vclient->path, safe_strerror(errno)); 
      exit(1);
    }
  
  if (ret >= 0)
    {
      if (! S_ISSOCK(s_stat.st_mode))
	{
	  fprintf (stderr, "vtysh_connect(%s): Not a socket\n",
		   vclient->path);
	  exit (1);
	}
      
    }

  sock = socket (AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
    {
#ifdef DEBUG
      fprintf(stderr, "vtysh_connect(%s): socket = %s\n", vclient->path,
	      safe_strerror(errno));
#endif /* DEBUG */
      return -1;
    }

  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, vclient->path, strlen (vclient->path));
#ifdef HAVE_STRUCT_SOCKADDR_UN_SUN_LEN
  len = addr.sun_len = SUN_LEN(&addr);
#else
  len = sizeof (addr.sun_family) + strlen (addr.sun_path);
#endif /* HAVE_STRUCT_SOCKADDR_UN_SUN_LEN */

  ret = connect (sock, (struct sockaddr *) &addr, len);
  if (ret < 0)
    {
#ifdef DEBUG
      fprintf(stderr, "vtysh_connect(%s): connect = %s\n", vclient->path,
	      safe_strerror(errno));
#endif /* DEBUG */
      close (sock);
      return -1;
    }
  vclient->fd = sock;

  return 0;
}

int
vtysh_connect_all(const char *daemon_name)
{
  u_int i;
  int rc = 0;
  int matches = 0;

  for (i = 0; i < array_size(vtysh_client); i++)
    {
      if (!daemon_name || !strcmp(daemon_name, vtysh_client[i].name))
	{
	  matches++;
	  if (vtysh_connect(&vtysh_client[i]) == 0)
	    rc++;
	  /* We need direct access to ripd in vtysh_exit_ripd_only. */
	  if (vtysh_client[i].flag == VTYSH_RIPD)
	    ripd_client = &vtysh_client[i];
	}
    }
  if (!matches)
    fprintf(stderr, "Error: no daemons match name %s!\n", daemon_name);
  return rc;
}

/* To disable readline's filename completion. */
static char *
vtysh_completion_entry_function (const char *ignore, int invoking_key)
{
  return NULL;
}

void
vtysh_readline_init (void)
{
  /* readline related settings. */
  rl_bind_key ('?', (rl_command_func_t *) vtysh_rl_describe);
  rl_completion_entry_function = vtysh_completion_entry_function;
  rl_attempted_completion_function = (rl_completion_func_t *)new_completion;
}

char *
vtysh_prompt (void)
{
  static struct utsname names;
  static char buf[100];
  const char*hostname;
  extern struct host host;

  hostname = host.name;

  if (!hostname)
    {
      if (!names.nodename[0])
	uname (&names);
      hostname = names.nodename;
    }

  snprintf (buf, sizeof buf, cmd_prompt (vty->node), hostname);

  return buf;
}

void
vtysh_init_vty (void)
{
  /* Make vty structure. */
  vty = vty_new ();
  vty->type = VTY_SHELL;
  vty->node = VIEW_NODE;

  /* Initialize commands. */
  cmd_init (0);

  /* Install nodes. */
  install_node (&bgp_node, NULL);
  install_node (&rip_node, NULL);
  install_node (&interface_node, NULL);
  install_node (&rmap_node, NULL);
  install_node (&zebra_node, NULL);
  install_node (&bgp_vpnv4_node, NULL);
  install_node (&bgp_ipv4_node, NULL);
  install_node (&bgp_ipv4m_node, NULL);
/* #ifdef HAVE_IPV6 */
  install_node (&bgp_ipv6_node, NULL);
  install_node (&bgp_ipv6m_node, NULL);
/* #endif */
  install_node (&ospf_node, NULL);
/* #ifdef HAVE_IPV6 */
  install_node (&ripng_node, NULL);
  install_node (&ospf6_node, NULL);
/* #endif */
  install_node (&babel_node, NULL);
  install_node (&keychain_node, NULL);
  install_node (&keychain_key_node, NULL);
  install_node (&isis_node, NULL);
  install_node (&vty_node, NULL);

  vtysh_install_default (VIEW_NODE);
  vtysh_install_default (ENABLE_NODE);
  vtysh_install_default (CONFIG_NODE);
  vtysh_install_default (BGP_NODE);
  vtysh_install_default (RIP_NODE);
  vtysh_install_default (INTERFACE_NODE);
  vtysh_install_default (RMAP_NODE);
  vtysh_install_default (ZEBRA_NODE);
  vtysh_install_default (BGP_VPNV4_NODE);
  vtysh_install_default (BGP_IPV4_NODE);
  vtysh_install_default (BGP_IPV4M_NODE);
  vtysh_install_default (BGP_IPV6_NODE);
  vtysh_install_default (BGP_IPV6M_NODE);
  vtysh_install_default (OSPF_NODE);
  vtysh_install_default (RIPNG_NODE);
  vtysh_install_default (OSPF6_NODE);
  vtysh_install_default (BABEL_NODE);
  vtysh_install_default (ISIS_NODE);
  vtysh_install_default (KEYCHAIN_NODE);
  vtysh_install_default (KEYCHAIN_KEY_NODE);
  vtysh_install_default (VTY_NODE);

  install_element (VIEW_NODE, &vtysh_enable_cmd);
  install_element (ENABLE_NODE, &vtysh_config_terminal_cmd);
  install_element (ENABLE_NODE, &vtysh_disable_cmd);

  /* "exit" command. */
  install_element (VIEW_NODE, &vtysh_exit_all_cmd);
  install_element (VIEW_NODE, &vtysh_quit_all_cmd);
  install_element (CONFIG_NODE, &vtysh_exit_all_cmd);
  /* install_element (CONFIG_NODE, &vtysh_quit_all_cmd); */
  install_element (ENABLE_NODE, &vtysh_exit_all_cmd);
  install_element (ENABLE_NODE, &vtysh_quit_all_cmd);
  install_element (RIP_NODE, &vtysh_exit_ripd_cmd);
  install_element (RIP_NODE, &vtysh_quit_ripd_cmd);
  install_element (RIPNG_NODE, &vtysh_exit_ripngd_cmd);
  install_element (RIPNG_NODE, &vtysh_quit_ripngd_cmd);
  install_element (OSPF_NODE, &vtysh_exit_ospfd_cmd);
  install_element (OSPF_NODE, &vtysh_quit_ospfd_cmd);
  install_element (OSPF6_NODE, &vtysh_exit_ospf6d_cmd);
  install_element (OSPF6_NODE, &vtysh_quit_ospf6d_cmd);
  install_element (BGP_NODE, &vtysh_exit_bgpd_cmd);
  install_element (BGP_NODE, &vtysh_quit_bgpd_cmd);
  install_element (BGP_VPNV4_NODE, &vtysh_exit_bgpd_cmd);
  install_element (BGP_VPNV4_NODE, &vtysh_quit_bgpd_cmd);
  install_element (BGP_IPV4_NODE, &vtysh_exit_bgpd_cmd);
  install_element (BGP_IPV4_NODE, &vtysh_quit_bgpd_cmd);
  install_element (BGP_IPV4M_NODE, &vtysh_exit_bgpd_cmd);
  install_element (BGP_IPV4M_NODE, &vtysh_quit_bgpd_cmd);
  install_element (BGP_IPV6_NODE, &vtysh_exit_bgpd_cmd);
  install_element (BGP_IPV6_NODE, &vtysh_quit_bgpd_cmd);
  install_element (BGP_IPV6M_NODE, &vtysh_exit_bgpd_cmd);
  install_element (BGP_IPV6M_NODE, &vtysh_quit_bgpd_cmd);
  install_element (ISIS_NODE, &vtysh_exit_isisd_cmd);
  install_element (ISIS_NODE, &vtysh_quit_isisd_cmd);
  install_element (KEYCHAIN_NODE, &vtysh_exit_ripd_cmd);
  install_element (KEYCHAIN_NODE, &vtysh_quit_ripd_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_exit_ripd_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_quit_ripd_cmd);
  install_element (RMAP_NODE, &vtysh_exit_rmap_cmd);
  install_element (RMAP_NODE, &vtysh_quit_rmap_cmd);
  install_element (VTY_NODE, &vtysh_exit_line_vty_cmd);
  install_element (VTY_NODE, &vtysh_quit_line_vty_cmd);

  /* "end" command. */
  install_element (CONFIG_NODE, &vtysh_end_all_cmd);
  install_element (ENABLE_NODE, &vtysh_end_all_cmd);
  install_element (RIP_NODE, &vtysh_end_all_cmd);
  install_element (RIPNG_NODE, &vtysh_end_all_cmd);
  install_element (OSPF_NODE, &vtysh_end_all_cmd);
  install_element (OSPF6_NODE, &vtysh_end_all_cmd);
  install_element (BABEL_NODE, &vtysh_end_all_cmd);
  install_element (BGP_NODE, &vtysh_end_all_cmd);
  install_element (BGP_IPV4_NODE, &vtysh_end_all_cmd);
  install_element (BGP_IPV4M_NODE, &vtysh_end_all_cmd);
  install_element (BGP_VPNV4_NODE, &vtysh_end_all_cmd);
  install_element (BGP_IPV6_NODE, &vtysh_end_all_cmd);
  install_element (BGP_IPV6M_NODE, &vtysh_end_all_cmd);
  install_element (ISIS_NODE, &vtysh_end_all_cmd);
  install_element (KEYCHAIN_NODE, &vtysh_end_all_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_end_all_cmd);
  install_element (RMAP_NODE, &vtysh_end_all_cmd);
  install_element (VTY_NODE, &vtysh_end_all_cmd);

  install_element (INTERFACE_NODE, &interface_desc_cmd);
  install_element (INTERFACE_NODE, &no_interface_desc_cmd);
  install_element (INTERFACE_NODE, &vtysh_end_all_cmd);
  install_element (INTERFACE_NODE, &vtysh_exit_interface_cmd);
  install_element (INTERFACE_NODE, &vtysh_quit_interface_cmd);
  install_element (CONFIG_NODE, &router_rip_cmd);
#ifdef HAVE_IPV6
  install_element (CONFIG_NODE, &router_ripng_cmd);
#endif
  install_element (CONFIG_NODE, &router_ospf_cmd);
#ifdef HAVE_IPV6
  install_element (CONFIG_NODE, &router_ospf6_cmd);
#endif
  install_element (CONFIG_NODE, &router_babel_cmd);
  install_element (CONFIG_NODE, &router_isis_cmd);
  install_element (CONFIG_NODE, &router_bgp_cmd);
  install_element (CONFIG_NODE, &router_bgp_view_cmd);
  install_element (BGP_NODE, &address_family_vpnv4_cmd);
  install_element (BGP_NODE, &address_family_vpnv4_unicast_cmd);
  install_element (BGP_NODE, &address_family_ipv4_unicast_cmd);
  install_element (BGP_NODE, &address_family_ipv4_multicast_cmd);
#ifdef HAVE_IPV6
  install_element (BGP_NODE, &address_family_ipv6_cmd);
  install_element (BGP_NODE, &address_family_ipv6_unicast_cmd);
#endif
  install_element (BGP_VPNV4_NODE, &exit_address_family_cmd);
  install_element (BGP_IPV4_NODE, &exit_address_family_cmd);
  install_element (BGP_IPV4M_NODE, &exit_address_family_cmd);
  install_element (BGP_IPV6_NODE, &exit_address_family_cmd);
  install_element (BGP_IPV6M_NODE, &exit_address_family_cmd);
  install_element (CONFIG_NODE, &key_chain_cmd);
  install_element (CONFIG_NODE, &route_map_cmd);
  install_element (CONFIG_NODE, &vtysh_line_vty_cmd);
  install_element (KEYCHAIN_NODE, &key_cmd);
  install_element (KEYCHAIN_NODE, &key_chain_cmd);
  install_element (KEYCHAIN_KEY_NODE, &key_chain_cmd);
  install_element (CONFIG_NODE, &vtysh_interface_cmd);
  install_element (CONFIG_NODE, &vtysh_no_interface_cmd);
  install_element (ENABLE_NODE, &vtysh_show_running_config_cmd);
  install_element (ENABLE_NODE, &vtysh_copy_runningconfig_startupconfig_cmd);
  install_element (ENABLE_NODE, &vtysh_write_file_cmd);
  install_element (ENABLE_NODE, &vtysh_write_cmd);

  /* "write terminal" command. */
  install_element (ENABLE_NODE, &vtysh_write_terminal_cmd);
 
  install_element (CONFIG_NODE, &vtysh_integrated_config_cmd);
  install_element (CONFIG_NODE, &no_vtysh_integrated_config_cmd);

  /* "write memory" command. */
  install_element (ENABLE_NODE, &vtysh_write_memory_cmd);

  install_element (VIEW_NODE, &vtysh_terminal_length_cmd);
  install_element (ENABLE_NODE, &vtysh_terminal_length_cmd);
  install_element (VIEW_NODE, &vtysh_terminal_no_length_cmd);
  install_element (ENABLE_NODE, &vtysh_terminal_no_length_cmd);
  install_element (VIEW_NODE, &vtysh_show_daemons_cmd);
  install_element (ENABLE_NODE, &vtysh_show_daemons_cmd);

  install_element (VIEW_NODE, &vtysh_ping_cmd);
  install_element (VIEW_NODE, &vtysh_ping_ip_cmd);
  install_element (VIEW_NODE, &vtysh_traceroute_cmd);
  install_element (VIEW_NODE, &vtysh_traceroute_ip_cmd);
#ifdef HAVE_IPV6
  install_element (VIEW_NODE, &vtysh_ping6_cmd);
  install_element (VIEW_NODE, &vtysh_traceroute6_cmd);
#endif
  install_element (VIEW_NODE, &vtysh_telnet_cmd);
  install_element (VIEW_NODE, &vtysh_telnet_port_cmd);
  install_element (VIEW_NODE, &vtysh_ssh_cmd);
  install_element (ENABLE_NODE, &vtysh_ping_cmd);
  install_element (ENABLE_NODE, &vtysh_ping_ip_cmd);
  install_element (ENABLE_NODE, &vtysh_traceroute_cmd);
  install_element (ENABLE_NODE, &vtysh_traceroute_ip_cmd);
#ifdef HAVE_IPV6
  install_element (ENABLE_NODE, &vtysh_ping6_cmd);
  install_element (ENABLE_NODE, &vtysh_traceroute6_cmd);
#endif
  install_element (ENABLE_NODE, &vtysh_telnet_cmd);
  install_element (ENABLE_NODE, &vtysh_telnet_port_cmd);
  install_element (ENABLE_NODE, &vtysh_ssh_cmd);
  install_element (ENABLE_NODE, &vtysh_start_shell_cmd);
  install_element (ENABLE_NODE, &vtysh_start_bash_cmd);
  install_element (ENABLE_NODE, &vtysh_start_zsh_cmd);
  
  install_element (VIEW_NODE, &vtysh_show_memory_cmd);
  install_element (ENABLE_NODE, &vtysh_show_memory_cmd);

  /* Logging */
  install_element (ENABLE_NODE, &vtysh_show_logging_cmd);
  install_element (VIEW_NODE, &vtysh_show_logging_cmd);
  install_element (CONFIG_NODE, &vtysh_log_stdout_cmd);
  install_element (CONFIG_NODE, &vtysh_log_stdout_level_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_stdout_cmd);
  install_element (CONFIG_NODE, &vtysh_log_file_cmd);
  install_element (CONFIG_NODE, &vtysh_log_file_level_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_file_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_file_level_cmd);
  install_element (CONFIG_NODE, &vtysh_log_monitor_cmd);
  install_element (CONFIG_NODE, &vtysh_log_monitor_level_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_monitor_cmd);
  install_element (CONFIG_NODE, &vtysh_log_syslog_cmd);
  install_element (CONFIG_NODE, &vtysh_log_syslog_level_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_syslog_cmd);
  install_element (CONFIG_NODE, &vtysh_log_trap_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_trap_cmd);
  install_element (CONFIG_NODE, &vtysh_log_facility_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_facility_cmd);
  install_element (CONFIG_NODE, &vtysh_log_record_priority_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_record_priority_cmd);
  install_element (CONFIG_NODE, &vtysh_log_timestamp_precision_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_timestamp_precision_cmd);

  install_element (CONFIG_NODE, &vtysh_service_password_encrypt_cmd);
  install_element (CONFIG_NODE, &no_vtysh_service_password_encrypt_cmd);

  install_element (CONFIG_NODE, &vtysh_password_cmd);
  install_element (CONFIG_NODE, &vtysh_password_text_cmd);
  install_element (CONFIG_NODE, &vtysh_enable_password_cmd);
  install_element (CONFIG_NODE, &vtysh_enable_password_text_cmd);
  install_element (CONFIG_NODE, &no_vtysh_enable_password_cmd);

}
