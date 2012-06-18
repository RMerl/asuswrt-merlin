#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>

extern struct streamtab ppp_ahdlcinfo;

static struct fmodsw fsw = {
    "ppp_ahdl",
    &ppp_ahdlcinfo,
    D_NEW | D_MP | D_MTQPAIR
};

extern struct mod_ops mod_strmodops;

static struct modlstrmod modlstrmod = {
    &mod_strmodops,
    "PPP async HDLC module",
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
