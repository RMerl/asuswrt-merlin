/* 
   Unix SMB/CIFS implementation.
   ads (active directory) utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Andrew Bartlett 2001
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"

/* return a ldap dn path from a string, given separators and field name
   caller must free
*/
char *ads_build_path(const char *realm, const char *sep, const char *field, int reverse)
{
	char *p, *r;
	int numbits = 0;
	char *ret;
	int len;
	char *saveptr;

	r = SMB_STRDUP(realm);

	if (!r || !*r) {
		return r;
	}

	for (p=r; *p; p++) {
		if (strchr(sep, *p)) {
			numbits++;
		}
	}

	len = (numbits+1)*(strlen(field)+1) + strlen(r) + 1;

	ret = (char *)SMB_MALLOC(len);
	if (!ret) {
		free(r);
		return NULL;
	}

	strlcpy(ret,field, len);
	p=strtok_r(r, sep, &saveptr);
	if (p) {
		strlcat(ret, p, len);
	
		while ((p=strtok_r(NULL, sep, &saveptr)) != NULL) {
			int retval;
			char *s = NULL;
			if (reverse)
				retval = asprintf(&s, "%s%s,%s", field, p, ret);
			else
				retval = asprintf(&s, "%s,%s%s", ret, field, p);
			free(ret);
			if (retval == -1) {
				free(r);
				return NULL;
			}
			ret = SMB_STRDUP(s);
			free(s);
		}
	}

	free(r);
	return ret;
}

/* return a dn of the form "dc=AA,dc=BB,dc=CC" from a 
   realm of the form AA.BB.CC 
   caller must free
*/
char *ads_build_dn(const char *realm)
{
	return ads_build_path(realm, ".", "dc=", 0);
}

/* return a DNS name in the for aa.bb.cc from the DN  
   "dc=AA,dc=BB,dc=CC".  caller must free
*/
char *ads_build_domain(const char *dn)
{
	char *dnsdomain = NULL;
	
	/* result should always be shorter than the DN */

	if ( (dnsdomain = SMB_STRDUP( dn )) == NULL ) {
		DEBUG(0,("ads_build_domain: malloc() failed!\n"));		
		return NULL;		
	}	

	strlower_m( dnsdomain );	
	all_string_sub( dnsdomain, "dc=", "", 0);
	all_string_sub( dnsdomain, ",", ".", 0 );

	return dnsdomain;	
}



#ifndef LDAP_PORT
#define LDAP_PORT 389
#endif

/*
  initialise a ADS_STRUCT, ready for some ads_ ops
*/
ADS_STRUCT *ads_init(const char *realm, 
		     const char *workgroup,
		     const char *ldap_server)
{
	ADS_STRUCT *ads;
	int wrap_flags;
	
	ads = SMB_XMALLOC_P(ADS_STRUCT);
	ZERO_STRUCTP(ads);
	
	ads->server.realm = realm? SMB_STRDUP(realm) : NULL;
	ads->server.workgroup = workgroup ? SMB_STRDUP(workgroup) : NULL;
	ads->server.ldap_server = ldap_server? SMB_STRDUP(ldap_server) : NULL;

	/* we need to know if this is a foreign realm */
	if (realm && *realm && !strequal(lp_realm(), realm)) {
		ads->server.foreign = 1;
	}
	if (workgroup && *workgroup && !strequal(lp_workgroup(), workgroup)) {
		ads->server.foreign = 1;
	}

	/* the caller will own the memory by default */
	ads->is_mine = 1;

	wrap_flags = lp_client_ldap_sasl_wrapping();
	if (wrap_flags == -1) {
		wrap_flags = 0;
	}

	ads->auth.flags = wrap_flags;

	return ads;
}

/*
  free the memory used by the ADS structure initialized with 'ads_init(...)'
*/
void ads_destroy(ADS_STRUCT **ads)
{
	if (ads && *ads) {
		bool is_mine;

		is_mine = (*ads)->is_mine;
#if HAVE_LDAP
		ads_disconnect(*ads);
#endif
		SAFE_FREE((*ads)->server.realm);
		SAFE_FREE((*ads)->server.workgroup);
		SAFE_FREE((*ads)->server.ldap_server);

		SAFE_FREE((*ads)->auth.realm);
		SAFE_FREE((*ads)->auth.password);
		SAFE_FREE((*ads)->auth.user_name);
		SAFE_FREE((*ads)->auth.kdc_server);

		SAFE_FREE((*ads)->config.realm);
		SAFE_FREE((*ads)->config.bind_path);
		SAFE_FREE((*ads)->config.ldap_server_name);
		SAFE_FREE((*ads)->config.server_site_name);
		SAFE_FREE((*ads)->config.client_site_name);
		SAFE_FREE((*ads)->config.schema_path);
		SAFE_FREE((*ads)->config.config_path);
		
		ZERO_STRUCTP(*ads);

		if ( is_mine )
			SAFE_FREE(*ads);
	}
}
