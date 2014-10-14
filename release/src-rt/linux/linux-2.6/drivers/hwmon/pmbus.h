/*
 * pmbus.h - Common defines and structures for PMBus devices
 *
 * Copyright (c) 2010, 2011 Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef PMBUS_H
#define PMBUS_H

/*
 * Registers
 */
#define PMBUS_PAGE			0x00
#define PMBUS_OPERATION			0x01
#define PMBUS_ON_OFF_CONFIG		0x02
#define PMBUS_CLEAR_FAULTS		0x03
#define PMBUS_PHASE			0x04

#define PMBUS_CAPABILITY		0x19
#define PMBUS_QUERY			0x1A

#define PMBUS_VOUT_MODE			0x20
#define PMBUS_VOUT_COMMAND		0x21
#define PMBUS_VOUT_TRIM			0x22
#define PMBUS_VOUT_CAL_OFFSET		0x23
#define PMBUS_VOUT_MAX			0x24
#define PMBUS_VOUT_MARGIN_HIGH		0x25
#define PMBUS_VOUT_MARGIN_LOW		0x26
#define PMBUS_VOUT_TRANSITION_RATE	0x27
#define PMBUS_VOUT_DROOP		0x28
#define PMBUS_VOUT_SCALE_LOOP		0x29
#define PMBUS_VOUT_SCALE_MONITOR	0x2A

#define PMBUS_COEFFICIENTS		0x30
#define PMBUS_POUT_MAX			0x31

#define PMBUS_FAN_CONFIG_12		0x3A
#define PMBUS_FAN_COMMAND_1		0x3B
#define PMBUS_FAN_COMMAND_2		0x3C
#define PMBUS_FAN_CONFIG_34		0x3D
#define PMBUS_FAN_COMMAND_3		0x3E
#define PMBUS_FAN_COMMAND_4		0x3F

#define PMBUS_VOUT_OV_FAULT_LIMIT	0x40
#define PMBUS_VOUT_OV_FAULT_RESPONSE	0x41
#define PMBUS_VOUT_OV_WARN_LIMIT	0x42
#define PMBUS_VOUT_UV_WARN_LIMIT	0x43
#define PMBUS_VOUT_UV_FAULT_LIMIT	0x44
#define PMBUS_VOUT_UV_FAULT_RESPONSE	0x45
#define PMBUS_IOUT_OC_FAULT_LIMIT	0x46
#define PMBUS_IOUT_OC_FAULT_RESPONSE	0x47
#define PMBUS_IOUT_OC_LV_FAULT_LIMIT	0x48
#define PMBUS_IOUT_OC_LV_FAULT_RESPONSE	0x49
#define PMBUS_IOUT_OC_WARN_LIMIT	0x4A
#define PMBUS_IOUT_UC_FAULT_LIMIT	0x4B
#define PMBUS_IOUT_UC_FAULT_RESPONSE	0x4C

#define PMBUS_OT_FAULT_LIMIT		0x4F
#define PMBUS_OT_FAULT_RESPONSE		0x50
#define PMBUS_OT_WARN_LIMIT		0x51
#define PMBUS_UT_WARN_LIMIT		0x52
#define PMBUS_UT_FAULT_LIMIT		0x53
#define PMBUS_UT_FAULT_RESPONSE		0x54
#define PMBUS_VIN_OV_FAULT_LIMIT	0x55
#define PMBUS_VIN_OV_FAULT_RESPONSE	0x56
#define PMBUS_VIN_OV_WARN_LIMIT		0x57
#define PMBUS_VIN_UV_WARN_LIMIT		0x58
#define PMBUS_VIN_UV_FAULT_LIMIT	0x59

#define PMBUS_IIN_OC_FAULT_LIMIT	0x5B
#define PMBUS_IIN_OC_WARN_LIMIT		0x5D

#define PMBUS_POUT_OP_FAULT_LIMIT	0x68
#define PMBUS_POUT_OP_WARN_LIMIT	0x6A
#define PMBUS_PIN_OP_WARN_LIMIT		0x6B

#define PMBUS_STATUS_BYTE		0x78
#define PMBUS_STATUS_WORD		0x79
#define PMBUS_STATUS_VOUT		0x7A
#define PMBUS_STATUS_IOUT		0x7B
#define PMBUS_STATUS_INPUT		0x7C
#define PMBUS_STATUS_TEMPERATURE	0x7D
#define PMBUS_STATUS_CML		0x7E
#define PMBUS_STATUS_OTHER		0x7F
#define PMBUS_STATUS_MFR_SPECIFIC	0x80
#define PMBUS_STATUS_FAN_12		0x81
#define PMBUS_STATUS_FAN_34		0x82

#define PMBUS_READ_VIN			0x88
#define PMBUS_READ_IIN			0x89
#define PMBUS_READ_VCAP			0x8A
#define PMBUS_READ_VOUT			0x8B
#define PMBUS_READ_IOUT			0x8C
#define PMBUS_READ_TEMPERATURE_1	0x8D
#define PMBUS_READ_TEMPERATURE_2	0x8E
#define PMBUS_READ_TEMPERATURE_3	0x8F
#define PMBUS_READ_FAN_SPEED_1		0x90
#define PMBUS_READ_FAN_SPEED_2		0x91
#define PMBUS_READ_FAN_SPEED_3		0x92
#define PMBUS_READ_FAN_SPEED_4		0x93
#define PMBUS_READ_DUTY_CYCLE		0x94
#define PMBUS_READ_FREQUENCY		0x95
#define PMBUS_READ_POUT			0x96
#define PMBUS_READ_PIN			0x97

#define PMBUS_REVISION			0x98
#define PMBUS_MFR_ID			0x99
#define PMBUS_MFR_MODEL			0x9A
#define PMBUS_MFR_REVISION		0x9B
#define PMBUS_MFR_LOCATION		0x9C
#define PMBUS_MFR_DATE			0x9D
#define PMBUS_MFR_SERIAL		0x9E

/*
 * CAPABILITY
 */
#define PB_CAPABILITY_SMBALERT		(1<<4)
#define PB_CAPABILITY_ERROR_CHECK	(1<<7)

/*
 * VOUT_MODE
 */
#define PB_VOUT_MODE_MODE_MASK		0xe0
#define PB_VOUT_MODE_PARAM_MASK		0x1f

#define PB_VOUT_MODE_LINEAR		0x00
#define PB_VOUT_MODE_VID		0x20
#define PB_VOUT_MODE_DIRECT		0x40

/*
 * Fan configuration
 */
#define PB_FAN_2_PULSE_MASK		((1 << 0) | (1 << 1))
#define PB_FAN_2_RPM			(1 << 2)
#define PB_FAN_2_INSTALLED		(1 << 3)
#define PB_FAN_1_PULSE_MASK		((1 << 4) | (1 << 5))
#define PB_FAN_1_RPM			(1 << 6)
#define PB_FAN_1_INSTALLED		(1 << 7)

/*
 * STATUS_BYTE, STATUS_WORD (lower)
 */
#define PB_STATUS_NONE_ABOVE		(1<<0)
#define PB_STATUS_CML			(1<<1)
#define PB_STATUS_TEMPERATURE		(1<<2)
#define PB_STATUS_VIN_UV		(1<<3)
#define PB_STATUS_IOUT_OC		(1<<4)
#define PB_STATUS_VOUT_OV		(1<<5)
#define PB_STATUS_OFF			(1<<6)
#define PB_STATUS_BUSY			(1<<7)

/*
 * STATUS_WORD (upper)
 */
#define PB_STATUS_UNKNOWN		(1<<8)
#define PB_STATUS_OTHER			(1<<9)
#define PB_STATUS_FANS			(1<<10)
#define PB_STATUS_POWER_GOOD_N		(1<<11)
#define PB_STATUS_WORD_MFR		(1<<12)
#define PB_STATUS_INPUT			(1<<13)
#define PB_STATUS_IOUT_POUT		(1<<14)
#define PB_STATUS_VOUT			(1<<15)

/*
 * STATUS_IOUT
 */
#define PB_POUT_OP_WARNING		(1<<0)
#define PB_POUT_OP_FAULT		(1<<1)
#define PB_POWER_LIMITING		(1<<2)
#define PB_CURRENT_SHARE_FAULT		(1<<3)
#define PB_IOUT_UC_FAULT		(1<<4)
#define PB_IOUT_OC_WARNING		(1<<5)
#define PB_IOUT_OC_LV_FAULT		(1<<6)
#define PB_IOUT_OC_FAULT		(1<<7)

/*
 * STATUS_VOUT, STATUS_INPUT
 */
#define PB_VOLTAGE_UV_FAULT		(1<<4)
#define PB_VOLTAGE_UV_WARNING		(1<<5)
#define PB_VOLTAGE_OV_WARNING		(1<<6)
#define PB_VOLTAGE_OV_FAULT		(1<<7)

/*
 * STATUS_INPUT
 */
#define PB_PIN_OP_WARNING		(1<<0)
#define PB_IIN_OC_WARNING		(1<<1)
#define PB_IIN_OC_FAULT			(1<<2)

/*
 * STATUS_TEMPERATURE
 */
#define PB_TEMP_UT_FAULT		(1<<4)
#define PB_TEMP_UT_WARNING		(1<<5)
#define PB_TEMP_OT_WARNING		(1<<6)
#define PB_TEMP_OT_FAULT		(1<<7)

/*
 * STATUS_FAN
 */
#define PB_FAN_AIRFLOW_WARNING		(1<<0)
#define PB_FAN_AIRFLOW_FAULT		(1<<1)
#define PB_FAN_FAN2_SPEED_OVERRIDE	(1<<2)
#define PB_FAN_FAN1_SPEED_OVERRIDE	(1<<3)
#define PB_FAN_FAN2_WARNING		(1<<4)
#define PB_FAN_FAN1_WARNING		(1<<5)
#define PB_FAN_FAN2_FAULT		(1<<6)
#define PB_FAN_FAN1_FAULT		(1<<7)

/*
 * CML_FAULT_STATUS
 */
#define PB_CML_FAULT_OTHER_MEM_LOGIC	(1<<0)
#define PB_CML_FAULT_OTHER_COMM		(1<<1)
#define PB_CML_FAULT_PROCESSOR		(1<<3)
#define PB_CML_FAULT_MEMORY		(1<<4)
#define PB_CML_FAULT_PACKET_ERROR	(1<<5)
#define PB_CML_FAULT_INVALID_DATA	(1<<6)
#define PB_CML_FAULT_INVALID_COMMAND	(1<<7)

enum pmbus_sensor_classes {
	PSC_VOLTAGE_IN = 0,
	PSC_VOLTAGE_OUT,
	PSC_CURRENT_IN,
	PSC_CURRENT_OUT,
	PSC_POWER,
	PSC_TEMPERATURE,
	PSC_FAN,
	PSC_NUM_CLASSES		/* Number of power sensor classes */
};

#define PMBUS_PAGES	32	/* Per PMBus specification */

/* Functionality bit mask */
#define PMBUS_HAVE_VIN		(1 << 0)
#define PMBUS_HAVE_VCAP		(1 << 1)
#define PMBUS_HAVE_VOUT		(1 << 2)
#define PMBUS_HAVE_IIN		(1 << 3)
#define PMBUS_HAVE_IOUT		(1 << 4)
#define PMBUS_HAVE_PIN		(1 << 5)
#define PMBUS_HAVE_POUT		(1 << 6)
#define PMBUS_HAVE_FAN12	(1 << 7)
#define PMBUS_HAVE_FAN34	(1 << 8)
#define PMBUS_HAVE_TEMP		(1 << 9)
#define PMBUS_HAVE_TEMP2	(1 << 10)
#define PMBUS_HAVE_TEMP3	(1 << 11)
#define PMBUS_HAVE_STATUS_VOUT	(1 << 12)
#define PMBUS_HAVE_STATUS_IOUT	(1 << 13)
#define PMBUS_HAVE_STATUS_INPUT	(1 << 14)
#define PMBUS_HAVE_STATUS_TEMP	(1 << 15)
#define PMBUS_HAVE_STATUS_FAN12	(1 << 16)
#define PMBUS_HAVE_STATUS_FAN34	(1 << 17)

struct pmbus_driver_info {
	int pages;		/* Total number of pages */
	bool direct[PSC_NUM_CLASSES];
				/* true if device uses direct data format
				   for the given sensor class */
	/*
	 * Support one set of coefficients for each sensor type
	 * Used for chips providing data in direct mode.
	 */
	int m[PSC_NUM_CLASSES];	/* mantissa for direct data format */
	int b[PSC_NUM_CLASSES];	/* offset */
	int R[PSC_NUM_CLASSES];	/* exponent */

	u32 func[PMBUS_PAGES];	/* Functionality, per page */
	/*
	 * The get_status function maps manufacturing specific status values
	 * into PMBus standard status values.
	 * This function is optional and only necessary if chip specific status
	 * register values have to be mapped into standard PMBus status register
	 * values.
	 */
	int (*get_status)(struct i2c_client *client, int page, int reg);
	/*
	 * The identify function determines supported PMBus functionality.
	 * This function is only necessary if a chip driver supports multiple
	 * chips, and the chip functionality is not pre-determined.
	 */
	int (*identify)(struct i2c_client *client,
			struct pmbus_driver_info *info);
};

/* Function declarations */

int pmbus_set_page(struct i2c_client *client, u8 page);
int pmbus_read_word_data(struct i2c_client *client, u8 page, u8 reg);
void pmbus_clear_faults(struct i2c_client *client);
bool pmbus_check_byte_register(struct i2c_client *client, int page, int reg);
bool pmbus_check_word_register(struct i2c_client *client, int page, int reg);
int pmbus_do_probe(struct i2c_client *client, const struct i2c_device_id *id,
		   struct pmbus_driver_info *info);
int pmbus_do_remove(struct i2c_client *client);
const struct pmbus_driver_info *pmbus_get_driver_info(struct i2c_client
						      *client);

#endif /* PMBUS_H */
