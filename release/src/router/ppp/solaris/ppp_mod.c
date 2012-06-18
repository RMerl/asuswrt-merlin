/*
 * ppp_mod.c - modload support for PPP pseudo-device driver.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 *
 * $Id: ppp_mod.c,v 1.1.1.4 2003/10/14 08:09:54 sparq Exp $
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
    "PPP-2.3 multiplexing driver",
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
    return strcmp(ddi_get_name(dip), "ppp") == 0? DDI_IDENTIFIED:
	DDI_NOT_IDENTIFIED;
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
