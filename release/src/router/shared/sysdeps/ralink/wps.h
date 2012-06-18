/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#ifndef __WPS__H__
#define __WPS__H__

#define WSC_CONF_STATUS_STR		"WscConfStatus"
#define WSC_CONF_STATUS_UNCONFIGURED    1   /* these value are taken from 2860 driver Release Note document. */
#define WSC_CONF_STATUS_CONFIGURED      2

/*
 * ripped from driver wsc.h,....ugly
 */
#define PACKED  __attribute__ ((packed))
#define USHORT  unsigned short
#define UCHAR   unsigned char

typedef struct PACKED _WSC_CONFIGURED_VALUE {
    USHORT WscConfigured; // 1 un-configured; 2 configured
    UCHAR   WscSsid[32 + 1];
    USHORT WscAuthMode; // mandatory, 0x01: open, 0x02: wpa-psk, 0x04: shared, 0x08:wpa, 0x10: wpa2, 0x
    USHORT  WscEncrypType;  // 0x01: none, 0x02: wep, 0x04: tkip, 0x08: aes
    UCHAR   DefaultKeyIdx;
    UCHAR   WscWPAKey[64 + 1];
} WSC_CONFIGURED_VALUE;

#define WSC_ID_VERSION				0x104A
#define WSC_ID_VERSION_LEN			1
#define WSC_ID_VERSION_BEACON			0x00000001

#define WSC_ID_SC_STATE				0x1044
#define WSC_ID_SC_STATE_LEN			1
#define WSC_ID_SC_STATE_BEACON			0x00000002

#define WSC_ID_AP_SETUP_LOCKED			0x1057
#define WSC_ID_AP_SETUP_LOCKED_LEN		1
#define WSC_ID_AP_SETUP_LOCKED_BEACON		0x00000004

#define WSC_ID_SEL_REGISTRAR			0x1041
#define WSC_ID_SEL_REGISTRAR_LEN		1
#define WSC_ID_SEL_REGISTRAR_BEACON		0x00000008

#define WSC_ID_DEVICE_PWD_ID			0x1012
#define WSC_ID_DEVICE_PWD_ID_LEN		2
#define WSC_ID_DEVICE_PWD_ID_BEACON		0x00000010


#define WSC_ID_SEL_REG_CFG_METHODS		0x1053
#define WSC_ID_SEL_REG_CFG_METHODS_LEN		2
#define WSC_ID_SEL_REG_CFG_METHODS_BEACON	0x00000020

#define WSC_ID_UUID_E				0x1047
#define WSC_ID_UUID_E_LEN			16
#define WSC_ID_UUID_E_BEACON			0x00000040

#define WSC_ID_RF_BAND				0x103C
#define WSC_ID_RF_BAND_LEN			1
#define WSC_ID_RF_BAND_BEACON			0x00000080

#define WSC_ID_PRIMARY_DEVICE_TYPE		0x1054
#define WSC_ID_PRIMARY_DEVICE_TYPE_LEN		8
#define WSC_ID_PRIMARY_DEVICE_TYPE_BEACON	0x00000100

#endif /* __WPS__H_ */
