/*
 * Linux driver for digital TV devices equipped with B2C2 FlexcopII(b)/III
 * flexcop-eeprom.c - eeprom access methods (currently only MAC address reading)
 * see flexcop.c for copyright information
 */
#include "flexcop.h"


static u8 calc_lrc(u8 *buf, int len)
{
	int i;
	u8 sum = 0;
	for (i = 0; i < len; i++)
		sum = sum ^ buf[i];
	return sum;
}

static int flexcop_eeprom_request(struct flexcop_device *fc,
	flexcop_access_op_t op, u16 addr, u8 *buf, u16 len, int retries)
{
	int i,ret = 0;
	u8 chipaddr =  0x50 | ((addr >> 8) & 3);
	for (i = 0; i < retries; i++) {
		ret = fc->i2c_request(&fc->fc_i2c_adap[1], op, chipaddr,
			addr & 0xff, buf, len);
		if (ret == 0)
			break;
	}
	return ret;
}

static int flexcop_eeprom_lrc_read(struct flexcop_device *fc, u16 addr,
		u8 *buf, u16 len, int retries)
{
	int ret = flexcop_eeprom_request(fc, FC_READ, addr, buf, len, retries);
	if (ret == 0)
		if (calc_lrc(buf, len - 1) != buf[len - 1])
			ret = -EINVAL;
	return ret;
}

/* JJ's comment about extended == 1: it is not presently used anywhere but was
 * added to the low-level functions for possible support of EUI64 */
int flexcop_eeprom_check_mac_addr(struct flexcop_device *fc, int extended)
{
	u8 buf[8];
	int ret = 0;

	if ((ret = flexcop_eeprom_lrc_read(fc,0x3f8,buf,8,4)) == 0) {
		if (extended != 0) {
			err("TODO: extended (EUI64) MAC addresses aren't "
				"completely supported yet");
			ret = -EINVAL;
		} else
			memcpy(fc->dvb_adapter.proposed_mac,buf,6);
	}
	return ret;
}
EXPORT_SYMBOL(flexcop_eeprom_check_mac_addr);
