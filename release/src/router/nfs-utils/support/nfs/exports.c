/*
 * support/nfs/export.c
 *
 * Parse the exports file. Derived from the unfsd implementation.
 *
 * Authors:	Donald J. Becker, <becker@super.org>
 *		Rick Sladkey, <jrs@world.std.com>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Olaf Kirch, <okir@monad.swb.de>
 *		Alexander O. Yuriev, <alex@bach.cis.temple.edu>
 *
 *		This software maybe be used for any purpose provided
 *		the above copyright notice is retained.  It is supplied
 *		as is, with no warranty expressed or implied.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include "nfslib.h"
#include "exportfs.h"
#include "xmalloc.h"
#include "xlog.h"
#include "xio.h"
#include "pseudoflavors.h"

#define EXPORT_DEFAULT_FLAGS	\
  (NFSEXP_READONLY|NFSEXP_ROOTSQUASH|NFSEXP_GATHERED_WRITES|NFSEXP_NOSUBTREECHECK)

struct flav_info flav_map[] = {
	{ "krb5",	RPC_AUTH_GSS_KRB5	},
	{ "krb5i",	RPC_AUTH_GSS_KRB5I	},
	{ "krb5p",	RPC_AUTH_GSS_KRB5P	},
	{ "lipkey",	RPC_AUTH_GSS_LKEY	},
	{ "lipkey-i",	RPC_AUTH_GSS_LKEYI	},
	{ "lipkey-p",	RPC_AUTH_GSS_LKEYP	},
	{ "spkm3",	RPC_AUTH_GSS_SPKM	},
	{ "spkm3i",	RPC_AUTH_GSS_SPKMI	},
	{ "spkm3p",	RPC_AUTH_GSS_SPKMP	},
	{ "unix",	AUTH_UNIX		},
	{ "sys",	AUTH_SYS		},
	{ "null",	AUTH_NULL		},
	{ "none",	AUTH_NONE		},
};

const int flav_map_size = sizeof(flav_map)/sizeof(flav_map[0]);

int export_errno;

static char	*efname = NULL;
static XFILE	*efp = NULL;
static int	first;
static int	has_default_opts, has_default_subtree_opts;
static int	*squids = NULL, nsquids = 0,
		*sqgids = NULL, nsqgids = 0;

static int	getexport(char *exp, int len);
static int	getpath(char *path, int len);
static int	parseopts(char *cp, struct exportent *ep, int warn, int *had_subtree_opt_ptr);
static int	parsesquash(char *list, int **idp, int *lenp, char **ep);
static int	parsenum(char **cpp);
static void	freesquash(void);
static void	syntaxerr(char *msg);

void
setexportent(char *fname, char *type)
{
	if (efp)
		endexportent();
	if (!fname)
		fname = _PATH_EXPORTS;
	if (!(efp = xfopen(fname, type)))
		xlog(L_ERROR, "can't open %s for %sing",
				fname, strcmp(type, "r")? "writ" : "read");
	efname = strdup(fname);
	first = 1;
}

struct exportent *
getexportent(int fromkernel, int fromexports)
{
	static struct exportent	ee, def_ee;
	char		exp[512], *hostname;
	char		rpath[MAXPATHLEN+1];
	char		*opt, *sp;
	int		ok;

	if (!efp)
		return NULL;

	freesquash();

	if (first || (ok = getexport(exp, sizeof(exp))) == 0) {
		has_default_opts = 0;
		has_default_subtree_opts = 0;
	
		def_ee.e_flags = EXPORT_DEFAULT_FLAGS;
		/* some kernels assume the default is sync rather than
		 * async.  More recent kernels always report one or other,
		 * but this test makes sure we assume same as kernel
		 * Ditto for wgather
		 */
		if (fromkernel) {
			def_ee.e_flags &= ~NFSEXP_ASYNC;
			def_ee.e_flags &= ~NFSEXP_GATHERED_WRITES;
		}
		def_ee.e_anonuid = 65534;
		def_ee.e_anongid = 65534;
		def_ee.e_squids = NULL;
		def_ee.e_sqgids = NULL;
		def_ee.e_mountpoint = NULL;
		def_ee.e_fslocmethod = FSLOC_NONE;
		def_ee.e_fslocdata = NULL;
		def_ee.e_secinfo[0].flav = NULL;
		def_ee.e_nsquids = 0;
		def_ee.e_nsqgids = 0;

		ok = getpath(def_ee.e_path, sizeof(def_ee.e_path));
		if (ok <= 0)
			return NULL;

		ok = getexport(exp, sizeof(exp));
	}
	if (ok < 0) {
		xlog(L_ERROR, "expected client(options...)");
		export_errno = EINVAL;
		return NULL;
	}
	first = 0;
		
	/* Check for default options */
	if (exp[0] == '-') {
		if (parseopts(exp + 1, &def_ee, 0, &has_default_subtree_opts) < 0)
			return NULL;
		
		has_default_opts = 1;

		ok = getexport(exp, sizeof(exp));
		if (ok < 0) {
			xlog(L_ERROR, "expected client(options...)");
			export_errno = EINVAL;
			return NULL;
		}
	}

	ee = def_ee;

	/* Check for default client */
	if (ok == 0)
		exp[0] = '\0';

	hostname = exp;
	if ((opt = strchr(exp, '(')) != NULL) {
		if (opt == exp) {
			xlog(L_WARNING, "No host name given with %s %s, suggest *%s to avoid warning", ee.e_path, exp, exp);
			hostname = "*";
		}
		*opt++ = '\0';
		if (!(sp = strchr(opt, ')')) || sp[1] != '\0') {
			syntaxerr("bad option list");
			export_errno = EINVAL;
			return NULL;
		}
		*sp = '\0';
	} else {
		if (!has_default_opts)
			xlog(L_WARNING, "No options for %s %s: suggest %s(sync) to avoid warning", ee.e_path, exp, exp);
	}
	xfree(ee.e_hostname);
	ee.e_hostname = xstrdup(hostname);

	if (parseopts(opt, &ee, fromexports && !has_default_subtree_opts, NULL) < 0)
		return NULL;

	/* resolve symlinks */
	if (realpath(ee.e_path, rpath) != NULL) {
		rpath[sizeof (rpath) - 1] = '\0';
		strncpy(ee.e_path, rpath, sizeof (ee.e_path) - 1);
		ee.e_path[sizeof (ee.e_path) - 1] = '\0';
	}

	return &ee;
}

void secinfo_show(FILE *fp, struct exportent *ep)
{
	struct sec_entry *p1, *p2;
	int flags;

	for (p1=ep->e_secinfo; p1->flav; p1=p2) {

		fprintf(fp, ",sec=%s", p1->flav->flavour);
		for (p2=p1+1; (p2->flav != NULL) && (p1->flags == p2->flags);
								p2++) {
			fprintf(fp, ":%s", p2->flav->flavour);
		}
		flags = p1->flags;
		fprintf(fp, ",%s", (flags & NFSEXP_READONLY) ? "ro" : "rw");
		fprintf(fp, ",%sroot_squash", (flags & NFSEXP_ROOTSQUASH)?
				"" : "no_");
		fprintf(fp, ",%sall_squash", (flags & NFSEXP_ALLSQUASH)?
				"" : "no_");
	}
}

void
putexportent(struct exportent *ep)
{
	FILE	*fp;
	int	*id, i;
	char	*esc=ep->e_path;

	if (!efp)
		return;

	fp = efp->x_fp;
	for (i=0; esc[i]; i++)
	        if (iscntrl(esc[i]) || esc[i] == '"' || esc[i] == '\\' || esc[i] == '#' || isspace(esc[i]))
			fprintf(fp, "\\%03o", esc[i]);
		else
			fprintf(fp, "%c", esc[i]);

	fprintf(fp, "\t%s(", ep->e_hostname);
	fprintf(fp, "%s,", (ep->e_flags & NFSEXP_READONLY)? "ro" : "rw");
	fprintf(fp, "%ssync,", (ep->e_flags & NFSEXP_ASYNC)? "a" : "");
	fprintf(fp, "%swdelay,", (ep->e_flags & NFSEXP_GATHERED_WRITES)?
				"" : "no_");
	fprintf(fp, "%shide,", (ep->e_flags & NFSEXP_NOHIDE)?
				"no" : "");
	fprintf(fp, "%scrossmnt,", (ep->e_flags & NFSEXP_CROSSMOUNT)?
				"" : "no");
	fprintf(fp, "%ssecure,", (ep->e_flags & NFSEXP_INSECURE_PORT)?
				"in" : "");
	fprintf(fp, "%sroot_squash,", (ep->e_flags & NFSEXP_ROOTSQUASH)?
				"" : "no_");
	fprintf(fp, "%sall_squash,", (ep->e_flags & NFSEXP_ALLSQUASH)?
				"" : "no_");
	fprintf(fp, "%ssubtree_check,", (ep->e_flags & NFSEXP_NOSUBTREECHECK)?
		"no_" : "");
	fprintf(fp, "%ssecure_locks,", (ep->e_flags & NFSEXP_NOAUTHNLM)?
		"in" : "");
	fprintf(fp, "%sacl,", (ep->e_flags & NFSEXP_NOACL)?
		"no_" : "");
	if (ep->e_flags & NFSEXP_FSID) {
		fprintf(fp, "fsid=%d,", ep->e_fsid);
	}
	if (ep->e_uuid)
		fprintf(fp, "fsid=%s,", ep->e_uuid);
	if (ep->e_mountpoint)
		fprintf(fp, "mountpoint%s%s,",
			ep->e_mountpoint[0]?"=":"", ep->e_mountpoint);
	switch (ep->e_fslocmethod) {
	case FSLOC_NONE:
		break;
	case FSLOC_REFER:
		fprintf(fp, "refer=%s,", ep->e_fslocdata);
		break;
	case FSLOC_REPLICA:
		fprintf(fp, "replicas=%s,", ep->e_fslocdata);
		break;
#ifdef DEBUG
	case FSLOC_STUB:
		fprintf(fp, "fsloc=stub,");
		break;
#endif
	default:
		xlog(L_ERROR, "unknown fsloc method for %s:%s",
		     ep->e_hostname, ep->e_path);
	}
	if ((id = ep->e_squids) != NULL) {
		fprintf(fp, "squash_uids=");
		for (i = 0; i < ep->e_nsquids; i += 2)
			if (id[i] != id[i+1])
				fprintf(fp, "%d-%d,", id[i], id[i+1]);
			else
				fprintf(fp, "%d,", id[i]);
	}
	if ((id = ep->e_sqgids) != NULL) {
		fprintf(fp, "squash_gids=");
		for (i = 0; i < ep->e_nsquids; i += 2)
			if (id[i] != id[i+1])
				fprintf(fp, "%d-%d,", id[i], id[i+1]);
			else
				fprintf(fp, "%d,", id[i]);
	}
	fprintf(fp, "anonuid=%d,anongid=%d", ep->e_anonuid, ep->e_anongid);
	secinfo_show(fp, ep);
	fprintf(fp, ")\n");
}

void
endexportent(void)
{
	if (efp)
		xfclose(efp);
	efp = NULL;
	if (efname)
		free(efname);
	efname = NULL;
	freesquash();
}

void
dupexportent(struct exportent *dst, struct exportent *src)
{
	int	n;

	*dst = *src;
	if ((n = src->e_nsquids) != 0) {
		dst->e_squids = (int *) xmalloc(n * sizeof(int));
		memcpy(dst->e_squids, src->e_squids, n * sizeof(int));
	}
	if ((n = src->e_nsqgids) != 0) {
		dst->e_sqgids = (int *) xmalloc(n * sizeof(int));
		memcpy(dst->e_sqgids, src->e_sqgids, n * sizeof(int));
	}
	if (src->e_mountpoint)
		dst->e_mountpoint = strdup(src->e_mountpoint);
	if (src->e_fslocdata)
		dst->e_fslocdata = strdup(src->e_fslocdata);
	dst->e_hostname = NULL;
}

struct exportent *
mkexportent(char *hname, char *path, char *options)
{
	static struct exportent	ee;

	ee.e_flags = EXPORT_DEFAULT_FLAGS;
	ee.e_anonuid = 65534;
	ee.e_anongid = 65534;
	ee.e_squids = NULL;
	ee.e_sqgids = NULL;
	ee.e_mountpoint = NULL;
	ee.e_fslocmethod = FSLOC_NONE;
	ee.e_fslocdata = NULL;
	ee.e_secinfo[0].flav = NULL;
	ee.e_nsquids = 0;
	ee.e_nsqgids = 0;
	ee.e_uuid = NULL;

	xfree(ee.e_hostname);
	ee.e_hostname = xstrdup(hname);

	if (strlen(path) >= sizeof(ee.e_path)) {
		xlog(L_WARNING, "path name %s too long", path);
		return NULL;
	}
	strncpy(ee.e_path, path, sizeof (ee.e_path));
	ee.e_path[sizeof (ee.e_path) - 1] = '\0';
	if (parseopts(options, &ee, 0, NULL) < 0)
		return NULL;
	return &ee;
}

int
updateexportent(struct exportent *eep, char *options)
{
	if (parseopts(options, eep, 0, NULL) < 0)
		return 0;
	return 1;
}


static int valid_uuid(char *uuid)
{
	/* must have 32 hex digits */
	int cnt;
	for (cnt = 0 ; *uuid; uuid++)
		if (isxdigit(*uuid))
			cnt++;
	return cnt == 32;
}

/*
 * Append the given flavor to the exportent's e_secinfo array, or
 * do nothing if it's already there.  Returns the index of flavor
 * in the resulting array in any case.
 */
static int secinfo_addflavor(struct flav_info *flav, struct exportent *ep)
{
	struct sec_entry *p;

	for (p=ep->e_secinfo; p->flav; p++) {
		if (p->flav == flav)
			return p - ep->e_secinfo;
	}
	if (p - ep->e_secinfo >= SECFLAVOR_COUNT) {
		xlog(L_ERROR, "more than %d security flavors on an export\n",
			SECFLAVOR_COUNT);
		return -1;
	}
	p->flav = flav;
	p->flags = ep->e_flags;
	(p+1)->flav = NULL;
	return p - ep->e_secinfo;
}

static struct flav_info *find_flavor(char *name)
{
	struct flav_info *flav;
	for (flav = flav_map; flav < flav_map + flav_map_size; flav++)
		if (strcmp(flav->flavour, name) == 0)
			return flav;
	return NULL;
}

/* @str is a colon seperated list of security flavors.  Their order
 * is recorded in @ep, and a bitmap corresponding to the list is returned.
 * A zero return indicates an error.
 */
static unsigned int parse_flavors(char *str, struct exportent *ep)
{
	unsigned int out=0;
	char *flavor;
	int bit;

	while ( (flavor=strsep(&str, ":")) ) {
		struct flav_info *flav = find_flavor(flavor);
		if (flav == NULL) {
			xlog(L_ERROR, "unknown flavor %s\n", flavor);
			return 0;
		}
		bit = secinfo_addflavor(flav, ep);
		if (bit < 0)
			return 0;
		out |= 1<<bit;
	}
	return out;
}

/* Sets the bits in @mask for the appropriate security flavor flags. */
static void setflags(int mask, unsigned int active, struct exportent *ep)
{
	int bit=0;

	ep->e_flags |= mask;

	while (active) {
		if (active & 1)
			ep->e_secinfo[bit].flags |= mask;
		bit++;
		active >>= 1;
	}
}

/* Clears the bits in @mask for the appropriate security flavor flags. */
static void clearflags(int mask, unsigned int active, struct exportent *ep)
{
	int bit=0;

	ep->e_flags &= ~mask;

	while (active) {
		if (active & 1)
			ep->e_secinfo[bit].flags &= ~mask;
		bit++;
		active >>= 1;
	}
}

/* options that can vary per flavor: */
#define NFSEXP_SECINFO_FLAGS (NFSEXP_READONLY | NFSEXP_ROOTSQUASH \
					| NFSEXP_ALLSQUASH)

/*
 * Parse option string pointed to by cp and set mount options accordingly.
 */
static int
parseopts(char *cp, struct exportent *ep, int warn, int *had_subtree_opt_ptr)
{
	struct sec_entry *p;
	int	had_subtree_opt = 0;
	char 	*flname = efname?efname:"command line";
	int	flline = efp?efp->x_line:0;
	unsigned int active = 0;

	squids = ep->e_squids; nsquids = ep->e_nsquids;
	sqgids = ep->e_sqgids; nsqgids = ep->e_nsqgids;
	if (!cp)
		goto out;

	while (isblank(*cp))
		cp++;

	while (*cp) {
		char *opt = strdup(cp);
		char *optstart = cp;
		while (*cp && *cp != ',')
			cp++;
		if (*cp) {
			opt[cp-optstart] = '\0';
			cp++;
		}

		/* process keyword */
		if (strcmp(opt, "ro") == 0)
			setflags(NFSEXP_READONLY, active, ep);
		else if (strcmp(opt, "rw") == 0)
			clearflags(NFSEXP_READONLY, active, ep);
		else if (!strcmp(opt, "secure"))
			ep->e_flags &= ~NFSEXP_INSECURE_PORT;
		else if (!strcmp(opt, "insecure"))
			ep->e_flags |= NFSEXP_INSECURE_PORT;
		else if (!strcmp(opt, "sync"))
			ep->e_flags &= ~NFSEXP_ASYNC;
		else if (!strcmp(opt, "async"))
			ep->e_flags |= NFSEXP_ASYNC;
		else if (!strcmp(opt, "nohide"))
			ep->e_flags |= NFSEXP_NOHIDE;
		else if (!strcmp(opt, "hide"))
			ep->e_flags &= ~NFSEXP_NOHIDE;
		else if (!strcmp(opt, "crossmnt"))
			ep->e_flags |= NFSEXP_CROSSMOUNT;
		else if (!strcmp(opt, "nocrossmnt"))
			ep->e_flags &= ~NFSEXP_CROSSMOUNT;
		else if (!strcmp(opt, "wdelay"))
			ep->e_flags |= NFSEXP_GATHERED_WRITES;
		else if (!strcmp(opt, "no_wdelay"))
			ep->e_flags &= ~NFSEXP_GATHERED_WRITES;
		else if (strcmp(opt, "root_squash") == 0)
			setflags(NFSEXP_ROOTSQUASH, active, ep);
		else if (!strcmp(opt, "no_root_squash"))
			clearflags(NFSEXP_ROOTSQUASH, active, ep);
		else if (strcmp(opt, "all_squash") == 0)
			setflags(NFSEXP_ALLSQUASH, active, ep);
		else if (strcmp(opt, "no_all_squash") == 0)
			clearflags(NFSEXP_ALLSQUASH, active, ep);
		else if (strcmp(opt, "subtree_check") == 0) {
			had_subtree_opt = 1;
			ep->e_flags &= ~NFSEXP_NOSUBTREECHECK;
		} else if (strcmp(opt, "no_subtree_check") == 0) {
			had_subtree_opt = 1;
			ep->e_flags |= NFSEXP_NOSUBTREECHECK;
		} else if (strcmp(opt, "auth_nlm") == 0)
			ep->e_flags &= ~NFSEXP_NOAUTHNLM;
		else if (strcmp(opt, "no_auth_nlm") == 0)
			ep->e_flags |= NFSEXP_NOAUTHNLM;
		else if (strcmp(opt, "secure_locks") == 0)
			ep->e_flags &= ~NFSEXP_NOAUTHNLM;
		else if (strcmp(opt, "insecure_locks") == 0)
			ep->e_flags |= NFSEXP_NOAUTHNLM;
		else if (strcmp(opt, "acl") == 0)
			ep->e_flags &= ~NFSEXP_NOACL;
		else if (strcmp(opt, "no_acl") == 0)
			ep->e_flags |= NFSEXP_NOACL;
		else if (strncmp(opt, "anonuid=", 8) == 0) {
			char *oe;
			ep->e_anonuid = strtol(opt+8, &oe, 10);
			if (opt[8]=='\0' || *oe != '\0') {
				xlog(L_ERROR, "%s: %d: bad anonuid \"%s\"\n",
				     flname, flline, opt);	
bad_option:
				free(opt);
				export_errno = EINVAL;
				return -1;
			}
		} else if (strncmp(opt, "anongid=", 8) == 0) {
			char *oe;
			ep->e_anongid = strtol(opt+8, &oe, 10);
			if (opt[8]=='\0' || *oe != '\0') {
				xlog(L_ERROR, "%s: %d: bad anongid \"%s\"\n",
				     flname, flline, opt);	
				goto bad_option;
			}
		} else if (strncmp(opt, "squash_uids=", 12) == 0) {
			if (parsesquash(opt+12, &squids, &nsquids, &cp) < 0) {
				goto bad_option;
			}
		} else if (strncmp(opt, "squash_gids=", 12) == 0) {
			if (parsesquash(opt+12, &sqgids, &nsqgids, &cp) < 0) {
				goto bad_option;
			}
		} else if (strncmp(opt, "fsid=", 5) == 0) {
			char *oe;
			if (strcmp(opt+5, "root") == 0) {
				ep->e_fsid = 0;
				ep->e_flags |= NFSEXP_FSID;
			} else {
				ep->e_fsid = strtoul(opt+5, &oe, 0);
				if (opt[5]!='\0' && *oe == '\0') 
					ep->e_flags |= NFSEXP_FSID;
				else if (valid_uuid(opt+5))
					ep->e_uuid = strdup(opt+5);
				else {
					xlog(L_ERROR, "%s: %d: bad fsid \"%s\"\n",
					     flname, flline, opt);	
					goto bad_option;
				}
			}
		} else if (strcmp(opt, "mountpoint")==0 ||
			   strcmp(opt, "mp") == 0 ||
			   strncmp(opt, "mountpoint=", 11)==0 ||
			   strncmp(opt, "mp=", 3) == 0) {
			char * mp = strchr(opt, '=');
			if (mp)
				ep->e_mountpoint = strdup(mp+1);
			else
				ep->e_mountpoint = strdup("");
#ifdef DEBUG
		} else if (strncmp(opt, "fsloc=", 6) == 0) {
			if (strcmp(opt+6, "stub") == 0)
				ep->e_fslocmethod = FSLOC_STUB;
			else {
				xlog(L_ERROR, "%s:%d: bad option %s\n",
				     flname, flline, opt);
				goto bad_option;
			}
#endif
		} else if (strncmp(opt, "refer=", 6) == 0) {
			ep->e_fslocmethod = FSLOC_REFER;
			ep->e_fslocdata = strdup(opt+6);
		} else if (strncmp(opt, "replicas=", 9) == 0) {
			ep->e_fslocmethod = FSLOC_REPLICA;
			ep->e_fslocdata = strdup(opt+9);
		} else if (strncmp(opt, "sec=", 4) == 0) {
			active = parse_flavors(opt+4, ep);
			if (!active)
				goto bad_option;
		} else {
			xlog(L_ERROR, "%s:%d: unknown keyword \"%s\"\n",
					flname, flline, opt);
			ep->e_flags |= NFSEXP_ALLSQUASH | NFSEXP_READONLY;
			goto bad_option;
		}
		free(opt);
		while (isblank(*cp))
			cp++;
	}
	/*
	 * Turn on nohide which will allow this export to cross over
	 * the 'mount --bind' mount point.
	 */
	if (ep->e_fslocdata)
		ep->e_flags |= NFSEXP_NOHIDE;

	for (p = ep->e_secinfo; p->flav; p++)
		p->flags |= ep->e_flags & ~NFSEXP_SECINFO_FLAGS;
	ep->e_squids = squids;
	ep->e_sqgids = sqgids;
	ep->e_nsquids = nsquids;
	ep->e_nsqgids = nsqgids;

out:
	if (warn && !had_subtree_opt)
		xlog(L_WARNING, "%s [%d]: Neither 'subtree_check' or 'no_subtree_check' specified for export \"%s:%s\".\n"
				"  Assuming default behaviour ('no_subtree_check').\n"
		     		"  NOTE: this default has changed since nfs-utils version 1.0.x\n",

				flname, flline,
				ep->e_hostname, ep->e_path);
	if (had_subtree_opt_ptr)
		*had_subtree_opt_ptr = had_subtree_opt;

	return 1;
}

static int
parsesquash(char *list, int **idp, int *lenp, char **ep)
{
	char	*cp = list;
	int	id0, id1;
	int	len = *lenp;
	int	*id = *idp;

	if (**ep)
	    *--(*ep) = ',';

	do {
		id0 = parsenum(&cp);
		if (*cp == '-') {
			cp++;
			id1 = parsenum(&cp);
		} else {
			id1 = id0;
		}
		if (id0 == -1 || id1 == -1) {
			syntaxerr("uid/gid -1 not permitted");
			return -1;
		}
		if ((len % 8) == 0)
			id = (int *) xrealloc(id, (len + 8) * sizeof(*id));
		id[len++] = id0;
		id[len++] = id1;
		if (!*cp || *cp == ')' || (*cp == ',' && !isdigit(cp[1])))
			break;
		if (*cp != ',') {
			syntaxerr("bad uid/gid list");
			return -1;
		}
		cp++;
	} while(1);

	if (**ep == ',') (*ep)++;

	*lenp = len;
	*idp = id;
	return 1;
}

static void
freesquash(void)
{
	if (squids) {
		xfree (squids);
		squids = NULL;
		nsquids = 0;
	}
	if (sqgids) {
		xfree (sqgids);
		sqgids = NULL;
		nsqgids = 0;
	}
}

static int
parsenum(char **cpp)
{
	char	*cp = *cpp, c;
	int	num = 0;

	if (**cpp == '-')
		(*cpp)++;
	while (isdigit(**cpp))
		(*cpp)++;
	c = **cpp; **cpp = '\0'; num = atoi(cp); **cpp = c;
	return num;
}

static int
getpath(char *path, int len)
{
	xskip(efp, " \t\n");
	return xgettok(efp, 0, path, len);
}

static int
getexport(char *exp, int len)
{
	int	ok;

	xskip(efp, " \t");
	if ((ok = xgettok(efp, 0, exp, len)) < 0)
		xlog(L_ERROR, "%s:%d: syntax error",
			efname?"command line":efname, efp->x_line);
	return ok;
}

static void
syntaxerr(char *msg)
{
	xlog(L_ERROR, "%s:%d: syntax error: %s",
			efname, efp?efp->x_line:0, msg);
}

