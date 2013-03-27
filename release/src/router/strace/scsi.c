#include "defs.h"

#ifdef LINUX
#include <sys/ioctl.h>
#include <scsi/sg.h>

static const struct xlat sg_io_dxfer_direction[] = {
	{SG_DXFER_NONE,        "SG_DXFER_NONE"},
	{SG_DXFER_TO_DEV,      "SG_DXFER_TO_DEV"},
	{SG_DXFER_FROM_DEV,    "SG_DXFER_FROM_DEV"},
	{SG_DXFER_TO_FROM_DEV, "SG_DXFER_TO_FROM_DEV"},
	{0, NULL}
};

static void
print_sg_io_buffer(struct tcb *tcp, unsigned char *addr, int len)
{
	unsigned char *buf = NULL;
	int     allocated, i;

	if (len == 0)
		return;
	allocated = (len > max_strlen) ? max_strlen : len;
	if (len < 0 ||
	    (buf = malloc(allocated)) == NULL ||
	    umoven(tcp, (unsigned long) addr, allocated, (char *) buf) < 0) {
		tprintf("%p", addr);
		free(buf);
		return;
	}
	tprintf("%02x", buf[0]);
	for (i = 1; i < allocated; ++i)
		tprintf(", %02x", buf[i]);
	free(buf);
	if (allocated != len)
		tprintf(", ...");
}

static void
print_sg_io_req(struct tcb *tcp, struct sg_io_hdr *sg_io)
{
	tprintf("{'%c', ", sg_io->interface_id);
	printxval(sg_io_dxfer_direction, sg_io->dxfer_direction,
		  "SG_DXFER_???");
	tprintf(", cmd[%u]=[", sg_io->cmd_len);
	print_sg_io_buffer(tcp, sg_io->cmdp, sg_io->cmd_len);
	tprintf("], mx_sb_len=%d, ", sg_io->mx_sb_len);
	tprintf("iovec_count=%d, ", sg_io->iovec_count);
	tprintf("dxfer_len=%u, ", sg_io->dxfer_len);
	tprintf("timeout=%u, ", sg_io->timeout);
	tprintf("flags=%#x", sg_io->flags);

	if (sg_io->dxfer_direction == SG_DXFER_TO_DEV ||
	    sg_io->dxfer_direction == SG_DXFER_TO_FROM_DEV) {
		tprintf(", data[%u]=[", sg_io->dxfer_len);
		printstr(tcp, (unsigned long) sg_io->dxferp,
			 sg_io->dxfer_len);
		tprintf("]");
	}
}

static void
print_sg_io_res(struct tcb *tcp, struct sg_io_hdr *sg_io)
{
	if (sg_io->dxfer_direction == SG_DXFER_FROM_DEV ||
	    sg_io->dxfer_direction == SG_DXFER_TO_FROM_DEV) {
		tprintf(", data[%u]=[", sg_io->dxfer_len);
		printstr(tcp, (unsigned long) sg_io->dxferp,
			 sg_io->dxfer_len);
		tprintf("]");
	}
	tprintf(", status=%02x, ", sg_io->status);
	tprintf("masked_status=%02x, ", sg_io->masked_status);
	tprintf("sb[%u]=[", sg_io->sb_len_wr);
	print_sg_io_buffer(tcp, sg_io->sbp, sg_io->sb_len_wr);
	tprintf("], host_status=%#x, ", sg_io->host_status);
	tprintf("driver_status=%#x, ", sg_io->driver_status);
	tprintf("resid=%d, ", sg_io->resid);
	tprintf("duration=%d, ", sg_io->duration);
	tprintf("info=%#x}", sg_io->info);
}

int
scsi_ioctl(struct tcb *tcp, long code, long arg)
{
	switch (code) {
	case SG_IO:
		if (entering(tcp)) {
			struct sg_io_hdr sg_io;

			if (umove(tcp, arg, &sg_io) < 0)
				tprintf(", %#lx", arg);
			else {
				tprintf(", ");
				print_sg_io_req(tcp, &sg_io);
			}
		}
		if (exiting(tcp)) {
			struct sg_io_hdr sg_io;

			if (!syserror(tcp) && umove(tcp, arg, &sg_io) >= 0)
				print_sg_io_res(tcp, &sg_io);
			else
				tprintf("}");
		}
		break;
	default:
		if (entering(tcp))
			tprintf(", %#lx", arg);
		break;
	}
	return 1;
}
#endif /* LINUX */
