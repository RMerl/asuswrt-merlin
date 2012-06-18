#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sun/vddrv.h>

extern struct streamtab pppinfo;
extern int ppp_count;
extern int nchrdev;

static struct vdldrv vd = {
    VDMAGIC_PSEUDO,
    "ppp"
};

extern int nodev();

static struct cdevsw ppp_cdevsw = {
    nodev, nodev, nodev, nodev, nodev, nodev, nodev, 0,
    &pppinfo
};

static struct cdevsw old_entry;

int
ppp_vdcmd(fun, vdp, vdi, vds)
    unsigned int fun;
    struct vddrv *vdp;
    addr_t vdi;
    struct vdstat *vds;
{
    static int majnum = -1;
    int n, maj;

    switch (fun) {
    case VDLOAD:
	/*
	 * It seems like modload doesn't install the cdevsw entry
	 * for us.  Oh well...
	 */
	for (maj = 1; maj < nchrdev; ++maj)
	    if (cdevsw[maj].d_open == vd_unuseddev)
		break;
	if (maj >= nchrdev)
	    return ENODEV;
	vd.Drv_charmajor = maj;
	old_entry = cdevsw[maj];
	cdevsw[maj] = ppp_cdevsw;
	vd.Drv_cdevsw = &ppp_cdevsw;
	vdp->vdd_vdtab = (struct vdlinkage *) &vd;
	majnum = maj;
	break;

    case VDUNLOAD:
	if (ppp_count > 0)
	    return EBUSY;
	if (vd.Drv_charmajor > 0)
	    cdevsw[vd.Drv_charmajor] = old_entry;
	break;

    case VDSTAT:
	/*
	 * We have to fool the modstat command into thinking
	 * that this module is actually a driver! This is
	 * so that installation commands that use the -exec
	 * option of modload to run a shell script find out
	 * the block and/or char major numbers of the driver
	 * loaded (so that the shell script can go off to
	 * /dev and *MAKE* the bloody device nodes- remember
	 * they might change from one load to another if
	 * you don't hardwire the number!).
	 */
	vds->vds_magic = VDMAGIC_DRV;
	vds->vds_modinfo[0] = (char) 0;
	vds->vds_modinfo[1] = (char) majnum;
	break;

    default:
	return EIO;
    }
    return 0;
}
