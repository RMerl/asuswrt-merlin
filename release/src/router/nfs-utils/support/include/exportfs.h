/*
 * support/include/exportfs.h
 *
 * Declarations for exportfs and mountd
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef EXPORTFS_H
#define EXPORTFS_H

#include <netdb.h>
#include "nfslib.h"

enum {
	MCL_FQDN = 0,
	MCL_SUBNETWORK,
	MCL_IPADDR = MCL_SUBNETWORK,
	MCL_WILDCARD,
	MCL_NETGROUP,
	MCL_ANONYMOUS,
	MCL_GSS,
	MCL_MAXTYPES
};

enum {
	FSLOC_NONE = 0,
	FSLOC_REFER,
	FSLOC_REPLICA,
	FSLOC_STUB
};

typedef struct mclient {
	struct mclient *	m_next;
	char *			m_hostname;
	int			m_type;
	int			m_naddr;
	struct in_addr		m_addrlist[NFSCLNT_ADDRMAX];
	int			m_exported;	/* exported to nfsd */
	int			m_count;
} nfs_client;

typedef struct mexport {
	struct mexport *	m_next;
	struct mclient *	m_client;
	struct exportent	m_export;
	int			m_exported;	/* known to knfsd. -1 means not sure */
	int			m_xtabent  : 1,	/* xtab entry exists */
				m_mayexport: 1,	/* derived from xtabbed */
				m_changed  : 1, /* options (may) have changed */
				m_warned   : 1; /* warned about multiple exports
						 * matching one client */
} nfs_export;

#define HASH_TABLE_SIZE 1021

typedef struct _exp_hash_entry {
	nfs_export * p_first;
  	nfs_export * p_last;
} exp_hash_entry;

typedef struct _exp_hash_table {
	nfs_export * p_head;
	exp_hash_entry entries[HASH_TABLE_SIZE];
} exp_hash_table;

extern exp_hash_table exportlist[MCL_MAXTYPES];

extern nfs_client *		clientlist[MCL_MAXTYPES];

nfs_client *			client_lookup(char *hname, int canonical);
nfs_client *			client_find(struct hostent *);
void				client_add(nfs_client *);
nfs_client *			client_dup(nfs_client *, struct hostent *);
int				client_gettype(char *hname);
int				client_check(nfs_client *, struct hostent *);
int				client_match(nfs_client *, char *hname);
void				client_release(nfs_client *);
void				client_freeall(void);
char *				client_compose(struct hostent *he);
struct hostent *		client_resolve(struct in_addr addr);
int 				client_member(char *client, char *name);

int				export_read(char *fname);
void			export_add(nfs_export *);
void				export_reset(nfs_export *);
nfs_export *			export_lookup(char *hname, char *path, int caconical);
nfs_export *			export_find(struct hostent *, char *path);
nfs_export *			export_allowed(struct hostent *, char *path);
nfs_export *			export_create(struct exportent *, int canonical);
nfs_export *			export_dup(nfs_export *, struct hostent *);
void				export_freeall(void);
int				export_export(nfs_export *);
int				export_unexport(nfs_export *);

int				xtab_mount_read(void);
int				xtab_export_read(void);
int				xtab_mount_write(void);
int				xtab_export_write(void);
void				xtab_append(nfs_export *);

int				rmtab_read(void);

struct nfskey *			key_lookup(char *hname);

/* Record export error.  */
extern int export_errno;

#endif /* EXPORTFS_H */
