/*
 * support/export/hostname.c
 *
 * Functions for hostname.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*
#define TEST
*/

#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <xlog.h>
#ifdef TEST
#define xmalloc malloc
#else
#include "xmalloc.h"
#include "misc.h"
#endif

#define ALIGNMENT	sizeof (char *)

static int
align (int len, int al)
{
  int i;
  i = len % al;
  if (i)
    len += al - i;
  return len;
}

struct hostent *
get_hostent (const char *addr, int len, int type)
{
  struct hostent *cp;
  int len_ent;
  const char *name;
  int len_name;
  int num_aliases = 1;
  int len_aliases = sizeof (char *);
  int num_addr_list = 1;
  int len_addr_list = sizeof (char *);
  int pos;
  struct in_addr *ipv4;

  switch (type)
    {
    case AF_INET:
      ipv4 = (struct in_addr *) addr;
      name = inet_ntoa (*ipv4);
      break;

    default:
      return NULL;
    }

  len_ent = align (sizeof (*cp), ALIGNMENT);
  len_name = align (strlen (name) + 1, ALIGNMENT);

  num_addr_list++;
  len_addr_list += align (len, ALIGNMENT) + sizeof (char *);

  cp = (struct hostent *) xmalloc (len_ent + len_name + len_aliases
				   + len_addr_list);

  cp->h_addrtype = type;
  cp->h_length = len;
  pos = len_ent;
  cp->h_name = (char *) &(((char *) cp) [pos]);
  strcpy (cp->h_name, name);

  pos += len_name;
  cp->h_aliases = (char **) &(((char *) cp) [pos]);
  pos += num_aliases * sizeof (char *);
  cp->h_aliases [0] = NULL;

  pos = len_ent + len_name + len_aliases;
  cp->h_addr_list = (char **) &(((char *) cp) [pos]);
  pos += num_addr_list * sizeof (char *);
  cp->h_addr_list [0] = (char *) &(((char *) cp) [pos]);
  memcpy (cp->h_addr_list [0], addr, cp->h_length);
  pos += align (cp->h_length, ALIGNMENT);
  cp->h_addr_list [1] = NULL;

  return cp;
}

struct hostent *
hostent_dup (struct hostent *hp)
{
  int len_ent = align (sizeof (*hp), ALIGNMENT);
  int len_name = align (strlen (hp->h_name) + 1, ALIGNMENT);
  int num_aliases = 1;
  int len_aliases = sizeof (char *);
  int num_addr_list = 1;
  int len_addr_list = sizeof (char *);
  int pos, i;
  char **sp;
  struct hostent *cp;

  for (sp = hp->h_aliases; sp && *sp; sp++)
    {
      num_aliases++;
      len_aliases += align (strlen (*sp) + 1, ALIGNMENT)
		     + sizeof (char *);
    }

  for (sp = hp->h_addr_list; *sp; sp++)
    {
      num_addr_list++;
      len_addr_list += align (hp->h_length, ALIGNMENT)
		       + sizeof (char *);
    }

  cp = (struct hostent *) xmalloc (len_ent + len_name + len_aliases
				   + len_addr_list);

  *cp = *hp;
  pos = len_ent;
  cp->h_name = (char *) &(((char *) cp) [pos]);
  strcpy (cp->h_name, hp->h_name);

  pos += len_name;
  cp->h_aliases = (char **) &(((char *) cp) [pos]);
  pos += num_aliases * sizeof (char *);
  for (sp = hp->h_aliases, i = 0; i < num_aliases; i++, sp++)
    if (sp && *sp)
      {
	cp->h_aliases [i] = (char *) &(((char *) cp) [pos]);
	strcpy (cp->h_aliases [i], *sp);
	pos += align (strlen (*sp) + 1, ALIGNMENT);
      }
    else
      cp->h_aliases [i] = NULL;

  pos = len_ent + len_name + len_aliases;
  cp->h_addr_list = (char **) &(((char *) cp) [pos]);
  pos += num_addr_list * sizeof (char *);
  for (sp = hp->h_addr_list, i = 0; i < num_addr_list; i++, sp++)
    if (*sp)
      {
	cp->h_addr_list [i] = (char *) &(((char *) cp) [pos]);
	memcpy (cp->h_addr_list [i], *sp, hp->h_length);
	pos += align (hp->h_length, ALIGNMENT);
      }
    else
      cp->h_addr_list [i] = *sp;

  return cp;
}

static int
is_hostname(const char *sp)
{
  if (*sp == '\0' || *sp == '@')
    return 0;

  for (; *sp; sp++)
    {
      if (*sp == '*' || *sp == '?' || *sp == '[' || *sp == '/')
	return 0;
      if (*sp == '\\' && sp[1])
	sp++;
    }

  return 1;
}

int
matchhostname (const char *h1, const char *h2)
{
  struct hostent *hp1, *hp2;
  int status;

  if (strcasecmp (h1, h2) == 0)
    return 1;

  if (!is_hostname (h1) || !is_hostname (h2))
    return 0;

  hp1 = gethostbyname (h1);
  if (hp1 == NULL)
    return 0;

  hp1 = hostent_dup (hp1);

  hp2 = gethostbyname (h2);
  if (hp2)
    {
      if (strcasecmp (hp1->h_name, hp2->h_name) == 0)
	status = 1;
      else
	{
	  char **ap1, **ap2;

	  status = 0;
	  for (ap1 = hp1->h_addr_list; *ap1 && status == 0; ap1++)
	    for (ap2 = hp2->h_addr_list; *ap2; ap2++)
	      if (memcmp (*ap1, *ap2, sizeof (struct in_addr)) == 0)
		{
		  status = 1;
		  break;
		}
	}
    }
  else
    status = 0;

  free (hp1);
  return status;
}


/* Map IP to hostname, and then map back to addr to make sure it is a
 * reliable hostname
 */
struct hostent *
get_reliable_hostbyaddr(const char *addr, int len, int type)
{
	struct hostent *hp = NULL;

	struct hostent *reverse;
	struct hostent *forward;
	char **sp;

	reverse = gethostbyaddr (addr, len, type);
	if (!reverse)
		return NULL;

	/* must make sure the hostent is authorative. */

	reverse = hostent_dup (reverse);
	forward = gethostbyname (reverse->h_name);

	if (forward) {
		/* now make sure the "addr" is in the list */
		for (sp = forward->h_addr_list ; *sp ; sp++) {
			if (memcmp (*sp, addr, forward->h_length) == 0)
				break;
		}

		if (*sp) {
			/* it's valid */
			hp = hostent_dup (forward);
		}
		else {
			/* it was a FAKE */
			xlog (L_WARNING, "Fake hostname %s for %s - forward lookup doesn't match reverse",
			      reverse->h_name, inet_ntoa(*(struct in_addr*)addr));
		}
	}
	else {
		/* never heard of it. misconfigured DNS? */
		xlog (L_WARNING, "Fake hostname %s for %s - forward lookup doesn't exist",
		      reverse->h_name, inet_ntoa(*(struct in_addr*)addr));
	}

	free (reverse);
	return hp;
}


#ifdef TEST
void
print_host (struct hostent *hp)
{
  char **sp;

  if (hp)
    {
      printf ("official hostname: %s\n", hp->h_name);
      printf ("aliases:\n");
      for (sp = hp->h_aliases; *sp; sp++)
	printf ("  %s\n", *sp);
      printf ("IP addresses:\n");
      for (sp = hp->h_addr_list; *sp; sp++)
	printf ("  %s\n", inet_ntoa (*(struct in_addr *) *sp));
    }
  else
    printf ("Not host information\n");
}

int
main (int argc, char **argv)
{
  struct hostent *hp = gethostbyname (argv [1]);
  struct hostent *cp;
  struct in_addr addr;

  print_host (hp);

  if (hp)
    {
      cp = hostent_dup (hp);
      print_host (cp);
      free (cp);
    }
  printf ("127.0.0.1 == %s: %d\n", argv [1],
	  matchhostname ("127.0.0.1", argv [1]));
  addr.s_addr = inet_addr(argv [2]);
  printf ("%s\n", inet_ntoa (addr));
  cp = get_hostent ((const char *)&addr, sizeof(addr), AF_INET);
  print_host (cp);
  return 0;
}
#endif
