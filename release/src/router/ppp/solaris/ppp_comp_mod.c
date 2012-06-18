/*
 * ppp_comp_mod.c - modload support for PPP compression STREAMS module.
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
 * $Id: ppp_comp_mod.c,v 1.1.1.4 2003/10/14 08:09:54 sparq Exp $
 */

/*
 * This file is used under Solaris 2.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>

extern struct streamtab ppp_compinfo;

static struct fmodsw fsw = {
    "ppp_comp",
    &ppp_compinfo,
    D_NEW | D_MP | D_MTQPAIR
};

extern struct mod_ops mod_strmodops;

static struct modlstrmod modlstrmod = {
    &mod_strmodops,
    "PPP compression module",
    &fsw
};

static struct modlinkage modlinkage = {
    MODREV_1,
    (void *) &modlstrmod,
    NULL
};

/*
 * Entry points for modloading.
 */
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
