/*
 * IEEE 802.11 defines
 *
 * Copyright (c) 2001-2002, SSH Communications Security Corp and Jouni Malinen
 * <jkmaline@cc.hut.fi>
 * Copyright (c) 2002-2003, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2005, Devicescape Software, Inc.
 * Copyright (c) 2006, Michael Wu <flamingice@sourmilk.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef LINUX_IEEE80211_H
#define LINUX_IEEE80211_H

#include <linux/types.h>
#include <asm/byteorder.h>

/*
 * DS bit usage
 *
 * TA = transmitter address
 * RA = receiver address
 * DA = destination address
 * SA = source address
 *
 * ToDS    FromDS  A1(RA)  A2(TA)  A3      A4      Use
 * -----------------------------------------------------------------
 *  0       0       DA      SA      BSSID   -       IBSS/DLS
 *  0       1       DA      BSSID   SA      -       AP -> STA
 *  1       0       BSSID   SA      DA      -       AP <- STA
 *  1       1       RA      TA      DA      SA      unspecified (WDS)
 */

#define FCS_LEN 4

#define IEEE80211_FCTL_VERS		0x0003
#define IEEE80211_FCTL_FTYPE		0x000c
#define IEEE80211_FCTL_STYPE		0x00f0
#define IEEE80211_FCTL_TODS		0x0100
#define IEEE80211_FCTL_FROMDS		0x0200
#define IEEE80211_FCTL_MOREFRAGS	0x0400
#define IEEE80211_FCTL_RETRY		0x0800
#define IEEE80211_FCTL_PM		0x1000
#define IEEE80211_FCTL_MOREDATA		0x2000
#define IEEE80211_FCTL_PROTECTED	0x4000
#define IEEE80211_FCTL_ORDER		0x8000

#define IEEE80211_SCTL_FRAG		0x000F
#define IEEE80211_SCTL_SEQ		0xFFF0

#define IEEE80211_FTYPE_MGMT		0x0000
#define IEEE80211_FTYPE_CTL		0x0004
#define IEEE80211_FTYPE_DATA		0x0008

/* management */
#define IEEE80211_STYPE_ASSOC_REQ	0x0000
#define IEEE80211_STYPE_ASSOC_RESP	0x0010
#define IEEE80211_STYPE_REASSOC_REQ	0x0020
#define IEEE80211_STYPE_REASSOC_RESP	0x0030
#define IEEE80211_STYPE_PROBE_REQ	0x0040
#define IEEE80211_STYPE_PROBE_RESP	0x0050
#define IEEE80211_STYPE_BEACON		0x0080
#define IEEE80211_STYPE_ATIM		0x0090
#define IEEE80211_STYPE_DISASSOC	0x00A0
#define IEEE80211_STYPE_AUTH		0x00B0
#define IEEE80211_STYPE_DEAUTH		0x00C0
#define IEEE80211_STYPE_ACTION		0x00D0

/* control */
#define IEEE80211_STYPE_BACK_REQ	0x0080
#define IEEE80211_STYPE_BACK		0x0090
#define IEEE80211_STYPE_PSPOLL		0x00A0
#define IEEE80211_STYPE_RTS		0x00B0
#define IEEE80211_STYPE_CTS		0x00C0
#define IEEE80211_STYPE_ACK		0x00D0
#define IEEE80211_STYPE_CFEND		0x00E0
#define IEEE80211_STYPE_CFENDACK	0x00F0

/* data */
#define IEEE80211_STYPE_DATA			0x0000
#define IEEE80211_STYPE_DATA_CFACK		0x0010
#define IEEE80211_STYPE_DATA_CFPOLL		0x0020
#define IEEE80211_STYPE_DATA_CFACKPOLL		0x0030
#define IEEE80211_STYPE_NULLFUNC		0x0040
#define IEEE80211_STYPE_CFACK			0x0050
#define IEEE80211_STYPE_CFPOLL			0x0060
#define IEEE80211_STYPE_CFACKPOLL		0x0070
#define IEEE80211_STYPE_QOS_DATA		0x0080
#define IEEE80211_STYPE_QOS_DATA_CFACK		0x0090
#define IEEE80211_STYPE_QOS_DATA_CFPOLL		0x00A0
#define IEEE80211_STYPE_QOS_DATA_CFACKPOLL	0x00B0
#define IEEE80211_STYPE_QOS_NULLFUNC		0x00C0
#define IEEE80211_STYPE_QOS_CFACK		0x00D0
#define IEEE80211_STYPE_QOS_CFPOLL		0x00E0
#define IEEE80211_STYPE_QOS_CFACKPOLL		0x00F0


/* miscellaneous IEEE 802.11 constants */
#define IEEE80211_MAX_FRAG_THRESHOLD	2352
#define IEEE80211_MAX_RTS_THRESHOLD	2353
#define IEEE80211_MAX_AID		2007
#define IEEE80211_MAX_TIM_LEN		251
/* Maximum size for the MA-UNITDATA primitive, 802.11 standard section
   6.2.1.1.2.

   802.11e clarifies the figure in section 7.1.2. The frame body is
   up to 2304 octets long (maximum MSDU size) plus any crypt overhead. */
#define IEEE80211_MAX_DATA_LEN		2304
/* 30 byte 4 addr hdr, 2 byte QoS, 2304 byte MSDU, 12 byte crypt, 4 byte FCS */
#define IEEE80211_MAX_FRAME_LEN		2352

#define IEEE80211_MAX_SSID_LEN		32

#define IEEE80211_MAX_MESH_ID_LEN	32

#define IEEE80211_QOS_CTL_LEN		2
#define IEEE80211_QOS_CTL_TID_MASK	0x000F
#define IEEE80211_QOS_CTL_TAG1D_MASK	0x0007

/* U-APSD queue for WMM IEs sent by AP */
#define IEEE80211_WMM_IE_AP_QOSINFO_UAPSD	(1<<7)

/* U-APSD queues for WMM IEs sent by STA */
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_VO	(1<<0)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_VI	(1<<1)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_BK	(1<<2)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_BE	(1<<3)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_MASK	0x0f

/* U-APSD max SP length for WMM IEs sent by STA */
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL	0x00
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_2	0x01
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_4	0x02
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_6	0x03
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_MASK	0x03
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_SHIFT	5

#define IEEE80211_HT_CTL_LEN		4

struct ieee80211_hdr {
	__le16 frame_control;
	__le16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	__le16 seq_ctrl;
	u8 addr4[6];
} __attribute__ ((packed));

struct ieee80211_hdr_3addr {
	__le16 frame_control;
	__le16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	__le16 seq_ctrl;
} __attribute__ ((packed));

struct ieee80211_qos_hdr {
	__le16 frame_control;
	__le16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	__le16 seq_ctrl;
	__le16 qos_ctrl;
} __attribute__ ((packed));

/**
 * ieee80211_has_tods - check if IEEE80211_FCTL_TODS is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_tods(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_TODS)) != 0;
}

/**
 * ieee80211_has_fromds - check if IEEE80211_FCTL_FROMDS is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_fromds(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FROMDS)) != 0;
}

/**
 * ieee80211_has_a4 - check if IEEE80211_FCTL_TODS and IEEE80211_FCTL_FROMDS are set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_a4(__le16 fc)
{
	__le16 tmp = cpu_to_le16(IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS);
	return (fc & tmp) == tmp;
}

/**
 * ieee80211_has_morefrags - check if IEEE80211_FCTL_MOREFRAGS is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_morefrags(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_MOREFRAGS)) != 0;
}

/**
 * ieee80211_has_retry - check if IEEE80211_FCTL_RETRY is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_retry(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_RETRY)) != 0;
}

/**
 * ieee80211_has_pm - check if IEEE80211_FCTL_PM is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_pm(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_PM)) != 0;
}

/**
 * ieee80211_has_moredata - check if IEEE80211_FCTL_MOREDATA is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_moredata(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_MOREDATA)) != 0;
}

/**
 * ieee80211_has_protected - check if IEEE80211_FCTL_PROTECTED is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_protected(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_PROTECTED)) != 0;
}

/**
 * ieee80211_has_order - check if IEEE80211_FCTL_ORDER is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_has_order(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_ORDER)) != 0;
}

/**
 * ieee80211_is_mgmt - check if type is IEEE80211_FTYPE_MGMT
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_mgmt(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT);
}

/**
 * ieee80211_is_ctl - check if type is IEEE80211_FTYPE_CTL
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_ctl(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL);
}

/**
 * ieee80211_is_data - check if type is IEEE80211_FTYPE_DATA
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_data(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_DATA);
}

/**
 * ieee80211_is_data_qos - check if type is IEEE80211_FTYPE_DATA and IEEE80211_STYPE_QOS_DATA is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_data_qos(__le16 fc)
{
	/*
	 * mask with QOS_DATA rather than IEEE80211_FCTL_STYPE as we just need
	 * to check the one bit
	 */
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_STYPE_QOS_DATA)) ==
	       cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_DATA);
}

/**
 * ieee80211_is_data_present - check if type is IEEE80211_FTYPE_DATA and has data
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_data_present(__le16 fc)
{
	/*
	 * mask with 0x40 and test that that bit is clear to only return true
	 * for the data-containing substypes.
	 */
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | 0x40)) ==
	       cpu_to_le16(IEEE80211_FTYPE_DATA);
}

/**
 * ieee80211_is_assoc_req - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_ASSOC_REQ
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_assoc_req(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ASSOC_REQ);
}

/**
 * ieee80211_is_assoc_resp - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_ASSOC_RESP
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_assoc_resp(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ASSOC_RESP);
}

/**
 * ieee80211_is_reassoc_req - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_REASSOC_REQ
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_reassoc_req(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_REASSOC_REQ);
}

/**
 * ieee80211_is_reassoc_resp - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_REASSOC_RESP
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_reassoc_resp(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_REASSOC_RESP);
}

/**
 * ieee80211_is_probe_req - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_PROBE_REQ
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_probe_req(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_REQ);
}

/**
 * ieee80211_is_probe_resp - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_PROBE_RESP
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_probe_resp(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP);
}

/**
 * ieee80211_is_beacon - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_BEACON
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_beacon(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
}

/**
 * ieee80211_is_atim - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_ATIM
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_atim(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ATIM);
}

/**
 * ieee80211_is_disassoc - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_DISASSOC
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_disassoc(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_DISASSOC);
}

/**
 * ieee80211_is_auth - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_AUTH
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_auth(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_AUTH);
}

/**
 * ieee80211_is_deauth - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_DEAUTH
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_deauth(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_DEAUTH);
}

/**
 * ieee80211_is_action - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_ACTION
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_action(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION);
}

/**
 * ieee80211_is_back_req - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_BACK_REQ
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_back_req(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_BACK_REQ);
}

/**
 * ieee80211_is_back - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_BACK
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_back(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_BACK);
}

/**
 * ieee80211_is_pspoll - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_PSPOLL
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_pspoll(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_PSPOLL);
}

/**
 * ieee80211_is_rts - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_RTS
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_rts(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_RTS);
}

/**
 * ieee80211_is_cts - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_CTS
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_cts(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTS);
}

/**
 * ieee80211_is_ack - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_ACK
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_ack(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_ACK);
}

/**
 * ieee80211_is_cfend - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_CFEND
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_cfend(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CFEND);
}

/**
 * ieee80211_is_cfendack - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_CFENDACK
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_cfendack(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CFENDACK);
}

/**
 * ieee80211_is_nullfunc - check if frame is a regular (non-QoS) nullfunc frame
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_nullfunc(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_NULLFUNC);
}

/**
 * ieee80211_is_qos_nullfunc - check if frame is a QoS nullfunc frame
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_qos_nullfunc(__le16 fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	       cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_NULLFUNC);
}

struct ieee80211s_hdr {
	u8 flags;
	u8 ttl;
	__le32 seqnum;
	u8 eaddr1[6];
	u8 eaddr2[6];
	u8 eaddr3[6];
} __attribute__ ((packed));

/* Mesh flags */
#define MESH_FLAGS_AE_A4 	0x1
#define MESH_FLAGS_AE_A5_A6	0x2
#define MESH_FLAGS_AE		0x3
#define MESH_FLAGS_PS_DEEP	0x4

/**
 * struct ieee80211_quiet_ie
 *
 * This structure refers to "Quiet information element"
 */
struct ieee80211_quiet_ie {
	u8 count;
	u8 period;
	__le16 duration;
	__le16 offset;
} __attribute__ ((packed));

/**
 * struct ieee80211_msrment_ie
 *
 * This structure refers to "Measurement Request/Report information element"
 */
struct ieee80211_msrment_ie {
	u8 token;
	u8 mode;
	u8 type;
	u8 request[0];
} __attribute__ ((packed));

/**
 * struct ieee80211_channel_sw_ie
 *
 * This structure refers to "Channel Switch Announcement information element"
 */
struct ieee80211_channel_sw_ie {
	u8 mode;
	u8 new_ch_num;
	u8 count;
} __attribute__ ((packed));

/**
 * struct ieee80211_tim
 *
 * This structure refers to "Traffic Indication Map information element"
 */
struct ieee80211_tim_ie {
	u8 dtim_count;
	u8 dtim_period;
	u8 bitmap_ctrl;
	/* variable size: 1 - 251 bytes */
	u8 virtual_map[1];
} __attribute__ ((packed));

/**
 * struct ieee80211_meshconf_ie
 *
 * This structure refers to "Mesh Configuration information element"
 */
struct ieee80211_meshconf_ie {
	u8 meshconf_psel;
	u8 meshconf_pmetric;
	u8 meshconf_congest;
	u8 meshconf_synch;
	u8 meshconf_auth;
	u8 meshconf_form;
	u8 meshconf_cap;
} __attribute__ ((packed));

/**
 * struct ieee80211_rann_ie
 *
 * This structure refers to "Root Announcement information element"
 */
struct ieee80211_rann_ie {
	u8 rann_flags;
	u8 rann_hopcount;
	u8 rann_ttl;
	u8 rann_addr[6];
	u32 rann_seq;
	u32 rann_metric;
} __attribute__ ((packed));

#define WLAN_SA_QUERY_TR_ID_LEN 2

struct ieee80211_mgmt {
	__le16 frame_control;
	__le16 duration;
	u8 da[6];
	u8 sa[6];
	u8 bssid[6];
	__le16 seq_ctrl;
	union {
		struct {
			__le16 auth_alg;
			__le16 auth_transaction;
			__le16 status_code;
			/* possibly followed by Challenge text */
			u8 variable[0];
		} __attribute__ ((packed)) auth;
		struct {
			__le16 reason_code;
		} __attribute__ ((packed)) deauth;
		struct {
			__le16 capab_info;
			__le16 listen_interval;
			/* followed by SSID and Supported rates */
			u8 variable[0];
		} __attribute__ ((packed)) assoc_req;
		struct {
			__le16 capab_info;
			__le16 status_code;
			__le16 aid;
			/* followed by Supported rates */
			u8 variable[0];
		} __attribute__ ((packed)) assoc_resp, reassoc_resp;
		struct {
			__le16 capab_info;
			__le16 listen_interval;
			u8 current_ap[6];
			/* followed by SSID and Supported rates */
			u8 variable[0];
		} __attribute__ ((packed)) reassoc_req;
		struct {
			__le16 reason_code;
		} __attribute__ ((packed)) disassoc;
		struct {
			__le64 timestamp;
			__le16 beacon_int;
			__le16 capab_info;
			/* followed by some of SSID, Supported rates,
			 * FH Params, DS Params, CF Params, IBSS Params, TIM */
			u8 variable[0];
		} __attribute__ ((packed)) beacon;
		struct {
			/* only variable items: SSID, Supported rates */
			u8 variable[0];
		} __attribute__ ((packed)) probe_req;
		struct {
			__le64 timestamp;
			__le16 beacon_int;
			__le16 capab_info;
			/* followed by some of SSID, Supported rates,
			 * FH Params, DS Params, CF Params, IBSS Params */
			u8 variable[0];
		} __attribute__ ((packed)) probe_resp;
		struct {
			u8 category;
			union {
				struct {
					u8 action_code;
					u8 dialog_token;
					u8 status_code;
					u8 variable[0];
				} __attribute__ ((packed)) wme_action;
				struct{
					u8 action_code;
					u8 element_id;
					u8 length;
					struct ieee80211_channel_sw_ie sw_elem;
				} __attribute__((packed)) chan_switch;
				struct{
					u8 action_code;
					u8 dialog_token;
					u8 element_id;
					u8 length;
					struct ieee80211_msrment_ie msr_elem;
				} __attribute__((packed)) measurement;
				struct{
					u8 action_code;
					u8 dialog_token;
					__le16 capab;
					__le16 timeout;
					__le16 start_seq_num;
				} __attribute__((packed)) addba_req;
				struct{
					u8 action_code;
					u8 dialog_token;
					__le16 status;
					__le16 capab;
					__le16 timeout;
				} __attribute__((packed)) addba_resp;
				struct{
					u8 action_code;
					__le16 params;
					__le16 reason_code;
				} __attribute__((packed)) delba;
				struct{
					u8 action_code;
					/* capab_info for open and confirm,
					 * reason for close
					 */
					__le16 aux;
					/* Followed in plink_confirm by status
					 * code, AID and supported rates,
					 * and directly by supported rates in
					 * plink_open and plink_close
					 */
					u8 variable[0];
				} __attribute__((packed)) plink_action;
				struct{
					u8 action_code;
					u8 variable[0];
				} __attribute__((packed)) mesh_action;
				struct {
					u8 action;
					u8 trans_id[WLAN_SA_QUERY_TR_ID_LEN];
				} __attribute__ ((packed)) sa_query;
				struct {
					u8 action;
					u8 smps_control;
				} __attribute__ ((packed)) ht_smps;
			} u;
		} __attribute__ ((packed)) action;
	} u;
} __attribute__ ((packed));

/* mgmt header + 1 byte category code */
#define IEEE80211_MIN_ACTION_SIZE offsetof(struct ieee80211_mgmt, u.action.u)


/* Management MIC information element (IEEE 802.11w) */
struct ieee80211_mmie {
	u8 element_id;
	u8 length;
	__le16 key_id;
	u8 sequence_number[6];
	u8 mic[8];
} __attribute__ ((packed));

/* Control frames */
struct ieee80211_rts {
	__le16 frame_control;
	__le16 duration;
	u8 ra[6];
	u8 ta[6];
} __attribute__ ((packed));

struct ieee80211_cts {
	__le16 frame_control;
	__le16 duration;
	u8 ra[6];
} __attribute__ ((packed));

struct ieee80211_pspoll {
	__le16 frame_control;
	__le16 aid;
	u8 bssid[6];
	u8 ta[6];
} __attribute__ ((packed));

/**
 * struct ieee80211_bar - HT Block Ack Request
 *
 * This structure refers to "HT BlockAckReq" as
 * described in 802.11n draft section 7.2.1.7.1
 */
struct ieee80211_bar {
	__le16 frame_control;
	__le16 duration;
	__u8 ra[6];
	__u8 ta[6];
	__le16 control;
	__le16 start_seq_num;
} __attribute__((packed));

/* 802.11 BAR control masks */
#define IEEE80211_BAR_CTRL_ACK_POLICY_NORMAL     0x0000
#define IEEE80211_BAR_CTRL_CBMTID_COMPRESSED_BA  0x0004


#define IEEE80211_HT_MCS_MASK_LEN		10

/**
 * struct ieee80211_mcs_info - MCS information
 * @rx_mask: RX mask
 * @rx_highest: highest supported RX rate. If set represents
 *	the highest supported RX data rate in units of 1 Mbps.
 *	If this field is 0 this value should not be used to
 *	consider the highest RX data rate supported.
 * @tx_params: TX parameters
 */
struct ieee80211_mcs_info {
	u8 rx_mask[IEEE80211_HT_MCS_MASK_LEN];
	__le16 rx_highest;
	u8 tx_params;
	u8 reserved[3];
} __attribute__((packed));

/* 802.11n HT capability MSC set */
#define IEEE80211_HT_MCS_RX_HIGHEST_MASK	0x3ff
#define IEEE80211_HT_MCS_TX_DEFINED		0x01
#define IEEE80211_HT_MCS_TX_RX_DIFF		0x02
/* value 0 == 1 stream etc */
#define IEEE80211_HT_MCS_TX_MAX_STREAMS_MASK	0x0C
#define IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT	2
#define		IEEE80211_HT_MCS_TX_MAX_STREAMS	4
#define IEEE80211_HT_MCS_TX_UNEQUAL_MODULATION	0x10

/*
 * 802.11n D5.0 20.3.5 / 20.6 says:
 * - indices 0 to 7 and 32 are single spatial stream
 * - 8 to 31 are multiple spatial streams using equal modulation
 *   [8..15 for two streams, 16..23 for three and 24..31 for four]
 * - remainder are multiple spatial streams using unequal modulation
 */
#define IEEE80211_HT_MCS_UNEQUAL_MODULATION_START 33
#define IEEE80211_HT_MCS_UNEQUAL_MODULATION_START_BYTE \
	(IEEE80211_HT_MCS_UNEQUAL_MODULATION_START / 8)

/**
 * struct ieee80211_ht_cap - HT capabilities
 *
 * This structure is the "HT capabilities element" as
 * described in 802.11n D5.0 7.3.2.57
 */
struct ieee80211_ht_cap {
	__le16 cap_info;
	u8 ampdu_params_info;

	/* 16 bytes MCS information */
	struct ieee80211_mcs_info mcs;

	__le16 extended_ht_cap_info;
	__le32 tx_BF_cap_info;
	u8 antenna_selection_info;
} __attribute__ ((packed));

/* 802.11n HT capabilities masks (for cap_info) */
#define IEEE80211_HT_CAP_LDPC_CODING		0x0001
#define IEEE80211_HT_CAP_SUP_WIDTH_20_40	0x0002
#define IEEE80211_HT_CAP_SM_PS			0x000C
#define		IEEE80211_HT_CAP_SM_PS_SHIFT	2
#define IEEE80211_HT_CAP_GRN_FLD		0x0010
#define IEEE80211_HT_CAP_SGI_20			0x0020
#define IEEE80211_HT_CAP_SGI_40			0x0040
#define IEEE80211_HT_CAP_TX_STBC		0x0080
#define IEEE80211_HT_CAP_RX_STBC		0x0300
#define		IEEE80211_HT_CAP_RX_STBC_SHIFT	8
#define IEEE80211_HT_CAP_DELAY_BA		0x0400
#define IEEE80211_HT_CAP_MAX_AMSDU		0x0800
#define IEEE80211_HT_CAP_DSSSCCK40		0x1000
#define IEEE80211_HT_CAP_RESERVED		0x2000
#define IEEE80211_HT_CAP_40MHZ_INTOLERANT	0x4000
#define IEEE80211_HT_CAP_LSIG_TXOP_PROT		0x8000

/* 802.11n HT capability AMPDU settings (for ampdu_params_info) */
#define IEEE80211_HT_AMPDU_PARM_FACTOR		0x03
#define IEEE80211_HT_AMPDU_PARM_DENSITY		0x1C
#define		IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT	2

/*
 * Maximum length of AMPDU that the STA can receive.
 * Length = 2 ^ (13 + max_ampdu_length_exp) - 1 (octets)
 */
enum ieee80211_max_ampdu_length_exp {
	IEEE80211_HT_MAX_AMPDU_8K = 0,
	IEEE80211_HT_MAX_AMPDU_16K = 1,
	IEEE80211_HT_MAX_AMPDU_32K = 2,
	IEEE80211_HT_MAX_AMPDU_64K = 3
};

#define IEEE80211_HT_MAX_AMPDU_FACTOR 13

/* Minimum MPDU start spacing */
enum ieee80211_min_mpdu_spacing {
	IEEE80211_HT_MPDU_DENSITY_NONE = 0,	/* No restriction */
	IEEE80211_HT_MPDU_DENSITY_0_25 = 1,	/* 1/4 usec */
	IEEE80211_HT_MPDU_DENSITY_0_5 = 2,	/* 1/2 usec */
	IEEE80211_HT_MPDU_DENSITY_1 = 3,	/* 1 usec */
	IEEE80211_HT_MPDU_DENSITY_2 = 4,	/* 2 usec */
	IEEE80211_HT_MPDU_DENSITY_4 = 5,	/* 4 usec */
	IEEE80211_HT_MPDU_DENSITY_8 = 6,	/* 8 usec */
	IEEE80211_HT_MPDU_DENSITY_16 = 7	/* 16 usec */
};

/**
 * struct ieee80211_ht_info - HT information
 *
 * This structure is the "HT information element" as
 * described in 802.11n D5.0 7.3.2.58
 */
struct ieee80211_ht_info {
	u8 control_chan;
	u8 ht_param;
	__le16 operation_mode;
	__le16 stbc_param;
	u8 basic_set[16];
} __attribute__ ((packed));

/* for ht_param */
#define IEEE80211_HT_PARAM_CHA_SEC_OFFSET		0x03
#define		IEEE80211_HT_PARAM_CHA_SEC_NONE		0x00
#define		IEEE80211_HT_PARAM_CHA_SEC_ABOVE	0x01
#define		IEEE80211_HT_PARAM_CHA_SEC_BELOW	0x03
#define IEEE80211_HT_PARAM_CHAN_WIDTH_ANY		0x04
#define IEEE80211_HT_PARAM_RIFS_MODE			0x08
#define IEEE80211_HT_PARAM_SPSMP_SUPPORT		0x10
#define IEEE80211_HT_PARAM_SERV_INTERVAL_GRAN		0xE0

/* for operation_mode */
#define IEEE80211_HT_OP_MODE_PROTECTION			0x0003
#define		IEEE80211_HT_OP_MODE_PROTECTION_NONE		0
#define		IEEE80211_HT_OP_MODE_PROTECTION_NONMEMBER	1
#define		IEEE80211_HT_OP_MODE_PROTECTION_20MHZ		2
#define		IEEE80211_HT_OP_MODE_PROTECTION_NONHT_MIXED	3
#define IEEE80211_HT_OP_MODE_NON_GF_STA_PRSNT		0x0004
#define IEEE80211_HT_OP_MODE_NON_HT_STA_PRSNT		0x0010

/* for stbc_param */
#define IEEE80211_HT_STBC_PARAM_DUAL_BEACON		0x0040
#define IEEE80211_HT_STBC_PARAM_DUAL_CTS_PROT		0x0080
#define IEEE80211_HT_STBC_PARAM_STBC_BEACON		0x0100
#define IEEE80211_HT_STBC_PARAM_LSIG_TXOP_FULLPROT	0x0200
#define IEEE80211_HT_STBC_PARAM_PCO_ACTIVE		0x0400
#define IEEE80211_HT_STBC_PARAM_PCO_PHASE		0x0800


/* block-ack parameters */
#define IEEE80211_ADDBA_PARAM_POLICY_MASK 0x0002
#define IEEE80211_ADDBA_PARAM_TID_MASK 0x003C
#define IEEE80211_ADDBA_PARAM_BUF_SIZE_MASK 0xFFC0
#define IEEE80211_DELBA_PARAM_TID_MASK 0xF000
#define IEEE80211_DELBA_PARAM_INITIATOR_MASK 0x0800

/*
 * A-PMDU buffer sizes
 * According to IEEE802.11n spec size varies from 8K to 64K (in powers of 2)
 */
#define IEEE80211_MIN_AMPDU_BUF 0x8
#define IEEE80211_MAX_AMPDU_BUF 0x40


/* Spatial Multiplexing Power Save Modes (for capability) */
#define WLAN_HT_CAP_SM_PS_STATIC	0
#define WLAN_HT_CAP_SM_PS_DYNAMIC	1
#define WLAN_HT_CAP_SM_PS_INVALID	2
#define WLAN_HT_CAP_SM_PS_DISABLED	3

/* for SM power control field lower two bits */
#define WLAN_HT_SMPS_CONTROL_DISABLED	0
#define WLAN_HT_SMPS_CONTROL_STATIC	1
#define WLAN_HT_SMPS_CONTROL_DYNAMIC	3

/* Authentication algorithms */
#define WLAN_AUTH_OPEN 0
#define WLAN_AUTH_SHARED_KEY 1
#define WLAN_AUTH_FT 2
#define WLAN_AUTH_LEAP 128

#define WLAN_AUTH_CHALLENGE_LEN 128

#define WLAN_CAPABILITY_ESS		(1<<0)
#define WLAN_CAPABILITY_IBSS		(1<<1)
#define WLAN_CAPABILITY_CF_POLLABLE	(1<<2)
#define WLAN_CAPABILITY_CF_POLL_REQUEST	(1<<3)
#define WLAN_CAPABILITY_PRIVACY		(1<<4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE	(1<<5)
#define WLAN_CAPABILITY_PBCC		(1<<6)
#define WLAN_CAPABILITY_CHANNEL_AGILITY	(1<<7)

/* 802.11h */
#define WLAN_CAPABILITY_SPECTRUM_MGMT	(1<<8)
#define WLAN_CAPABILITY_QOS		(1<<9)
#define WLAN_CAPABILITY_SHORT_SLOT_TIME	(1<<10)
#define WLAN_CAPABILITY_DSSS_OFDM	(1<<13)
/* measurement */
#define IEEE80211_SPCT_MSR_RPRT_MODE_LATE	(1<<0)
#define IEEE80211_SPCT_MSR_RPRT_MODE_INCAPABLE	(1<<1)
#define IEEE80211_SPCT_MSR_RPRT_MODE_REFUSED	(1<<2)

#define IEEE80211_SPCT_MSR_RPRT_TYPE_BASIC	0
#define IEEE80211_SPCT_MSR_RPRT_TYPE_CCA	1
#define IEEE80211_SPCT_MSR_RPRT_TYPE_RPI	2


/* 802.11g ERP information element */
#define WLAN_ERP_NON_ERP_PRESENT (1<<0)
#define WLAN_ERP_USE_PROTECTION (1<<1)
#define WLAN_ERP_BARKER_PREAMBLE (1<<2)

/* WLAN_ERP_BARKER_PREAMBLE values */
enum {
	WLAN_ERP_PREAMBLE_SHORT = 0,
	WLAN_ERP_PREAMBLE_LONG = 1,
};

/* Status codes */
enum ieee80211_statuscode {
	WLAN_STATUS_SUCCESS = 0,
	WLAN_STATUS_UNSPECIFIED_FAILURE = 1,
	WLAN_STATUS_CAPS_UNSUPPORTED = 10,
	WLAN_STATUS_REASSOC_NO_ASSOC = 11,
	WLAN_STATUS_ASSOC_DENIED_UNSPEC = 12,
	WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG = 13,
	WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION = 14,
	WLAN_STATUS_CHALLENGE_FAIL = 15,
	WLAN_STATUS_AUTH_TIMEOUT = 16,
	WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA = 17,
	WLAN_STATUS_ASSOC_DENIED_RATES = 18,
	/* 802.11b */
	WLAN_STATUS_ASSOC_DENIED_NOSHORTPREAMBLE = 19,
	WLAN_STATUS_ASSOC_DENIED_NOPBCC = 20,
	WLAN_STATUS_ASSOC_DENIED_NOAGILITY = 21,
	/* 802.11h */
	WLAN_STATUS_ASSOC_DENIED_NOSPECTRUM = 22,
	WLAN_STATUS_ASSOC_REJECTED_BAD_POWER = 23,
	WLAN_STATUS_ASSOC_REJECTED_BAD_SUPP_CHAN = 24,
	/* 802.11g */
	WLAN_STATUS_ASSOC_DENIED_NOSHORTTIME = 25,
	WLAN_STATUS_ASSOC_DENIED_NODSSSOFDM = 26,
	/* 802.11w */
	WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY = 30,
	WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION = 31,
	/* 802.11i */
	WLAN_STATUS_INVALID_IE = 40,
	WLAN_STATUS_INVALID_GROUP_CIPHER = 41,
	WLAN_STATUS_INVALID_PAIRWISE_CIPHER = 42,
	WLAN_STATUS_INVALID_AKMP = 43,
	WLAN_STATUS_UNSUPP_RSN_VERSION = 44,
	WLAN_STATUS_INVALID_RSN_IE_CAP = 45,
	WLAN_STATUS_CIPHER_SUITE_REJECTED = 46,
	/* 802.11e */
	WLAN_STATUS_UNSPECIFIED_QOS = 32,
	WLAN_STATUS_ASSOC_DENIED_NOBANDWIDTH = 33,
	WLAN_STATUS_ASSOC_DENIED_LOWACK = 34,
	WLAN_STATUS_ASSOC_DENIED_UNSUPP_QOS = 35,
	WLAN_STATUS_REQUEST_DECLINED = 37,
	WLAN_STATUS_INVALID_QOS_PARAM = 38,
	WLAN_STATUS_CHANGE_TSPEC = 39,
	WLAN_STATUS_WAIT_TS_DELAY = 47,
	WLAN_STATUS_NO_DIRECT_LINK = 48,
	WLAN_STATUS_STA_NOT_PRESENT = 49,
	WLAN_STATUS_STA_NOT_QSTA = 50,
};


/* Reason codes */
enum ieee80211_reasoncode {
	WLAN_REASON_UNSPECIFIED = 1,
	WLAN_REASON_PREV_AUTH_NOT_VALID = 2,
	WLAN_REASON_DEAUTH_LEAVING = 3,
	WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY = 4,
	WLAN_REASON_DISASSOC_AP_BUSY = 5,
	WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA = 6,
	WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA = 7,
	WLAN_REASON_DISASSOC_STA_HAS_LEFT = 8,
	WLAN_REASON_STA_REQ_ASSOC_WITHOUT_AUTH = 9,
	/* 802.11h */
	WLAN_REASON_DISASSOC_BAD_POWER = 10,
	WLAN_REASON_DISASSOC_BAD_SUPP_CHAN = 11,
	/* 802.11i */
	WLAN_REASON_INVALID_IE = 13,
	WLAN_REASON_MIC_FAILURE = 14,
	WLAN_REASON_4WAY_HANDSHAKE_TIMEOUT = 15,
	WLAN_REASON_GROUP_KEY_HANDSHAKE_TIMEOUT = 16,
	WLAN_REASON_IE_DIFFERENT = 17,
	WLAN_REASON_INVALID_GROUP_CIPHER = 18,
	WLAN_REASON_INVALID_PAIRWISE_CIPHER = 19,
	WLAN_REASON_INVALID_AKMP = 20,
	WLAN_REASON_UNSUPP_RSN_VERSION = 21,
	WLAN_REASON_INVALID_RSN_IE_CAP = 22,
	WLAN_REASON_IEEE8021X_FAILED = 23,
	WLAN_REASON_CIPHER_SUITE_REJECTED = 24,
	/* 802.11e */
	WLAN_REASON_DISASSOC_UNSPECIFIED_QOS = 32,
	WLAN_REASON_DISASSOC_QAP_NO_BANDWIDTH = 33,
	WLAN_REASON_DISASSOC_LOW_ACK = 34,
	WLAN_REASON_DISASSOC_QAP_EXCEED_TXOP = 35,
	WLAN_REASON_QSTA_LEAVE_QBSS = 36,
	WLAN_REASON_QSTA_NOT_USE = 37,
	WLAN_REASON_QSTA_REQUIRE_SETUP = 38,
	WLAN_REASON_QSTA_TIMEOUT = 39,
	WLAN_REASON_QSTA_CIPHER_NOT_SUPP = 45,
};


/* Information Element IDs */
enum ieee80211_eid {
	WLAN_EID_SSID = 0,
	WLAN_EID_SUPP_RATES = 1,
	WLAN_EID_FH_PARAMS = 2,
	WLAN_EID_DS_PARAMS = 3,
	WLAN_EID_CF_PARAMS = 4,
	WLAN_EID_TIM = 5,
	WLAN_EID_IBSS_PARAMS = 6,
	WLAN_EID_CHALLENGE = 16,

	WLAN_EID_COUNTRY = 7,
	WLAN_EID_HP_PARAMS = 8,
	WLAN_EID_HP_TABLE = 9,
	WLAN_EID_REQUEST = 10,

	WLAN_EID_QBSS_LOAD = 11,
	WLAN_EID_EDCA_PARAM_SET = 12,
	WLAN_EID_TSPEC = 13,
	WLAN_EID_TCLAS = 14,
	WLAN_EID_SCHEDULE = 15,
	WLAN_EID_TS_DELAY = 43,
	WLAN_EID_TCLAS_PROCESSING = 44,
	WLAN_EID_QOS_CAPA = 46,
	/* 802.11s
	 *
	 * All mesh EID numbers are pending IEEE 802.11 ANA approval.
	 * The numbers have been incremented from those suggested in
	 * 802.11s/D2.0 so that MESH_CONFIG does not conflict with
	 * EXT_SUPP_RATES.
	 */
	WLAN_EID_MESH_CONFIG = 51,
	WLAN_EID_MESH_ID = 52,
	WLAN_EID_PEER_LINK = 55,
	WLAN_EID_PREQ = 68,
	WLAN_EID_PREP = 69,
	WLAN_EID_PERR = 70,
	WLAN_EID_RANN = 49,	/* compatible with FreeBSD */

	WLAN_EID_PWR_CONSTRAINT = 32,
	WLAN_EID_PWR_CAPABILITY = 33,
	WLAN_EID_TPC_REQUEST = 34,
	WLAN_EID_TPC_REPORT = 35,
	WLAN_EID_SUPPORTED_CHANNELS = 36,
	WLAN_EID_CHANNEL_SWITCH = 37,
	WLAN_EID_MEASURE_REQUEST = 38,
	WLAN_EID_MEASURE_REPORT = 39,
	WLAN_EID_QUIET = 40,
	WLAN_EID_IBSS_DFS = 41,

	WLAN_EID_ERP_INFO = 42,
	WLAN_EID_EXT_SUPP_RATES = 50,

	WLAN_EID_HT_CAPABILITY = 45,
	WLAN_EID_HT_INFORMATION = 61,

	WLAN_EID_RSN = 48,
	WLAN_EID_MMIE = 76,
	WLAN_EID_WPA = 221,
	WLAN_EID_GENERIC = 221,
	WLAN_EID_VENDOR_SPECIFIC = 221,
	WLAN_EID_QOS_PARAMETER = 222,

	WLAN_EID_AP_CHAN_REPORT = 51,
	WLAN_EID_NEIGHBOR_REPORT = 52,
	WLAN_EID_RCPI = 53,
	WLAN_EID_BSS_AVG_ACCESS_DELAY = 63,
	WLAN_EID_ANTENNA_INFO = 64,
	WLAN_EID_RSNI = 65,
	WLAN_EID_MEASUREMENT_PILOT_TX_INFO = 66,
	WLAN_EID_BSS_AVAILABLE_CAPACITY = 67,
	WLAN_EID_BSS_AC_ACCESS_DELAY = 68,
	WLAN_EID_RRM_ENABLED_CAPABILITIES = 70,
	WLAN_EID_MULTIPLE_BSSID = 71,

	WLAN_EID_MOBILITY_DOMAIN = 54,
	WLAN_EID_FAST_BSS_TRANSITION = 55,
	WLAN_EID_TIMEOUT_INTERVAL = 56,
	WLAN_EID_RIC_DATA = 57,
	WLAN_EID_RIC_DESCRIPTOR = 75,

	WLAN_EID_DSE_REGISTERED_LOCATION = 58,
	WLAN_EID_SUPPORTED_REGULATORY_CLASSES = 59,
	WLAN_EID_EXT_CHANSWITCH_ANN = 60,
};

/* Action category code */
enum ieee80211_category {
	WLAN_CATEGORY_SPECTRUM_MGMT = 0,
	WLAN_CATEGORY_QOS = 1,
	WLAN_CATEGORY_DLS = 2,
	WLAN_CATEGORY_BACK = 3,
	WLAN_CATEGORY_PUBLIC = 4,
	WLAN_CATEGORY_HT = 7,
	WLAN_CATEGORY_SA_QUERY = 8,
	WLAN_CATEGORY_PROTECTED_DUAL_OF_ACTION = 9,
	WLAN_CATEGORY_WMM = 17,
	WLAN_CATEGORY_MESH_PLINK = 30,		/* Pending ANA approval */
	WLAN_CATEGORY_MESH_PATH_SEL = 32,	/* Pending ANA approval */
	WLAN_CATEGORY_VENDOR_SPECIFIC_PROTECTED = 126,
	WLAN_CATEGORY_VENDOR_SPECIFIC = 127,
};

/* SPECTRUM_MGMT action code */
enum ieee80211_spectrum_mgmt_actioncode {
	WLAN_ACTION_SPCT_MSR_REQ = 0,
	WLAN_ACTION_SPCT_MSR_RPRT = 1,
	WLAN_ACTION_SPCT_TPC_REQ = 2,
	WLAN_ACTION_SPCT_TPC_RPRT = 3,
	WLAN_ACTION_SPCT_CHL_SWITCH = 4,
};

/* HT action codes */
enum ieee80211_ht_actioncode {
	WLAN_HT_ACTION_NOTIFY_CHANWIDTH = 0,
	WLAN_HT_ACTION_SMPS = 1,
	WLAN_HT_ACTION_PSMP = 2,
	WLAN_HT_ACTION_PCO_PHASE = 3,
	WLAN_HT_ACTION_CSI = 4,
	WLAN_HT_ACTION_NONCOMPRESSED_BF = 5,
	WLAN_HT_ACTION_COMPRESSED_BF = 6,
	WLAN_HT_ACTION_ASEL_IDX_FEEDBACK = 7,
};

/* Security key length */
enum ieee80211_key_len {
	WLAN_KEY_LEN_WEP40 = 5,
	WLAN_KEY_LEN_WEP104 = 13,
	WLAN_KEY_LEN_CCMP = 16,
	WLAN_KEY_LEN_TKIP = 32,
	WLAN_KEY_LEN_AES_CMAC = 16,
};

/*
 * IEEE 802.11-2007 7.3.2.9 Country information element
 *
 * Minimum length is 8 octets, ie len must be evenly
 * divisible by 2
 */

/* Although the spec says 8 I'm seeing 6 in practice */
#define IEEE80211_COUNTRY_IE_MIN_LEN	6

/*
 * For regulatory extension stuff see IEEE 802.11-2007
 * Annex I (page 1141) and Annex J (page 1147). Also
 * review 7.3.2.9.
 *
 * When dot11RegulatoryClassesRequired is true and the
 * first_channel/reg_extension_id is >= 201 then the IE
 * compromises of the 'ext' struct represented below:
 *
 *  - Regulatory extension ID - when generating IE this just needs
 *    to be monotonically increasing for each triplet passed in
 *    the IE
 *  - Regulatory class - index into set of rules
 *  - Coverage class - index into air propagation time (Table 7-27),
 *    in microseconds, you can compute the air propagation time from
 *    the index by multiplying by 3, so index 10 yields a propagation
 *    of 10 us. Valid values are 0-31, values 32-255 are not defined
 *    yet. A value of 0 inicates air propagation of <= 1 us.
 *
 *  See also Table I.2 for Emission limit sets and table
 *  I.3 for Behavior limit sets. Table J.1 indicates how to map
 *  a reg_class to an emission limit set and behavior limit set.
 */
#define IEEE80211_COUNTRY_EXTENSION_ID 201

/*
 *  Channels numbers in the IE must be monotonically increasing
 *  if dot11RegulatoryClassesRequired is not true.
 *
 *  If dot11RegulatoryClassesRequired is true consecutive
 *  subband triplets following a regulatory triplet shall
 *  have monotonically increasing first_channel number fields.
 *
 *  Channel numbers shall not overlap.
 *
 *  Note that max_power is signed.
 */
struct ieee80211_country_ie_triplet {
	union {
		struct {
			u8 first_channel;
			u8 num_channels;
			s8 max_power;
		} __attribute__ ((packed)) chans;
		struct {
			u8 reg_extension_id;
			u8 reg_class;
			u8 coverage_class;
		} __attribute__ ((packed)) ext;
	};
} __attribute__ ((packed));

enum ieee80211_timeout_interval_type {
	WLAN_TIMEOUT_REASSOC_DEADLINE = 1 /* 802.11r */,
	WLAN_TIMEOUT_KEY_LIFETIME = 2 /* 802.11r */,
	WLAN_TIMEOUT_ASSOC_COMEBACK = 3 /* 802.11w */,
};

/* BACK action code */
enum ieee80211_back_actioncode {
	WLAN_ACTION_ADDBA_REQ = 0,
	WLAN_ACTION_ADDBA_RESP = 1,
	WLAN_ACTION_DELBA = 2,
};

/* BACK (block-ack) parties */
enum ieee80211_back_parties {
	WLAN_BACK_RECIPIENT = 0,
	WLAN_BACK_INITIATOR = 1,
};

/* SA Query action */
enum ieee80211_sa_query_action {
	WLAN_ACTION_SA_QUERY_REQUEST = 0,
	WLAN_ACTION_SA_QUERY_RESPONSE = 1,
};


/* A-MSDU 802.11n */
#define IEEE80211_QOS_CONTROL_A_MSDU_PRESENT 0x0080

/* cipher suite selectors */
#define WLAN_CIPHER_SUITE_USE_GROUP	0x000FAC00
#define WLAN_CIPHER_SUITE_WEP40		0x000FAC01
#define WLAN_CIPHER_SUITE_TKIP		0x000FAC02
/* reserved: 				0x000FAC03 */
#define WLAN_CIPHER_SUITE_CCMP		0x000FAC04
#define WLAN_CIPHER_SUITE_WEP104	0x000FAC05
#define WLAN_CIPHER_SUITE_AES_CMAC	0x000FAC06

/* AKM suite selectors */
#define WLAN_AKM_SUITE_8021X		0x000FAC01
#define WLAN_AKM_SUITE_PSK		0x000FAC02

#define WLAN_MAX_KEY_LEN		32

#define WLAN_PMKID_LEN			16

/**
 * ieee80211_get_qos_ctl - get pointer to qos control bytes
 * @hdr: the frame
 *
 * The qos ctrl bytes come after the frame_control, duration, seq_num
 * and 3 or 4 addresses of length ETH_ALEN.
 * 3 addr: 2 + 2 + 2 + 3*6 = 24
 * 4 addr: 2 + 2 + 2 + 4*6 = 30
 */
static inline u8 *ieee80211_get_qos_ctl(struct ieee80211_hdr *hdr)
{
	if (ieee80211_has_a4(hdr->frame_control))
		return (u8 *)hdr + 30;
	else
		return (u8 *)hdr + 24;
}

/**
 * ieee80211_get_SA - get pointer to SA
 * @hdr: the frame
 *
 * Given an 802.11 frame, this function returns the offset
 * to the source address (SA). It does not verify that the
 * header is long enough to contain the address, and the
 * header must be long enough to contain the frame control
 * field.
 */
static inline u8 *ieee80211_get_SA(struct ieee80211_hdr *hdr)
{
	if (ieee80211_has_a4(hdr->frame_control))
		return hdr->addr4;
	if (ieee80211_has_fromds(hdr->frame_control))
		return hdr->addr3;
	return hdr->addr2;
}

/**
 * ieee80211_get_DA - get pointer to DA
 * @hdr: the frame
 *
 * Given an 802.11 frame, this function returns the offset
 * to the destination address (DA). It does not verify that
 * the header is long enough to contain the address, and the
 * header must be long enough to contain the frame control
 * field.
 */
static inline u8 *ieee80211_get_DA(struct ieee80211_hdr *hdr)
{
	if (ieee80211_has_tods(hdr->frame_control))
		return hdr->addr3;
	else
		return hdr->addr1;
}

/**
 * ieee80211_is_robust_mgmt_frame - check if frame is a robust management frame
 * @hdr: the frame (buffer must include at least the first octet of payload)
 */
static inline bool ieee80211_is_robust_mgmt_frame(struct ieee80211_hdr *hdr)
{
	if (ieee80211_is_disassoc(hdr->frame_control) ||
	    ieee80211_is_deauth(hdr->frame_control))
		return true;

	if (ieee80211_is_action(hdr->frame_control)) {
		u8 *category;

		/*
		 * Action frames, excluding Public Action frames, are Robust
		 * Management Frames. However, if we are looking at a Protected
		 * frame, skip the check since the data may be encrypted and
		 * the frame has already been found to be a Robust Management
		 * Frame (by the other end).
		 */
		if (ieee80211_has_protected(hdr->frame_control))
			return true;
		category = ((u8 *) hdr) + 24;
		return *category != WLAN_CATEGORY_PUBLIC &&
			*category != WLAN_CATEGORY_HT &&
			*category != WLAN_CATEGORY_VENDOR_SPECIFIC;
	}

	return false;
}

/**
 * ieee80211_fhss_chan_to_freq - get channel frequency
 * @channel: the FHSS channel
 *
 * Convert IEEE802.11 FHSS channel to frequency (MHz)
 * Ref IEEE 802.11-2007 section 14.6
 */
static inline int ieee80211_fhss_chan_to_freq(int channel)
{
	if ((channel > 1) && (channel < 96))
		return channel + 2400;
	else
		return -1;
}

/**
 * ieee80211_freq_to_fhss_chan - get channel
 * @freq: the channels frequency
 *
 * Convert frequency (MHz) to IEEE802.11 FHSS channel
 * Ref IEEE 802.11-2007 section 14.6
 */
static inline int ieee80211_freq_to_fhss_chan(int freq)
{
	if ((freq > 2401) && (freq < 2496))
		return freq - 2400;
	else
		return -1;
}

/**
 * ieee80211_dsss_chan_to_freq - get channel center frequency
 * @channel: the DSSS channel
 *
 * Convert IEEE802.11 DSSS channel to the center frequency (MHz).
 * Ref IEEE 802.11-2007 section 15.6
 */
static inline int ieee80211_dsss_chan_to_freq(int channel)
{
	if ((channel > 0) && (channel < 14))
		return 2407 + (channel * 5);
	else if (channel == 14)
		return 2484;
	else
		return -1;
}

/**
 * ieee80211_freq_to_dsss_chan - get channel
 * @freq: the frequency
 *
 * Convert frequency (MHz) to IEEE802.11 DSSS channel
 * Ref IEEE 802.11-2007 section 15.6
 *
 * This routine selects the channel with the closest center frequency.
 */
static inline int ieee80211_freq_to_dsss_chan(int freq)
{
	if ((freq >= 2410) && (freq < 2475))
		return (freq - 2405) / 5;
	else if ((freq >= 2482) && (freq < 2487))
		return 14;
	else
		return -1;
}

/* Convert IEEE802.11 HR DSSS channel to frequency (MHz) and back
 * Ref IEEE 802.11-2007 section 18.4.6.2
 *
 * The channels and frequencies are the same as those defined for DSSS
 */
#define ieee80211_hr_chan_to_freq(chan) ieee80211_dsss_chan_to_freq(chan)
#define ieee80211_freq_to_hr_chan(freq) ieee80211_freq_to_dsss_chan(freq)

/* Convert IEEE802.11 ERP channel to frequency (MHz) and back
 * Ref IEEE 802.11-2007 section 19.4.2
 */
#define ieee80211_erp_chan_to_freq(chan) ieee80211_hr_chan_to_freq(chan)
#define ieee80211_freq_to_erp_chan(freq) ieee80211_freq_to_hr_chan(freq)

/**
 * ieee80211_ofdm_chan_to_freq - get channel center frequency
 * @s_freq: starting frequency == (dotChannelStartingFactor/2) MHz
 * @channel: the OFDM channel
 *
 * Convert IEEE802.11 OFDM channel to center frequency (MHz)
 * Ref IEEE 802.11-2007 section 17.3.8.3.2
 */
static inline int ieee80211_ofdm_chan_to_freq(int s_freq, int channel)
{
	if ((channel > 0) && (channel <= 200) &&
	    (s_freq >= 4000))
		return s_freq + (channel * 5);
	else
		return -1;
}

/**
 * ieee80211_freq_to_ofdm_channel - get channel
 * @s_freq: starting frequency == (dotChannelStartingFactor/2) MHz
 * @freq: the frequency
 *
 * Convert frequency (MHz) to IEEE802.11 OFDM channel
 * Ref IEEE 802.11-2007 section 17.3.8.3.2
 *
 * This routine selects the channel with the closest center frequency.
 */
static inline int ieee80211_freq_to_ofdm_chan(int s_freq, int freq)
{
	if ((freq > (s_freq + 2)) && (freq <= (s_freq + 1202)) &&
	    (s_freq >= 4000))
		return (freq + 2 - s_freq) / 5;
	else
		return -1;
}

/**
 * ieee80211_tu_to_usec - convert time units (TU) to microseconds
 * @tu: the TUs
 */
static inline unsigned long ieee80211_tu_to_usec(unsigned long tu)
{
	return 1024 * tu;
}

/**
 * ieee80211_check_tim - check if AID bit is set in TIM
 * @tim: the TIM IE
 * @tim_len: length of the TIM IE
 * @aid: the AID to look for
 */
static inline bool ieee80211_check_tim(struct ieee80211_tim_ie *tim,
				       u8 tim_len, u16 aid)
{
	u8 mask;
	u8 index, indexn1, indexn2;

	if (unlikely(!tim || tim_len < sizeof(*tim)))
		return false;

	aid &= 0x3fff;
	index = aid / 8;
	mask  = 1 << (aid & 7);

	indexn1 = tim->bitmap_ctrl & 0xfe;
	indexn2 = tim_len + indexn1 - 4;

	if (index < indexn1 || index > indexn2)
		return false;

	index -= indexn1;

	return !!(tim->virtual_map[index] & mask);
}

#endif /* LINUX_IEEE80211_H */
