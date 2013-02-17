/*
 * support/export/export.c
 *
 * Maintain list of exported file systems.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <stdlib.h>
#include "xmalloc.h"
#include "nfslib.h"
#include "exportfs.h"

exp_hash_table exportlist[MCL_MAXTYPES] = {{NULL, {{NULL,NULL}, }}, }; 
static int export_hash(char *);

static void	export_init(nfs_export *exp, nfs_client *clp,
					struct exportent *nep);
static int	export_check(nfs_export *, struct hostent *, char *);
static nfs_export *
		export_allowed_internal(struct hostent *hp, char *path);

int
export_read(char *fname)
{
	struct exportent	*eep;
	nfs_export		*exp;

	setexportent(fname, "r");
	while ((eep = getexportent(0,1)) != NULL) {
	  exp = export_lookup(eep->e_hostname, eep->e_path, 0);
	  if (!exp)
	    export_create(eep,0);
	  else {
	    if (exp->m_export.e_flags != eep->e_flags) {
	      xlog(L_ERROR, "incompatible duplicated export entries:");
	      xlog(L_ERROR, "\t%s:%s (0x%x) [IGNORED]", eep->e_hostname,
		   eep->e_path, eep->e_flags);
	      xlog(L_ERROR, "\t%s:%s (0x%x)", exp->m_export.e_hostname,
		   exp->m_export.e_path, exp->m_export.e_flags);
	    }
	    else {
	      xlog(L_ERROR, "duplicated export entries:");
	      xlog(L_ERROR, "\t%s:%s", eep->e_hostname, eep->e_path);
	      xlog(L_ERROR, "\t%s:%s", exp->m_export.e_hostname,
		   exp->m_export.e_path);
	    }
	  }
	}
	endexportent();

	return 0;
}

/*
 * Create an in-core export struct from an export entry.
 */
nfs_export *
export_create(struct exportent *xep, int canonical)
{
	nfs_client	*clp;
	nfs_export	*exp;

	if (!(clp = client_lookup(xep->e_hostname, canonical))) {
		/* bad export entry; complaint already logged */
		return NULL;
	}
	exp = (nfs_export *) xmalloc(sizeof(*exp));
	export_init(exp, clp, xep);
	export_add(exp);

	return exp;
}

static void
export_init(nfs_export *exp, nfs_client *clp, struct exportent *nep)
{
	struct exportent	*e = &exp->m_export;

	dupexportent(e, nep);
	if (nep->e_hostname)
		e->e_hostname = xstrdup(nep->e_hostname);

	exp->m_exported = 0;
	exp->m_xtabent = 0;
	exp->m_mayexport = 0;
	exp->m_changed = 0;
	exp->m_warned = 0;
	exp->m_client = clp;
	clp->m_count++;
}

/*
 * Duplicate exports data. The in-core export struct retains the
 * original hostname from /etc/exports, while the in-core client struct
 * gets the newly found FQDN.
 */
nfs_export *
export_dup(nfs_export *exp, struct hostent *hp)
{
	nfs_export		*new;
	nfs_client		*clp;

	new = (nfs_export *) xmalloc(sizeof(*new));
	memcpy(new, exp, sizeof(*new));
	dupexportent(&new->m_export, &exp->m_export);
	if (exp->m_export.e_hostname)
		new->m_export.e_hostname = xstrdup(exp->m_export.e_hostname);
	clp = client_dup(exp->m_client, hp);
	clp->m_count++;
	new->m_client = clp;
	new->m_mayexport = exp->m_mayexport;
	new->m_exported = 0;
	new->m_xtabent = 0;
	new->m_changed = 0;
	new->m_warned = 0;
	export_add(new);

	return new;
}
/*
 * Add export entry to hash table
 */
void 
export_add(nfs_export *exp)
{
	exp_hash_table *p_tbl;
	exp_hash_entry *p_hen;
	nfs_export *p_next;

	int type = exp->m_client->m_type;
	int pos;

	pos = export_hash(exp->m_export.e_path);
	p_tbl = &(exportlist[type]); /* pointer to hash table */
	p_hen = &(p_tbl->entries[pos]); /* pointer to hash table entry */

	if (!(p_hen->p_first)) { /* hash table entry is empty */ 
 		p_hen->p_first = exp;
 		p_hen->p_last  = exp;

 		exp->m_next = p_tbl->p_head;
 		p_tbl->p_head = exp;
	} else { /* hash table entry is NOT empty */
		p_next = p_hen->p_last->m_next;
		p_hen->p_last->m_next = exp;
		exp->m_next = p_next;
		p_hen->p_last = exp;
	}
}

nfs_export *
export_find(struct hostent *hp, char *path)
{
	nfs_export	*exp;
	int		i;

	for (i = 0; i < MCL_MAXTYPES; i++) {
		for (exp = exportlist[i].p_head; exp; exp = exp->m_next) {
			if (!export_check(exp, hp, path))
				continue;
			if (exp->m_client->m_type == MCL_FQDN)
				return exp;
			return export_dup(exp, hp);
		}
	}

	return NULL;
}

static nfs_export *
export_allowed_internal (struct hostent *hp, char *path)
{
	nfs_export	*exp;
	int		i;

	for (i = 0; i < MCL_MAXTYPES; i++) {
		for (exp = exportlist[i].p_head; exp; exp = exp->m_next) {
			if (!exp->m_mayexport ||
			    !export_check(exp, hp, path))
				continue;
			return exp;
		}
	}

	return NULL;
}

nfs_export *
export_allowed(struct hostent *hp, char *path)
{
	nfs_export		*exp;
	char			epath[MAXPATHLEN+1];
	char			*p = NULL;

	if (path [0] != '/') return NULL;

	strncpy(epath, path, sizeof (epath) - 1);
	epath[sizeof (epath) - 1] = '\0';

	/* Try the longest matching exported pathname. */
	while (1) {
		exp = export_allowed_internal (hp, epath);
		if (exp)
			return exp;
		/* We have to treat the root, "/", specially. */
		if (p == &epath[1]) break;
		p = strrchr(epath, '/');
		if (p == epath) p++;
		*p = '\0';
	}

	return NULL;
}

/*
 * Search hash table for export entry. 
 */  
nfs_export *
export_lookup(char *hname, char *path, int canonical) 
{
	nfs_client *clp;
	nfs_export *exp;
	exp_hash_entry *p_hen;

	int pos;

	clp = client_lookup(hname, canonical);
	if(clp == NULL)
		return NULL;

	pos = export_hash(path);
	p_hen = &(exportlist[clp->m_type].entries[pos]); 
	for(exp = p_hen->p_first; exp && (exp != p_hen->p_last->m_next); 
  			exp = exp->m_next) {
		if (exp->m_client == clp && !strcmp(exp->m_export.e_path, path)) {
  			return exp;
		}
	}
	return NULL;
}

static int
export_check(nfs_export *exp, struct hostent *hp, char *path)
{
	if (strcmp(path, exp->m_export.e_path))
		return 0;

	return client_check(exp->m_client, hp);
}

void
export_freeall(void)
{
	nfs_export	*exp, *nxt;
	int		i, j;

	for (i = 0; i < MCL_MAXTYPES; i++) {
		for (exp = exportlist[i].p_head; exp; exp = nxt) {
			nxt = exp->m_next;
			client_release(exp->m_client);
			if (exp->m_export.e_squids)
				xfree(exp->m_export.e_squids);
			if (exp->m_export.e_sqgids)
				xfree(exp->m_export.e_sqgids);
			if (exp->m_export.e_mountpoint)
				free(exp->m_export.e_mountpoint);
			if (exp->m_export.e_fslocdata)
				xfree(exp->m_export.e_fslocdata);
			xfree(exp->m_export.e_hostname);
			xfree(exp);
		}
      for(j = 0; j < HASH_TABLE_SIZE; j++) {
        exportlist[i].entries[j].p_first = NULL;
        exportlist[i].entries[j].p_last = NULL;
      }
      exportlist[i].p_head = NULL;
	}
	client_freeall();
}

/*
 * Compute and returns integer from string. 
 * Note: Its understood the smae integers can be same for 
 *       different strings, but it should not matter.
 */
static unsigned int 
strtoint(char *str)
{
	int i = 0;
	unsigned int n = 0;

	while ( str[i] != '\0') {
		n+=((int)str[i])*i;
		i++;
	}
	return n;
}

/*
 * Hash function
 */
static int 
export_hash(char *str)
{
	int num = strtoint(str);

	return num % HASH_TABLE_SIZE;
}
