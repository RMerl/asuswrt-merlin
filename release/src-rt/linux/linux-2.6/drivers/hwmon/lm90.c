/*
 * lm90.c - Part of lm_sensors, Linux kernel modules for hardware
 *          monitoring
 * Copyright (C) 2003-2010  Jean Delvare <khali@linux-fr.org>
 *
 * Based on the lm83 driver. The LM90 is a sensor chip made by National
 * Semiconductor. It reports up to two temperatures (its own plus up to
 * one external one) with a 0.125 deg resolution (1 deg for local
 * temperature) and a 3-4 deg accuracy.
 *
 * This driver also supports the LM89 and LM99, two other sensor chips
 * made by National Semiconductor. Both have an increased remote
 * temperature measurement accuracy (1 degree), and the LM99
 * additionally shifts remote temperatures (measured and limits) by 16
 * degrees, which allows for higher temperatures measurement.
 * Note that there is no way to differentiate between both chips.
 * When device is auto-detected, the driver will assume an LM99.
 *
 * This driver also supports the LM86, another sensor chip made by
 * National Semiconductor. It is exactly similar to the LM90 except it
 * has a higher accuracy.
 *
 * This driver also supports the ADM1032, a sensor chip made by Analog
 * Devices. That chip is similar to the LM90, with a few differences
 * that are not handled by this driver. Among others, it has a higher
 * accuracy than the LM90, much like the LM86 does.
 *
 * This driver also supports the MAX6657, MAX6658 and MAX6659 sensor
 * chips made by Maxim. These chips are similar to the LM86.
 * Note that there is no easy way to differentiate between the three
 * variants. We use the device address to detect MAX6659, which will result
 * in a detection as max6657 if it is on address 0x4c. The extra address
 * and features of the MAX6659 are only supported if the chip is configured
 * explicitly as max6659, or if its address is not 0x4c.
 * These chips lack the remote temperature offset feature.
 *
 * This driver also supports the MAX6646, MAX6647, MAX6648, MAX6649 and
 * MAX6692 chips made by Maxim.  These are again similar to the LM86,
 * but they use unsigned temperature values and can report temperatures
 * from 0 to 145 degrees.
 *
 * This driver also supports the MAX6680 and MAX6681, two other sensor
 * chips made by Maxim. These are quite similar to the other Maxim
 * chips. The MAX6680 and MAX6681 only differ in the pinout so they can
 * be treated identically.
 *
 * This driver also supports the MAX6695 and MAX6696, two other sensor
 * chips made by Maxim. These are also quite similar to other Maxim
 * chips, but support three temperature sensors instead of two. MAX6695
 * and MAX6696 only differ in the pinout so they can be treated identically.
 *
 * This driver also supports ADT7461 and ADT7461A from Analog Devices as well as
 * NCT1008 from ON Semiconductor. The chips are supported in both compatibility
 * and extended mode. They are mostly compatible with LM90 except for a data
 * format difference for the temperature value registers.
 *
 * Since the LM90 was the first chipset supported by this driver, most
 * comments will refer to this chipset, but are actually general and
 * concern all supported chipsets, unless mentioned otherwise.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

/*
 * Addresses to scan
 * Address is fully defined internally and cannot be changed except for
 * MAX6659, MAX6680 and MAX6681.
 * LM86, LM89, LM90, LM99, ADM1032, ADM1032-1, ADT7461, ADT7461A, MAX6649,
 * MAX6657, MAX6658, NCT1008 and W83L771 have address 0x4c.
 * ADM1032-2, ADT7461-2, ADT7461A-2, LM89-1, LM99-1, MAX6646, and NCT1008D
 * have address 0x4d.
 * MAX6647 has address 0x4e.
 * MAX6659 can have address 0x4c, 0x4d or 0x4e.
 * MAX6680 and MAX6681 can have address 0x18, 0x19, 0x1a, 0x29, 0x2a, 0x2b,
 * 0x4c, 0x4d or 0x4e.
 */

static const unsigned short normal_i2c[] = {
	0x18, 0x19, 0x1a, 0x29, 0x2a, 0x2b, 0x4c, 0x4d, 0x4e, I2C_CLIENT_END };

enum chips { lm90, adm1032, lm99, lm86, max6657, max6659, adt7461, max6680,
	max6646, w83l771, max6696 };

/*
 * The LM90 registers
 */

#define LM90_REG_R_MAN_ID		0xFE
#define LM90_REG_R_CHIP_ID		0xFF
#define LM90_REG_R_CONFIG1		0x03
#define LM90_REG_W_CONFIG1		0x09
#define LM90_REG_R_CONFIG2		0xBF
#define LM90_REG_W_CONFIG2		0xBF
#define LM90_REG_R_CONVRATE		0x04
#define LM90_REG_W_CONVRATE		0x0A
#define LM90_REG_R_STATUS		0x02
#define LM90_REG_R_LOCAL_TEMP		0x00
#define LM90_REG_R_LOCAL_HIGH		0x05
#define LM90_REG_W_LOCAL_HIGH		0x0B
#define LM90_REG_R_LOCAL_LOW		0x06
#define LM90_REG_W_LOCAL_LOW		0x0C
#define LM90_REG_R_LOCAL_CRIT		0x20
#define LM90_REG_W_LOCAL_CRIT		0x20
#define LM90_REG_R_REMOTE_TEMPH		0x01
#define LM90_REG_R_REMOTE_TEMPL		0x10
#define LM90_REG_R_REMOTE_OFFSH		0x11
#define LM90_REG_W_REMOTE_OFFSH		0x11
#define LM90_REG_R_REMOTE_OFFSL		0x12
#define LM90_REG_W_REMOTE_OFFSL		0x12
#define LM90_REG_R_REMOTE_HIGHH		0x07
#define LM90_REG_W_REMOTE_HIGHH		0x0D
#define LM90_REG_R_REMOTE_HIGHL		0x13
#define LM90_REG_W_REMOTE_HIGHL		0x13
#define LM90_REG_R_REMOTE_LOWH		0x08
#define LM90_REG_W_REMOTE_LOWH		0x0E
#define LM90_REG_R_REMOTE_LOWL		0x14
#define LM90_REG_W_REMOTE_LOWL		0x14
#define LM90_REG_R_REMOTE_CRIT		0x19
#define LM90_REG_W_REMOTE_CRIT		0x19
#define LM90_REG_R_TCRIT_HYST		0x21
#define LM90_REG_W_TCRIT_HYST		0x21

/* MAX6646/6647/6649/6657/6658/6659/6695/6696 registers */

#define MAX6657_REG_R_LOCAL_TEMPL	0x11
#define MAX6696_REG_R_STATUS2		0x12
#define MAX6659_REG_R_REMOTE_EMERG	0x16
#define MAX6659_REG_W_REMOTE_EMERG	0x16
#define MAX6659_REG_R_LOCAL_EMERG	0x17
#define MAX6659_REG_W_LOCAL_EMERG	0x17

#define LM90_DEF_CONVRATE_RVAL	6	/* Def conversion rate register value */
#define LM90_MAX_CONVRATE_MS	16000	/* Maximum conversion rate in ms */

/*
 * Device flags
 */
#define LM90_FLAG_ADT7461_EXT	(1 << 0) /* ADT7461 extended mode	*/
/* Device features */
#define LM90_HAVE_OFFSET	(1 << 1) /* temperature offset register	*/
#define LM90_HAVE_LOCAL_EXT	(1 << 2) /* extended local temperature	*/
#define LM90_HAVE_REM_LIMIT_EXT	(1 << 3) /* extended remote limit	*/
#define LM90_HAVE_EMERGENCY	(1 << 4) /* 3rd upper (emergency) limit	*/
#define LM90_HAVE_EMERGENCY_ALARM (1 << 5)/* emergency alarm		*/
#define LM90_HAVE_TEMP3		(1 << 6) /* 3rd temperature sensor	*/
#define LM90_HAVE_BROKEN_ALERT	(1 << 7) /* Broken alert		*/

/*
 * Driver data (common to all clients)
 */

static const struct i2c_device_id lm90_id[] = {
	{ "adm1032", adm1032 },
	{ "adt7461", adt7461 },
	{ "adt7461a", adt7461 },
	{ "lm90", lm90 },
	{ "lm86", lm86 },
	{ "lm89", lm86 },
	{ "lm99", lm99 },
	{ "max6646", max6646 },
	{ "max6647", max6646 },
	{ "max6649", max6646 },
	{ "max6657", max6657 },
	{ "max6658", max6657 },
	{ "max6659", max6659 },
	{ "max6680", max6680 },
	{ "max6681", max6680 },
	{ "max6695", max6696 },
	{ "max6696", max6696 },
	{ "nct1008", adt7461 },
	{ "w83l771", w83l771 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm90_id);

/*
 * chip type specific parameters
 */
struct lm90_params {
	u32 flags;		/* Capabilities */
	u16 alert_alarms;	/* Which alarm bits trigger ALERT# */
				/* Upper 8 bits for max6695/96 */
	u8 max_convrate;	/* Maximum conversion rate register value */
};

static const struct lm90_params lm90_params[] = {
	[adm1032] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT,
		.alert_alarms = 0x7c,
		.max_convrate = 10,
	},
	[adt7461] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT
		  | LM90_HAVE_BROKEN_ALERT,
		.alert_alarms = 0x7c,
		.max_convrate = 10,
	},
	[lm86] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT,
		.alert_alarms = 0x7b,
		.max_convrate = 9,
	},
	[lm90] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT,
		.alert_alarms = 0x7b,
		.max_convrate = 9,
	},
	[lm99] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT,
		.alert_alarms = 0x7b,
		.max_convrate = 9,
	},
	[max6646] = {
		.flags = LM90_HAVE_LOCAL_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 6,
	},
	[max6657] = {
		.flags = LM90_HAVE_LOCAL_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 8,
	},
	[max6659] = {
		.flags = LM90_HAVE_LOCAL_EXT | LM90_HAVE_EMERGENCY,
		.alert_alarms = 0x7c,
		.max_convrate = 8,
	},
	[max6680] = {
		.flags = LM90_HAVE_OFFSET,
		.alert_alarms = 0x7c,
		.max_convrate = 7,
	},
	[max6696] = {
		.flags = LM90_HAVE_LOCAL_EXT | LM90_HAVE_EMERGENCY
		  | LM90_HAVE_EMERGENCY_ALARM | LM90_HAVE_TEMP3,
		.alert_alarms = 0x187c,
		.max_convrate = 6,
	},
	[w83l771] = {
		.flags = LM90_HAVE_OFFSET | LM90_HAVE_REM_LIMIT_EXT,
		.alert_alarms = 0x7c,
		.max_convrate = 8,
	},
};

/*
 * Client data (each client gets its own)
 */

struct lm90_data {
	struct device *hwmon_dev;
	struct mutex update_lock;
	char valid; /* zero until following fields are valid */
	unsigned long last_updated; /* in jiffies */
	int kind;
	u32 flags;

	int update_interval;	/* in milliseconds */

	u8 config_orig;		/* Original configuration register value */
	u8 convrate_orig;	/* Original conversion rate register value */
	u16 alert_alarms;	/* Which alarm bits trigger ALERT# */
				/* Upper 8 bits for max6695/96 */
	u8 max_convrate;	/* Maximum conversion rate */

	/* registers values */
	s8 temp8[8];	/* 0: local low limit
			   1: local high limit
			   2: local critical limit
			   3: remote critical limit
			   4: local emergency limit (max6659 and max6695/96)
			   5: remote emergency limit (max6659 and max6695/96)
			   6: remote 2 critical limit (max6695/96 only)
			   7: remote 2 emergency limit (max6695/96 only) */
	s16 temp11[8];	/* 0: remote input
			   1: remote low limit
			   2: remote high limit
			   3: remote offset (except max6646, max6657/58/59,
					     and max6695/96)
			   4: local input
			   5: remote 2 input (max6695/96 only)
			   6: remote 2 low limit (max6695/96 only)
			   7: remote 2 high limit (ma6695/96 only) */
	u8 temp_hyst;
	u16 alarms; /* bitvector (upper 8 bits for max6695/96) */
};

/*
 * Support functions
 */

/*
 * The ADM1032 supports PEC but not on write byte transactions, so we need
 * to explicitly ask for a transaction without PEC.
 */
static inline s32 adm1032_write_byte(struct i2c_client *client, u8 value)
{
	return i2c_smbus_xfer(client->adapter, client->addr,
			      client->flags & ~I2C_CLIENT_PEC,
			      I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}

/*
 * It is assumed that client->update_lock is held (unless we are in
 * detection or initialization steps). This matters when PEC is enabled,
 * because we don't want the address pointer to change between the write
 * byte and the read byte transactions.
 */
static int lm90_read_reg(struct i2c_client *client, u8 reg, u8 *value)
{
	int err;

	if (client->flags & I2C_CLIENT_PEC) {
		err = adm1032_write_byte(client, reg);
		if (err >= 0)
			err = i2c_smbus_read_byte(client);
	} else
		err = i2c_smbus_read_byte_data(client, reg);

	if (err < 0) {
		dev_warn(&client->dev, "Register %#02x read failed (%d)\n",
			 reg, err);
		return err;
	}
	*value = err;

	return 0;
}

static int lm90_read16(struct i2c_client *client, u8 regh, u8 regl, u16 *value)
{
	int err;
	u8 oldh, newh, l;

	/*
	 * There is a trick here. We have to read two registers to have the
	 * sensor temperature, but we have to beware a conversion could occur
	 * between the readings. The datasheet says we should either use
	 * the one-shot conversion register, which we don't want to do
	 * (disables hardware monitoring) or monitor the busy bit, which is
	 * impossible (we can't read the values and monitor that bit at the
	 * exact same time). So the solution used here is to read the high
	 * byte once, then the low byte, then the high byte again. If the new
	 * high byte matches the old one, then we have a valid reading. Else
	 * we have to read the low byte again, and now we believe we have a
	 * correct reading.
	 */
	if ((err = lm90_read_reg(client, regh, &oldh))
	 || (err = lm90_read_reg(client, regl, &l))
	 || (err = lm90_read_reg(client, regh, &newh)))
		return err;
	if (oldh != newh) {
		err = lm90_read_reg(client, regl, &l);
		if (err)
			return err;
	}
	*value = (newh << 8) | l;

	return 0;
}

/*
 * client->update_lock must be held when calling this function (unless we are
 * in detection or initialization steps), and while a remote channel other
 * than channel 0 is selected. Also, calling code must make sure to re-select
 * external channel 0 before releasing the lock. This is necessary because
 * various registers have different meanings as a result of selecting a
 * non-default remote channel.
 */
static inline void lm90_select_remote_channel(struct i2c_client *client,
					      struct lm90_data *data,
					      int channel)
{
	u8 config;

	if (data->kind == max6696) {
		lm90_read_reg(client, LM90_REG_R_CONFIG1, &config);
		config &= ~0x08;
		if (channel)
			config |= 0x08;
		i2c_smbus_write_byte_data(client, LM90_REG_W_CONFIG1,
					  config);
	}
}

/*
 * Set conversion rate.
 * client->update_lock must be held when calling this function (unless we are
 * in detection or initialization steps).
 */
static void lm90_set_convrate(struct i2c_client *client, struct lm90_data *data,
			      unsigned int interval)
{
	int i;
	unsigned int update_interval;

	/* Shift calculations to avoid rounding errors */
	interval <<= 6;

	/* find the nearest update rate */
	for (i = 0, update_interval = LM90_MAX_CONVRATE_MS << 6;
	     i < data->max_convrate; i++, update_interval >>= 1)
		if (interval >= update_interval * 3 / 4)
			break;

	i2c_smbus_write_byte_data(client, LM90_REG_W_CONVRATE, i);
	data->update_interval = DIV_ROUND_CLOSEST(update_interval, 64);
}

static struct lm90_data *lm90_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lm90_data *data = i2c_get_clientdata(client);
	unsigned long next_update;

	mutex_lock(&data->update_lock);

	next_update = data->last_updated
	  + msecs_to_jiffies(data->update_interval) + 1;
	if (time_after(jiffies, next_update) || !data->valid) {
		u8 h, l;
		u8 alarms;

		dev_dbg(&client->dev, "Updating lm90 data.\n");
		lm90_read_reg(client, LM90_REG_R_LOCAL_LOW, &data->temp8[0]);
		lm90_read_reg(client, LM90_REG_R_LOCAL_HIGH, &data->temp8[1]);
		lm90_read_reg(client, LM90_REG_R_LOCAL_CRIT, &data->temp8[2]);
		lm90_read_reg(client, LM90_REG_R_REMOTE_CRIT, &data->temp8[3]);
		lm90_read_reg(client, LM90_REG_R_TCRIT_HYST, &data->temp_hyst);

		if (data->flags & LM90_HAVE_LOCAL_EXT) {
			lm90_read16(client, LM90_REG_R_LOCAL_TEMP,
				    MAX6657_REG_R_LOCAL_TEMPL,
				    &data->temp11[4]);
		} else {
			if (lm90_read_reg(client, LM90_REG_R_LOCAL_TEMP,
					  &h) == 0)
				data->temp11[4] = h << 8;
		}
		lm90_read16(client, LM90_REG_R_REMOTE_TEMPH,
			    LM90_REG_R_REMOTE_TEMPL, &data->temp11[0]);

		if (lm90_read_reg(client, LM90_REG_R_REMOTE_LOWH, &h) == 0) {
			data->temp11[1] = h << 8;
			if ((data->flags & LM90_HAVE_REM_LIMIT_EXT)
			 && lm90_read_reg(client, LM90_REG_R_REMOTE_LOWL,
					  &l) == 0)
				data->temp11[1] |= l;
		}
		if (lm90_read_reg(client, LM90_REG_R_REMOTE_HIGHH, &h) == 0) {
			data->temp11[2] = h << 8;
			if ((data->flags & LM90_HAVE_REM_LIMIT_EXT)
			 && lm90_read_reg(client, LM90_REG_R_REMOTE_HIGHL,
					  &l) == 0)
				data->temp11[2] |= l;
		}

		if (data->flags & LM90_HAVE_OFFSET) {
			if (lm90_read_reg(client, LM90_REG_R_REMOTE_OFFSH,
					  &h) == 0
			 && lm90_read_reg(client, LM90_REG_R_REMOTE_OFFSL,
					  &l) == 0)
				data->temp11[3] = (h << 8) | l;
		}
		if (data->flags & LM90_HAVE_EMERGENCY) {
			lm90_read_reg(client, MAX6659_REG_R_LOCAL_EMERG,
				      &data->temp8[4]);
			lm90_read_reg(client, MAX6659_REG_R_REMOTE_EMERG,
				      &data->temp8[5]);
		}
		lm90_read_reg(client, LM90_REG_R_STATUS, &alarms);
		data->alarms = alarms;	/* save as 16 bit value */

		if (data->kind == max6696) {
			lm90_select_remote_channel(client, data, 1);
			lm90_read_reg(client, LM90_REG_R_REMOTE_CRIT,
				      &data->temp8[6]);
			lm90_read_reg(client, MAX6659_REG_R_REMOTE_EMERG,
				      &data->temp8[7]);
			lm90_read16(client, LM90_REG_R_REMOTE_TEMPH,
				    LM90_REG_R_REMOTE_TEMPL, &data->temp11[5]);
			if (!lm90_read_reg(client, LM90_REG_R_REMOTE_LOWH, &h))
				data->temp11[6] = h << 8;
			if (!lm90_read_reg(client, LM90_REG_R_REMOTE_HIGHH, &h))
				data->temp11[7] = h << 8;
			lm90_select_remote_channel(client, data, 0);

			if (!lm90_read_reg(client, MAX6696_REG_R_STATUS2,
					   &alarms))
				data->alarms |= alarms << 8;
		}

		/* Re-enable ALERT# output if it was originally enabled and
		 * relevant alarms are all clear */
		if ((data->config_orig & 0x80) == 0
		 && (data->alarms & data->alert_alarms) == 0) {
			u8 config;

			lm90_read_reg(client, LM90_REG_R_CONFIG1, &config);
			if (config & 0x80) {
				dev_dbg(&client->dev, "Re-enabling ALERT#\n");
				i2c_smbus_write_byte_data(client,
							  LM90_REG_W_CONFIG1,
							  config & ~0x80);
			}
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

/*
 * Conversions
 * For local temperatures and limits, critical limits and the hysteresis
 * value, the LM90 uses signed 8-bit values with LSB = 1 degree Celsius.
 * For remote temperatures and limits, it uses signed 11-bit values with
 * LSB = 0.125 degree Celsius, left-justified in 16-bit registers.  Some
 * Maxim chips use unsigned values.
 */

static inline int temp_from_s8(s8 val)
{
	return val * 1000;
}

static inline int temp_from_u8(u8 val)
{
	return val * 1000;
}

static inline int temp_from_s16(s16 val)
{
	return val / 32 * 125;
}

static inline int temp_from_u16(u16 val)
{
	return val / 32 * 125;
}

static s8 temp_to_s8(long val)
{
	if (val <= -128000)
		return -128;
	if (val >= 127000)
		return 127;
	if (val < 0)
		return (val - 500) / 1000;
	return (val + 500) / 1000;
}

static u8 temp_to_u8(long val)
{
	if (val <= 0)
		return 0;
	if (val >= 255000)
		return 255;
	return (val + 500) / 1000;
}

static s16 temp_to_s16(long val)
{
	if (val <= -128000)
		return 0x8000;
	if (val >= 127875)
		return 0x7FE0;
	if (val < 0)
		return (val - 62) / 125 * 32;
	return (val + 62) / 125 * 32;
}

static u8 hyst_to_reg(long val)
{
	if (val <= 0)
		return 0;
	if (val >= 30500)
		return 31;
	return (val + 500) / 1000;
}

/*
 * ADT7461 in compatibility mode is almost identical to LM90 except that
 * attempts to write values that are outside the range 0 < temp < 127 are
 * treated as the boundary value.
 *
 * ADT7461 in "extended mode" operation uses unsigned integers offset by
 * 64 (e.g., 0 -> -64 degC).  The range is restricted to -64..191 degC.
 */
static inline int temp_from_u8_adt7461(struct lm90_data *data, u8 val)
{
	if (data->flags & LM90_FLAG_ADT7461_EXT)
		return (val - 64) * 1000;
	else
		return temp_from_s8(val);
}

static inline int temp_from_u16_adt7461(struct lm90_data *data, u16 val)
{
	if (data->flags & LM90_FLAG_ADT7461_EXT)
		return (val - 0x4000) / 64 * 250;
	else
		return temp_from_s16(val);
}

static u8 temp_to_u8_adt7461(struct lm90_data *data, long val)
{
	if (data->flags & LM90_FLAG_ADT7461_EXT) {
		if (val <= -64000)
			return 0;
		if (val >= 191000)
			return 0xFF;
		return (val + 500 + 64000) / 1000;
	} else {
		if (val <= 0)
			return 0;
		if (val >= 127000)
			return 127;
		return (val + 500) / 1000;
	}
}

static u16 temp_to_u16_adt7461(struct lm90_data *data, long val)
{
	if (data->flags & LM90_FLAG_ADT7461_EXT) {
		if (val <= -64000)
			return 0;
		if (val >= 191750)
			return 0xFFC0;
		return (val + 64000 + 125) / 250 * 64;
	} else {
		if (val <= 0)
			return 0;
		if (val >= 127750)
			return 0x7FC0;
		return (val + 125) / 250 * 64;
	}
}

/*
 * Sysfs stuff
 */

static ssize_t show_temp8(struct device *dev, struct device_attribute *devattr,
			  char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct lm90_data *data = lm90_update_device(dev);
	int temp;

	if (data->kind == adt7461)
		temp = temp_from_u8_adt7461(data, data->temp8[attr->index]);
	else if (data->kind == max6646)
		temp = temp_from_u8(data->temp8[attr->index]);
	else
		temp = temp_from_s8(data->temp8[attr->index]);

	/* +16 degrees offset for temp2 for the LM99 */
	if (data->kind == lm99 && attr->index == 3)
		temp += 16000;

	return sprintf(buf, "%d\n", temp);
}

static ssize_t set_temp8(struct device *dev, struct device_attribute *devattr,
			 const char *buf, size_t count)
{
	static const u8 reg[8] = {
		LM90_REG_W_LOCAL_LOW,
		LM90_REG_W_LOCAL_HIGH,
		LM90_REG_W_LOCAL_CRIT,
		LM90_REG_W_REMOTE_CRIT,
		MAX6659_REG_W_LOCAL_EMERG,
		MAX6659_REG_W_REMOTE_EMERG,
		LM90_REG_W_REMOTE_CRIT,
		MAX6659_REG_W_REMOTE_EMERG,
	};

	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct i2c_client *client = to_i2c_client(dev);
	struct lm90_data *data = i2c_get_clientdata(client);
	int nr = attr->index;
	long val;
	int err;

	err = strict_strtol(buf, 10, &val);
	if (err < 0)
		return err;

	/* +16 degrees offset for temp2 for the LM99 */
	if (data->kind == lm99 && attr->index == 3)
		val -= 16000;

	mutex_lock(&data->update_lock);
	if (data->kind == adt7461)
		data->temp8[nr] = temp_to_u8_adt7461(data, val);
	else if (data->kind == max6646)
		data->temp8[nr] = temp_to_u8(val);
	else
		data->temp8[nr] = temp_to_s8(val);

	lm90_select_remote_channel(client, data, nr >= 6);
	i2c_smbus_write_byte_data(client, reg[nr], data->temp8[nr]);
	lm90_select_remote_channel(client, data, 0);

	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_temp11(struct device *dev, struct device_attribute *devattr,
			   char *buf)
{
	struct sensor_device_attribute_2 *attr = to_sensor_dev_attr_2(devattr);
	struct lm90_data *data = lm90_update_device(dev);
	int temp;

	if (data->kind == adt7461)
		temp = temp_from_u16_adt7461(data, data->temp11[attr->index]);
	else if (data->kind == max6646)
		temp = temp_from_u16(data->temp11[attr->index]);
	else
		temp = temp_from_s16(data->temp11[attr->index]);

	/* +16 degrees offset for temp2 for the LM99 */
	if (data->kind == lm99 &&  attr->index <= 2)
		temp += 16000;

	return sprintf(buf, "%d\n", temp);
}

static ssize_t set_temp11(struct device *dev, struct device_attribute *devattr,
			  const char *buf, size_t count)
{
	struct {
		u8 high;
		u8 low;
		int channel;
	} reg[5] = {
		{ LM90_REG_W_REMOTE_LOWH, LM90_REG_W_REMOTE_LOWL, 0 },
		{ LM90_REG_W_REMOTE_HIGHH, LM90_REG_W_REMOTE_HIGHL, 0 },
		{ LM90_REG_W_REMOTE_OFFSH, LM90_REG_W_REMOTE_OFFSL, 0 },
		{ LM90_REG_W_REMOTE_LOWH, LM90_REG_W_REMOTE_LOWL, 1 },
		{ LM90_REG_W_REMOTE_HIGHH, LM90_REG_W_REMOTE_HIGHL, 1 }
	};

	struct sensor_device_attribute_2 *attr = to_sensor_dev_attr_2(devattr);
	struct i2c_client *client = to_i2c_client(dev);
	struct lm90_data *data = i2c_get_clientdata(client);
	int nr = attr->nr;
	int index = attr->index;
	long val;
	int err;

	err = strict_strtol(buf, 10, &val);
	if (err < 0)
		return err;

	/* +16 degrees offset for temp2 for the LM99 */
	if (data->kind == lm99 && index <= 2)
		val -= 16000;

	mutex_lock(&data->update_lock);
	if (data->kind == adt7461)
		data->temp11[index] = temp_to_u16_adt7461(data, val);
	else if (data->kind == max6646)
		data->temp11[index] = temp_to_u8(val) << 8;
	else if (data->flags & LM90_HAVE_REM_LIMIT_EXT)
		data->temp11[index] = temp_to_s16(val);
	else
		data->temp11[index] = temp_to_s8(val) << 8;

	lm90_select_remote_channel(client, data, reg[nr].channel);
	i2c_smbus_write_byte_data(client, reg[nr].high,
				  data->temp11[index] >> 8);
	if (data->flags & LM90_HAVE_REM_LIMIT_EXT)
		i2c_smbus_write_byte_data(client, reg[nr].low,
					  data->temp11[index] & 0xff);
	lm90_select_remote_channel(client, data, 0);

	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_temphyst(struct device *dev,
			     struct device_attribute *devattr,
			     char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct lm90_data *data = lm90_update_device(dev);
	int temp;

	if (data->kind == adt7461)
		temp = temp_from_u8_adt7461(data, data->temp8[attr->index]);
	else if (data->kind == max6646)
		temp = temp_from_u8(data->temp8[attr->index]);
	else
		temp = temp_from_s8(data->temp8[attr->index]);

	/* +16 degrees offset for temp2 for the LM99 */
	if (data->kind == lm99 && attr->index == 3)
		temp += 16000;

	return sprintf(buf, "%d\n", temp - temp_from_s8(data->temp_hyst));
}

static ssize_t set_temphyst(struct device *dev, struct device_attribute *dummy,
			    const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lm90_data *data = i2c_get_clientdata(client);
	long val;
	int err;
	int temp;

	err = strict_strtol(buf, 10, &val);
	if (err < 0)
		return err;

	mutex_lock(&data->update_lock);
	if (data->kind == adt7461)
		temp = temp_from_u8_adt7461(data, data->temp8[2]);
	else if (data->kind == max6646)
		temp = temp_from_u8(data->temp8[2]);
	else
		temp = temp_from_s8(data->temp8[2]);

	data->temp_hyst = hyst_to_reg(temp - val);
	i2c_smbus_write_byte_data(client, LM90_REG_W_TCRIT_HYST,
				  data->temp_hyst);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_alarms(struct device *dev, struct device_attribute *dummy,
			   char *buf)
{
	struct lm90_data *data = lm90_update_device(dev);
	return sprintf(buf, "%d\n", data->alarms);
}

static ssize_t show_alarm(struct device *dev, struct device_attribute
			  *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct lm90_data *data = lm90_update_device(dev);
	int bitnr = attr->index;

	return sprintf(buf, "%d\n", (data->alarms >> bitnr) & 1);
}

static ssize_t show_update_interval(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct lm90_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->update_interval);
}

static ssize_t set_update_interval(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lm90_data *data = i2c_get_clientdata(client);
	unsigned long val;
	int err;

	err = strict_strtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	lm90_set_convrate(client, data, val);
	mutex_unlock(&data->update_lock);

	return count;
}

static SENSOR_DEVICE_ATTR_2(temp1_input, S_IRUGO, show_temp11, NULL, 0, 4);
static SENSOR_DEVICE_ATTR_2(temp2_input, S_IRUGO, show_temp11, NULL, 0, 0);
static SENSOR_DEVICE_ATTR(temp1_min, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 0);
static SENSOR_DEVICE_ATTR_2(temp2_min, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 0, 1);
static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 1);
static SENSOR_DEVICE_ATTR_2(temp2_max, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 1, 2);
static SENSOR_DEVICE_ATTR(temp1_crit, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 2);
static SENSOR_DEVICE_ATTR(temp2_crit, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 3);
static SENSOR_DEVICE_ATTR(temp1_crit_hyst, S_IWUSR | S_IRUGO, show_temphyst,
	set_temphyst, 2);
static SENSOR_DEVICE_ATTR(temp2_crit_hyst, S_IRUGO, show_temphyst, NULL, 3);
static SENSOR_DEVICE_ATTR_2(temp2_offset, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 2, 3);

/* Individual alarm files */
static SENSOR_DEVICE_ATTR(temp1_crit_alarm, S_IRUGO, show_alarm, NULL, 0);
static SENSOR_DEVICE_ATTR(temp2_crit_alarm, S_IRUGO, show_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(temp2_fault, S_IRUGO, show_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(temp2_min_alarm, S_IRUGO, show_alarm, NULL, 3);
static SENSOR_DEVICE_ATTR(temp2_max_alarm, S_IRUGO, show_alarm, NULL, 4);
static SENSOR_DEVICE_ATTR(temp1_min_alarm, S_IRUGO, show_alarm, NULL, 5);
static SENSOR_DEVICE_ATTR(temp1_max_alarm, S_IRUGO, show_alarm, NULL, 6);
/* Raw alarm file for compatibility */
static DEVICE_ATTR(alarms, S_IRUGO, show_alarms, NULL);

static DEVICE_ATTR(update_interval, S_IRUGO | S_IWUSR, show_update_interval,
		   set_update_interval);

static struct attribute *lm90_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp2_min.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp2_max.dev_attr.attr,
	&sensor_dev_attr_temp1_crit.dev_attr.attr,
	&sensor_dev_attr_temp2_crit.dev_attr.attr,
	&sensor_dev_attr_temp1_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp2_crit_hyst.dev_attr.attr,

	&sensor_dev_attr_temp1_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_fault.dev_attr.attr,
	&sensor_dev_attr_temp2_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_max_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_max_alarm.dev_attr.attr,
	&dev_attr_alarms.attr,
	&dev_attr_update_interval.attr,
	NULL
};

static const struct attribute_group lm90_group = {
	.attrs = lm90_attributes,
};

/*
 * Additional attributes for devices with emergency sensors
 */
static SENSOR_DEVICE_ATTR(temp1_emergency, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 4);
static SENSOR_DEVICE_ATTR(temp2_emergency, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 5);
static SENSOR_DEVICE_ATTR(temp1_emergency_hyst, S_IRUGO, show_temphyst,
			  NULL, 4);
static SENSOR_DEVICE_ATTR(temp2_emergency_hyst, S_IRUGO, show_temphyst,
			  NULL, 5);

static struct attribute *lm90_emergency_attributes[] = {
	&sensor_dev_attr_temp1_emergency.dev_attr.attr,
	&sensor_dev_attr_temp2_emergency.dev_attr.attr,
	&sensor_dev_attr_temp1_emergency_hyst.dev_attr.attr,
	&sensor_dev_attr_temp2_emergency_hyst.dev_attr.attr,
	NULL
};

static const struct attribute_group lm90_emergency_group = {
	.attrs = lm90_emergency_attributes,
};

static SENSOR_DEVICE_ATTR(temp1_emergency_alarm, S_IRUGO, show_alarm, NULL, 15);
static SENSOR_DEVICE_ATTR(temp2_emergency_alarm, S_IRUGO, show_alarm, NULL, 13);

static struct attribute *lm90_emergency_alarm_attributes[] = {
	&sensor_dev_attr_temp1_emergency_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_emergency_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group lm90_emergency_alarm_group = {
	.attrs = lm90_emergency_alarm_attributes,
};

/*
 * Additional attributes for devices with 3 temperature sensors
 */
static SENSOR_DEVICE_ATTR_2(temp3_input, S_IRUGO, show_temp11, NULL, 0, 5);
static SENSOR_DEVICE_ATTR_2(temp3_min, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 3, 6);
static SENSOR_DEVICE_ATTR_2(temp3_max, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 4, 7);
static SENSOR_DEVICE_ATTR(temp3_crit, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 6);
static SENSOR_DEVICE_ATTR(temp3_crit_hyst, S_IRUGO, show_temphyst, NULL, 6);
static SENSOR_DEVICE_ATTR(temp3_emergency, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 7);
static SENSOR_DEVICE_ATTR(temp3_emergency_hyst, S_IRUGO, show_temphyst,
			  NULL, 7);

static SENSOR_DEVICE_ATTR(temp3_crit_alarm, S_IRUGO, show_alarm, NULL, 9);
static SENSOR_DEVICE_ATTR(temp3_fault, S_IRUGO, show_alarm, NULL, 10);
static SENSOR_DEVICE_ATTR(temp3_min_alarm, S_IRUGO, show_alarm, NULL, 11);
static SENSOR_DEVICE_ATTR(temp3_max_alarm, S_IRUGO, show_alarm, NULL, 12);
static SENSOR_DEVICE_ATTR(temp3_emergency_alarm, S_IRUGO, show_alarm, NULL, 14);

static struct attribute *lm90_temp3_attributes[] = {
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp3_min.dev_attr.attr,
	&sensor_dev_attr_temp3_max.dev_attr.attr,
	&sensor_dev_attr_temp3_crit.dev_attr.attr,
	&sensor_dev_attr_temp3_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp3_emergency.dev_attr.attr,
	&sensor_dev_attr_temp3_emergency_hyst.dev_attr.attr,

	&sensor_dev_attr_temp3_fault.dev_attr.attr,
	&sensor_dev_attr_temp3_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp3_max_alarm.dev_attr.attr,
	&sensor_dev_attr_temp3_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_temp3_emergency_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group lm90_temp3_group = {
	.attrs = lm90_temp3_attributes,
};

/* pec used for ADM1032 only */
static ssize_t show_pec(struct device *dev, struct device_attribute *dummy,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	return sprintf(buf, "%d\n", !!(client->flags & I2C_CLIENT_PEC));
}

static ssize_t set_pec(struct device *dev, struct device_attribute *dummy,
		       const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	long val;
	int err;

	err = strict_strtol(buf, 10, &val);
	if (err < 0)
		return err;

	switch (val) {
	case 0:
		client->flags &= ~I2C_CLIENT_PEC;
		break;
	case 1:
		client->flags |= I2C_CLIENT_PEC;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(pec, S_IWUSR | S_IRUGO, show_pec, set_pec);

/*
 * Real code
 */

/* Return 0 if detection is successful, -ENODEV otherwise */
static int lm90_detect(struct i2c_client *new_client,
		       struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = new_client->adapter;
	int address = new_client->addr;
	const char *name = NULL;
	int man_id, chip_id, reg_config1, reg_convrate;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	/* detection and identification */
	if ((man_id = i2c_smbus_read_byte_data(new_client,
						LM90_REG_R_MAN_ID)) < 0
	 || (chip_id = i2c_smbus_read_byte_data(new_client,
						LM90_REG_R_CHIP_ID)) < 0
	 || (reg_config1 = i2c_smbus_read_byte_data(new_client,
						LM90_REG_R_CONFIG1)) < 0
	 || (reg_convrate = i2c_smbus_read_byte_data(new_client,
						LM90_REG_R_CONVRATE)) < 0)
		return -ENODEV;

	if ((address == 0x4C || address == 0x4D)
	 && man_id == 0x01) { /* National Semiconductor */
		int reg_config2;

		reg_config2 = i2c_smbus_read_byte_data(new_client,
						LM90_REG_R_CONFIG2);
		if (reg_config2 < 0)
			return -ENODEV;

		if ((reg_config1 & 0x2A) == 0x00
		 && (reg_config2 & 0xF8) == 0x00
		 && reg_convrate <= 0x09) {
			if (address == 0x4C
			 && (chip_id & 0xF0) == 0x20) { /* LM90 */
				name = "lm90";
			} else
			if ((chip_id & 0xF0) == 0x30) { /* LM89/LM99 */
				name = "lm99";
				dev_info(&adapter->dev,
					 "Assuming LM99 chip at 0x%02x\n",
					 address);
				dev_info(&adapter->dev,
					 "If it is an LM89, instantiate it "
					 "with the new_device sysfs "
					 "interface\n");
			} else
			if (address == 0x4C
			 && (chip_id & 0xF0) == 0x10) { /* LM86 */
				name = "lm86";
			}
		}
	} else
	if ((address == 0x4C || address == 0x4D)
	 && man_id == 0x41) { /* Analog Devices */
		if ((chip_id & 0xF0) == 0x40 /* ADM1032 */
		 && (reg_config1 & 0x3F) == 0x00
		 && reg_convrate <= 0x0A) {
			name = "adm1032";
			/* The ADM1032 supports PEC, but only if combined
			   transactions are not used. */
			if (i2c_check_functionality(adapter,
						    I2C_FUNC_SMBUS_BYTE))
				info->flags |= I2C_CLIENT_PEC;
		} else
		if (chip_id == 0x51 /* ADT7461 */
		 && (reg_config1 & 0x1B) == 0x00
		 && reg_convrate <= 0x0A) {
			name = "adt7461";
		} else
		if (chip_id == 0x57 /* ADT7461A, NCT1008 */
		 && (reg_config1 & 0x1B) == 0x00
		 && reg_convrate <= 0x0A) {
			name = "adt7461a";
		}
	} else
	if (man_id == 0x4D) { /* Maxim */
		int reg_emerg, reg_emerg2, reg_status2;

		/*
		 * We read MAX6659_REG_R_REMOTE_EMERG twice, and re-read
		 * LM90_REG_R_MAN_ID in between. If MAX6659_REG_R_REMOTE_EMERG
		 * exists, both readings will reflect the same value. Otherwise,
		 * the readings will be different.
		 */
		if ((reg_emerg = i2c_smbus_read_byte_data(new_client,
						MAX6659_REG_R_REMOTE_EMERG)) < 0
		 || i2c_smbus_read_byte_data(new_client, LM90_REG_R_MAN_ID) < 0
		 || (reg_emerg2 = i2c_smbus_read_byte_data(new_client,
						MAX6659_REG_R_REMOTE_EMERG)) < 0
		 || (reg_status2 = i2c_smbus_read_byte_data(new_client,
						MAX6696_REG_R_STATUS2)) < 0)
			return -ENODEV;

		/*
		 * The MAX6657, MAX6658 and MAX6659 do NOT have a chip_id
		 * register. Reading from that address will return the last
		 * read value, which in our case is those of the man_id
		 * register. Likewise, the config1 register seems to lack a
		 * low nibble, so the value will be those of the previous
		 * read, so in our case those of the man_id register.
		 * MAX6659 has a third set of upper temperature limit registers.
		 * Those registers also return values on MAX6657 and MAX6658,
		 * thus the only way to detect MAX6659 is by its address.
		 * For this reason it will be mis-detected as MAX6657 if its
		 * address is 0x4C.
		 */
		if (chip_id == man_id
		 && (address == 0x4C || address == 0x4D || address == 0x4E)
		 && (reg_config1 & 0x1F) == (man_id & 0x0F)
		 && reg_convrate <= 0x09) {
			if (address == 0x4C)
				name = "max6657";
			else
				name = "max6659";
		} else
		/*
		 * Even though MAX6695 and MAX6696 do not have a chip ID
		 * register, reading it returns 0x01. Bit 4 of the config1
		 * register is unused and should return zero when read. Bit 0 of
		 * the status2 register is unused and should return zero when
		 * read.
		 *
		 * MAX6695 and MAX6696 have an additional set of temperature
		 * limit registers. We can detect those chips by checking if
		 * one of those registers exists.
		 */
		if (chip_id == 0x01
		 && (reg_config1 & 0x10) == 0x00
		 && (reg_status2 & 0x01) == 0x00
		 && reg_emerg == reg_emerg2
		 && reg_convrate <= 0x07) {
			name = "max6696";
		} else
		/*
		 * The chip_id register of the MAX6680 and MAX6681 holds the
		 * revision of the chip. The lowest bit of the config1 register
		 * is unused and should return zero when read, so should the
		 * second to last bit of config1 (software reset).
		 */
		if (chip_id == 0x01
		 && (reg_config1 & 0x03) == 0x00
		 && reg_convrate <= 0x07) {
			name = "max6680";
		} else
		/*
		 * The chip_id register of the MAX6646/6647/6649 holds the
		 * revision of the chip. The lowest 6 bits of the config1
		 * register are unused and should return zero when read.
		 */
		if (chip_id == 0x59
		 && (reg_config1 & 0x3f) == 0x00
		 && reg_convrate <= 0x07) {
			name = "max6646";
		}
	} else
	if (address == 0x4C
	 && man_id == 0x5C) { /* Winbond/Nuvoton */
		int reg_config2;

		reg_config2 = i2c_smbus_read_byte_data(new_client,
						LM90_REG_R_CONFIG2);
		if (reg_config2 < 0)
			return -ENODEV;

		if ((reg_config1 & 0x2A) == 0x00
		 && (reg_config2 & 0xF8) == 0x00) {
			if (chip_id == 0x01 /* W83L771W/G */
			 && reg_convrate <= 0x09) {
				name = "w83l771";
			} else
			if ((chip_id & 0xFE) == 0x10 /* W83L771AWG/ASG */
			 && reg_convrate <= 0x08) {
				name = "w83l771";
			}
		}
	}

	if (!name) { /* identification failed */
		dev_dbg(&adapter->dev,
			"Unsupported chip at 0x%02x (man_id=0x%02X, "
			"chip_id=0x%02X)\n", address, man_id, chip_id);
		return -ENODEV;
	}

	strlcpy(info->type, name, I2C_NAME_SIZE);

	return 0;
}

static void lm90_remove_files(struct i2c_client *client, struct lm90_data *data)
{
	if (data->flags & LM90_HAVE_TEMP3)
		sysfs_remove_group(&client->dev.kobj, &lm90_temp3_group);
	if (data->flags & LM90_HAVE_EMERGENCY_ALARM)
		sysfs_remove_group(&client->dev.kobj,
				   &lm90_emergency_alarm_group);
	if (data->flags & LM90_HAVE_EMERGENCY)
		sysfs_remove_group(&client->dev.kobj,
				   &lm90_emergency_group);
	if (data->flags & LM90_HAVE_OFFSET)
		device_remove_file(&client->dev,
				   &sensor_dev_attr_temp2_offset.dev_attr);
	device_remove_file(&client->dev, &dev_attr_pec);
	sysfs_remove_group(&client->dev.kobj, &lm90_group);
}

static void lm90_init_client(struct i2c_client *client)
{
	u8 config, convrate;
	struct lm90_data *data = i2c_get_clientdata(client);

	if (lm90_read_reg(client, LM90_REG_R_CONVRATE, &convrate) < 0) {
		dev_warn(&client->dev, "Failed to read convrate register!\n");
		convrate = LM90_DEF_CONVRATE_RVAL;
	}
	data->convrate_orig = convrate;

	/*
	 * Start the conversions.
	 */
	lm90_set_convrate(client, data, 500);	/* 500ms; 2Hz conversion rate */
	if (lm90_read_reg(client, LM90_REG_R_CONFIG1, &config) < 0) {
		dev_warn(&client->dev, "Initialization failed!\n");
		return;
	}
	data->config_orig = config;

	/* Check Temperature Range Select */
	if (data->kind == adt7461) {
		if (config & 0x04)
			data->flags |= LM90_FLAG_ADT7461_EXT;
	}

	/*
	 * Put MAX6680/MAX8881 into extended resolution (bit 0x10,
	 * 0.125 degree resolution) and range (0x08, extend range
	 * to -64 degree) mode for the remote temperature sensor.
	 */
	if (data->kind == max6680)
		config |= 0x18;

	/*
	 * Select external channel 0 for max6695/96
	 */
	if (data->kind == max6696)
		config &= ~0x08;

	config &= 0xBF;	/* run */
	if (config != data->config_orig) /* Only write if changed */
		i2c_smbus_write_byte_data(client, LM90_REG_W_CONFIG1, config);
}

static int lm90_probe(struct i2c_client *new_client,
		      const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(new_client->dev.parent);
	struct lm90_data *data;
	int err;

	data = kzalloc(sizeof(struct lm90_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	i2c_set_clientdata(new_client, data);
	mutex_init(&data->update_lock);

	/* Set the device type */
	data->kind = id->driver_data;
	if (data->kind == adm1032) {
		if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
			new_client->flags &= ~I2C_CLIENT_PEC;
	}

	/* Different devices have different alarm bits triggering the
	 * ALERT# output */
	data->alert_alarms = lm90_params[data->kind].alert_alarms;

	/* Set chip capabilities */
	data->flags = lm90_params[data->kind].flags;

	/* Set maximum conversion rate */
	data->max_convrate = lm90_params[data->kind].max_convrate;

	/* Initialize the LM90 chip */
	lm90_init_client(new_client);

	/* Register sysfs hooks */
	err = sysfs_create_group(&new_client->dev.kobj, &lm90_group);
	if (err)
		goto exit_free;
	if (new_client->flags & I2C_CLIENT_PEC) {
		err = device_create_file(&new_client->dev, &dev_attr_pec);
		if (err)
			goto exit_remove_files;
	}
	if (data->flags & LM90_HAVE_OFFSET) {
		err = device_create_file(&new_client->dev,
					&sensor_dev_attr_temp2_offset.dev_attr);
		if (err)
			goto exit_remove_files;
	}
	if (data->flags & LM90_HAVE_EMERGENCY) {
		err = sysfs_create_group(&new_client->dev.kobj,
					 &lm90_emergency_group);
		if (err)
			goto exit_remove_files;
	}
	if (data->flags & LM90_HAVE_EMERGENCY_ALARM) {
		err = sysfs_create_group(&new_client->dev.kobj,
					 &lm90_emergency_alarm_group);
		if (err)
			goto exit_remove_files;
	}
	if (data->flags & LM90_HAVE_TEMP3) {
		err = sysfs_create_group(&new_client->dev.kobj,
					 &lm90_temp3_group);
		if (err)
			goto exit_remove_files;
	}

	data->hwmon_dev = hwmon_device_register(&new_client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove_files;
	}

	return 0;

exit_remove_files:
	lm90_remove_files(new_client, data);
exit_free:
	kfree(data);
exit:
	return err;
}

static int lm90_remove(struct i2c_client *client)
{
	struct lm90_data *data = i2c_get_clientdata(client);

	hwmon_device_unregister(data->hwmon_dev);
	lm90_remove_files(client, data);

	/* Restore initial configuration */
	i2c_smbus_write_byte_data(client, LM90_REG_W_CONVRATE,
				  data->convrate_orig);
	i2c_smbus_write_byte_data(client, LM90_REG_W_CONFIG1,
				  data->config_orig);

	kfree(data);
	return 0;
}

static void lm90_alert(struct i2c_client *client, unsigned int flag)
{
	struct lm90_data *data = i2c_get_clientdata(client);
	u8 config, alarms, alarms2 = 0;

	lm90_read_reg(client, LM90_REG_R_STATUS, &alarms);

	if (data->kind == max6696)
		lm90_read_reg(client, MAX6696_REG_R_STATUS2, &alarms2);

	if ((alarms & 0x7f) == 0 && (alarms2 & 0xfe) == 0) {
		dev_info(&client->dev, "Everything OK\n");
	} else {
		if (alarms & 0x61)
			dev_warn(&client->dev,
				 "temp%d out of range, please check!\n", 1);
		if (alarms & 0x1a)
			dev_warn(&client->dev,
				 "temp%d out of range, please check!\n", 2);
		if (alarms & 0x04)
			dev_warn(&client->dev,
				 "temp%d diode open, please check!\n", 2);

		if (alarms2 & 0x18)
			dev_warn(&client->dev,
				 "temp%d out of range, please check!\n", 3);

		/* Disable ALERT# output, because these chips don't implement
		  SMBus alert correctly; they should only hold the alert line
		  low briefly. */
		if ((data->flags & LM90_HAVE_BROKEN_ALERT)
		 && (alarms & data->alert_alarms)) {
			dev_dbg(&client->dev, "Disabling ALERT#\n");
			lm90_read_reg(client, LM90_REG_R_CONFIG1, &config);
			i2c_smbus_write_byte_data(client, LM90_REG_W_CONFIG1,
						  config | 0x80);
		}
	}
}

static struct i2c_driver lm90_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "lm90",
	},
	.probe		= lm90_probe,
	.remove		= lm90_remove,
	.alert		= lm90_alert,
	.id_table	= lm90_id,
	.detect		= lm90_detect,
	.address_list	= normal_i2c,
};

static int __init sensors_lm90_init(void)
{
	return i2c_add_driver(&lm90_driver);
}

static void __exit sensors_lm90_exit(void)
{
	i2c_del_driver(&lm90_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("LM90/ADM1032 driver");
MODULE_LICENSE("GPL");

module_init(sensors_lm90_init);
module_exit(sensors_lm90_exit);
