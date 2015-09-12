/*
 * Linux driver for digital TV devices equipped with B2C2 FlexcopII(b)/III
 * flexcop-sram.c - functions for controlling the SRAM
 * see flexcop.c for copyright information
 */
#include "flexcop.h"

static void flexcop_sram_set_chip(struct flexcop_device *fc,
		flexcop_sram_type_t type)
{
	flexcop_set_ibi_value(wan_ctrl_reg_71c, sram_chip, type);
}

int flexcop_sram_init(struct flexcop_device *fc)
{
	switch (fc->rev) {
	case FLEXCOP_II:
	case FLEXCOP_IIB:
		flexcop_sram_set_chip(fc, FC_SRAM_1_32KB);
		break;
	case FLEXCOP_III:
		flexcop_sram_set_chip(fc, FC_SRAM_1_48KB);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int flexcop_sram_set_dest(struct flexcop_device *fc, flexcop_sram_dest_t dest,
		 flexcop_sram_dest_target_t target)
{
	flexcop_ibi_value v;
	v = fc->read_ibi_reg(fc, sram_dest_reg_714);

	if (fc->rev != FLEXCOP_III && target == FC_SRAM_DEST_TARGET_FC3_CA) {
		err("SRAM destination target to available on FlexCopII(b)\n");
		return -EINVAL;
	}
	deb_sram("sram dest: %x target: %x\n", dest, target);

	if (dest & FC_SRAM_DEST_NET)
		v.sram_dest_reg_714.NET_Dest = target;
	if (dest & FC_SRAM_DEST_CAI)
		v.sram_dest_reg_714.CAI_Dest = target;
	if (dest & FC_SRAM_DEST_CAO)
		v.sram_dest_reg_714.CAO_Dest = target;
	if (dest & FC_SRAM_DEST_MEDIA)
		v.sram_dest_reg_714.MEDIA_Dest = target;

	fc->write_ibi_reg(fc,sram_dest_reg_714,v);
	udelay(1000); /* TODO delay really necessary */

	return 0;
}
EXPORT_SYMBOL(flexcop_sram_set_dest);

void flexcop_wan_set_speed(struct flexcop_device *fc, flexcop_wan_speed_t s)
{
	flexcop_set_ibi_value(wan_ctrl_reg_71c,wan_speed_sig,s);
}
EXPORT_SYMBOL(flexcop_wan_set_speed);

void flexcop_sram_ctrl(struct flexcop_device *fc, int usb_wan, int sramdma, int maximumfill)
{
	flexcop_ibi_value v = fc->read_ibi_reg(fc,sram_dest_reg_714);
	v.sram_dest_reg_714.ctrl_usb_wan = usb_wan;
	v.sram_dest_reg_714.ctrl_sramdma = sramdma;
	v.sram_dest_reg_714.ctrl_maximumfill = maximumfill;
	fc->write_ibi_reg(fc,sram_dest_reg_714,v);
}
EXPORT_SYMBOL(flexcop_sram_ctrl);
