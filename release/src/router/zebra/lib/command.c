/* Command interpreter routine for virtual terminal [aka TeletYpe]
   Copyright (C) 1997, 98, 99 Kunihiro Ishiguro

This file is part of GNU Zebra.
 
GNU Zebra is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2, or (at your
option) any later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <zebra.h>

#include "command.h"
#include "memory.h"
#include "log.h"
#include "version.h"

/* Command vector which includes some level of command lists. Normally
   each daemon maintains each own cmdvec. */
vector cmdvec;

/* Host information structure. */
struct host host;

/* Default motd string. */
char *default_motd = 
"\r\n\
Hello, this is zebra (version " ZEBRA_VERSION ").\r\n\
Copyright 1996-2004 Kunihiro Ishiguro.\r\n\
\r\n";

/* Standard command node structures. */
struct cmd_node auth_node =
{
  AUTH_NODE,
  "Password: ",
};

struct cmd_node view_node =
{
  VIEW_NODE,
  "%s> ",
};

struct cmd_node auth_enable_node =
{
  AUTH_ENABLE_NODE,
  "Password: ",
};

struct cmd_node enable_node =
{
  ENABLE_NODE,
  "%s# ",
};

struct cmd_node config_node =
{
  CONFIG_NODE,
  "%s(config)# ",
  1
};

/* Utility function to concatenate argv argument into a single string
   with inserting ' ' character between each argument.  */
char *
argv_concat (char **argv, int argc, int shift)
{
  int i;
  int len;
  int index;
  char *str;

  str = NULL;
  index = 0;

  for (i = shift; i < argc; i++)
    {
      len = strlen (argv[i]);

      if (i == shift)
	{
	  str = XSTRDUP (MTYPE_TMP, argv[i]);
	  index = len;
	}
      else
	{
	  str = XREALLOC (MTYPE_TMP, str, (index + len + 2));
	  str[index++] = ' ';
	  memcpy (str + index, argv[i], len);
	  index += len;
	  str[index] = '\0';
	}
    }
  return str;
}

/* Install top node of command vector. */
void
install_node (struct cmd_node *node, 
	      int (*func) (struct vty *))
{
  vector_set_index (cmdvec, node->node, node);
  node->func = func;
  node->cmd_vector = vector_init (VECTOR_MIN_SIZE);
}

/* Compare two command's string.  Used in sort_node (). */
int
cmp_node (const void *p, const void *q)
{
  struct cmd_element *a = *(struct cmd_element **)p;
  struct cmd_element *b = *(struct cmd_element **)q;

  return strcmp (a->string, b->string);
}

int
cmp_desc (const void *p, const void *q)
{
  struct desc *a = *(struct desc **)p;
  struct desc *b = *(struct desc **)q;

  return strcmp (a->cmd, b->cmd);
}

/* Sort each node's command element according to command string. */
void
sort_node ()
{
  int i, j;
  struct cmd_node *cnode;
  vector descvec;
  struct cmd_element *cmd_element;

  for (i = 0; i < vector_max (cmdvec); i++) 
    if ((cnode = vector_slot (cmdvec, i)) != NULL)
      {	
	vector cmd_vector = cnode->cmd_vector;
	qsort (cmd_vector->index, cmd_vector->max, sizeof (void *), cmp_node);

	for (j = 0; j < vector_max (cmd_vector); j++)
	  if ((cmd_element = vector_slot (cmd_vector, j)) != NULL)
	    {
	      descvec = vector_slot (cmd_element->strvec,
				     vector_max (cmd_element->strvec) - 1);
	      qsort (descvec->index, descvec->max, sizeof (void *), cmp_desc);
	    }
      }
}

/* Breaking up string into each command piece. I assume given
   character is separated by a space character. Return value is a
   vector which includes char ** data element. */
vector
cmd_make_strvec (char *string)
{
  char *cp, *start, *token;
  int strlen;
  vector strvec;
  
  if (string == NULL)
    return NULL;
  
  cp = string;

  /* Skip white spaces. */
  while (isspace ((int) *cp) && *cp != '\0')
    cp++;

  /* Return if there is only white spaces */
  if (*cp == '\0')
    return NULL;

  if (*cp == '!' || *cp == '#')
    return NULL;

  /* Prepare return vector. */
  strvec = vector_init (VECTOR_MIN_SIZE);

  /* Copy each command piece and set into vector. */
  while (1) 
    {
      start = cp;
      while (!(isspace ((int) *cp) || *cp == '\r' || *cp == '\n') &&
	     *cp != '\0')
	cp++;
      strlen = cp - start;
      token = XMALLOC (MTYPE_STRVEC, strlen + 1);
      memcpy (token, start, strlen);
      *(token + strlen) = '\0';
      vector_set (strvec, token);

      while ((isspace ((int) *cp) || *cp == '\n' || *cp == '\r') &&
	     *cp != '\0')
	cp++;

      if (*cp == '\0')
	return strvec;
    }
}

/* Free allocated string vector. */
void
cmd_free_strvec (vector v)
{
  int i;
  char *cp;

  if (!v)
    return;

  for (i = 0; i < vector_max (v); i++)
    if ((cp = vector_slot (v, i)) != NULL)
      XFREE (MTYPE_STRVEC, cp);

  vector_free (v);
}

/* Fetch next description.  Used in cmd_make_descvec(). */
char *
cmd_desc_str (char **string)
{
  char *cp, *start, *token;
  int strlen;
  
  cp = *string;

  if (cp == NULL)
    return NULL;

  /* Skip white spaces. */
  while (isspace ((int) *cp) && *cp != '\0')
    cp++;

  /* Return if there is only white spaces */
  if (*cp == '\0')
    return NULL;

  start = cp;

  while (!(*cp == '\r' || *cp == '\n') && *cp != '\0')
    cp++;

  strlen = cp - start;
  token = XMALLOC (MTYPE_STRVEC, strlen + 1);
  memcpy (token, start, strlen);
  *(token + strlen) = '\0';

  *string = cp;

  return token;
}

/* New string vector. */
vector
cmd_make_descvec (char *string, char *descstr)
{
  int multiple = 0;
  char *sp;
  char *token;
  int len;
  char *cp;
  char *dp;
  vector allvec;
  vector strvec = NULL;
  struct desc *desc;

  cp = string;
  dp = descstr;

  if (cp == NULL)
    return NULL;

  allvec = vector_init (VECTOR_MIN_SIZE);

  while (1)
    {
      while (isspace ((int) *cp) && *cp != '\0')
	cp++;

      if (*cp == '(')
	{
	  multiple = 1;
	  cp++;
	}
      if (*cp == ')')
	{
	  multiple = 0;
	  cp++;
	}
      if (*cp == '|')
	{
	  if (! multiple)
	    {
	      fprintf (stderr, "Command parse error!: %s\n", string);
	      exit (1);
	    }
	  cp++;
	}
      
      while (isspace ((int) *cp) && *cp != '\0')
	cp++;

      if (*cp == '(')
	{
	  multiple = 1;
	  cp++;
	}

      if (*cp == '\0') 
	return allvec;

      sp = cp;

      while (! (isspace ((int) *cp) || *cp == '\r' || *cp == '\n' || *cp == ')' || *cp == '|') && *cp != '\0')
	cp++;

      len = cp - sp;

      token = XMALLOC (MTYPE_STRVEC, len + 1);
      memcpy (token, sp, len);
      *(token + len) = '\0';

      desc = XCALLOC (MTYPE_DESC, sizeof (struct desc));
      desc->cmd = token;
      desc->str = cmd_desc_str (&dp);

      if (multiple)
	{
	  if (multiple == 1)
	    {
	      strvec = vector_init (VECTOR_MIN_SIZE);
	      vector_set (allvec, strvec);
	    }
	  multiple++;
	}
      else
	{
	  strvec = vector_init (VECTOR_MIN_SIZE);
	  vector_set (allvec, strvec);
	}
      vector_set (strvec, desc);
    }
}

/* Count mandantory string vector size.  This is to determine inputed
   command has enough command length. */
int
cmd_cmdsize (vector strvec)
{
  int i;
  char *str;
  int size = 0;
  vector descvec;

  for (i = 0; i < vector_max (strvec); i++)
    {
      descvec = vector_slot (strvec, i);

      if (vector_max (descvec) == 1)
	{
	  struct desc *desc = vector_slot (descvec, 0);

	  str = desc->cmd;
	  
	  if (str == NULL || CMD_OPTION (str))
	    return size;
	  else
	    size++;
	}
      else
	size++;
    }
  return size;
}

/* Return prompt character of specified node. */
char *
cmd_prompt (enum node_type node)
{
  struct cmd_node *cnode;

  cnode = vector_slot (cmdvec, node);
  return cnode->prompt;
}

/* Install a command into a node. */
void
install_element (enum node_type ntype, struct cmd_element *cmd)
{
  struct cmd_node *cnode;

  cnode = vector_slot (cmdvec, ntype);

  if (cnode == NULL) 
    {
      fprintf (stderr, "Command node %d doesn't exist, please check it\n",
	       ntype);
      exit (1);
    }

  vector_set (cnode->cmd_vector, cmd);

  cmd->strvec = cmd_make_descvec (cmd->string, cmd->doc);
  cmd->cmdsize = cmd_cmdsize (cmd->strvec);
}

static unsigned char itoa64[] =	
"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
to64(char *s, long v, int n)
{
  while (--n >= 0) 
    {
      *s++ = itoa64[v&0x3f];
      v >>= 6;
    }
}

char *zencrypt (char *passwd)
{
  char salt[6];
  struct timeval tv;
  char *crypt (const char *, const char *);

  gettimeofday(&tv,0);
  
  to64(&salt[0], random(), 3);
  to64(&salt[3], tv.tv_usec, 3);
  salt[5] = '\0';

  return crypt (passwd, salt);
}

char *
syslog_facility_print (int facility)
{
  switch (facility)
    {
      case LOG_KERN:
	return "kern";
	break;
      case LOG_USER:
	return "user";
	break;
      case LOG_MAIL:
	return "mail";
	break;
      case LOG_DAEMON:
	return "daemon";
	break;
      case LOG_AUTH:
	return "auth";
	break;
      case LOG_SYSLOG:
	return "syslog";
	break;
      case LOG_LPR:
	return "lpr";
	break;
      case LOG_NEWS:
	return "news";
	break;
      case LOG_UUCP:
	return "uucp";
	break;
      case LOG_CRON:
	return "cron";
	break;
      case LOG_LOCAL0:
	return "local0";
	break;
      case LOG_LOCAL1:
	return "local1";
	break;
      case LOG_LOCAL2:
	return "local2";
	break;
      case LOG_LOCAL3:
	return "local3";
	break;
      case LOG_LOCAL4:
	return "local4";
	break;
      case LOG_LOCAL5:
	return "local5";
	break;
      case LOG_LOCAL6:
	return "local6";
	break;
      case LOG_LOCAL7:
	return "local7";
	break;
      default:
	break;
    }
  return "";
}

/* This function write configuration of this host. */
int
config_write_host (struct vty *vty)
{
  if (host.name)
    vty_out (vty, "hostname %s%s", host.name, VTY_NEWLINE);

  if (host.encrypt)
    {
      if (host.password_encrypt)
        vty_out (vty, "password 8 %s%s", host.password_encrypt, VTY_NEWLINE); 
      if (host.enable_encrypt)
        vty_out (vty, "enable password 8 %s%s", host.enable_encrypt, VTY_NEWLINE); 
    }
  else
    {
      if (host.password)
        vty_out (vty, "password %s%s", host.password, VTY_NEWLINE);
      else if (host.password_encrypt)
        vty_out (vty, "password 8 %s%s", host.password_encrypt, VTY_NEWLINE); 
	
      if (host.enable)
        vty_out (vty, "enable password %s%s", host.enable, VTY_NEWLINE);
      else if (host.enable_encrypt)
        vty_out (vty, "enable password 8 %s%s", host.enable_encrypt, VTY_NEWLINE); 
    }

  if (host.logfile)
    vty_out (vty, "log file %s%s", host.logfile, VTY_NEWLINE);

  if (host.log_stdout)
    vty_out (vty, "log stdout%s", VTY_NEWLINE);

  if (host.log_syslog)
    {
      vty_out (vty, "log syslog");
      if (zlog_default->facility != LOG_DAEMON)
	vty_out (vty, " facility %s", syslog_facility_print (zlog_default->facility));
      vty_out (vty, "%s", VTY_NEWLINE);
    }
  if (zlog_default->maskpri != LOG_DEBUG)
    vty_out (vty, "log trap %s%s", zlog_priority[zlog_default->maskpri], VTY_NEWLINE);

  if (zlog_default->record_priority == 1)
    vty_out (vty, "log record-priority%s", VTY_NEWLINE);

  if (host.advanced)
    vty_out (vty, "service advanced-vty%s", VTY_NEWLINE);

  if (host.encrypt)
    vty_out (vty, "service password-encryption%s", VTY_NEWLINE);

  if (host.lines >= 0)
    vty_out (vty, "service terminal-length %d%s", host.lines,
	     VTY_NEWLINE);

  if (! host.motd)
    vty_out (vty, "no banner motd%s", VTY_NEWLINE);

  return 1;
}

/* Utility function for getting command vector. */
vector
cmd_node_vector (vector v, enum node_type ntype)
{
  struct cmd_node *cnode = vector_slot (v, ntype);
  return cnode->cmd_vector;
}

/* Filter command vector by symbol */
int
cmd_filter_by_symbol (char *command, char *symbol)
{
  int i, lim;

  if (strcmp (symbol, "IPV4_ADDRESS") == 0)
    {
      i = 0;
      lim = strlen (command);
      while (i < lim)
	{
	  if (! (isdigit ((int) command[i]) || command[i] == '.' || command[i] == '/'))
	    return 1;
	  i++;
	}
      return 0;
    }
  if (strcmp (symbol, "STRING") == 0)
    {
      i = 0;
      lim = strlen (command);
      while (i < lim)
	{
	  if (! (isalpha ((int) command[i]) || command[i] == '_' || command[i] == '-'))
	    return 1;
	  i++;
	}
      return 0;
    }
  if (strcmp (symbol, "IFNAME") == 0)
    {
      i = 0;
      lim = strlen (command);
      while (i < lim)
	{
	  if (! isalnum ((int) command[i]))
	    return 1;
	  i++;
	}
      return 0;
    }
  return 0;
}

/* Completion match types. */
enum match_type 
{
  no_match,
  extend_match,
  ipv4_prefix_match,
  ipv4_match,
  ipv6_prefix_match,
  ipv6_match,
  range_match,
  vararg_match,
  partly_match,
  exact_match 
};

enum match_type
cmd_ipv4_match (char *str)
{
  char *sp;
  int dots = 0, nums = 0;
  char buf[4];

  if (str == NULL)
    return partly_match;

  for (;;)
    {
      memset (buf, 0, sizeof (buf));
      sp = str;
      while (*str != '\0')
	{
	  if (*str == '.')
	    {
	      if (dots >= 3)
		return no_match;

	      if (*(str + 1) == '.')
		return no_match;

	      if (*(str + 1) == '\0')
		return partly_match;

	      dots++;
	      break;
	    }
	  if (!isdigit ((int) *str))
	    return no_match;

	  str++;
	}

      if (str - sp > 3)
	return no_match;

      strncpy (buf, sp, str - sp);
      if (atoi (buf) > 255)
	return no_match;

      nums++;

      if (*str == '\0')
	break;

      str++;
    }

  if (nums < 4)
    return partly_match;

  return exact_match;
}

enum match_type
cmd_ipv4_prefix_match (char *str)
{
  char *sp;
  int dots = 0;
  char buf[4];

  if (str == NULL)
    return partly_match;

  for (;;)
    {
      memset (buf, 0, sizeof (buf));
      sp = str;
      while (*str != '\0' && *str != '/')
	{
	  if (*str == '.')
	    {
	      if (dots == 3)
		return no_match;

	      if (*(str + 1) == '.' || *(str + 1) == '/')
		return no_match;

	      if (*(str + 1) == '\0')
		return partly_match;

	      dots++;
	      break;
	    }

	  if (!isdigit ((int) *str))
	    return no_match;

	  str++;
	}

      if (str - sp > 3)
	return no_match;

      strncpy (buf, sp, str - sp);
      if (atoi (buf) > 255)
	return no_match;

      if (dots == 3)
	{
	  if (*str == '/')
	    {
	      if (*(str + 1) == '\0')
		return partly_match;

	      str++;
	      break;
	    }
	  else if (*str == '\0')
	    return partly_match;
	}

      if (*str == '\0')
	return partly_match;

      str++;
    }

  sp = str;
  while (*str != '\0')
    {
      if (!isdigit ((int) *str))
	return no_match;

      str++;
    }

  if (atoi (sp) > 32)
    return no_match;

  return exact_match;
}

#define IPV6_ADDR_STR		"0123456789abcdefABCDEF:.%"
#define IPV6_PREFIX_STR		"0123456789abcdefABCDEF:.%/"
#define STATE_START		1
#define STATE_COLON		2
#define STATE_DOUBLE		3
#define STATE_ADDR		4
#define STATE_DOT               5
#define STATE_SLASH		6
#define STATE_MASK		7

enum match_type
cmd_ipv6_match (char *str)
{
  int state = STATE_START;
  int colons = 0, nums = 0, double_colon = 0;
  char *sp = NULL;

  if (str == NULL)
    return partly_match;

  if (strspn (str, IPV6_ADDR_STR) != strlen (str))
    return no_match;

  while (*str != '\0')
    {
      switch (state)
	{
	case STATE_START:
	  if (*str == ':')
	    {
	      if (*(str + 1) != ':' && *(str + 1) != '\0')
		return no_match;
     	      colons--;
	      state = STATE_COLON;
	    }
	  else
	    {
	      sp = str;
	      state = STATE_ADDR;
	    }

	  continue;
	case STATE_COLON:
	  colons++;
	  if (*(str + 1) == ':')
	    state = STATE_DOUBLE;
	  else
	    {
	      sp = str + 1;
	      state = STATE_ADDR;
	    }
	  break;
	case STATE_DOUBLE:
	  if (double_colon)
	    return no_match;

	  if (*(str + 1) == ':')
	    return no_match;
	  else
	    {
	      if (*(str + 1) != '\0')
		colons++;
	      sp = str + 1;
	      state = STATE_ADDR;
	    }

	  double_colon++;
	  nums++;
	  break;
	case STATE_ADDR:
	  if (*(str + 1) == ':' || *(str + 1) == '\0')
	    {
	      if (str - sp > 3)
		return no_match;

	      nums++;
	      state = STATE_COLON;
	    }
	  if (*(str + 1) == '.')
	    state = STATE_DOT;
	  break;
	case STATE_DOT:
	  state = STATE_ADDR;
	  break;
	default:
	  break;
	}

      if (nums > 8)
	return no_match;

      if (colons > 7)
	return no_match;

      str++;
    }

#if 0
  if (nums < 11)
    return partly_match;
#endif /* 0 */

  return exact_match;
}

enum match_type
cmd_ipv6_prefix_match (char *str)
{
  int state = STATE_START;
  int colons = 0, nums = 0, double_colon = 0;
  int mask;
  char *sp = NULL;
  char *endptr = NULL;

  if (str == NULL)
    return partly_match;

  if (strspn (str, IPV6_PREFIX_STR) != strlen (str))
    return no_match;

  while (*str != '\0' && state != STATE_MASK)
    {
      switch (state)
	{
	case STATE_START:
	  if (*str == ':')
	    {
	      if (*(str + 1) != ':' && *(str + 1) != '\0')
		return no_match;
	      colons--;
	      state = STATE_COLON;
	    }
	  else
	    {
	      sp = str;
	      state = STATE_ADDR;
	    }

	  continue;
	case STATE_COLON:
	  colons++;
	  if (*(str + 1) == '/')
	    return no_match;
	  else if (*(str + 1) == ':')
	    state = STATE_DOUBLE;
	  else
	    {
	      sp = str + 1;
	      state = STATE_ADDR;
	    }
	  break;
	case STATE_DOUBLE:
	  if (double_colon)
	    return no_match;

	  if (*(str + 1) == ':')
	    return no_match;
	  else
	    {
	      if (*(str + 1) != '\0' && *(str + 1) != '/')
		colons++;
	      sp = str + 1;

	      if (*(str + 1) == '/')
		state = STATE_SLASH;
	      else
		state = STATE_ADDR;
	    }

	  double_colon++;
	  nums += 1;
	  break;
	case STATE_ADDR:
	  if (*(str + 1) == ':' || *(str + 1) == '.'
	      || *(str + 1) == '\0' || *(str + 1) == '/')
	    {
	      if (str - sp > 3)
		return no_match;

	      for (; sp <= str; sp++)
		if (*sp == '/')
		  return no_match;

	      nums++;

	      if (*(str + 1) == ':')
		state = STATE_COLON;
	      else if (*(str + 1) == '.')
		state = STATE_DOT;
	      else if (*(str + 1) == '/')
		state = STATE_SLASH;
	    }
	  break;
	case STATE_DOT:
	  state = STATE_ADDR;
	  break;
	case STATE_SLASH:
	  if (*(str + 1) == '\0')
	    return partly_match;

	  state = STATE_MASK;
	  break;
	default:
	  break;
	}

      if (nums > 11)
	return no_match;

      if (colons > 7)
	return no_match;

      str++;
    }

  if (state < STATE_MASK)
    return partly_match;

  mask = strtol (str, &endptr, 10);
  if (*endptr != '\0')
    return no_match;

  if (mask < 0 || mask > 128)
    return no_match;
  
/* I don't know why mask < 13 makes command match partly.
   Forgive me to make this comments. I Want to set static default route
   because of lack of function to originate default in ospf6d; sorry
       yasu
  if (mask < 13)
    return partly_match;
*/

  return exact_match;
}

#define DECIMAL_STRLEN_MAX 10

int
cmd_range_match (char *range, char *str)
{
  char *p;
  char buf[DECIMAL_STRLEN_MAX + 1];
  char *endptr = NULL;
  unsigned long min, max, val;

  if (str == NULL)
    return 1;

  val = strtoul (str, &endptr, 10);
  if (*endptr != '\0')
    return 0;

  range++;
  p = strchr (range, '-');
  if (p == NULL)
    return 0;
  if (p - range > DECIMAL_STRLEN_MAX)
    return 0;
  strncpy (buf, range, p - range);
  buf[p - range] = '\0';
  min = strtoul (buf, &endptr, 10);
  if (*endptr != '\0')
    return 0;

  range = p + 1;
  p = strchr (range, '>');
  if (p == NULL)
    return 0;
  if (p - range > DECIMAL_STRLEN_MAX)
    return 0;
  strncpy (buf, range, p - range);
  buf[p - range] = '\0';
  max = strtoul (buf, &endptr, 10);
  if (*endptr != '\0')
    return 0;

  if (val < min || val > max)
    return 0;

  return 1;
}

/* Make completion match and return match type flag. */
enum match_type
cmd_filter_by_completion (char *command, vector v, int index)
{
  int i;
  char *str;
  struct cmd_element *cmd_element;
  enum match_type match_type;
  vector descvec;
  struct desc *desc;
  
  match_type = no_match;

  /* If command and cmd_element string does not match set NULL to vector */
  for (i = 0; i < vector_max (v); i++) 
    if ((cmd_element = vector_slot (v, i)) != NULL)
      {
	if (index >= vector_max (cmd_element->strvec))
	  vector_slot (v, i) = NULL;
	else
	  {
	    int j;
	    int matched = 0;

	    descvec = vector_slot (cmd_element->strvec, index);
	    
	    for (j = 0; j < vector_max (descvec); j++)
	      {
		desc = vector_slot (descvec, j);
		str = desc->cmd;

		if (CMD_VARARG (str))
		  {
		    if (match_type < vararg_match)
		      match_type = vararg_match;
		    matched++;
		  }
		else if (CMD_RANGE (str))
		  {
		    if (cmd_range_match (str, command))
		      {
			if (match_type < range_match)
			  match_type = range_match;

			matched++;
		      }
		  }
		else if (CMD_IPV6 (str))
		  {
		    if (cmd_ipv6_match (command))
		      {
			if (match_type < ipv6_match)
			  match_type = ipv6_match;

			matched++;
		      }
		  }
		else if (CMD_IPV6_PREFIX (str))
		  {
		    if (cmd_ipv6_prefix_match (command))
		      {
			if (match_type < ipv6_prefix_match)
			  match_type = ipv6_prefix_match;

			matched++;
		      }
		  }
		else if (CMD_IPV4 (str))
		  {
		    if (cmd_ipv4_match (command))
		      {
			if (match_type < ipv4_match)
			  match_type = ipv4_match;

			matched++;
		      }
		  }
		else if (CMD_IPV4_PREFIX (str))
		  {
		    if (cmd_ipv4_prefix_match (command))
		      {
			if (match_type < ipv4_prefix_match)
			  match_type = ipv4_prefix_match;
			matched++;
		      }
		  }
		else
		/* Check is this point's argument optional ? */
		if (CMD_OPTION (str) || CMD_VARIABLE (str))
		  {
		    if (match_type < extend_match)
		      match_type = extend_match;
		    matched++;
		  }
		else if (strncmp (command, str, strlen (command)) == 0)
		  {
		    if (strcmp (command, str) == 0) 
		      match_type = exact_match;
		    else
		      {
			if (match_type < partly_match)
			  match_type = partly_match;
		      }
		    matched++;
		  }
	      }
	    if (! matched)
	      vector_slot (v, i) = NULL;
	  }
      }
  return match_type;
}

/* Filter vector by command character with index. */
enum match_type
cmd_filter_by_string (char *command, vector v, int index)
{
  int i;
  char *str;
  struct cmd_element *cmd_element;
  enum match_type match_type;
  vector descvec;
  struct desc *desc;
  
  match_type = no_match;

  /* If command and cmd_element string does not match set NULL to vector */
  for (i = 0; i < vector_max (v); i++) 
    if ((cmd_element = vector_slot (v, i)) != NULL)
      {
	/* If given index is bigger than max string vector of command,
           set NULL*/
	if (index >= vector_max (cmd_element->strvec))
	  vector_slot (v, i) = NULL;
	else 
	  {
	    int j;
	    int matched = 0;

	    descvec = vector_slot (cmd_element->strvec, index);

	    for (j = 0; j < vector_max (descvec); j++)
	      {
		desc = vector_slot (descvec, j);
		str = desc->cmd;

		if (CMD_VARARG (str))
		  {
		    if (match_type < vararg_match)
		      match_type = vararg_match;
		    matched++;
		  }
		else if (CMD_RANGE (str))
		  {
		    if (cmd_range_match (str, command))
		      {
			if (match_type < range_match)
			  match_type = range_match;
			matched++;
		      }
		  }
		else if (CMD_IPV6 (str))
		  {
		    if (cmd_ipv6_match (command) == exact_match)
		      {
			if (match_type < ipv6_match)
			  match_type = ipv6_match;
			matched++;
		      }
		  }
		else if (CMD_IPV6_PREFIX (str))
		  {
		    if (cmd_ipv6_prefix_match (command) == exact_match)
		      {
			if (match_type < ipv6_prefix_match)
			  match_type = ipv6_prefix_match;
			matched++;
		      }
		  }
		else if (CMD_IPV4 (str))
		  {
		    if (cmd_ipv4_match (command) == exact_match)
		      {
			if (match_type < ipv4_match)
			  match_type = ipv4_match;
			matched++;
		      }
		  }
		else if (CMD_IPV4_PREFIX (str))
		  {
		    if (cmd_ipv4_prefix_match (command) == exact_match)
		      {
			if (match_type < ipv4_prefix_match)
			  match_type = ipv4_prefix_match;
			matched++;
		      }
		  }
		else if (CMD_OPTION (str) || CMD_VARIABLE (str))
		  {
		    if (match_type < extend_match)
		      match_type = extend_match;
		    matched++;
		  }
		else
		  {		  
		    if (strcmp (command, str) == 0)
		      {
			match_type = exact_match;
			matched++;
		      }
		  }
	      }
	    if (! matched)
	      vector_slot (v, i) = NULL;
	  }
      }
  return match_type;
}

/* Check ambiguous match */
int
is_cmd_ambiguous (char *command, vector v, int index, enum match_type type)
{
  int i;
  int j;
  char *str = NULL;
  struct cmd_element *cmd_element;
  char *matched = NULL;
  vector descvec;
  struct desc *desc;
  
  for (i = 0; i < vector_max (v); i++) 
    if ((cmd_element = vector_slot (v, i)) != NULL)
      {
	int match = 0;

	descvec = vector_slot (cmd_element->strvec, index);

	for (j = 0; j < vector_max (descvec); j++)
	  {
	    enum match_type ret;

	    desc = vector_slot (descvec, j);
	    str = desc->cmd;

	    switch (type)
	      {
	      case exact_match:
		if (! (CMD_OPTION (str) || CMD_VARIABLE (str))
		    && strcmp (command, str) == 0)
		  match++;
		break;
	      case partly_match:
		if (! (CMD_OPTION (str) || CMD_VARIABLE (str))
		    && strncmp (command, str, strlen (command)) == 0)
		  {
		    if (matched && strcmp (matched, str) != 0)
		      return 1;	/* There is ambiguous match. */
		    else
		      matched = str;
		    match++;
		  }
		break;
	      case range_match:
		if (cmd_range_match (str, command))
		  {
		    if (matched && strcmp (matched, str) != 0)
		      return 1;
		    else
		      matched = str;
		    match++;
		  }
		break;
 	      case ipv6_match:
		if (CMD_IPV6 (str))
		  match++;
		break;
	      case ipv6_prefix_match:
		if ((ret = cmd_ipv6_prefix_match (command)) != no_match)
		  {
		    if (ret == partly_match)
		      return 2; /* There is incomplete match. */

		    match++;
		  }
		break;
	      case ipv4_match:
		if (CMD_IPV4 (str))
		  match++;
		break;
	      case ipv4_prefix_match:
		if ((ret = cmd_ipv4_prefix_match (command)) != no_match)
		  {
		    if (ret == partly_match)
		      return 2; /* There is incomplete match. */

		    match++;
		  }
		break;
	      case extend_match:
		if (CMD_OPTION (str) || CMD_VARIABLE (str))
		  match++;
		break;
	      case no_match:
	      default:
		break;
	      }
	  }
	if (! match)
	  vector_slot (v, i) = NULL;
      }
  return 0;
}

/* If src matches dst return dst string, otherwise return NULL */
char *
cmd_entry_function (char *src, char *dst)
{
  /* Skip variable arguments. */
  if (CMD_OPTION (dst) || CMD_VARIABLE (dst) || CMD_VARARG (dst) ||
      CMD_IPV4 (dst) || CMD_IPV4_PREFIX (dst) || CMD_RANGE (dst))
    return NULL;

  /* In case of 'command \t', given src is NULL string. */
  if (src == NULL)
    return dst;

  /* Matched with input string. */
  if (strncmp (src, dst, strlen (src)) == 0)
    return dst;

  return NULL;
}

/* If src matches dst return dst string, otherwise return NULL */
/* This version will return the dst string always if it is
   CMD_VARIABLE for '?' key processing */
char *
cmd_entry_function_desc (char *src, char *dst)
{
  if (CMD_VARARG (dst))
    return dst;

  if (CMD_RANGE (dst))
    {
      if (cmd_range_match (dst, src))
	return dst;
      else
	return NULL;
    }

  if (CMD_IPV6 (dst))
    {
      if (cmd_ipv6_match (src))
	return dst;
      else
	return NULL;
    }

  if (CMD_IPV6_PREFIX (dst))
    {
      if (cmd_ipv6_prefix_match (src))
	return dst;
      else
	return NULL;
    }

  if (CMD_IPV4 (dst))
    {
      if (cmd_ipv4_match (src))
	return dst;
      else
	return NULL;
    }

  if (CMD_IPV4_PREFIX (dst))
    {
      if (cmd_ipv4_prefix_match (src))
	return dst;
      else
	return NULL;
    }

  /* Optional or variable commands always match on '?' */
  if (CMD_OPTION (dst) || CMD_VARIABLE (dst))
    return dst;

  /* In case of 'command \t', given src is NULL string. */
  if (src == NULL)
    return dst;

  if (strncmp (src, dst, strlen (src)) == 0)
    return dst;
  else
    return NULL;
}

/* Check same string element existence.  If it isn't there return
    1. */
int
cmd_unique_string (vector v, char *str)
{
  int i;
  char *match;

  for (i = 0; i < vector_max (v); i++)
    if ((match = vector_slot (v, i)) != NULL)
      if (strcmp (match, str) == 0)
	return 0;
  return 1;
}

/* Compare string to description vector.  If there is same string
   return 1 else return 0. */
int
desc_unique_string (vector v, char *str)
{
  int i;
  struct desc *desc;

  for (i = 0; i < vector_max (v); i++)
    if ((desc = vector_slot (v, i)) != NULL)
      if (strcmp (desc->cmd, str) == 0)
	return 1;
  return 0;
}

/* '?' describe command support. */
vector
cmd_describe_command (vector vline, struct vty *vty, int *status)
{
  int i;
  vector cmd_vector;
#define INIT_MATCHVEC_SIZE 10
  vector matchvec;
  struct cmd_element *cmd_element;
  int index;
  int ret;
  enum match_type match;
  char *command;
  static struct desc desc_cr = { "<cr>", "" };

  /* Set index. */
  index = vector_max (vline) - 1;

  /* Make copy vector of current node's command vector. */
  cmd_vector = vector_copy (cmd_node_vector (cmdvec, vty->node));

  /* Prepare match vector */
  matchvec = vector_init (INIT_MATCHVEC_SIZE);

  /* Filter commands. */
  /* Only words precedes current word will be checked in this loop. */
  for (i = 0; i < index; i++)
    {
      command = vector_slot (vline, i);
      match = cmd_filter_by_completion (command, cmd_vector, i);

      if (match == vararg_match)
	{
	  struct cmd_element *cmd_element;
	  vector descvec;
	  int j, k;

	  for (j = 0; j < vector_max (cmd_vector); j++)
	    if ((cmd_element = vector_slot (cmd_vector, j)) != NULL)
	      {
		descvec = vector_slot (cmd_element->strvec,
				       vector_max (cmd_element->strvec) - 1);
		for (k = 0; k < vector_max (descvec); k++)
		  {
		    struct desc *desc = vector_slot (descvec, k);
		    vector_set (matchvec, desc);
		  }
	      }

	  vector_set (matchvec, &desc_cr);
	  vector_free (cmd_vector);

	  return matchvec;
	}

      if ((ret = is_cmd_ambiguous (command, cmd_vector, i, match)) == 1)
	{
	  vector_free (cmd_vector);
	  *status = CMD_ERR_AMBIGUOUS;
	  return NULL;
	}
      else if (ret == 2)
	{
	  vector_free (cmd_vector);
	  *status = CMD_ERR_NO_MATCH;
	  return NULL;
	}
    }

  /* Prepare match vector */
  /*  matchvec = vector_init (INIT_MATCHVEC_SIZE); */

  /* Make sure that cmd_vector is filtered based on current word */
  command = vector_slot (vline, index);
  if (command)
    match = cmd_filter_by_completion (command, cmd_vector, index);

  /* Make description vector. */
  for (i = 0; i < vector_max (cmd_vector); i++)
    if ((cmd_element = vector_slot (cmd_vector, i)) != NULL)
      {
	char *string = NULL;
	vector strvec = cmd_element->strvec;

        /* if command is NULL, index may be equal to vector_max */
	if (command && index >= vector_max (strvec))
	  vector_slot (cmd_vector, i) = NULL;
	else
	  {
	    /* Check if command is completed. */
            if (command == NULL && index == vector_max (strvec))
	      {
		string = "<cr>";
		if (! desc_unique_string (matchvec, string))
		  vector_set (matchvec, &desc_cr);
	      }
	    else
	      {
		int j;
		vector descvec = vector_slot (strvec, index);
		struct desc *desc;

		for (j = 0; j < vector_max (descvec); j++)
		  {
		    desc = vector_slot (descvec, j);
		    string = cmd_entry_function_desc (command, desc->cmd);
		    if (string)
		      {
			/* Uniqueness check */
			if (! desc_unique_string (matchvec, string))
			  vector_set (matchvec, desc);
		      }
		  }
	      }
	  }
      }
  vector_free (cmd_vector);

  if (vector_slot (matchvec, 0) == NULL)
    {
      vector_free (matchvec);
      *status= CMD_ERR_NO_MATCH;
    }
  else
    *status = CMD_SUCCESS;

  return matchvec;
}

/* Check LCD of matched command. */
int
cmd_lcd (char **matched)
{
  int i;
  int j;
  int lcd = -1;
  char *s1, *s2;
  char c1, c2;

  if (matched[0] == NULL || matched[1] == NULL)
    return 0;

  for (i = 1; matched[i] != NULL; i++)
    {
      s1 = matched[i - 1];
      s2 = matched[i];

      for (j = 0; (c1 = s1[j]) && (c2 = s2[j]); j++)
	if (c1 != c2)
	  break;

      if (lcd < 0)
	lcd = j;
      else
	{
	  if (lcd > j)
	    lcd = j;
	}
    }
  return lcd;
}

/* Command line completion support. */
char **
cmd_complete_command (vector vline, struct vty *vty, int *status)
{
  int i;
  vector cmd_vector = vector_copy (cmd_node_vector (cmdvec, vty->node));
#define INIT_MATCHVEC_SIZE 10
  vector matchvec;
  struct cmd_element *cmd_element;
  int index = vector_max (vline) - 1;
  char **match_str;
  struct desc *desc;
  vector descvec;
  char *command;
  int lcd;

  /* First, filter by preceeding command string */
  for (i = 0; i < index; i++)
    {
      enum match_type match;
      int ret;

      command = vector_slot (vline, i);

      /* First try completion match, if there is exactly match return 1 */
      match = cmd_filter_by_completion (command, cmd_vector, i);

      /* If there is exact match then filter ambiguous match else check
	 ambiguousness. */
      if ((ret = is_cmd_ambiguous (command, cmd_vector, i, match)) == 1)
	{
	  vector_free (cmd_vector);
	  *status = CMD_ERR_AMBIGUOUS;
	  return NULL;
	}
      /*
	else if (ret == 2)
	{
	  vector_free (cmd_vector);
	  *status = CMD_ERR_NO_MATCH;
	  return NULL;
	}
      */
    }

  /* Prepare match vector. */
  matchvec = vector_init (INIT_MATCHVEC_SIZE);

  /* Now we got into completion */
  for (i = 0; i < vector_max (cmd_vector); i++)
    if ((cmd_element = vector_slot (cmd_vector, i)) != NULL)
      {
	char *string;
	vector strvec = cmd_element->strvec;
	
	/* Check field length */
	if (index >= vector_max (strvec))
	  vector_slot (cmd_vector, i) = NULL;
	else 
	  {
	    int j;

	    descvec = vector_slot (strvec, index);
	    for (j = 0; j < vector_max (descvec); j++)
	      {
		desc = vector_slot (descvec, j);

		if ((string = cmd_entry_function (vector_slot (vline, index),
						  desc->cmd)))
		  if (cmd_unique_string (matchvec, string))
		    vector_set (matchvec, XSTRDUP (MTYPE_TMP, string));
	      }
	  }
      }

  /* We don't need cmd_vector any more. */
  vector_free (cmd_vector);

  /* No matched command */
  if (vector_slot (matchvec, 0) == NULL)
    {
      vector_free (matchvec);

      /* In case of 'command \t' pattern.  Do you need '?' command at
         the end of the line. */
      if (vector_slot (vline, index) == '\0')
	*status = CMD_ERR_NOTHING_TODO;
      else
	*status = CMD_ERR_NO_MATCH;
      return NULL;
    }

  /* Only one matched */
  if (vector_slot (matchvec, 1) == NULL)
    {
      match_str = (char **) matchvec->index;
      vector_only_wrapper_free (matchvec);
      *status = CMD_COMPLETE_FULL_MATCH;
      return match_str;
    }
  /* Make it sure last element is NULL. */
  vector_set (matchvec, NULL);

  /* Check LCD of matched strings. */
  if (vector_slot (vline, index) != NULL)
    {
      lcd = cmd_lcd ((char **) matchvec->index);

      if (lcd)
	{
	  int len = strlen (vector_slot (vline, index));
	  
	  if (len < lcd)
	    {
	      char *lcdstr;
	      
	      lcdstr = XMALLOC (MTYPE_TMP, lcd + 1);
	      memcpy (lcdstr, matchvec->index[0], lcd);
	      lcdstr[lcd] = '\0';

	      /* match_str = (char **) &lcdstr; */

	      /* Free matchvec. */
	      for (i = 0; i < vector_max (matchvec); i++)
		{
		  if (vector_slot (matchvec, i))
		    XFREE (MTYPE_TMP, vector_slot (matchvec, i));
		}
	      vector_free (matchvec);

      	      /* Make new matchvec. */
	      matchvec = vector_init (INIT_MATCHVEC_SIZE);
	      vector_set (matchvec, lcdstr);
	      match_str = (char **) matchvec->index;
	      vector_only_wrapper_free (matchvec);

	      *status = CMD_COMPLETE_MATCH;
	      return match_str;
	    }
	}
    }

  match_str = (char **) matchvec->index;
  vector_only_wrapper_free (matchvec);
  *status = CMD_COMPLETE_LIST_MATCH;
  return match_str;
}

/* Execute command by argument vline vector. */
int
cmd_execute_command (vector vline, struct vty *vty, struct cmd_element **cmd)
{
  int i;
  int index;
  vector cmd_vector;
  struct cmd_element *cmd_element;
  struct cmd_element *matched_element;
  unsigned int matched_count, incomplete_count;
  int argc;
  char *argv[CMD_ARGC_MAX];
  enum match_type match = 0;
  int varflag;
  char *command;

  /* Make copy of command elements. */
  cmd_vector = vector_copy (cmd_node_vector (cmdvec, vty->node));

  for (index = 0; index < vector_max (vline); index++) 
    {
      int ret;

      command = vector_slot (vline, index);

      match = cmd_filter_by_completion (command, cmd_vector, index);

      if (match == vararg_match)
	break;

      ret = is_cmd_ambiguous (command, cmd_vector, index, match);

      if (ret == 1)
	{
	  vector_free (cmd_vector);
	  return CMD_ERR_AMBIGUOUS;
	}
      else if (ret == 2)
	{
	  vector_free (cmd_vector);
	  return CMD_ERR_NO_MATCH;
	}
    }

  /* Check matched count. */
  matched_element = NULL;
  matched_count = 0;
  incomplete_count = 0;

  for (i = 0; i < vector_max (cmd_vector); i++) 
    if (vector_slot (cmd_vector,i) != NULL)
      {
	cmd_element = vector_slot (cmd_vector,i);

	if (match == vararg_match || index >= cmd_element->cmdsize)
	  {
	    matched_element = cmd_element;
#if 0
	    printf ("DEBUG: %s\n", cmd_element->string);
#endif
	    matched_count++;
	  }
	else
	  {
	    incomplete_count++;
	  }
      }
  
  /* Finish of using cmd_vector. */
  vector_free (cmd_vector);

  /* To execute command, matched_count must be 1.*/
  if (matched_count == 0) 
    {
      if (incomplete_count)
	return CMD_ERR_INCOMPLETE;
      else
	return CMD_ERR_NO_MATCH;
    }

  if (matched_count > 1) 
    return CMD_ERR_AMBIGUOUS;

  /* Argument treatment */
  varflag = 0;
  argc = 0;

  for (i = 0; i < vector_max (vline); i++)
    {
      if (varflag)
	argv[argc++] = vector_slot (vline, i);
      else
	{	  
	  vector descvec = vector_slot (matched_element->strvec, i);

	  if (vector_max (descvec) == 1)
	    {
	      struct desc *desc = vector_slot (descvec, 0);
	      char *str = desc->cmd;

	      if (CMD_VARARG (str))
		varflag = 1;

	      if (varflag || CMD_VARIABLE (str) || CMD_OPTION (str))
		argv[argc++] = vector_slot (vline, i);
	    }
	  else
	    argv[argc++] = vector_slot (vline, i);
	}

      if (argc >= CMD_ARGC_MAX)
	return CMD_ERR_EXEED_ARGC_MAX;
    }

  /* For vtysh execution. */
  if (cmd)
    *cmd = matched_element;

  if (matched_element->daemon)
    return CMD_SUCCESS_DAEMON;

  /* Execute matched command. */
  return (*matched_element->func) (matched_element, vty, argc, argv);
}

/* Execute command by argument readline. */
int
cmd_execute_command_strict (vector vline, struct vty *vty, 
			    struct cmd_element **cmd)
{
  int i;
  int index;
  vector cmd_vector;
  struct cmd_element *cmd_element;
  struct cmd_element *matched_element;
  unsigned int matched_count, incomplete_count;
  int argc;
  char *argv[CMD_ARGC_MAX];
  int varflag;
  enum match_type match = 0;
  char *command;

  /* Make copy of command element */
  cmd_vector = vector_copy (cmd_node_vector (cmdvec, vty->node));

  for (index = 0; index < vector_max (vline); index++) 
    {
      int ret;

      command = vector_slot (vline, index);

      match = cmd_filter_by_string (vector_slot (vline, index), 
				    cmd_vector, index);

      /* If command meets '.VARARG' then finish matching. */
      if (match == vararg_match)
	break;

      ret = is_cmd_ambiguous (command, cmd_vector, index, match);
      if (ret == 1)
	{
	  vector_free (cmd_vector);
	  return CMD_ERR_AMBIGUOUS;
	}
      if (ret == 2)
	{
	  vector_free (cmd_vector);
	  return CMD_ERR_NO_MATCH;
	}
    }

  /* Check matched count. */
  matched_element = NULL;
  matched_count = 0;
  incomplete_count = 0;
  for (i = 0; i < vector_max (cmd_vector); i++) 
    if (vector_slot (cmd_vector,i) != NULL)
      {
	cmd_element = vector_slot (cmd_vector,i);

	if (match == vararg_match || index >= cmd_element->cmdsize)
	  {
	    matched_element = cmd_element;
	    matched_count++;
	  }
	else
	  incomplete_count++;
      }
  
  /* Finish of using cmd_vector. */
  vector_free (cmd_vector);

  /* To execute command, matched_count must be 1.*/
  if (matched_count == 0) 
    {
      if (incomplete_count)
	return CMD_ERR_INCOMPLETE;
      else
	return CMD_ERR_NO_MATCH;
    }

  if (matched_count > 1) 
    return CMD_ERR_AMBIGUOUS;

  /* Argument treatment */
  varflag = 0;
  argc = 0;

  for (i = 0; i < vector_max (vline); i++)
    {
      if (varflag)
	argv[argc++] = vector_slot (vline, i);
      else
	{	  
	  vector descvec = vector_slot (matched_element->strvec, i);

	  if (vector_max (descvec) == 1)
	    {
	      struct desc *desc = vector_slot (descvec, 0);
	      char *str = desc->cmd;

	      if (CMD_VARARG (str))
		varflag = 1;
	  
	      if (varflag || CMD_VARIABLE (str) || CMD_OPTION (str))
		argv[argc++] = vector_slot (vline, i);
	    }
	  else
	    argv[argc++] = vector_slot (vline, i);
	}

      if (argc >= CMD_ARGC_MAX)
	return CMD_ERR_EXEED_ARGC_MAX;
    }

  /* For vtysh execution. */
  if (cmd)
    *cmd = matched_element;

  if (matched_element->daemon)
    return CMD_SUCCESS_DAEMON;

  /* Now execute matched command */
  return (*matched_element->func) (matched_element, vty, argc, argv);
}

/* Configration make from file. */
int
config_from_file (struct vty *vty, FILE *fp)
{
  int ret;
  vector vline;

  while (fgets (vty->buf, VTY_BUFSIZ, fp))
    {
      vline = cmd_make_strvec (vty->buf);

      /* In case of comment line */
      if (vline == NULL)
	continue;
      /* Execute configuration command : this is strict match */
      ret = cmd_execute_command_strict (vline, vty, NULL);

      /* Try again with setting node to CONFIG_NODE */
      if (ret != CMD_SUCCESS && ret != CMD_WARNING)
	{
	  if (vty->node == KEYCHAIN_KEY_NODE)
	    {
	      vty->node = KEYCHAIN_NODE;

	      ret = cmd_execute_command_strict (vline, vty, NULL);

	      if (ret != CMD_SUCCESS && ret != CMD_WARNING)
		{
		  vty->node = CONFIG_NODE;
		  ret = cmd_execute_command_strict (vline, vty, NULL);
		}
	    }
	  else
	    {
	      vty->node = CONFIG_NODE;
	      ret = cmd_execute_command_strict (vline, vty, NULL);
	    }
	}	  

      cmd_free_strvec (vline);

      if (ret != CMD_SUCCESS && ret != CMD_WARNING)
	return ret;
    }
  return CMD_SUCCESS;
}

/* Configration from terminal */
DEFUN (config_terminal,
       config_terminal_cmd,
       "configure terminal",
       "Configuration from vty interface\n"
       "Configuration terminal\n")
{
  if (vty_config_lock (vty))
    vty->node = CONFIG_NODE;
  else
    {
      vty_out (vty, "VTY configuration is locked by other VTY%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

/* Enable command */
DEFUN (enable, 
       config_enable_cmd,
       "enable",
       "Turn on privileged mode command\n")
{
  /* If enable password is NULL, change to ENABLE_NODE */
  if ((host.enable == NULL && host.enable_encrypt == NULL) ||
      vty->type == VTY_SHELL_SERV)
    vty->node = ENABLE_NODE;
  else
    vty->node = AUTH_ENABLE_NODE;

  return CMD_SUCCESS;
}

/* Disable command */
DEFUN (disable, 
       config_disable_cmd,
       "disable",
       "Turn off privileged mode command\n")
{
  if (vty->node == ENABLE_NODE)
    vty->node = VIEW_NODE;
  return CMD_SUCCESS;
}

/* Down vty node level. */
DEFUN (config_exit,
       config_exit_cmd,
       "exit",
       "Exit current mode and down to previous mode\n")
{
  switch (vty->node)
    {
    case VIEW_NODE:
    case ENABLE_NODE:
      if (vty_shell (vty))
	exit (0);
      else
	vty->status = VTY_CLOSE;
      break;
    case CONFIG_NODE:
      vty->node = ENABLE_NODE;
      vty_config_unlock (vty);
      break;
    case INTERFACE_NODE:
    case ZEBRA_NODE:
    case BGP_NODE:
    case RIP_NODE:
    case RIPNG_NODE:
    case OSPF_NODE:
    case OSPF6_NODE:
    case KEYCHAIN_NODE:
    case MASC_NODE:
    case RMAP_NODE:
    case VTY_NODE:
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

/* quit is alias of exit. */
ALIAS (config_exit,
       config_quit_cmd,
       "quit",
       "Exit current mode and down to previous mode\n");
       
/* End of configuration. */
DEFUN (config_end,
       config_end_cmd,
       "end",
       "End current mode and change to enable mode.")
{
  switch (vty->node)
    {
    case VIEW_NODE:
    case ENABLE_NODE:
      /* Nothing to do. */
      break;
    case CONFIG_NODE:
    case INTERFACE_NODE:
    case ZEBRA_NODE:
    case RIP_NODE:
    case RIPNG_NODE:
    case BGP_NODE:
    case BGP_VPNV4_NODE:
    case BGP_IPV4_NODE:
    case BGP_IPV4M_NODE:
    case BGP_IPV6_NODE:
    case RMAP_NODE:
    case OSPF_NODE:
    case OSPF6_NODE:
    case KEYCHAIN_NODE:
    case KEYCHAIN_KEY_NODE:
    case MASC_NODE:
    case VTY_NODE:
      vty_config_unlock (vty);
      vty->node = ENABLE_NODE;
      break;
    default:
      break;
    }
  return CMD_SUCCESS;
}

/* Show version. */
DEFUN (show_version,
       show_version_cmd,
       "show version",
       SHOW_STR
       "Displays zebra version\n")
{
  vty_out (vty, "Zebra %s (%s).%s", ZEBRA_VERSION,
	   host_name,
	   VTY_NEWLINE);
  vty_out (vty, "Copyright 1996-2004, Kunihiro Ishiguro.%s", VTY_NEWLINE);

  return CMD_SUCCESS;
}

/* Help display function for all node. */
DEFUN (config_help,
       config_help_cmd,
       "help",
       "Description of the interactive help system\n")
{
  vty_out (vty, 
	   "Zebra VTY provides advanced help feature.  When you need help,%s\
anytime at the command line please press '?'.%s\
%s\
If nothing matches, the help list will be empty and you must backup%s\
 until entering a '?' shows the available options.%s\
Two styles of help are provided:%s\
1. Full help is available when you are ready to enter a%s\
command argument (e.g. 'show ?') and describes each possible%s\
argument.%s\
2. Partial help is provided when an abbreviated argument is entered%s\
   and you want to know what arguments match the input%s\
   (e.g. 'show me?'.)%s%s", VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE,
	   VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE,
	   VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
  return CMD_SUCCESS;
}

/* Help display function for all node. */
DEFUN (config_list,
       config_list_cmd,
       "list",
       "Print command list\n")
{
  int i;
  struct cmd_node *cnode = vector_slot (cmdvec, vty->node);
  struct cmd_element *cmd;

  for (i = 0; i < vector_max (cnode->cmd_vector); i++)
    if ((cmd = vector_slot (cnode->cmd_vector, i)) != NULL)
      vty_out (vty, "  %s%s", cmd->string,
	       VTY_NEWLINE);
  return CMD_SUCCESS;
}

/* Write current configuration into file. */
DEFUN (config_write_file, 
       config_write_file_cmd,
       "write file",  
       "Write running configuration to memory, network, or terminal\n"
       "Write to configuration file\n")
{
  int i;
  int fd;
  struct cmd_node *node;
  char *config_file;
  char *config_file_tmp = NULL;
  char *config_file_sav = NULL;
  struct vty *file_vty;

  /* Check and see if we are operating under vtysh configuration */
  if (host.config == NULL)
    {
      vty_out (vty, "Can't save to configuration file, using vtysh.%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Get filename. */
  config_file = host.config;
  
  config_file_sav = malloc (strlen (config_file) + strlen (CONF_BACKUP_EXT) + 1);
  strcpy (config_file_sav, config_file);
  strcat (config_file_sav, CONF_BACKUP_EXT);


  config_file_tmp = malloc (strlen (config_file) + 8);
  sprintf (config_file_tmp, "%s.XXXXXX", config_file);
  
  /* Open file to configuration write. */
  fd = mkstemp (config_file_tmp);
  if (fd < 0)
    {
      vty_out (vty, "Can't open configuration file %s.%s", config_file_tmp,
	       VTY_NEWLINE);
      free (config_file_tmp);
      free (config_file_sav);
      return CMD_WARNING;
    }
  
  /* Make vty for configuration file. */
  file_vty = vty_new ();
  file_vty->fd = fd;
  file_vty->type = VTY_FILE;

  /* Config file header print. */
  vty_out (file_vty, "!\n! Zebra configuration saved from vty\n!   ");
  vty_time_print (file_vty, 1);
  vty_out (file_vty, "!\n");

  for (i = 0; i < vector_max (cmdvec); i++)
    if ((node = vector_slot (cmdvec, i)) && node->func)
      {
	if ((*node->func) (file_vty))
	  vty_out (file_vty, "!\n");
      }
  vty_close (file_vty);

  if (unlink (config_file_sav) != 0)
    if (errno != ENOENT)
      {
	vty_out (vty, "Can't unlink backup configuration file %s.%s", config_file_sav,
		 VTY_NEWLINE);
	free (config_file_sav);
	free (config_file_tmp);
	unlink (config_file_tmp);	
	return CMD_WARNING;
      }
  if (link (config_file, config_file_sav) != 0)
    {
      vty_out (vty, "Can't backup old configuration file %s.%s", config_file_sav,
	        VTY_NEWLINE);
      free (config_file_sav);
      free (config_file_tmp);
      unlink (config_file_tmp);
      return CMD_WARNING;
    }
  sync ();
  if (unlink (config_file) != 0)
    {
      vty_out (vty, "Can't unlink configuration file %s.%s", config_file,
	        VTY_NEWLINE);
      free (config_file_sav);
      free (config_file_tmp);
      unlink (config_file_tmp);
      return CMD_WARNING;      
    }
  if (link (config_file_tmp, config_file) != 0)
    {
      vty_out (vty, "Can't save configuration file %s.%s", config_file,
	       VTY_NEWLINE);
      free (config_file_sav);
      free (config_file_tmp);
      unlink (config_file_tmp);
      return CMD_WARNING;      
    }
  unlink (config_file_tmp);
  sync ();
  
  free (config_file_sav);
  free (config_file_tmp);
  vty_out (vty, "Configuration saved to %s%s", config_file,
	   VTY_NEWLINE);
  return CMD_SUCCESS;
}

ALIAS (config_write_file, 
       config_write_cmd,
       "write",  
       "Write running configuration to memory, network, or terminal\n");

ALIAS (config_write_file, 
       config_write_memory_cmd,
       "write memory",  
       "Write running configuration to memory, network, or terminal\n"
       "Write configuration to the file (same as write file)\n");

ALIAS (config_write_file, 
       copy_runningconfig_startupconfig_cmd,
       "copy running-config startup-config",  
       "Copy configuration\n"
       "Copy running config to... \n"
       "Copy running config to startup config (same as write file)\n");

/* Write current configuration into the terminal. */
DEFUN (config_write_terminal,
       config_write_terminal_cmd,
       "write terminal",
       "Write running configuration to memory, network, or terminal\n"
       "Write to terminal\n")
{
  int i;
  struct cmd_node *node;

  if (vty->type == VTY_SHELL_SERV)
    {
      for (i = 0; i < vector_max (cmdvec); i++)
	if ((node = vector_slot (cmdvec, i)) && node->func && node->vtysh)
	  {
	    if ((*node->func) (vty))
	      vty_out (vty, "!%s", VTY_NEWLINE);
	  }
    }
  else
    {
      vty_out (vty, "%sCurrent configuration:%s", VTY_NEWLINE,
	       VTY_NEWLINE);
      vty_out (vty, "!%s", VTY_NEWLINE);

      for (i = 0; i < vector_max (cmdvec); i++)
	if ((node = vector_slot (cmdvec, i)) && node->func)
	  {
	    if ((*node->func) (vty))
	      vty_out (vty, "!%s", VTY_NEWLINE);
	  }
      vty_out (vty, "end%s",VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

/* Write current configuration into the terminal. */
ALIAS (config_write_terminal,
       show_running_config_cmd,
       "show running-config",
       SHOW_STR
       "running configuration\n");

/* Write startup configuration into the terminal. */
DEFUN (show_startup_config,
       show_startup_config_cmd,
       "show startup-config",
       SHOW_STR
       "Contentes of startup configuration\n")
{
  char buf[BUFSIZ];
  FILE *confp;

  confp = fopen (host.config, "r");
  if (confp == NULL)
    {
      vty_out (vty, "Can't open configuration file [%s]%s",
	       host.config, VTY_NEWLINE);
      return CMD_WARNING;
    }

  while (fgets (buf, BUFSIZ, confp))
    {
      char *cp = buf;

      while (*cp != '\r' && *cp != '\n' && *cp != '\0')
	cp++;
      *cp = '\0';

      vty_out (vty, "%s%s", buf, VTY_NEWLINE);
    }

  fclose (confp);

  return CMD_SUCCESS;
}

/* Hostname configuration */
DEFUN (config_hostname, 
       hostname_cmd,
       "hostname WORD",
       "Set system's network name\n"
       "This system's network name\n")
{
  if (!isalpha((int) *argv[0]))
    {
      vty_out (vty, "Please specify string starting with alphabet%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (host.name)
    XFREE (0, host.name);
    
  host.name = strdup (argv[0]);
  return CMD_SUCCESS;
}

DEFUN (config_no_hostname, 
       no_hostname_cmd,
       "no hostname [HOSTNAME]",
       NO_STR
       "Reset system's network name\n"
       "Host name of this router\n")
{
  if (host.name)
    XFREE (0, host.name);
  host.name = NULL;
  return CMD_SUCCESS;
}

/* VTY interface password set. */
DEFUN (config_password, password_cmd,
       "password (8|) WORD",
       "Assign the terminal connection password\n"
       "Specifies a HIDDEN password will follow\n"
       "dummy string \n"
       "The HIDDEN line password string\n")
{
  /* Argument check. */
  if (argc == 0)
    {
      vty_out (vty, "Please specify password.%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (argc == 2)
    {
      if (*argv[0] == '8')
	{
	  if (host.password)
	    XFREE (0, host.password);
	  host.password = NULL;
	  if (host.password_encrypt)
	    XFREE (0, host.password_encrypt);
	  host.password_encrypt = XSTRDUP (0, strdup (argv[1]));
	  return CMD_SUCCESS;
	}
      else
	{
	  vty_out (vty, "Unknown encryption type.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }

  if (!isalnum ((int) *argv[0]))
    {
      vty_out (vty, 
	       "Please specify string starting with alphanumeric%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (host.password)
    XFREE (0, host.password);
  host.password = NULL;

  if (host.password_encrypt)
    XFREE (0, host.password_encrypt);
  host.password_encrypt = NULL;

  if (host.encrypt)
    host.password_encrypt = XSTRDUP (0, zencrypt (argv[0]));
  else
    host.password = XSTRDUP (0, argv[0]);

  return CMD_SUCCESS;
}

ALIAS (config_password, password_text_cmd,
       "password LINE",
       "Assign the terminal connection password\n"
       "The UNENCRYPTED (cleartext) line password\n");

/* VTY enable password set. */
DEFUN (config_enable_password, enable_password_cmd,
       "enable password (8|) WORD",
       "Modify enable password parameters\n"
       "Assign the privileged level password\n"
       "Specifies a HIDDEN password will follow\n"
       "dummy string \n"
       "The HIDDEN 'enable' password string\n")
{
  /* Argument check. */
  if (argc == 0)
    {
      vty_out (vty, "Please specify password.%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Crypt type is specified. */
  if (argc == 2)
    {
      if (*argv[0] == '8')
	{
	  if (host.enable)
	    XFREE (0, host.enable);
	  host.enable = NULL;

	  if (host.enable_encrypt)
	    XFREE (0, host.enable_encrypt);
	  host.enable_encrypt = XSTRDUP (0, argv[1]);

	  return CMD_SUCCESS;
	}
      else
	{
	  vty_out (vty, "Unknown encryption type.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }

  if (!isalnum ((int) *argv[0]))
    {
      vty_out (vty, 
	       "Please specify string starting with alphanumeric%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (host.enable)
    XFREE (0, host.enable);
  host.enable = NULL;

  if (host.enable_encrypt)
    XFREE (0, host.enable_encrypt);
  host.enable_encrypt = NULL;

  /* Plain password input. */
  if (host.encrypt)
    host.enable_encrypt = XSTRDUP (0, zencrypt (argv[0]));
  else
    host.enable = XSTRDUP (0, argv[0]);

  return CMD_SUCCESS;
}

ALIAS (config_enable_password,
       enable_password_text_cmd,
       "enable password LINE",
       "Modify enable password parameters\n"
       "Assign the privileged level password\n"
       "The UNENCRYPTED (cleartext) 'enable' password\n");

/* VTY enable password delete. */
DEFUN (no_config_enable_password, no_enable_password_cmd,
       "no enable password",
       NO_STR
       "Modify enable password parameters\n"
       "Assign the privileged level password\n")
{
  if (host.enable)
    XFREE (0, host.enable);
  host.enable = NULL;

  if (host.enable_encrypt)
    XFREE (0, host.enable_encrypt);
  host.enable_encrypt = NULL;

  return CMD_SUCCESS;
}
	
DEFUN (service_password_encrypt,
       service_password_encrypt_cmd,
       "service password-encryption",
       "Set up miscellaneous service\n"
       "Enable encrypted passwords\n")
{
  if (host.encrypt)
    return CMD_SUCCESS;

  host.encrypt = 1;

  if (host.password)
    {
      if (host.password_encrypt)
	XFREE (0, host.password_encrypt);
      host.password_encrypt = XSTRDUP (0, zencrypt (host.password));
    }
  if (host.enable)
    {
      if (host.enable_encrypt)
	XFREE (0, host.enable_encrypt);
      host.enable_encrypt = XSTRDUP (0, zencrypt (host.enable));
    }

  return CMD_SUCCESS;
}

DEFUN (no_service_password_encrypt,
       no_service_password_encrypt_cmd,
       "no service password-encryption",
       NO_STR
       "Set up miscellaneous service\n"
       "Enable encrypted passwords\n")
{
  if (! host.encrypt)
    return CMD_SUCCESS;

  host.encrypt = 0;

  if (host.password)
    {
      if (host.password_encrypt)
	XFREE (0, host.password_encrypt);
      host.password_encrypt = NULL;
    }

  if (host.enable)
    {
      if (host.enable_encrypt)
	XFREE (0, host.enable_encrypt);
      host.enable_encrypt = NULL;
    }

  return CMD_SUCCESS;
}

DEFUN (config_terminal_length, config_terminal_length_cmd,
       "terminal length <0-512>",
       "Set terminal line parameters\n"
       "Set number of lines on a screen\n"
       "Number of lines on screen (0 for no pausing)\n")
{
  int lines;
  char *endptr = NULL;

  lines = strtol (argv[0], &endptr, 10);
  if (lines < 0 || lines > 512 || *endptr != '\0')
    {
      vty_out (vty, "length is malformed%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  vty->lines = lines;

  return CMD_SUCCESS;
}

DEFUN (config_terminal_no_length, config_terminal_no_length_cmd,
       "terminal no length",
       "Set terminal line parameters\n"
       NO_STR
       "Set number of lines on a screen\n")
{
  vty->lines = -1;
  return CMD_SUCCESS;
}

DEFUN (service_terminal_length, service_terminal_length_cmd,
       "service terminal-length <0-512>",
       "Set up miscellaneous service\n"
       "System wide terminal length configuration\n"
       "Number of lines of VTY (0 means no line control)\n")
{
  int lines;
  char *endptr = NULL;

  lines = strtol (argv[0], &endptr, 10);
  if (lines < 0 || lines > 512 || *endptr != '\0')
    {
      vty_out (vty, "length is malformed%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  host.lines = lines;

  return CMD_SUCCESS;
}

DEFUN (no_service_terminal_length, no_service_terminal_length_cmd,
       "no service terminal-length [<0-512>]",
       NO_STR
       "Set up miscellaneous service\n"
       "System wide terminal length configuration\n"
       "Number of lines of VTY (0 means no line control)\n")
{
  host.lines = -1;
  return CMD_SUCCESS;
}

DEFUN (config_log_stdout,
       config_log_stdout_cmd,
       "log stdout",
       "Logging control\n"
       "Logging goes to stdout\n")
{
  zlog_set_flag (NULL, ZLOG_STDOUT);
  host.log_stdout = 1;
  return CMD_SUCCESS;
}

DEFUN (no_config_log_stdout,
       no_config_log_stdout_cmd,
       "no log stdout",
       NO_STR
       "Logging control\n"
       "Cancel logging to stdout\n")
{
  zlog_reset_flag (NULL, ZLOG_STDOUT);
  host.log_stdout = 0;
  return CMD_SUCCESS;
}

DEFUN (config_log_file,
       config_log_file_cmd,
       "log file FILENAME",
       "Logging control\n"
       "Logging to file\n"
       "Logging filename\n")
{
  int ret;
  char *cwd;
  char *fullpath;

  /* Path detection. */
  if (! IS_DIRECTORY_SEP (*argv[0]))
    {
      cwd = getcwd (NULL, MAXPATHLEN);
      fullpath = XMALLOC (MTYPE_TMP,
			  strlen (cwd) + strlen (argv[0]) + 2);
      sprintf (fullpath, "%s/%s", cwd, argv[0]);
    }
  else
    fullpath = argv[0];

  ret = zlog_set_file (NULL, ZLOG_FILE, fullpath);

  if (!ret)
    {
      vty_out (vty, "can't open logfile %s\n", argv[0]);
      return CMD_WARNING;
    }

  if (host.logfile)
    XFREE (MTYPE_TMP, host.logfile);

  host.logfile = strdup (argv[0]);

  return CMD_SUCCESS;
}

DEFUN (no_config_log_file,
       no_config_log_file_cmd,
       "no log file [FILENAME]",
       NO_STR
       "Logging control\n"
       "Cancel logging to file\n"
       "Logging file name\n")
{
  zlog_reset_file (NULL);

  if (host.logfile)
    XFREE (MTYPE_TMP, host.logfile);

  host.logfile = NULL;

  return CMD_SUCCESS;
}

DEFUN (config_log_syslog,
       config_log_syslog_cmd,
       "log syslog",
       "Logging control\n"
       "Logging goes to syslog\n")
{
  zlog_set_flag (NULL, ZLOG_SYSLOG);
  host.log_syslog = 1;
  zlog_default->facility = LOG_DAEMON;
  return CMD_SUCCESS;
}

DEFUN (config_log_syslog_facility,
       config_log_syslog_facility_cmd,
       "log syslog facility (kern|user|mail|daemon|auth|syslog|lpr|news|uucp|cron|local0|local1|local2|local3|local4|local5|local6|local7)",
       "Logging control\n"
       "Logging goes to syslog\n"
       "Facility parameter for syslog messages\n"
       "Kernel\n"
       "User process\n"
       "Mail system\n"
       "System daemons\n"
       "Authorization system\n"
       "Syslog itself\n"
       "Line printer system\n"
       "USENET news\n"
       "Unix-to-Unix copy system\n"
       "Cron/at facility\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n")
{
  int facility = LOG_DAEMON;

  zlog_set_flag (NULL, ZLOG_SYSLOG);
  host.log_syslog = 1;

  if (strncmp (argv[0], "kern", 1) == 0)
    facility = LOG_KERN;
  else if (strncmp (argv[0], "user", 2) == 0)
    facility = LOG_USER;
  else if (strncmp (argv[0], "mail", 1) == 0)
    facility = LOG_MAIL;
  else if (strncmp (argv[0], "daemon", 1) == 0)
    facility = LOG_DAEMON;
  else if (strncmp (argv[0], "auth", 1) == 0)
    facility = LOG_AUTH;
  else if (strncmp (argv[0], "syslog", 1) == 0)
    facility = LOG_SYSLOG;
  else if (strncmp (argv[0], "lpr", 2) == 0)
    facility = LOG_LPR;
  else if (strncmp (argv[0], "news", 1) == 0)
    facility = LOG_NEWS;
  else if (strncmp (argv[0], "uucp", 2) == 0)
    facility = LOG_UUCP;
  else if (strncmp (argv[0], "cron", 1) == 0)
    facility = LOG_CRON;
  else if (strncmp (argv[0], "local0", 6) == 0)
    facility = LOG_LOCAL0;
  else if (strncmp (argv[0], "local1", 6) == 0)
    facility = LOG_LOCAL1;
  else if (strncmp (argv[0], "local2", 6) == 0)
    facility = LOG_LOCAL2;
  else if (strncmp (argv[0], "local3", 6) == 0)
    facility = LOG_LOCAL3;
  else if (strncmp (argv[0], "local4", 6) == 0)
    facility = LOG_LOCAL4;
  else if (strncmp (argv[0], "local5", 6) == 0)
    facility = LOG_LOCAL5;
  else if (strncmp (argv[0], "local6", 6) == 0)
    facility = LOG_LOCAL6;
  else if (strncmp (argv[0], "local7", 6) == 0)
    facility = LOG_LOCAL7;

  zlog_default->facility = facility;

  return CMD_SUCCESS;
}

DEFUN (no_config_log_syslog,
       no_config_log_syslog_cmd,
       "no log syslog",
       NO_STR
       "Logging control\n"
       "Cancel logging to syslog\n")
{
  zlog_reset_flag (NULL, ZLOG_SYSLOG);
  host.log_syslog = 0;
  zlog_default->facility = LOG_DAEMON;
  return CMD_SUCCESS;
}

ALIAS (no_config_log_syslog,
       no_config_log_syslog_facility_cmd,
       "no log syslog facility (kern|user|mail|daemon|auth|syslog|lpr|news|uucp|cron|local0|local1|local2|local3|local4|local5|local6|local7)",
       NO_STR
       "Logging control\n"
       "Logging goes to syslog\n"
       "Facility parameter for syslog messages\n"
       "Kernel\n"
       "User process\n"
       "Mail system\n"
       "System daemons\n"
       "Authorization system\n"
       "Syslog itself\n"
       "Line printer system\n"
       "USENET news\n"
       "Unix-to-Unix copy system\n"
       "Cron/at facility\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n"
       "Local use\n");

DEFUN (config_log_trap,
       config_log_trap_cmd,
       "log trap (emergencies|alerts|critical|errors|warnings|notifications|informational|debugging)",
       "Logging control\n"
       "Limit logging to specifed level\n")
{
  int new_level ;
  
  for ( new_level = 0 ; zlog_priority [new_level] != NULL ; new_level ++ )
    {
    if ( strcmp ( argv[0], zlog_priority [new_level] ) == 0 )
      /* found new logging level */
      {
      zlog_default->maskpri = new_level;
      return CMD_SUCCESS;
      }
    }
  return CMD_ERR_NO_MATCH;
}

DEFUN (no_config_log_trap,
       no_config_log_trap_cmd,
       "no log trap",
       NO_STR
       "Logging control\n"
       "Permit all logging information\n")
{
  zlog_default->maskpri = LOG_DEBUG;
  return CMD_SUCCESS;
}

DEFUN (config_log_record_priority,
       config_log_record_priority_cmd,
       "log record-priority",
       "Logging control\n"
       "Log the priority of the message within the message\n")
{
  zlog_default->record_priority = 1 ;
  return CMD_SUCCESS;
}

DEFUN (no_config_log_record_priority,
       no_config_log_record_priority_cmd,
       "no log record-priority",
       NO_STR
       "Logging control\n"
       "Do not log the priority of the message within the message\n")
{
  zlog_default->record_priority = 0 ;
  return CMD_SUCCESS;
}


DEFUN (banner_motd_default,
       banner_motd_default_cmd,
       "banner motd default",
       "Set banner string\n"
       "Strings for motd\n"
       "Default string\n")
{
  host.motd = default_motd;
  return CMD_SUCCESS;
}

DEFUN (no_banner_motd,
       no_banner_motd_cmd,
       "no banner motd",
       NO_STR
       "Set banner string\n"
       "Strings for motd\n")
{
  host.motd = NULL;
  return CMD_SUCCESS;
}

/* Set config filename.  Called from vty.c */
void
host_config_set (char *filename)
{
  host.config = strdup (filename);
}

void
install_default (enum node_type node)
{
  install_element (node, &config_exit_cmd);
  install_element (node, &config_quit_cmd);
  install_element (node, &config_end_cmd);
  install_element (node, &config_help_cmd);
  install_element (node, &config_list_cmd);

  install_element (node, &config_write_terminal_cmd);
  install_element (node, &config_write_file_cmd);
  install_element (node, &config_write_memory_cmd);
  install_element (node, &config_write_cmd);
  install_element (node, &show_running_config_cmd);
}

/* Initialize command interface. Install basic nodes and commands. */
void
cmd_init (int terminal)
{
  /* Allocate initial top vector of commands. */
  cmdvec = vector_init (VECTOR_MIN_SIZE);

  /* Default host value settings. */
  host.name = NULL;
  host.password = NULL;
  host.enable = NULL;
  host.logfile = NULL;
  host.config = NULL;
  host.lines = -1;
  host.motd = default_motd;

  /* Install top nodes. */
  install_node (&view_node, NULL);
  install_node (&enable_node, NULL);
  install_node (&auth_node, NULL);
  install_node (&auth_enable_node, NULL);
  install_node (&config_node, config_write_host);

  /* Each node's basic commands. */
  install_element (VIEW_NODE, &show_version_cmd);
  if (terminal)
    {
      install_element (VIEW_NODE, &config_list_cmd);
      install_element (VIEW_NODE, &config_exit_cmd);
      install_element (VIEW_NODE, &config_quit_cmd);
      install_element (VIEW_NODE, &config_help_cmd);
      install_element (VIEW_NODE, &config_enable_cmd);
      install_element (VIEW_NODE, &config_terminal_length_cmd);
      install_element (VIEW_NODE, &config_terminal_no_length_cmd);
    }

  if (terminal)
    {
      install_default (ENABLE_NODE);
      install_element (ENABLE_NODE, &config_disable_cmd);
      install_element (ENABLE_NODE, &config_terminal_cmd);
      install_element (ENABLE_NODE, &copy_runningconfig_startupconfig_cmd);
    }
  install_element (ENABLE_NODE, &show_startup_config_cmd);
  install_element (ENABLE_NODE, &show_version_cmd);
  install_element (ENABLE_NODE, &config_terminal_length_cmd);
  install_element (ENABLE_NODE, &config_terminal_no_length_cmd);

  if (terminal)
    install_default (CONFIG_NODE);
  install_element (CONFIG_NODE, &hostname_cmd);
  install_element (CONFIG_NODE, &no_hostname_cmd);
  install_element (CONFIG_NODE, &password_cmd);
  install_element (CONFIG_NODE, &password_text_cmd);
  install_element (CONFIG_NODE, &enable_password_cmd);
  install_element (CONFIG_NODE, &enable_password_text_cmd);
  install_element (CONFIG_NODE, &no_enable_password_cmd);
  if (terminal)
    {
      install_element (CONFIG_NODE, &config_log_stdout_cmd);
      install_element (CONFIG_NODE, &no_config_log_stdout_cmd);
      install_element (CONFIG_NODE, &config_log_file_cmd);
      install_element (CONFIG_NODE, &no_config_log_file_cmd);
      install_element (CONFIG_NODE, &config_log_syslog_cmd);
      install_element (CONFIG_NODE, &config_log_syslog_facility_cmd);
      install_element (CONFIG_NODE, &no_config_log_syslog_cmd);
      install_element (CONFIG_NODE, &no_config_log_syslog_facility_cmd);
      install_element (CONFIG_NODE, &config_log_trap_cmd);
      install_element (CONFIG_NODE, &no_config_log_trap_cmd);
      install_element (CONFIG_NODE, &config_log_record_priority_cmd);
      install_element (CONFIG_NODE, &no_config_log_record_priority_cmd);
      install_element (CONFIG_NODE, &service_password_encrypt_cmd);
      install_element (CONFIG_NODE, &no_service_password_encrypt_cmd);
      install_element (CONFIG_NODE, &banner_motd_default_cmd);
      install_element (CONFIG_NODE, &no_banner_motd_cmd);
      install_element (CONFIG_NODE, &service_terminal_length_cmd);
      install_element (CONFIG_NODE, &no_service_terminal_length_cmd);
    }

  srand(time(NULL));
}
