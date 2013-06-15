/* 
   Unix SMB/CIFS implementation.
   Idmap NSS headers

   Copyright (C) Gerald Carter             2006

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA  02111-1307, USA.   
*/

#ifndef _IDMAP_NSS_H
#define _IDMAP_NSS_H

#ifndef HAVE_LDAP
#  ifndef LDAPMessage
#    define LDAPMessage void
#  endif
#endif

/* The interface version specifier */

#define SMB_NSS_INFO_INTERFACE_VERSION	  1

/* List of available backends.  All backends must 
   register themselves */

struct nss_function_entry {
	struct nss_function_entry *prev, *next;
	
	const char *name;
	struct nss_info_methods *methods;
};

/* List of configured domains.  Each domain points 
   back to its configured backend. */

struct nss_domain_entry {
	struct nss_domain_entry *prev, *next;

	const char *domain;

	NTSTATUS init_status;	
	struct nss_function_entry *backend;

	/* hold state on a per domain basis */

	void *state;
};

/* API */

struct nss_info_methods {
	NTSTATUS (*init)( struct nss_domain_entry *e );
	NTSTATUS (*get_nss_info)( struct nss_domain_entry *e, 
				  const DOM_SID *sid, 
				  TALLOC_CTX *ctx, 
				  ADS_STRUCT *ads, LDAPMessage *msg,
				  char **homedir, char **shell, char **gecos, gid_t *p_gid);
	NTSTATUS (*close_fn)( void );
};


/* The following definitions come from nsswitch/nss_info.c  */

NTSTATUS smb_register_idmap_nss(int version, 
				const char *name, 
				struct nss_info_methods *methods);

NTSTATUS nss_init( const char **nss_list );

NTSTATUS nss_get_info( const char *domain, const DOM_SID *user_sid,
                       TALLOC_CTX *ctx,
		       ADS_STRUCT *ads, LDAPMessage *msg,
                       char **homedir, char **shell, char **gecos,
                       gid_t *p_gid);

NTSTATUS nss_close( const char *parameters );

#endif /* _IDMAP_NSS_H_ */

