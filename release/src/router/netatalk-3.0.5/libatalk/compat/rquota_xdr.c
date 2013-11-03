/*
 *
 * taken from the quota-1.55 used on linux. here's the bsd copyright:
 *
 * Copyright (c) 1980, 1990 Regents of the University of California. All
 * rights reserved.
 *
 * This code is derived from software contributed to Berkeley by Robert Elz at
 * The University of Melbourne.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h> /* to get __GNU_LIBRARY__ */

/* list of machines that don't have these functions:
	solaris
	linux libc5
*/
#if defined(NEED_RQUOTA) || defined(SOLARIS) || (defined(__GNU_LIBRARY__) && __GNU_LIBRARY__ < 6)

#include <rpc/rpc.h>
#include <rpcsvc/rquota.h>

#ifndef u_int
#define u_int unsigned
#endif

bool_t
xdr_getquota_args(xdrs, objp)
	XDR *xdrs;
	getquota_args *objp;
{
	if (!xdr_string(xdrs, &objp->gqa_pathp, RQ_PATHLEN)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->gqa_uid)) {
		return (FALSE);
	}
	return (TRUE);
}


bool_t
xdr_rquota(xdrs, objp)
	XDR *xdrs;
	rquota *objp;
{
	if (!xdr_int(xdrs, &objp->rq_bsize)) {
		return (FALSE);
	}
	if (!xdr_bool(xdrs, &objp->rq_active)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_bhardlimit)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_bsoftlimit)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_curblocks)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_fhardlimit)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_fsoftlimit)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_curfiles)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_btimeleft)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->rq_ftimeleft)) {
		return (FALSE);
	}
	return (TRUE);
}




bool_t
xdr_gqr_status(xdrs, objp)
	XDR *xdrs;
	gqr_status *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}


bool_t
xdr_getquota_rslt(xdrs, objp)
	XDR *xdrs;
	getquota_rslt *objp;
{
	if (!xdr_gqr_status(xdrs, &objp->status)) {
		return (FALSE);
	}
	switch (objp->status) {
	case Q_OK:
		if (!xdr_rquota(xdrs, &objp->getquota_rslt_u.gqr_rquota)) {
			return (FALSE);
		}
		break;
	case Q_NOQUOTA:
		break;
	case Q_EPERM:
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}
#endif
