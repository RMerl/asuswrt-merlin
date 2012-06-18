/*
 * ppp_comp_mod.c - modload support for PPP compression STREAMS module.
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
 * $Id: ppp_comp_mod.c,v 1.2 2002/12/06 09:49:16 paulus Exp $
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
