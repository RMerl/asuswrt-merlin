/*
 * IS-IS Rout(e)ing protocol - isis_misc.h
 *                             Miscellanous routines
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <zebra.h>

#include "stream.h"
#include "vty.h"
#include "hash.h"
#include "if.h"
#include "command.h"

#include "isisd/dict.h"
#include "isisd/isis_constants.h"
#include "isisd/isis_common.h"
#include "isisd/isis_flags.h"
#include "isisd/isis_circuit.h"
#include "isisd/isis_csm.h"
#include "isisd/isisd.h"
#include "isisd/isis_misc.h"

#include "isisd/isis_tlv.h"
#include "isisd/isis_lsp.h"
#include "isisd/isis_constants.h"
#include "isisd/isis_adjacency.h"
#include "isisd/isis_dynhn.h"

/* staticly assigned vars for printing purposes */
struct in_addr new_prefix;
/* len of xxxx.xxxx.xxxx + place for #0 termination */
char sysid[15];
/* len of xxxx.xxxx.xxxx + place for #0 termination */
char snpa[15];
/* len of xx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xx */
char isonet[51];
/* + place for #0 termination */
/* len of xxxx.xxxx.xxxx.xx.xx + place for #0 termination */
char lspid[21];
/* len of xxYxxMxWxdxxhxxmxxs + place for #0 termination */
char datestring[20];
char nlpidstring[30];

/*
 * This converts the isonet to its printable format
 */
const char *
isonet_print (u_char * from, int len)
{
  int i = 0;
  char *pos = isonet;

  if (!from)
    return "unknown";

  while (i < len)
    {
      if (i & 1)
	{
	  sprintf (pos, "%02x", *(from + i));
	  pos += 2;
	}
      else
	{
	  if (i == (len - 1))
	    {			/* No dot at the end of address */
	      sprintf (pos, "%02x", *(from + i));
	      pos += 2;
	    }
	  else
	    {
	      sprintf (pos, "%02x.", *(from + i));
	      pos += 3;
	    }
	}
      i++;
    }
  *(pos) = '\0';
  return isonet;
}

/*
 * Returns 0 on error, length of buff on ok
 * extract dot from the dotted str, and insert all the number in a buff 
 */
int
dotformat2buff (u_char * buff, const char * dotted)
{
  int dotlen, len = 0;
  const char *pos = dotted;
  u_char number[3];
  int nextdotpos = 2;

  number[2] = '\0';
  dotlen = strlen(dotted);
  if (dotlen > 50)
    {
      /* this can't be an iso net, its too long */
      return 0;
    }

  while ((pos - dotted) < dotlen && len < 20)
    {
      if (*pos == '.')
	{
	  /* we expect the . at 2, and than every 5 */
	  if ((pos - dotted) != nextdotpos)
	    {
	      len = 0;
	      break;
	    }
	  nextdotpos += 5;
	  pos++;
	  continue;
	}
      /* we must have at least two chars left here */
      if (dotlen - (pos - dotted) < 2)
	{
	  len = 0;
	  break;
	}

      if ((isxdigit ((int) *pos)) && (isxdigit ((int) *(pos + 1))))
	{
	  memcpy (number, pos, 2);
	  pos += 2;
	}
      else
	{
	  len = 0;
	  break;
	}

      *(buff + len) = (char) strtol ((char *)number, NULL, 16);
      len++;
    }

  return len;
}

/*
 * conversion of XXXX.XXXX.XXXX to memory
 */
int
sysid2buff (u_char * buff, const char * dotted)
{
  int len = 0;
  const char *pos = dotted;
  u_char number[3];

  number[2] = '\0';
  // surely not a sysid_string if not 14 length
  if (strlen (dotted) != 14)
    {
      return 0;
    }

  while (len < ISIS_SYS_ID_LEN)
    {
      if (*pos == '.')
	{
	  /* the . is not positioned correctly */
	  if (((pos - dotted) != 4) && ((pos - dotted) != 9))
	    {
	      len = 0;
	      break;
	    }
	  pos++;
	  continue;
	}
      if ((isxdigit ((int) *pos)) && (isxdigit ((int) *(pos + 1))))
	{
	  memcpy (number, pos, 2);
	  pos += 2;
	}
      else
	{
	  len = 0;
	  break;
	}

      *(buff + len) = (char) strtol ((char *)number, NULL, 16);
      len++;
    }

  return len;

}

/*
 * converts the nlpids struct (filled by TLV #129)
 * into a string
 */

char *
nlpid2string (struct nlpids *nlpids)
{
  char *pos = nlpidstring;
  int i;

  for (i = 0; i < nlpids->count; i++)
    {
      switch (nlpids->nlpids[i])
	{
	case NLPID_IP:
	  pos += sprintf (pos, "IPv4");
	  break;
	case NLPID_IPV6:
	  pos += sprintf (pos, "IPv6");
	  break;
	case NLPID_SNAP:
	  pos += sprintf (pos, "SNAP");
	  break;
	case NLPID_CLNP:
	  pos += sprintf (pos, "CLNP");
	  break;
	case NLPID_ESIS:
	  pos += sprintf (pos, "ES-IS");
	  break;
	default:
	  pos += sprintf (pos, "unknown");
	  break;
	}
      if (nlpids->count - i > 1)
	pos += sprintf (pos, ", ");

    }

  *(pos) = '\0';

  return nlpidstring;
}

/*
 *  supports the given af ?
 */
int
speaks (struct nlpids *nlpids, int family)
{
  int i, speaks = 0;

  if (nlpids == (struct nlpids *) NULL)
    return speaks;
  for (i = 0; i < nlpids->count; i++)
    {
      if (family == AF_INET && nlpids->nlpids[i] == NLPID_IP)
	speaks = 1;
      if (family == AF_INET6 && nlpids->nlpids[i] == NLPID_IPV6)
	speaks = 1;
    }

  return speaks;
}

/*
 * Returns 0 on error, IS-IS Circuit Type on ok
 */
int
string2circuit_t (const char * str)
{

  if (!str)
    return 0;

  if (!strcmp (str, "level-1"))
    return IS_LEVEL_1;

  if (!strcmp (str, "level-2-only") || !strcmp (str, "level-2"))
    return IS_LEVEL_2;

  if (!strcmp (str, "level-1-2"))
    return IS_LEVEL_1_AND_2;

  return 0;
}

const char *
circuit_state2string (int state)
{

  switch (state)
    {
    case C_STATE_INIT:
      return "Init";
    case C_STATE_CONF:
      return "Config";
    case C_STATE_UP:
      return "Up";
    default:
      return "Unknown";
    }
  return NULL;
}

const char *
circuit_type2string (int type)
{

  switch (type)
    {
    case CIRCUIT_T_P2P:
      return "p2p";
    case CIRCUIT_T_BROADCAST:
      return "lan";
    case CIRCUIT_T_LOOPBACK:
      return "loopback";
    default:
      return "Unknown";
    }
  return NULL;
}

const char *
circuit_t2string (int circuit_t)
{
  switch (circuit_t)
    {
    case IS_LEVEL_1:
      return "L1";
    case IS_LEVEL_2:
      return "L2";
    case IS_LEVEL_1_AND_2:
      return "L1L2";
    default:
      return "??";
    }

  return NULL;			/* not reached */
}

const char *
syst2string (int type)
{
  switch (type)
    {
    case ISIS_SYSTYPE_ES:
      return "ES";
    case ISIS_SYSTYPE_IS:
      return "IS";
    case ISIS_SYSTYPE_L1_IS:
      return "1";
    case ISIS_SYSTYPE_L2_IS:
      return "2";
    default:
      return "??";
    }

  return NULL;			/* not reached */
}

/*
 * Print functions - we print to static vars
 */
const char *
snpa_print (u_char * from)
{
  int i = 0;
  u_char *pos = (u_char *)snpa;

  if (!from)
    return "unknown";

  while (i < ETH_ALEN - 1)
    {
      if (i & 1)
	{
	  sprintf ((char *)pos, "%02x.", *(from + i));
	  pos += 3;
	}
      else
	{
	  sprintf ((char *)pos, "%02x", *(from + i));
	  pos += 2;

	}
      i++;
    }

  sprintf ((char *)pos, "%02x", *(from + (ISIS_SYS_ID_LEN - 1)));
  pos += 2;
  *(pos) = '\0';

  return snpa;
}

const char *
sysid_print (u_char * from)
{
  int i = 0;
  char *pos = sysid;

  if (!from)
    return "unknown";

  while (i < ISIS_SYS_ID_LEN - 1)
    {
      if (i & 1)
	{
	  sprintf (pos, "%02x.", *(from + i));
	  pos += 3;
	}
      else
	{
	  sprintf (pos, "%02x", *(from + i));
	  pos += 2;

	}
      i++;
    }

  sprintf (pos, "%02x", *(from + (ISIS_SYS_ID_LEN - 1)));
  pos += 2;
  *(pos) = '\0';

  return sysid;
}

const char *
rawlspid_print (u_char * from)
{
  char *pos = lspid;
  if (!from)
    return "unknown";
  memcpy (pos, sysid_print (from), 15);
  pos += 14;
  sprintf (pos, ".%02x", LSP_PSEUDO_ID (from));
  pos += 3;
  sprintf (pos, "-%02x", LSP_FRAGMENT (from));
  pos += 3;

  *(pos) = '\0';

  return lspid;
}

const char *
time2string (u_int32_t time)
{
  char *pos = datestring;
  u_int32_t rest;

  if (time == 0)
    return "-";

  if (time / SECS_PER_YEAR)
    pos += sprintf (pos, "%uY", time / SECS_PER_YEAR);
  rest = time % SECS_PER_YEAR;
  if (rest / SECS_PER_MONTH)
    pos += sprintf (pos, "%uM", rest / SECS_PER_MONTH);
  rest = rest % SECS_PER_MONTH;
  if (rest / SECS_PER_WEEK)
    pos += sprintf (pos, "%uw", rest / SECS_PER_WEEK);
  rest = rest % SECS_PER_WEEK;
  if (rest / SECS_PER_DAY)
    pos += sprintf (pos, "%ud", rest / SECS_PER_DAY);
  rest = rest % SECS_PER_DAY;
  if (rest / SECS_PER_HOUR)
    pos += sprintf (pos, "%uh", rest / SECS_PER_HOUR);
  rest = rest % SECS_PER_HOUR;
  if (rest / SECS_PER_MINUTE)
    pos += sprintf (pos, "%um", rest / SECS_PER_MINUTE);
  rest = rest % SECS_PER_MINUTE;
  if (rest)
    pos += sprintf (pos, "%us", rest);

  *(pos) = 0;

  return datestring;
}

/*
 * routine to decrement a timer by a random
 * number
 *
 * first argument is the timer and the second is
 * the jitter
 */
unsigned long
isis_jitter (unsigned long timer, unsigned long jitter)
{
  int j, k;

  if (jitter >= 100)
    return timer;

  if (timer == 1)
    return timer;
  /* 
   * randomizing just the percent value provides
   * no good random numbers - hence the spread
   * to RANDOM_SPREAD (100000), which is ok as
   * most IS-IS timers are no longer than 16 bit
   */

  j = 1 + (int) ((RANDOM_SPREAD * rand ()) / (RAND_MAX + 1.0));

  k = timer - (timer * (100 - jitter)) / 100;

  timer = timer - (k * j / RANDOM_SPREAD);

  return timer;
}

struct in_addr
newprefix2inaddr (u_char * prefix_start, u_char prefix_masklen)
{
  memset (&new_prefix, 0, sizeof (new_prefix));
  memcpy (&new_prefix, prefix_start, (prefix_masklen & 0x3F) ?
	  ((((prefix_masklen & 0x3F) - 1) >> 3) + 1) : 0);
  return new_prefix;
}

/*
 * Returns host.name if any, otherwise
 * it returns the system hostname.
 */
const char *
unix_hostname (void)
{
  static struct utsname names;
  const char *hostname;

  hostname = host.name;
  if (!hostname)
    {
      uname (&names);
      hostname = names.nodename;
    }

  return hostname;
}

/*
 * Returns the dynamic hostname associated with the passed system ID.
 * If no dynamic hostname found then returns formatted system ID.
 */
const char *
print_sys_hostname (u_char *sysid)
{
  struct isis_dynhn *dyn;

  if (!sysid)
    return "nullsysid";

  /* For our system ID return our host name */
  if (memcmp(sysid, isis->sysid, ISIS_SYS_ID_LEN) == 0)
    return unix_hostname();

  dyn = dynhn_find_by_id (sysid);
  if (dyn)
    return (const char *)dyn->name.name;

  return sysid_print (sysid);
}

/*
 * This function is a generic utility that logs data of given length.
 * Move this to a shared lib so that any protocol can use it.
 */
void
zlog_dump_data (void *data, int len)
{
  int i;
  unsigned char *p;
  unsigned char c;
  char bytestr[4];
  char addrstr[10];
  char hexstr[ 16*3 + 5];
  char charstr[16*1 + 5];

  p = data;
  memset (bytestr, 0, sizeof(bytestr));
  memset (addrstr, 0, sizeof(addrstr));
  memset (hexstr, 0, sizeof(hexstr));
  memset (charstr, 0, sizeof(charstr));

  for (i = 1; i <= len; i++)
  {
    c = *p;
    if (isalnum (c) == 0)
      c = '.';

    /* store address for this line */
    if ((i % 16) == 1)
      snprintf (addrstr, sizeof(addrstr), "%p", p);

    /* store hex str (for left side) */
    snprintf (bytestr, sizeof (bytestr), "%02X ", *p);
    strncat (hexstr, bytestr, sizeof (hexstr) - strlen (hexstr) - 1);

    /* store char str (for right side) */
    snprintf (bytestr, sizeof (bytestr), "%c", c);
    strncat (charstr, bytestr, sizeof (charstr) - strlen (charstr) - 1);

    if ((i % 16) == 0)
    {
      /* line completed */
      zlog_debug ("[%8.8s]   %-50.50s  %s", addrstr, hexstr, charstr);
      hexstr[0] = 0;
      charstr[0] = 0;
    }
    else if ((i % 8) == 0)
    {
      /* half line: add whitespaces */
      strncat (hexstr, "  ", sizeof (hexstr) - strlen (hexstr) - 1);
      strncat (charstr, " ", sizeof (charstr) - strlen (charstr) - 1);
    }
    p++; /* next byte */
  }

  /* print rest of buffer if not empty */
  if (strlen (hexstr) > 0)
    zlog_debug ("[%8.8s]   %-50.50s  %s", addrstr, hexstr, charstr);
  return;
}
