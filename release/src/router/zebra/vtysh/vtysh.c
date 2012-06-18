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

/* Struct VTY. */
struct vty *vty;

/* VTY shell pager name. */
char *vtysh_pager_name = NULL;

/* VTY shell client structure. */
struct vtysh_client
{
  int fd;
} vtysh_client[VTYSH_INDEX_MAX];

/* When '^Z' is received from vty, move down to the enable mode. */
int
vtysh_end ()
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
	 "End current mode and down to previous mode\n")
{
  return vtysh_end (vty);
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_stdout,
	 vtysh_log_stdout_cmd,
	 "log stdout",
	 "Logging control\n"
	 "Logging goes to stdout\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_stdout,
	 no_vtysh_log_stdout_cmd,
	 "no log stdout",
	 NO_STR
	 "Logging control\n"
	 "Logging goes to stdout\n")
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

DEFUNSH (VTYSH_ALL,
	 vtysh_log_syslog,
	 vtysh_log_syslog_cmd,
	 "log syslog",
	 "Logging control\n"
	 "Logging goes to syslog\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_syslog,
	 no_vtysh_log_syslog_cmd,
	 "no log syslog",
	 NO_STR
	 "Logging control\n"
	 "Cancel logging to syslog\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 vtysh_log_trap,
	 vtysh_log_trap_cmd,
	 "log trap (emergencies|alerts|critical|errors|warnings|notifications|informational|debugging)",
	 "Logging control\n"
	 "Limit logging to specifed level\n")
{
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ALL,
	 no_vtysh_log_trap,
	 no_vtysh_log_trap_cmd,
	 "no log trap",
	 NO_STR
	 "Logging control\n"
	 "Permit all logging information\n")
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

void
vclient_close (struct vtysh_client *vclient)
{
  if (vclient->fd > 0)
    close (vclient->fd);
  vclient->fd = -1;
}


/* Following filled with debug code to trace a problematic condition
   under load - it SHOULD handle it.
*/
#define ERR_WHERE_STRING "vtysh(): vtysh_client_config(): "
int
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
	
  /* Allow enough room for buffer to read more than a few pages from socket
   */
  bufsz = 5 * sysconf(_SC_PAGESIZE) + 1;
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
	 reset read position */
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

  /* parse anything left in the buffer */
  vtysh_config_parse (buf);

  XFREE(MTYPE_TMP, buf);
  return ret;
}

int
vtysh_client_execute (struct vtysh_client *vclient, char *line, FILE *fp)
{
  int ret;
  char buf[1001];
  int nbytes;
  int i;

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
	  buf[nbytes] = '\0';
	  fprintf (fp, "%s", buf);
	  fflush (fp);

	  if (nbytes >= 4)
	    {
	      i = nbytes - 4;
	      if (buf[i] == '\0' && buf[i + 1] == '\0' && buf[i + 2] == '\0')
		{
		  ret = buf[i + 3];
		  break;
		}
	    }
	}
    }
  return ret;
}

void
vtysh_exit_ripd_only ()
{
  vtysh_client_execute (&vtysh_client[VTYSH_INDEX_RIP], "exit", stdout);
}


void
vtysh_pager_init ()
{
  vtysh_pager_name = getenv ("VTYSH_PAGER");
  if (! vtysh_pager_name)
    vtysh_pager_name = "more";
}

/* Command execution over the vty interface. */
void
vtysh_execute_func (char *line, int pager)
{
  int ret, cmd_stat;
  vector vline;
  struct cmd_element *cmd;
  FILE *fp = NULL;

  /* Split readline string up into the vector */
  vline = cmd_make_strvec (line);

  if (vline == NULL)
    return;

  ret = cmd_execute_command (vline, vty, &cmd);

  cmd_free_strvec (vline);

  switch (ret)
    {
    case CMD_WARNING:
      if (vty->type == VTY_FILE)
	printf ("Warning...\n");
      break;
    case CMD_ERR_AMBIGUOUS:
      printf ("%% Ambiguous command.\n");
      break;
    case CMD_ERR_NO_MATCH:
      printf ("%% Unknown command.\n");
      break;
    case CMD_ERR_INCOMPLETE:
      printf ("%% Command incomplete.\n");
      break;
    case CMD_SUCCESS_DAEMON:
      {
	if (pager && vtysh_pager_name)
	  {
	    fp = popen ("more", "w");
	    if (fp == NULL)
	      {
		perror ("popen");
		exit (1);
	      }
	  }
	else
	  fp = stdout;

	if (! strcmp(cmd->string,"configure terminal"))
	  {
	    cmd_stat = vtysh_client_execute (&vtysh_client[VTYSH_INDEX_ZEBRA],
					     line, fp);
	    if (cmd_stat != CMD_WARNING)
	      cmd_stat = vtysh_client_execute (&vtysh_client[VTYSH_INDEX_RIP],
					       line, fp);
	    if (cmd_stat != CMD_WARNING)
	      cmd_stat = vtysh_client_execute (&vtysh_client[VTYSH_INDEX_RIPNG], line, fp);
	    if (cmd_stat != CMD_WARNING)
	      cmd_stat = vtysh_client_execute (&vtysh_client[VTYSH_INDEX_OSPF],
					       line, fp);
	    if (cmd_stat != CMD_WARNING)
	      cmd_stat = vtysh_client_execute (&vtysh_client[VTYSH_INDEX_OSPF6], line, fp);
	    if (cmd_stat != CMD_WARNING)
	      cmd_stat = vtysh_client_execute (&vtysh_client[VTYSH_INDEX_BGP],
					       line, fp);
	    if (cmd_stat)
	      {
                line = "end";
                vline = cmd_make_strvec (line);

                if (vline == NULL)
		  {
		    if (pager && vtysh_pager_name && fp)
		      {
			if (pclose (fp) == -1)
			  {
			    perror ("pclose");
			    exit (1);
			  }
			fp = NULL;
		      }
		    return;
		  }

                ret = cmd_execute_command (vline, vty, &cmd);
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

	if (cmd->daemon & VTYSH_ZEBRA)
	  if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_ZEBRA], line, fp)
	      != CMD_SUCCESS)
	    break;
	if (cmd->daemon & VTYSH_RIPD)
	  if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_RIP], line, fp)
	      != CMD_SUCCESS)
	    break;
	if (cmd->daemon & VTYSH_RIPNGD)
	  if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_RIPNG], line, fp)
	      != CMD_SUCCESS)
	    break;
	if (cmd->daemon & VTYSH_OSPFD)
	  if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_OSPF], line, fp)
	      != CMD_SUCCESS)
	    break;
	if (cmd->daemon & VTYSH_OSPF6D)
	  if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_OSPF6], line, fp)
	      != CMD_SUCCESS)
	    break;
	if (cmd->daemon & VTYSH_BGPD)
	  if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_BGP], line, fp)
	      != CMD_SUCCESS)
	    break;
	if (cmd->func)
	  (*cmd->func) (cmd, vty, 0, NULL);
      }
    }
  if (pager && vtysh_pager_name && fp)
    {
      if (pclose (fp) == -1)
	{
	  perror ("pclose");
	  exit (1);
	}
      fp = NULL;
    }
}

void
vtysh_execute_no_pager (char *line)
{
  vtysh_execute_func (line, 0);
}

void
vtysh_execute (char *line)
{
  vtysh_execute_func (line, 1);
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

      /* In case of comment line */
      if (vline == NULL)
	continue;

      /* Execute configuration command : this is strict match */
      ret = cmd_execute_command_strict (vline, vty, &cmd);

      /* Try again with setting node to CONFIG_NODE */
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
	    printf ("Warning...\n");
	  break;
	case CMD_ERR_AMBIGUOUS:
	  printf ("%% Ambiguous command.\n");
	  break;
	case CMD_ERR_NO_MATCH:
	  printf ("%% Unknown command: %s", vty->buf);
	  break;
	case CMD_ERR_INCOMPLETE:
	  printf ("%% Command incomplete.\n");
	  break;
	case CMD_SUCCESS_DAEMON:
	  {
	    if (cmd->daemon & VTYSH_ZEBRA)
	      if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_ZEBRA],
					vty->buf, stdout) != CMD_SUCCESS)
		break;
	    if (cmd->daemon & VTYSH_RIPD)
	      if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_RIP],
					vty->buf, stdout) != CMD_SUCCESS)
		break;
	    if (cmd->daemon & VTYSH_RIPNGD)
	      if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_RIPNG],
					vty->buf, stdout) != CMD_SUCCESS)
		break;
	    if (cmd->daemon & VTYSH_OSPFD)
	      if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_OSPF],
					vty->buf, stdout) != CMD_SUCCESS)
		break;
	    if (cmd->daemon & VTYSH_OSPF6D)
	      if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_OSPF6],
					vty->buf, stdout) != CMD_SUCCESS)
		break;
	    if (cmd->daemon & VTYSH_BGPD)
	      if (vtysh_client_execute (&vtysh_client[VTYSH_INDEX_BGP],
					vty->buf, stdout) != CMD_SUCCESS)
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
vtysh_rl_describe ()
{
  int ret;
  int i;
  vector vline;
  vector describe;
  int width;
  struct desc *desc;

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

  printf ("\n");

  /* Ambiguous and no match error. */
  switch (ret)
    {
    case CMD_ERR_AMBIGUOUS:
      cmd_free_strvec (vline);
      printf ("%% Ambiguous command.\n");
      rl_on_new_line ();
      return 0;
      break;
    case CMD_ERR_NO_MATCH:
      cmd_free_strvec (vline);
      printf ("%% There is no matched command.\n");
      rl_on_new_line ();
      return 0;
      break;
    }  

  /* Get width of command string. */
  width = 0;
  for (i = 0; i < vector_max (describe); i++)
    if ((desc = vector_slot (describe, i)) != NULL)
      {
	int len;

	if (desc->cmd[0] == '\0')
	  continue;

	len = strlen (desc->cmd);
	if (desc->cmd[0] == '.')
	  len--;

	if (width < len)
	  width = len;
      }

  for (i = 0; i < vector_max (describe); i++)
    if ((desc = vector_slot (describe, i)) != NULL)
      {
	if (desc->cmd[0] == '\0')
	  continue;

	if (! desc->str)
	  printf ("  %-s\n",
		  desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd);
	else
	  printf ("  %-*s  %s\n",
		  width,
		  desc->cmd[0] == '.' ? desc->cmd + 1 : desc->cmd,
		  desc->str);
      }

  cmd_free_strvec (vline);
  vector_free (describe);

  rl_on_new_line();

  return 0;
}

/* result of cmd_complete_command() call will be stored here
   and used in new_completion() in order to put the space in
   correct places only */
int complete_status;

char *
command_generator (char *text, int state)
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

char **
new_completion (char *text, int start, int end)
{
  char **matches;

  matches = completion_matches (text, command_generator);

  if (matches)
    {
      rl_point = rl_end;
      if (complete_status == CMD_COMPLETE_FULL_MATCH)
	rl_pending_input = ' ';
    }

  return matches;
}

char **
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

/* BGP node structure. */
struct cmd_node bgp_node =
{
  BGP_NODE,
  "%s(config-router)# ",
};

/* BGP node structure. */
struct cmd_node rip_node =
{
  RIP_NODE,
  "%s(config-router)# ",
};

struct cmd_node interface_node =
{
  INTERFACE_NODE,
  "%s(config-if)# ",
};

DEFUNSH (VTYSH_BGPD,
	 router_bgp,
	 router_bgp_cmd,
	 "router bgp <1-65535>",
	 ROUTER_STR
	 BGP_STR
	 AS_STR)
{
  vty->node = BGP_NODE;
  return CMD_SUCCESS;
}

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
	 address_family_ipv4,
	 address_family_ipv4_cmd,
	 "address-family ipv4",
	 "Enter Address Family command mode\n"
	 "Address family\n")
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

/* Enable command */
DEFUNSH (VTYSH_ALL,
	 vtysh_enable, 
	 vtysh_enable_cmd,
	 "enable",
	 "Turn on privileged mode command\n")
{
  vty->node = ENABLE_NODE;
  return CMD_SUCCESS;
}

/* Disable command */
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

/* Configration from terminal */
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

int
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
    case MASC_NODE:
    case RMAP_NODE:
    case VTY_NODE:
    case KEYCHAIN_NODE:
      vty->node = CONFIG_NODE;
      break;
    case BGP_VPNV4_NODE:
    case BGP_IPV4_NODE:
    case BGP_IPV4M_NODE:
    case BGP_IPV6_NODE:
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
       "Exit current mode and down to previous mode\n");

DEFUNSH (VTYSH_BGPD,
	 exit_address_family,
	 exit_address_family_cmd,
	 "exit-address-family",
	 "Exit from Address Family configuration mode\n")
{
  if (vty->node == BGP_IPV4_NODE
      || vty->node == BGP_IPV4M_NODE
      || vty->node == BGP_VPNV4_NODE
      || vty->node == BGP_IPV6_NODE)
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
       "Exit current mode and down to previous mode\n");

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
       "Exit current mode and down to previous mode\n");

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
       "Exit current mode and down to previous mode\n");

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
       "Exit current mode and down to previous mode\n");

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
       "Exit current mode and down to previous mode\n");

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
       "Exit current mode and down to previous mode\n");

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
       "Exit current mode and down to previous mode\n");

DEFUNSH (VTYSH_ZEBRA|VTYSH_RIPD|VTYSH_OSPFD|VTYSH_OSPF6D,
	 vtysh_interface,
	 vtysh_interface_cmd,
	 "interface IFNAME",
	 "Select an interface to configure\n"
	 "Interface's name\n")
{
  vty->node = INTERFACE_NODE;
  return CMD_SUCCESS;
}

DEFUNSH (VTYSH_ZEBRA|VTYSH_RIPD|VTYSH_OSPFD|VTYSH_OSPF6D,
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
       "Exit current mode and down to previous mode\n");

DEFUN (vtysh_write_terminal,
       vtysh_write_terminal_cmd,
       "write terminal",
       "Write running configuration to memory, network, or terminal\n"
       "Write to terminal\n")
{
  int ret;
  char line[] = "write terminal\n";
  FILE *fp = NULL;

  if (vtysh_pager_name)
    {
      fp = popen ("more", "w");
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

  vtysh_config_write (fp);

  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_ZEBRA], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_RIP], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_RIPNG], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_OSPF], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_OSPF6], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_BGP], line);

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

  return CMD_SUCCESS;
}

DEFUN (vtysh_write_memory,
       vtysh_write_memory_cmd,
       "write memory",
       "Write running configuration to memory, network, or terminal\n"
       "Write configuration to the file (same as write file)\n")
{
  int ret;
  mode_t old_umask;
  char line[] = "write terminal\n";
  FILE *fp;
  char *integrate_sav = NULL;

  /* config files have 0600 perms... */ 
  old_umask = umask (0077);

  integrate_sav = malloc (strlen (integrate_default) 
			    + strlen (CONF_BACKUP_EXT) + 1);
  strcpy (integrate_sav, integrate_default);
  strcat (integrate_sav, CONF_BACKUP_EXT);


  printf ("Building Configuration...\n");

  /* Move current configuration file to backup config file */
  unlink (integrate_sav);
  rename (integrate_default, integrate_sav);

  fp = fopen (integrate_default, "w");
  if (fp == NULL)
    {
      printf ("%% Can't open configuration file %s.\n", integrate_default);
      umask (old_umask);
      return CMD_SUCCESS;
    }
  else
    printf ("[OK]\n");
	  

  vtysh_config_write (fp);

  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_ZEBRA], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_RIP], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_RIPNG], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_OSPF], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_OSPF6], line);
  ret = vtysh_client_config (&vtysh_client[VTYSH_INDEX_BGP], line);

  vtysh_config_dump (fp);

  fclose (fp);

  umask (old_umask);
  return CMD_SUCCESS;
}

ALIAS (vtysh_write_memory,
       vtysh_copy_runningconfig_startupconfig_cmd,
       "copy running-config startup-config",  
       "Copy from one file to another\n"
       "Copy from current system configuration\n"
       "Copy to startup configuration\n");

ALIAS (vtysh_write_memory,
       vtysh_write_file_cmd,
       "write file",
       "Write running configuration to memory, network, or terminal\n"
       "Write configuration to the file (same as write memory)\n");

ALIAS (vtysh_write_terminal,
       vtysh_show_running_config_cmd,
       "show running-config",
       SHOW_STR
       "Current operating configuration\n");

/* Execute command in child process. */
int
execute_command (char *command, int argc, char *arg1, char *arg2)
{
  int ret;
  pid_t pid;
  int status;

  /* Call fork(). */
  pid = fork ();

  if (pid < 0)
    {
      /* Failure of fork(). */
      fprintf (stderr, "Can't fork: %s\n", strerror (errno));
      exit (1);
    }
  else if (pid == 0)
    {
      /* This is child process. */
      switch (argc)
	{
	case 0:
	  ret = execlp (command, command, NULL);
	  break;
	case 1:
	  ret = execlp (command, command, arg1, NULL);
	  break;
	case 2:
	  ret = execlp (command, command, arg1, arg2, NULL);
	  break;
	}

      /* When execlp suceed, this part is not executed. */
      fprintf (stderr, "Can't execute %s: %s\n", command, strerror (errno));
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
       "send echo messages\n"
       "Ping destination address or hostname\n")
{
  execute_command ("ping", 1, argv[0], NULL);
  return CMD_SUCCESS;
}

DEFUN (vtysh_traceroute,
       vtysh_traceroute_cmd,
       "traceroute WORD",
       "Trace route to destination\n"
       "Trace route to destination address or hostname\n")
{
  execute_command ("traceroute", 1, argv[0], NULL);
  return CMD_SUCCESS;
}

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

/* Route map node structure. */
struct cmd_node rmap_node =
{
  RMAP_NODE,
  "%s(config-route-map)# "
};

/* Zebra node structure. */
struct cmd_node zebra_node =
{
  ZEBRA_NODE,
  "%s(config-router)# "
};

struct cmd_node bgp_vpnv4_node =
{
  BGP_VPNV4_NODE,
  "%s(config-router-af)# "
};

struct cmd_node bgp_ipv4_node =
{
  BGP_IPV4_NODE,
  "%s(config-router-af)# "
};

struct cmd_node bgp_ipv4m_node =
{
  BGP_IPV4M_NODE,
  "%s(config-router-af)# "
};

struct cmd_node bgp_ipv6_node =
{
  BGP_IPV6_NODE,
  "%s(config-router-af)# "
};

struct cmd_node ospf_node =
{
  OSPF_NODE,
  "%s(config-router)# "
};

/* RIPng node structure. */
struct cmd_node ripng_node =
{
  RIPNG_NODE,
  "%s(config-router)# "
};

/* OSPF6 node structure. */
struct cmd_node ospf6_node =
{
  OSPF6_NODE,
  "%s(config-ospf6)# "
};

struct cmd_node keychain_node =
{
  KEYCHAIN_NODE,
  "%s(config-keychain)# "
};

struct cmd_node keychain_key_node =
{
  KEYCHAIN_KEY_NODE,
  "%s(config-keychain-key)# "
};

void
vtysh_install_default (enum node_type node)
{
  install_element (node, &config_list_cmd);
}

/* Making connection to protocol daemon. */
int
vtysh_connect (struct vtysh_client *vclient, char *path)
{
  int ret;
  int sock, len;
  struct sockaddr_un addr;
  struct stat s_stat;
  uid_t euid;
  gid_t egid;

  memset (vclient, 0, sizeof (struct vtysh_client));
  vclient->fd = -1;

  /* Stat socket to see if we have permission to access it. */
  euid = geteuid();
  egid = getegid();
  ret = stat (path, &s_stat);
  if (ret < 0 && errno != ENOENT)
    {
      fprintf  (stderr, "vtysh_connect(%s): stat = %s\n", 
		path, strerror(errno)); 
      exit(1);
    }
  
  if (ret >= 0)
    {
      if (! S_ISSOCK(s_stat.st_mode))
	{
	  fprintf (stderr, "vtysh_connect(%s): Not a socket\n",
		   path);
	  exit (1);
	}
      
      if (euid != s_stat.st_uid 
	  || !(s_stat.st_mode & S_IWUSR)
	  || !(s_stat.st_mode & S_IRUSR))
	{
	  fprintf (stderr, "vtysh_connect(%s): No permission to access socket\n",
		   path);
	  exit (1);
	}
    }

  sock = socket (AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0)
    {
#ifdef DEBUG
      fprintf(stderr, "vtysh_connect(%s): socket = %s\n", path, strerror(errno));
#endif /* DEBUG */
      return -1;
    }

  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, path, strlen (path));
#ifdef HAVE_SUN_LEN
  len = addr.sun_len = SUN_LEN(&addr);
#else
  len = sizeof (addr.sun_family) + strlen (addr.sun_path);
#endif /* HAVE_SUN_LEN */

  ret = connect (sock, (struct sockaddr *) &addr, len);
  if (ret < 0)
    {
#ifdef DEBUG
      fprintf(stderr, "vtysh_connect(%s): connect = %s\n", path, strerror(errno));
#endif /* DEBUG */
      close (sock);
      return -1;
    }
  vclient->fd = sock;

  return 0;
}

void
vtysh_connect_all()
{
  /* Clear each daemons client structure. */
  vtysh_connect (&vtysh_client[VTYSH_INDEX_ZEBRA], ZEBRA_PATH);
  vtysh_connect (&vtysh_client[VTYSH_INDEX_RIP], RIP_PATH);
  vtysh_connect (&vtysh_client[VTYSH_INDEX_RIPNG], RIPNG_PATH);
  vtysh_connect (&vtysh_client[VTYSH_INDEX_OSPF], OSPF_PATH);
  vtysh_connect (&vtysh_client[VTYSH_INDEX_OSPF6], OSPF6_PATH);
  vtysh_connect (&vtysh_client[VTYSH_INDEX_BGP], BGP_PATH);
}


/* To disable readline's filename completion */
int
vtysh_completion_entry_function (int ignore, int invoking_key)
{
  return 0;
}

void
vtysh_readline_init ()
{
  /* readline related settings. */
  rl_bind_key ('?', vtysh_rl_describe);
  rl_completion_entry_function = vtysh_completion_entry_function;
  rl_attempted_completion_function = (CPPFunction *)new_completion;
  /* do not append space after completion. It will be appended
     in new_completion() function explicitly */
  rl_completion_append_character = '\0';
}

char *
vtysh_prompt ()
{
  struct utsname names;
  static char buf[100];
  const char*hostname;
  extern struct host host;

  hostname = host.name;

  if (!hostname)
    {
      uname (&names);
      hostname = names.nodename;
    }

  snprintf (buf, sizeof buf, cmd_prompt (vty->node), hostname);

  return buf;
}

void
vtysh_init_vty ()
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
/* #endif */
  install_node (&ospf_node, NULL);
/* #ifdef HAVE_IPV6 */
  install_node (&ripng_node, NULL);
  install_node (&ospf6_node, NULL);
/* #endif */
  install_node (&keychain_node, NULL);
  install_node (&keychain_key_node, NULL);

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
  vtysh_install_default (OSPF_NODE);
  vtysh_install_default (RIPNG_NODE);
  vtysh_install_default (OSPF6_NODE);
  vtysh_install_default (KEYCHAIN_NODE);
  vtysh_install_default (KEYCHAIN_KEY_NODE);

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
  install_element (KEYCHAIN_NODE, &vtysh_exit_ripd_cmd);
  install_element (KEYCHAIN_NODE, &vtysh_quit_ripd_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_exit_ripd_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_quit_ripd_cmd);
  install_element (RMAP_NODE, &vtysh_exit_rmap_cmd);
  install_element (RMAP_NODE, &vtysh_quit_rmap_cmd);

  /* "end" command. */
  install_element (CONFIG_NODE, &vtysh_end_all_cmd);
  install_element (ENABLE_NODE, &vtysh_end_all_cmd);
  install_element (RIP_NODE, &vtysh_end_all_cmd);
  install_element (RIPNG_NODE, &vtysh_end_all_cmd);
  install_element (OSPF_NODE, &vtysh_end_all_cmd);
  install_element (OSPF6_NODE, &vtysh_end_all_cmd);
  install_element (BGP_NODE, &vtysh_end_all_cmd);
  install_element (BGP_IPV4_NODE, &vtysh_end_all_cmd);
  install_element (BGP_IPV4M_NODE, &vtysh_end_all_cmd);
  install_element (BGP_VPNV4_NODE, &vtysh_end_all_cmd);
  install_element (BGP_IPV6_NODE, &vtysh_end_all_cmd);
  install_element (KEYCHAIN_NODE, &vtysh_end_all_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_end_all_cmd);
  install_element (RMAP_NODE, &vtysh_end_all_cmd);

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
  install_element (CONFIG_NODE, &router_bgp_cmd);
  install_element (BGP_NODE, &address_family_vpnv4_cmd);
  install_element (BGP_NODE, &address_family_vpnv4_unicast_cmd);
  install_element (BGP_NODE, &address_family_ipv4_cmd);
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
  install_element (CONFIG_NODE, &key_chain_cmd);
  install_element (CONFIG_NODE, &route_map_cmd);
  install_element (KEYCHAIN_NODE, &key_cmd);
  install_element (KEYCHAIN_NODE, &key_chain_cmd);
  install_element (KEYCHAIN_KEY_NODE, &key_chain_cmd);
  install_element (CONFIG_NODE, &vtysh_interface_cmd);
  install_element (ENABLE_NODE, &vtysh_show_running_config_cmd);
  install_element (ENABLE_NODE, &vtysh_copy_runningconfig_startupconfig_cmd);
  install_element (ENABLE_NODE, &vtysh_write_file_cmd);

  /* write terminal command */
  install_element (ENABLE_NODE, &vtysh_write_terminal_cmd);
  install_element (CONFIG_NODE, &vtysh_write_terminal_cmd);
  install_element (BGP_NODE, &vtysh_write_terminal_cmd);
  install_element (BGP_VPNV4_NODE, &vtysh_write_terminal_cmd);
  install_element (BGP_IPV4_NODE, &vtysh_write_terminal_cmd);
  install_element (BGP_IPV4M_NODE, &vtysh_write_terminal_cmd);
  install_element (BGP_IPV6_NODE, &vtysh_write_terminal_cmd);
  install_element (RIP_NODE, &vtysh_write_terminal_cmd);
  install_element (RIPNG_NODE, &vtysh_write_terminal_cmd);
  install_element (OSPF_NODE, &vtysh_write_terminal_cmd);
  install_element (OSPF6_NODE, &vtysh_write_terminal_cmd);
  install_element (INTERFACE_NODE, &vtysh_write_terminal_cmd);
  install_element (RMAP_NODE, &vtysh_write_terminal_cmd);
  install_element (KEYCHAIN_NODE, &vtysh_write_terminal_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_write_terminal_cmd);

  /* write memory command */
  install_element (ENABLE_NODE, &vtysh_write_memory_cmd);
  install_element (CONFIG_NODE, &vtysh_write_memory_cmd);
  install_element (BGP_NODE, &vtysh_write_memory_cmd);
  install_element (BGP_VPNV4_NODE, &vtysh_write_memory_cmd);
  install_element (BGP_IPV4_NODE, &vtysh_write_memory_cmd);
  install_element (BGP_IPV4M_NODE, &vtysh_write_memory_cmd);
  install_element (BGP_IPV6_NODE, &vtysh_write_memory_cmd);
  install_element (RIP_NODE, &vtysh_write_memory_cmd);
  install_element (RIPNG_NODE, &vtysh_write_memory_cmd);
  install_element (OSPF_NODE, &vtysh_write_memory_cmd);
  install_element (OSPF6_NODE, &vtysh_write_memory_cmd);
  install_element (INTERFACE_NODE, &vtysh_write_memory_cmd);
  install_element (RMAP_NODE, &vtysh_write_memory_cmd);
  install_element (KEYCHAIN_NODE, &vtysh_write_memory_cmd);
  install_element (KEYCHAIN_KEY_NODE, &vtysh_write_memory_cmd);

  install_element (VIEW_NODE, &vtysh_ping_cmd);
  install_element (VIEW_NODE, &vtysh_traceroute_cmd);
  install_element (VIEW_NODE, &vtysh_telnet_cmd);
  install_element (VIEW_NODE, &vtysh_telnet_port_cmd);
  install_element (ENABLE_NODE, &vtysh_ping_cmd);
  install_element (ENABLE_NODE, &vtysh_traceroute_cmd);
  install_element (ENABLE_NODE, &vtysh_telnet_cmd);
  install_element (ENABLE_NODE, &vtysh_telnet_port_cmd);
  install_element (ENABLE_NODE, &vtysh_start_shell_cmd);
  install_element (ENABLE_NODE, &vtysh_start_bash_cmd);
  install_element (ENABLE_NODE, &vtysh_start_zsh_cmd);

  install_element (CONFIG_NODE, &vtysh_log_stdout_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_stdout_cmd);
  install_element (CONFIG_NODE, &vtysh_log_file_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_file_cmd);
  install_element (CONFIG_NODE, &vtysh_log_syslog_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_syslog_cmd);
  install_element (CONFIG_NODE, &vtysh_log_trap_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_trap_cmd);
  install_element (CONFIG_NODE, &vtysh_log_record_priority_cmd);
  install_element (CONFIG_NODE, &no_vtysh_log_record_priority_cmd);
}
