#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sun/vddrv.h>

extern struct streamtab if_pppinfo;

static struct vdldrv vd = {
    VDMAGIC_USER,
    "if_ppp"
};

static int fmodsw_index = -1;

int
if_ppp_vdcmd(fun, vdp, vdi, vds)
    unsigned int fun;
    struct vddrv *vdp;
    addr_t vdi;
    struct vdstat *vds;
{
    int n, error;

    switch (fun) {
    case VDLOAD:
	vdp->vdd_vdtab = (struct vdlinkage *) &vd;
	if (fmodsw_index >= 0)
	    return EBUSY;
	for (n = 0; n < fmodcnt; ++n)
	    if (fmodsw[n].f_str == 0)
		break;
	if (n >= fmodcnt)
	    return ENODEV;
	strncpy(fmodsw[n].f_name, vd.Drv_name, FMNAMESZ+1);
	fmodsw[n].f_str = &if_pppinfo;
	fmodsw_index = n;
	break;

    case VDUNLOAD:
	if (fmodsw_index <= 0)
	    return EINVAL;
	error = if_ppp_unload();
	if (error != 0)
	    return error;
	fmodsw[fmodsw_index].f_name[0] = 0;
	fmodsw[fmodsw_index].f_str = 0;
	fmodsw_index = -1;
	break;

    case VDSTAT:
	break;

    default:
	return EIO;
    }
    return 0;
}
