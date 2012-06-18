#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sun/vddrv.h>

extern struct streamtab ppp_compinfo;
extern int ppp_comp_count;

static struct vdldrv vd = {
    VDMAGIC_USER,
    "ppp_comp"
};

static int fmodsw_index = -1;

int
ppp_comp_vdcmd(fun, vdp, vdi, vds)
    unsigned int fun;
    struct vddrv *vdp;
    addr_t vdi;
    struct vdstat *vds;
{
    int n;

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
	fmodsw[n].f_str = &ppp_compinfo;
	fmodsw_index = n;
	break;

    case VDUNLOAD:
	if (ppp_comp_count > 0)
	    return EBUSY;
	if (fmodsw_index <= 0)
	    return EINVAL;
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
