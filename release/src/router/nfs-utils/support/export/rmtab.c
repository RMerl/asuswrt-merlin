/*
 * support/export/rmntab.c
 *
 * Interface to the rmnt file.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "xmalloc.h"
#include "misc.h"
#include "nfslib.h"
#include "exportfs.h"
#include "xio.h"
#include "xlog.h"

int
rmtab_read(void)
{
	struct rmtabent		*rep;
	nfs_export		*exp = NULL;

	setrmtabent("r");
	while ((rep = getrmtabent(1, NULL)) != NULL) {
		struct hostent		*hp = NULL;
		int			htype;
		
		htype = client_gettype(rep->r_client);
		if ((htype == MCL_FQDN || htype == MCL_SUBNETWORK)
		    && (hp = gethostbyname (rep->r_client))
		    && (hp = hostent_dup (hp),
			exp = export_allowed (hp, rep->r_path))) {
			/* see if the entry already exists, otherwise this was an instantiated
			 * wild card, and we must add it
			 */
			nfs_export *exp2 = export_lookup(rep->r_client,
							exp->m_export.e_path, 0);
			if (!exp2) {
				struct exportent ee;
				dupexportent(&ee, &exp->m_export);
				ee.e_hostname = rep->r_client;
				exp2 = export_create(&ee, 0);
				exp2->m_changed = exp->m_changed;
			}
			free (hp);
			exp2->m_mayexport = 1;
		} else if (hp) /* export_allowed failed */
			free(hp);
	}
	if (errno == EINVAL) {
		/* Something goes wrong. We need to fix the rmtab
		   file. */
		int	lockid;
		FILE	*fp;
		if ((lockid = xflock(_PATH_RMTABLCK, "w")) < 0)
			return -1;
		rewindrmtabent();
		if (!(fp = fsetrmtabent(_PATH_RMTABTMP, "w"))) {
			endrmtabent ();
			xfunlock(lockid);
			return -1;
		}
		while ((rep = getrmtabent(0, NULL)) != NULL) {
			fputrmtabent(fp, rep, NULL);
		}
		if (rename(_PATH_RMTABTMP, _PATH_RMTAB) < 0) {
			xlog(L_ERROR, "couldn't rename %s to %s",
			     _PATH_RMTABTMP, _PATH_RMTAB);
		}
		endrmtabent();
		fendrmtabent(fp);
		xfunlock(lockid);
	}
	else {
		endrmtabent();
	}
	return 0;
}
