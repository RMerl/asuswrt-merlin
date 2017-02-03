/*
* getethertype.c
*
* This file was part of the NYS Library.
*
** The NYS Library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public License as
** published by the Free Software Foundation; either version 2 of the
** License, or (at your option) any later version.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/********************************************************************
* Description: Ethertype name service switch and the ethertypes 
* database access functions
* Author: Nick Fedchik <fnm@ukrsat.com>
* Checker: Bart De Schuymer <bdschuym@pandora.be>
* Origin: uClibc-0.9.16/libc/inet/getproto.c
* Created at: Mon Nov 11 12:20:11 EET 2002
********************************************************************/

#include <ctype.h>
#include <features.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ether.h>
#include <net/ethernet.h>

#include "ethernetdb.h"

#define	MAXALIASES	35

static FILE *etherf = NULL;
static char line[BUFSIZ + 1];
static struct ethertypeent et_ent;
static char *ethertype_aliases[MAXALIASES];
static int ethertype_stayopen;

void setethertypeent(int f)
{
	if (etherf == NULL)
		etherf = fopen(_PATH_ETHERTYPES, "r");
	else
		rewind(etherf);
	ethertype_stayopen |= f;
}

void endethertypeent(void)
{
	if (etherf) {
		fclose(etherf);
		etherf = NULL;
	}
	ethertype_stayopen = 0;
}

struct ethertypeent *getethertypeent(void)
{
	char *e;
	char *endptr;
	register char *cp, **q;

	if (etherf == NULL
	    && (etherf = fopen(_PATH_ETHERTYPES, "r")) == NULL) {
		return (NULL);
	}

again:
	if ((e = fgets(line, BUFSIZ, etherf)) == NULL) {
		return (NULL);
	}
	if (*e == '#')
		goto again;
	cp = strpbrk(e, "#\n");
	if (cp == NULL)
		goto again;
	*cp = '\0';
	et_ent.e_name = e;
	cp = strpbrk(e, " \t");
	if (cp == NULL)
		goto again;
	*cp++ = '\0';
	while (*cp == ' ' || *cp == '\t')
		cp++;
	e = strpbrk(cp, " \t");
	if (e != NULL)
		*e++ = '\0';
// Check point
	et_ent.e_ethertype = strtol(cp, &endptr, 16);
	if (*endptr != '\0'
	    || (et_ent.e_ethertype < ETH_ZLEN
		|| et_ent.e_ethertype > 0xFFFF))
		goto again;	// Skip invalid etherproto type entry
	q = et_ent.e_aliases = ethertype_aliases;
	if (e != NULL) {
		cp = e;
		while (cp && *cp) {
			if (*cp == ' ' || *cp == '\t') {
				cp++;
				continue;
			}
			if (q < &ethertype_aliases[MAXALIASES - 1])
				*q++ = cp;
			cp = strpbrk(cp, " \t");
			if (cp != NULL)
				*cp++ = '\0';
		}
	}
	*q = NULL;
	return (&et_ent);
}


struct ethertypeent *getethertypebyname(const char *name)
{
	register struct ethertypeent *e;
	register char **cp;

	setethertypeent(ethertype_stayopen);
	while ((e = getethertypeent()) != NULL) {
		if (strcasecmp(e->e_name, name) == 0)
			break;
		for (cp = e->e_aliases; *cp != 0; cp++)
			if (strcasecmp(*cp, name) == 0)
				goto found;
	}
found:
	if (!ethertype_stayopen)
		endethertypeent();
	return (e);
}

struct ethertypeent *getethertypebynumber(int type)
{
	register struct ethertypeent *e;

	setethertypeent(ethertype_stayopen);
	while ((e = getethertypeent()) != NULL)
		if (e->e_ethertype == type)
			break;
	if (!ethertype_stayopen)
		endethertypeent();
	return (e);
}
