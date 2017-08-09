/* 
   Unix SMB/CIFS implementation.
   Idmap NSS headers

   Copyright (C) Gerald Carter             2006

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
				  const struct dom_sid *sid,
				  TALLOC_CTX *ctx, 
				  const char **homedir, const char **shell,
				  const char **gecos, gid_t *p_gid);
	NTSTATUS (*map_to_alias)(TALLOC_CTX *mem_ctx,
				 struct nss_domain_entry *e,
				 const char *name, char **alias);
	NTSTATUS (*map_from_alias)(TALLOC_CTX *mem_ctx,
				   struct nss_domain_entry *e,
				   const char *alias, char **name);
	NTSTATUS (*close_fn)( void );
};


/* The following definitions come from nsswitch/nss_info.c  */

NTSTATUS smb_register_idmap_nss(int version, 
				const char *name, 
				struct nss_info_methods *methods);

NTSTATUS nss_get_info( const char *domain, const struct dom_sid *user_sid,
		       TALLOC_CTX *ctx,
		       const char **homedir, const char **shell,
		       const char **gecos, gid_t *p_gid);

NTSTATUS nss_map_to_alias( TALLOC_CTX *mem_ctx, const char *domain,
			   const char *name, char **alias );

NTSTATUS nss_map_from_alias( TALLOC_CTX *mem_ctx, const char *domain,
			     const char *alias, char **name );

NTSTATUS nss_close( const char *parameters );

/* The following definitions come from winbindd/nss_info.c  */


/* The following definitions come from winbindd/nss_info_template.c  */

NTSTATUS nss_info_template_init( void );

#endif /* _IDMAP_NSS_H_ */

