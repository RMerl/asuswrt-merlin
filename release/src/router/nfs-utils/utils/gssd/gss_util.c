/*
 *  Adapted in part from MIT Kerberos 5-1.2.1 slave/kprop.c and from
 *  http://docs.sun.com/?p=/doc/816-1331/6m7oo9sms&a=view
 *
 *  Copyright (c) 2002 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Andy Adamson <andros@umich.edu>
 *  J. Bruce Fields <bfields@umich.edu>
 *  Marius Aamodt Eriksen <marius@umich.edu>
 */

/*
 * slave/kprop.c
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Copyright 1994 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/file.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <netdb.h>
#include <fcntl.h>
#include <gssapi/gssapi.h>
#if defined(HAVE_KRB5) && !defined(GSS_C_NT_HOSTBASED_SERVICE)
#include <gssapi/gssapi_generic.h>
#define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name
#endif
#include "gss_util.h"
#include "err_util.h"
#include "gssd.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#ifdef HAVE_COM_ERR_H
#include <com_err.h>
#endif

/* Global gssd_credentials handle */
gss_cred_id_t gssd_creds;

gss_OID g_mechOid = GSS_C_NULL_OID;;

#if 0
static void
display_status_1(char *m, u_int32_t code, int type, const gss_OID mech)
{
	u_int32_t maj_stat, min_stat;
	gss_buffer_desc msg = GSS_C_EMPTY_BUFFER;
	u_int32_t msg_ctx = 0;
	char *typestr;

	switch (type) {
	case GSS_C_GSS_CODE:
		typestr = "GSS";
		break;
	case GSS_C_MECH_CODE:
		typestr = "mechanism";
		break;
	default:
		return;
		/* NOTREACHED */
	}

	for (;;) {
		maj_stat = gss_display_status(&min_stat, code,
		    type, mech, &msg_ctx, &msg);
		if (maj_stat != GSS_S_COMPLETE) {
			printerr(0, "ERROR: in call to "
				"gss_display_status called from %s\n", m);
			break;
		} else {
			printerr(0, "ERROR: GSS-API: (%s) error in %s(): %s\n",
			    typestr, m, (char *)msg.value);
		}

		if (msg.length != 0)
			(void) gss_release_buffer(&min_stat, &msg);

		if (msg_ctx == 0)
			break;
	}
}
#endif

static void
display_status_2(char *m, u_int32_t major, u_int32_t minor, const gss_OID mech)
{
	u_int32_t maj_stat1, min_stat1;
	u_int32_t maj_stat2, min_stat2;
	gss_buffer_desc maj_gss_buf = GSS_C_EMPTY_BUFFER;
	gss_buffer_desc min_gss_buf = GSS_C_EMPTY_BUFFER;
	char maj_buf[30], min_buf[30];
	char *maj, *min;
	u_int32_t msg_ctx = 0;
	int msg_verbosity = 0;

	/* Get major status message */
	maj_stat1 = gss_display_status(&min_stat1, major,
		GSS_C_GSS_CODE, mech, &msg_ctx, &maj_gss_buf);

	if (maj_stat1 != GSS_S_COMPLETE) {
		snprintf(maj_buf, sizeof(maj_buf), "(0x%08x)", major);
		maj = &maj_buf[0];
	} else {
		maj = maj_gss_buf.value;
	}

	/* Get minor status message */
	maj_stat2 = gss_display_status(&min_stat2, minor,
		GSS_C_MECH_CODE, mech, &msg_ctx, &min_gss_buf);

	if (maj_stat2 != GSS_S_COMPLETE) {
		snprintf(min_buf, sizeof(min_buf), "(0x%08x)", minor);
		min = &min_buf[0];
	} else {
		min = min_gss_buf.value;
	}

	if (major == GSS_S_CREDENTIALS_EXPIRED)
		msg_verbosity = 1;
	printerr(msg_verbosity, "ERROR: GSS-API: error in %s(): %s - %s\n",
		 m, maj, min);

	if (maj_gss_buf.length != 0)
		(void) gss_release_buffer(&min_stat1, &maj_gss_buf);
	if (min_gss_buf.length != 0)
		(void) gss_release_buffer(&min_stat2, &min_gss_buf);
}

void
pgsserr(char *msg, u_int32_t maj_stat, u_int32_t min_stat, const gss_OID mech)
{
	display_status_2(msg, maj_stat, min_stat, mech);
}

int
gssd_acquire_cred(char *server_name)
{
	gss_buffer_desc name;
	gss_name_t target_name;
	u_int32_t maj_stat, min_stat;
	u_int32_t ignore_maj_stat, ignore_min_stat;
	gss_buffer_desc pbuf;

	name.value = (void *)server_name;
	name.length = strlen(server_name);

	maj_stat = gss_import_name(&min_stat, &name,
			(const gss_OID) GSS_C_NT_HOSTBASED_SERVICE,
			&target_name);

	if (maj_stat != GSS_S_COMPLETE) {
		pgsserr("gss_import_name", maj_stat, min_stat, g_mechOid);
		return (FALSE);
	}

	maj_stat = gss_acquire_cred(&min_stat, target_name, 0,
			GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
			&gssd_creds, NULL, NULL);

	if (maj_stat != GSS_S_COMPLETE) {
		pgsserr("gss_acquire_cred", maj_stat, min_stat, g_mechOid);
		ignore_maj_stat = gss_display_name(&ignore_min_stat,
				target_name, &pbuf, NULL);
		if (ignore_maj_stat == GSS_S_COMPLETE) {
			printerr(1, "Unable to obtain credentials for '%.*s'\n",
				 pbuf.length, pbuf.value);
			ignore_maj_stat = gss_release_buffer(&ignore_min_stat,
							     &pbuf);
		}
	}

	ignore_maj_stat = gss_release_name(&ignore_min_stat, &target_name);

	return (maj_stat == GSS_S_COMPLETE);
}

int gssd_check_mechs(void)
{
	u_int32_t maj_stat, min_stat;
	gss_OID_set supported_mechs = GSS_C_NO_OID_SET;
	int retval = -1;

	maj_stat = gss_indicate_mechs(&min_stat, &supported_mechs);
	if (maj_stat != GSS_S_COMPLETE) {
		printerr(0, "Unable to obtain list of supported mechanisms. "
			 "Check that gss library is properly configured.\n");
		goto out;
	}
	if (supported_mechs == GSS_C_NO_OID_SET ||
	    supported_mechs->count == 0) {
		printerr(0, "Unable to obtain list of supported mechanisms. "
			 "Check that gss library is properly configured.\n");
		goto out;
	}
	maj_stat = gss_release_oid_set(&min_stat, &supported_mechs);
	retval = 0;
out:
	return retval;
}

