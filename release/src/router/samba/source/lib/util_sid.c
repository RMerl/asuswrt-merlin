/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Luke Kenneth Caseson Leighton 1998-1999
   Copyright (C) Jeremy Allison  1999
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"


extern int DEBUGLEVEL;
DOM_SID global_sam_sid;
extern pstring global_myname;
extern fstring global_myworkgroup;

/*
 * Some useful sids
 */

DOM_SID global_sid_S_1_5_0x20; /* local well-known domain */
DOM_SID global_sid_World_Domain;    /* everyone */
DOM_SID global_sid_World;    /* everyone */
DOM_SID global_sid_Creator_Owner_Domain;    /* Creator Owner */
DOM_SID global_sid_Creator_Owner;    /* Creator Owner */
DOM_SID global_sid_NT_Authority;    /* NT Authority */

typedef struct _known_sid_users {
	uint32 rid;
	uint8 sid_name_use;
	char *known_user_name;
} known_sid_users;

/* static known_sid_users no_users[] = {{0, 0, NULL}}; */
static known_sid_users everyone_users[] = {{ 0, SID_NAME_WKN_GRP, "Everyone" }, {0, 0, NULL}};
static known_sid_users creator_owner_users[] = {{ 0, SID_NAME_ALIAS, "Creator Owner" }, {0, 0, NULL}};
static known_sid_users nt_authority_users[] = {{ 1, SID_NAME_ALIAS, "Dialup" },
											   { 2, SID_NAME_ALIAS, "Network"},
											   { 3, SID_NAME_ALIAS, "Batch"},
											   { 4, SID_NAME_ALIAS, "Interactive"},
											   { 6, SID_NAME_ALIAS, "Service"},
											   { 7, SID_NAME_ALIAS, "AnonymousLogon"},
											   { 8, SID_NAME_ALIAS, "Proxy"},
											   { 9, SID_NAME_ALIAS, "ServerLogon"},
											   {0, 0, NULL}};

static struct sid_name_map_info
{
	DOM_SID *sid;
	char *name;
	known_sid_users *known_users;
}
sid_name_map[] =
{
	{ &global_sam_sid, global_myname, NULL},
	{ &global_sam_sid, global_myworkgroup, NULL},
	{ &global_sid_S_1_5_0x20, "BUILTIN", NULL},
	{ &global_sid_World_Domain, "", &everyone_users[0] },
	{ &global_sid_Creator_Owner_Domain, "", &creator_owner_users[0] },
	{ &global_sid_NT_Authority, "NT Authority", &nt_authority_users[0] },
	{ NULL, NULL, NULL}
};

/****************************************************************************
 Creates some useful well known sids
****************************************************************************/

void generate_wellknown_sids(void)
{
	string_to_sid(&global_sid_S_1_5_0x20, "S-1-5-32");
	string_to_sid(&global_sid_World_Domain, "S-1-1");
	string_to_sid(&global_sid_World, "S-1-1-0");
	string_to_sid(&global_sid_Creator_Owner_Domain, "S-1-3");
	string_to_sid(&global_sid_Creator_Owner, "S-1-3-0");
	string_to_sid(&global_sid_NT_Authority, "S-1-5");
}

/**************************************************************************
 Turns a domain SID into a name, returned in the nt_domain argument.
***************************************************************************/

BOOL map_domain_sid_to_name(DOM_SID *sid, char *nt_domain)
{
	fstring sid_str;
	int i = 0;
	sid_to_string(sid_str, sid);

	DEBUG(5,("map_domain_sid_to_name: %s\n", sid_str));

	if (nt_domain == NULL)
		return False;

	while (sid_name_map[i].sid != NULL) {
		sid_to_string(sid_str, sid_name_map[i].sid);
		DEBUG(5,("map_domain_sid_to_name: compare: %s\n", sid_str));
		if (sid_equal(sid_name_map[i].sid, sid)) {
			fstrcpy(nt_domain, sid_name_map[i].name);
			DEBUG(5,("map_domain_sid_to_name: found '%s'\n", nt_domain));
			return True;
		}
		i++;
	}

	DEBUG(5,("map_domain_sid_to_name: mapping for %s not found\n", sid_str));

    return False;
}

/**************************************************************************
 Looks up a known username from one of the known domains.
***************************************************************************/

BOOL lookup_known_rid(DOM_SID *sid, uint32 rid, char *name, uint8 *psid_name_use)
{
	int i = 0;
	struct sid_name_map_info *psnm;

	for(i = 0; sid_name_map[i].sid != NULL; i++) {
		psnm = &sid_name_map[i];
		if(sid_equal(psnm->sid, sid)) {
			int j;
			for(j = 0; psnm->known_users && psnm->known_users[j].known_user_name != NULL; j++) {
				if(rid == psnm->known_users[j].rid) {
					DEBUG(5,("lookup_builtin_rid: rid = %u, domain = '%s', user = '%s'\n",
						(unsigned int)rid, psnm->name, psnm->known_users[j].known_user_name ));
					fstrcpy( name, psnm->known_users[j].known_user_name);
					*psid_name_use = psnm->known_users[j].sid_name_use;
					return True;
				}
			}
		}
	}

	return False;
}

/**************************************************************************
 Turns a domain name into a SID.
 *** side-effect: if the domain name is NULL, it is set to our domain ***
***************************************************************************/

BOOL map_domain_name_to_sid(DOM_SID *sid, char *nt_domain)
{
	int i = 0;

	if (nt_domain == NULL) {
		DEBUG(5,("map_domain_name_to_sid: mapping NULL domain to our SID.\n"));
		sid_copy(sid, &global_sam_sid);
		return True;
	}

	if (nt_domain[0] == 0) {
		fstrcpy(nt_domain, global_myname);
		DEBUG(5,("map_domain_name_to_sid: overriding blank name to %s\n", nt_domain));
		sid_copy(sid, &global_sam_sid);
		return True;
    }

	DEBUG(5,("map_domain_name_to_sid: %s\n", nt_domain));

	while (sid_name_map[i].name != NULL) {
		DEBUG(5,("map_domain_name_to_sid: compare: %s\n", sid_name_map[i].name));
		if (strequal(sid_name_map[i].name, nt_domain)) {
			fstring sid_str;
			sid_copy(sid, sid_name_map[i].sid);
			sid_to_string(sid_str, sid_name_map[i].sid);
			DEBUG(5,("map_domain_name_to_sid: found %s\n", sid_str));
			return True;
		}
		i++;
	}

	DEBUG(0,("map_domain_name_to_sid: mapping to %s not found.\n", nt_domain));
	return False;
}

/**************************************************************************
 Splits a name of format \DOMAIN\name or name into its two components.
 Sets the DOMAIN name to global_myname if it has not been specified.
***************************************************************************/

void split_domain_name(const char *fullname, char *domain, char *name)
{
	pstring full_name;
	char *p;

	*domain = *name = '\0';

	if (fullname[0] == '\\')
		fullname++;

	pstrcpy(full_name, fullname);
	p = strchr(full_name+1, '\\');

	if (p != NULL) {
		*p = 0;
		fstrcpy(domain, full_name);
		fstrcpy(name, p+1);
	} else {
		fstrcpy(domain, global_myname);
		fstrcpy(name, full_name);
	}

	DEBUG(10,("split_domain_name:name '%s' split into domain :'%s' and user :'%s'\n",
			fullname, domain, name));
}

/*****************************************************************
 Convert a SID to an ascii string.
*****************************************************************/

char *sid_to_string(fstring sidstr_out, DOM_SID *sid)
{
  char subauth[16];
  int i;
  /* BIG NOTE: this function only does SIDS where the identauth is not >= 2^32 */
  uint32 ia = (sid->id_auth[5]) +
              (sid->id_auth[4] << 8 ) +
              (sid->id_auth[3] << 16) +
              (sid->id_auth[2] << 24);

  slprintf(sidstr_out, sizeof(fstring) - 1, "S-%u-%lu", (unsigned int)sid->sid_rev_num, (unsigned long)ia);

  for (i = 0; i < sid->num_auths; i++) {
    slprintf(subauth, sizeof(subauth)-1, "-%lu", (unsigned long)sid->sub_auths[i]);
    fstrcat(sidstr_out, subauth);
  }

  DEBUG(7,("sid_to_string returning %s\n", sidstr_out));
  return sidstr_out;
}

/*****************************************************************
 Convert a string to a SID. Returns True on success, False on fail.
*****************************************************************/  
   
BOOL string_to_sid(DOM_SID *sidout, char *sidstr)
{
  pstring tok;
  char *p = sidstr;
  /* BIG NOTE: this function only does SIDS where the identauth is not >= 2^32 */
  uint32 ia;

  memset((char *)sidout, '\0', sizeof(DOM_SID));

  if (StrnCaseCmp( sidstr, "S-", 2)) {
    DEBUG(0,("string_to_sid: Sid %s does not start with 'S-'.\n", sidstr));
    return False;
  }

  p += 2;
  if (!next_token(&p, tok, "-", sizeof(tok))) {
    DEBUG(0,("string_to_sid: Sid %s is not in a valid format.\n", sidstr));
    return False;
  }

  /* Get the revision number. */
  sidout->sid_rev_num = (uint8)strtoul(tok, NULL, 10);

  if (!next_token(&p, tok, "-", sizeof(tok))) {
    DEBUG(0,("string_to_sid: Sid %s is not in a valid format.\n", sidstr));
    return False;
  }

  /* identauth in decimal should be <  2^32 */
  ia = (uint32)strtoul(tok, NULL, 10);

  /* NOTE - the ia value is in big-endian format. */
  sidout->id_auth[0] = 0;
  sidout->id_auth[1] = 0;
  sidout->id_auth[2] = (ia & 0xff000000) >> 24;
  sidout->id_auth[3] = (ia & 0x00ff0000) >> 16;
  sidout->id_auth[4] = (ia & 0x0000ff00) >> 8;
  sidout->id_auth[5] = (ia & 0x000000ff);

  sidout->num_auths = 0;

  while(next_token(&p, tok, "-", sizeof(tok)) && 
	sidout->num_auths < MAXSUBAUTHS) {
    /* 
     * NOTE - the subauths are in native machine-endian format. They
     * are converted to little-endian when linearized onto the wire.
     */
	sid_append_rid(sidout, (uint32)strtoul(tok, NULL, 10));
  }

  DEBUG(7,("string_to_sid: converted SID %s ok\n", sidstr));

  return True;
}

/*****************************************************************
 Add a rid to the end of a sid
*****************************************************************/  

BOOL sid_append_rid(DOM_SID *sid, uint32 rid)
{
	if (sid->num_auths < MAXSUBAUTHS) {
		sid->sub_auths[sid->num_auths++] = rid;
		return True;
	}
	return False;
}

/*****************************************************************
 Removes the last rid from the end of a sid
*****************************************************************/  

BOOL sid_split_rid(DOM_SID *sid, uint32 *rid)
{
	if (sid->num_auths > 0) {
		sid->num_auths--;
		*rid = sid->sub_auths[sid->num_auths];
		return True;
	}
	return False;
}

/*****************************************************************
 Copies a sid
*****************************************************************/  

void sid_copy(DOM_SID *dst, DOM_SID *src)
{
	int i;

	dst->sid_rev_num = src->sid_rev_num;
	dst->num_auths = src->num_auths;

	memcpy(&dst->id_auth[0], &src->id_auth[0], sizeof(src->id_auth));

	for (i = 0; i < src->num_auths; i++)
		dst->sub_auths[i] = src->sub_auths[i];
}

/*****************************************************************
 Duplicates a sid - mallocs the target.
*****************************************************************/

DOM_SID *sid_dup(DOM_SID *src)
{
  DOM_SID *dst;

  if(!src)
    return NULL;

  if((dst = malloc(sizeof(DOM_SID))) != NULL) {
	memset(dst, '\0', sizeof(DOM_SID));
	sid_copy( dst, src);
  }

  return dst;
}

/*****************************************************************
 Write a sid out into on-the-wire format.
*****************************************************************/  

BOOL sid_linearize(char *outbuf, size_t len, DOM_SID *sid)
{
	size_t i;

	if(len < sid_size(sid))
		return False;

	SCVAL(outbuf,0,sid->sid_rev_num);
	SCVAL(outbuf,1,sid->num_auths);
	memcpy(&outbuf[2], sid->id_auth, 6);
	for(i = 0; i < sid->num_auths; i++)
		SIVAL(outbuf, 8 + (i*4), sid->sub_auths[i]);

	return True;
}

/*****************************************************************
 Compare two sids.
*****************************************************************/  

BOOL sid_equal(DOM_SID *sid1, DOM_SID *sid2)
{
	int i;

	/* compare most likely different rids, first: i.e start at end */
	for (i = sid1->num_auths-1; i >= 0; --i)
		if (sid1->sub_auths[i] != sid2->sub_auths[i])
			return False;

	if (sid1->num_auths != sid2->num_auths)
		return False;
	if (sid1->sid_rev_num != sid2->sid_rev_num)
		return False;

	for (i = 0; i < 6; i++)
		if (sid1->id_auth[i] != sid2->id_auth[i])
			return False;

	return True;
}


/*****************************************************************
 Calculates size of a sid.
*****************************************************************/  

size_t sid_size(DOM_SID *sid)
{
	if (sid == NULL)
		return 0;

	return sid->num_auths * sizeof(uint32) + 8;
}
