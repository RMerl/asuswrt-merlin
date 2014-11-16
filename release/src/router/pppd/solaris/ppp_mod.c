/*
 * ppp_mod.c - modload support for PPP pseudo-device driver.
 *
 * Copyright (c) 1994 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ppp_mod.c,v 1.3 2004/01/17 05:47:55 carlsonj Exp $
 */

/*
 * This file is used under Solaris 2.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>

#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x)	()
#endif

static int ppp_identify __P((dev_info_t *));
static int ppp_attach __P((dev_info_t *, ddi_attach_cmd_t));
static int ppp_detach __P((dev_info_t *, ddi_detach_cmd_t));
static int ppp_devinfo __P((dev_info_t *, ddi_info_cmd_t, void *, void **));

extern struct streamtab pppinfo;
extern krwlock_t ppp_lower_lock;

static dev_info_t *ppp_dip;

static struct cb_ops cb_ppp_ops = {
    nulldev, nulldev, nodev, nodev,	/* cb_open, ... */
    nodev, nodev, nodev, nodev,		/* cb_dump, ... */
    nodev, nodev, nodev, nochpoll,	/* cb_devmap, ... */
    ddi_prop_op,			/* cb_prop_op */
    &pppinfo,				/* cb_stream */
    D_NEW|D_MP|D_MTQPAIR|D_MTOUTPERIM|D_MTOCEXCL	/* cb_flag */
};

static struct dev_ops ppp_ops = {
    DEVO_REV,				/* devo_rev */
    0,					/* devo_refcnt */
    ppp_devinfo,			/* devo_getinfo */
    ppp_identify,			/* devo_identify */
    nulldev,				/* devo_probe */
    ppp_attach,				/* devo_attach */
    ppp_detach,				/* devo_detach */
    nodev,				/* devo_reset */
    &cb_ppp_ops,			/* devo_cb_ops */
    NULL				/* devo_bus_ops */
};

/*
 * Module linkage information
 */

static struct modldrv modldrv = {
    &mod_driverops,			/* says this is a pseudo driver */
    "PPP-2.4.7 multiplexing driver",
    &ppp_ops				/* driver ops */
};

static struct modlinkage modlinkage = {
    MODREV_1,
    (void *) &modldrv,
    NULL
};

int
_init(void)
{
    return mod_install(&modlinkage);
}

int
_fini(void)
{
    return mod_remove(&modlinkage);
}

int
_info(mip)
    struct modinfo *mip;
{
    return mod_info(&modlinkage, mip);
}

static int
ppp_identify(dip)
    dev_info_t *dip;
{
    /* This entry point is not used as of Solaris 10 */
#ifdef DDI_IDENTIFIED
    return strcmp(ddi_get_name(dip), "ppp") == 0? DDI_IDENTIFIED:
	DDI_NOT_IDENTIFIED;
#else
    return 0;
#endif
}

static int
ppp_attach(dip, cmd)
    dev_info_t *dip;
    ddi_attach_cmd_t cmd;
{

    if (cmd != DDI_ATTACH)
	return DDI_FAILURE;
    if (ddi_create_minor_node(dip, "ppp", S_IFCHR, 0, DDI_PSEUDO, CLONE_DEV)
	== DDI_FAILURE) {
	ddi_remove_minor_node(dip, NULL);
	return DDI_FAILURE;
    }
    rw_init(&ppp_lower_lock, NULL, RW_DRIVER, NULL);
    return DDI_SUCCESS;
}

static int
ppp_detach(dip, cmd)
    dev_info_t *dip;
    ddi_detach_cmd_t cmd;
{
    rw_destroy(&ppp_lower_lock);
    ddi_remove_minor_node(dip, NULL);
    return DDI_SUCCESS;
}

static int
ppp_devinfo(dip, cmd, arg, result)
    dev_info_t *dip;
    ddi_info_cmd_t cmd;
    void *arg;
    void **result;
{
    int error;

    error = DDI_SUCCESS;
    switch (cmd) {
    case DDI_INFO_DEVT2DEVINFO:
	if (ppp_dip == NULL)
	    error = DDI_FAILURE;
	else
	    *result = (void *) ppp_dip;
	break;
    case DDI_INFO_DEVT2INSTANCE:
	*result = NULL;
	break;
    default:
	error = DDI_FAILURE;
    }
    return error;
}
