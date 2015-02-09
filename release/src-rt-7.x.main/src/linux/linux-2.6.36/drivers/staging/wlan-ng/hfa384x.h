/* hfa384x.h
*
* Defines the constants and data structures for the hfa384x
*
* Copyright (C) 1999 AbsoluteValue Systems, Inc.  All Rights Reserved.
* --------------------------------------------------------------------
*
* linux-wlan
*
*   The contents of this file are subject to the Mozilla Public
*   License Version 1.1 (the "License"); you may not use this file
*   except in compliance with the License. You may obtain a copy of
*   the License at http://www.mozilla.org/MPL/
*
*   Software distributed under the License is distributed on an "AS
*   IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
*   implied. See the License for the specific language governing
*   rights and limitations under the License.
*
*   Alternatively, the contents of this file may be used under the
*   terms of the GNU Public License version 2 (the "GPL"), in which
*   case the provisions of the GPL are applicable instead of the
*   above.  If you wish to allow the use of your version of this file
*   only under the terms of the GPL and not to allow others to use
*   your version of this file under the MPL, indicate your decision
*   by deleting the provisions above and replace them with the notice
*   and other provisions required by the GPL.  If you do not delete
*   the provisions above, a recipient may use your version of this
*   file under either the MPL or the GPL.
*
* --------------------------------------------------------------------
*
* Inquiries regarding the linux-wlan Open Source project can be
* made directly to:
*
* AbsoluteValue Systems Inc.
* info@linux-wlan.com
* http://www.linux-wlan.com
*
* --------------------------------------------------------------------
*
* Portions of the development of this software were funded by
* Intersil Corporation as part of PRISM(R) chipset product development.
*
* --------------------------------------------------------------------
*
*   [Implementation and usage notes]
*
*   [References]
*	CW10 Programmer's Manual v1.5
*	IEEE 802.11 D10.0
*
* --------------------------------------------------------------------
*/

#ifndef _HFA384x_H
#define _HFA384x_H

#define HFA384x_FIRMWARE_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))

#include <linux/if_ether.h>

/*--- Mins & Maxs -----------------------------------*/
#define	HFA384x_PORTID_MAX		((u16)7)
#define	HFA384x_NUMPORTS_MAX		((u16)(HFA384x_PORTID_MAX+1))
#define	HFA384x_PDR_LEN_MAX		((u16)512) /* in bytes, from EK */
#define	HFA384x_PDA_RECS_MAX		((u16)200) /* a guess */
#define	HFA384x_PDA_LEN_MAX		((u16)1024) /* in bytes, from EK*/
#define	HFA384x_SCANRESULT_MAX		((u16)31)
#define	HFA384x_HSCANRESULT_MAX		((u16)31)
#define	HFA384x_CHINFORESULT_MAX	((u16)16)
#define	HFA384x_RID_GUESSING_MAXLEN	2048	/* I'm not really sure */
#define	HFA384x_RIDDATA_MAXLEN		HFA384x_RID_GUESSING_MAXLEN
#define	HFA384x_USB_RWMEM_MAXLEN	2048

/*--- Support Constants -----------------------------*/
#define		HFA384x_PORTTYPE_IBSS			((u16)0)
#define		HFA384x_PORTTYPE_BSS			((u16)1)
#define		HFA384x_PORTTYPE_PSUEDOIBSS		((u16)3)
#define		HFA384x_WEPFLAGS_PRIVINVOKED		((u16)BIT(0))
#define		HFA384x_WEPFLAGS_EXCLUDE		((u16)BIT(1))
#define		HFA384x_WEPFLAGS_DISABLE_TXCRYPT	((u16)BIT(4))
#define		HFA384x_WEPFLAGS_DISABLE_RXCRYPT	((u16)BIT(7))
#define 	HFA384x_ROAMMODE_HOSTSCAN_HOSTROAM	((u16)3)
#define 	HFA384x_PORTSTATUS_DISABLED		((u16)1)
#define		HFA384x_RATEBIT_1			((u16)1)
#define		HFA384x_RATEBIT_2			((u16)2)
#define		HFA384x_RATEBIT_5dot5			((u16)4)
#define		HFA384x_RATEBIT_11			((u16)8)

/*--- MAC Internal memory constants and macros ------*/
/* masks and macros used to manipulate MAC internal memory addresses. */
/* MAC internal memory addresses are 23 bit quantities.  The MAC uses
 * a paged address space where the upper 16 bits are the page number
 * and the lower 7 bits are the offset.  There are various Host API
 * elements that require two 16-bit quantities to specify a MAC
 * internal memory address.  Unfortunately, some of the API's use a
 * page/offset format where the offset value is JUST the lower seven
 * bits and the page is  the remaining 16 bits.  Some of the API's
 * assume that the 23 bit address has been split at the 16th bit.  We
 * refer to these two formats as AUX format and CMD format.  The
 * macros below help handle some of this.
 */

/* Mask bits for discarding unwanted pieces in a flat address */
#define		HFA384x_ADDR_FLAT_AUX_PAGE_MASK	(0x007fff80)
#define		HFA384x_ADDR_FLAT_AUX_OFF_MASK	(0x0000007f)
#define		HFA384x_ADDR_FLAT_CMD_PAGE_MASK	(0xffff0000)
#define		HFA384x_ADDR_FLAT_CMD_OFF_MASK	(0x0000ffff)

/* Mask bits for discarding unwanted pieces in AUX format
   16-bit address parts */
#define		HFA384x_ADDR_AUX_PAGE_MASK	(0xffff)
#define		HFA384x_ADDR_AUX_OFF_MASK	(0x007f)

/* Make a 32-bit flat address from AUX format 16-bit page and offset */
#define		HFA384x_ADDR_AUX_MKFLAT(p, o)	\
		((((u32)(((u16)(p))&HFA384x_ADDR_AUX_PAGE_MASK)) << 7) | \
		((u32)(((u16)(o))&HFA384x_ADDR_AUX_OFF_MASK)))

/* Make CMD format offset and page from a 32-bit flat address */
#define		HFA384x_ADDR_CMD_MKPAGE(f) \
		((u16)((((u32)(f))&HFA384x_ADDR_FLAT_CMD_PAGE_MASK)>>16))
#define		HFA384x_ADDR_CMD_MKOFF(f) \
		((u16)(((u32)(f))&HFA384x_ADDR_FLAT_CMD_OFF_MASK))

/*--- Controller Memory addresses -------------------*/
#define		HFA3842_PDA_BASE	(0x007f0000UL)
#define		HFA3841_PDA_BASE	(0x003f0000UL)
#define		HFA3841_PDA_BOGUS_BASE	(0x00390000UL)

/*--- Driver Download states  -----------------------*/
#define		HFA384x_DLSTATE_DISABLED		0
#define		HFA384x_DLSTATE_RAMENABLED		1
#define		HFA384x_DLSTATE_FLASHENABLED		2

/*--- Register Field Masks --------------------------*/
#define		HFA384x_CMD_AINFO		((u16)(BIT(14) | BIT(13) \
							| BIT(12) | BIT(11) \
							| BIT(10) | BIT(9) \
							| BIT(8)))
#define		HFA384x_CMD_MACPORT		((u16)(BIT(10) | BIT(9) | \
							BIT(8)))
#define		HFA384x_CMD_PROGMODE		((u16)(BIT(9) | BIT(8)))
#define		HFA384x_CMD_CMDCODE		((u16)(BIT(5) | BIT(4) | \
							BIT(3) | BIT(2) | \
							BIT(1) | BIT(0)))

#define		HFA384x_STATUS_RESULT		((u16)(BIT(14) | BIT(13) \
							| BIT(12) | BIT(11) \
							| BIT(10) | BIT(9) \
							| BIT(8)))

/*--- Command Code Constants --------------------------*/
/*--- Controller Commands --------------------------*/
#define		HFA384x_CMDCODE_INIT		((u16)0x00)
#define		HFA384x_CMDCODE_ENABLE		((u16)0x01)
#define		HFA384x_CMDCODE_DISABLE		((u16)0x02)

/*--- Regulate Commands --------------------------*/
#define		HFA384x_CMDCODE_INQ		((u16)0x11)

/*--- Configure Commands --------------------------*/
#define		HFA384x_CMDCODE_DOWNLD		((u16)0x22)

/*--- Debugging Commands -----------------------------*/
#define 	HFA384x_CMDCODE_MONITOR		((u16)(0x38))
#define		HFA384x_MONITOR_ENABLE		((u16)(0x0b))
#define		HFA384x_MONITOR_DISABLE		((u16)(0x0f))

/*--- Result Codes --------------------------*/
#define		HFA384x_CMD_ERR			((u16)(0x7F))

/*--- Programming Modes --------------------------
	MODE 0: Disable programming
	MODE 1: Enable volatile memory programming
	MODE 2: Enable non-volatile memory programming
	MODE 3: Program non-volatile memory section
--------------------------------------------------*/
#define		HFA384x_PROGMODE_DISABLE	((u16)0x00)
#define		HFA384x_PROGMODE_RAM		((u16)0x01)
#define		HFA384x_PROGMODE_NV		((u16)0x02)
#define		HFA384x_PROGMODE_NVWRITE	((u16)0x03)

/*--- Record ID Constants --------------------------*/
/*--------------------------------------------------------------------
Configuration RIDs: Network Parameters, Static Configuration Entities
--------------------------------------------------------------------*/
#define		HFA384x_RID_CNFPORTTYPE		((u16)0xFC00)
#define		HFA384x_RID_CNFOWNMACADDR	((u16)0xFC01)
#define		HFA384x_RID_CNFDESIREDSSID	((u16)0xFC02)
#define		HFA384x_RID_CNFOWNCHANNEL	((u16)0xFC03)
#define		HFA384x_RID_CNFOWNSSID		((u16)0xFC04)
#define		HFA384x_RID_CNFMAXDATALEN	((u16)0xFC07)

/*--------------------------------------------------------------------
Configuration RID lengths: Network Params, Static Config Entities
  This is the length of JUST the DATA part of the RID (does not
  include the len or code fields)
--------------------------------------------------------------------*/
#define		HFA384x_RID_CNFOWNMACADDR_LEN	((u16)6)
#define		HFA384x_RID_CNFDESIREDSSID_LEN	((u16)34)
#define		HFA384x_RID_CNFOWNSSID_LEN	((u16)34)

/*--------------------------------------------------------------------
Configuration RIDs: Network Parameters, Dynamic Configuration Entities
--------------------------------------------------------------------*/
#define		HFA384x_RID_CREATEIBSS		((u16)0xFC81)
#define		HFA384x_RID_FRAGTHRESH		((u16)0xFC82)
#define		HFA384x_RID_RTSTHRESH		((u16)0xFC83)
#define		HFA384x_RID_TXRATECNTL		((u16)0xFC84)
#define		HFA384x_RID_PROMISCMODE		((u16)0xFC85)

/*----------------------------------------------------------------------
Information RIDs: NIC Information
--------------------------------------------------------------------*/
#define		HFA384x_RID_MAXLOADTIME		((u16)0xFD00)
#define		HFA384x_RID_DOWNLOADBUFFER	((u16)0xFD01)
#define		HFA384x_RID_PRIIDENTITY		((u16)0xFD02)
#define		HFA384x_RID_PRISUPRANGE		((u16)0xFD03)
#define		HFA384x_RID_PRI_CFIACTRANGES	((u16)0xFD04)
#define		HFA384x_RID_NICSERIALNUMBER	((u16)0xFD0A)
#define		HFA384x_RID_NICIDENTITY		((u16)0xFD0B)
#define		HFA384x_RID_MFISUPRANGE		((u16)0xFD0C)
#define		HFA384x_RID_CFISUPRANGE		((u16)0xFD0D)
#define		HFA384x_RID_STAIDENTITY		((u16)0xFD20)
#define		HFA384x_RID_STASUPRANGE		((u16)0xFD21)
#define		HFA384x_RID_STA_MFIACTRANGES	((u16)0xFD22)
#define		HFA384x_RID_STA_CFIACTRANGES	((u16)0xFD23)

/*----------------------------------------------------------------------
Information RID Lengths: NIC Information
  This is the length of JUST the DATA part of the RID (does not
  include the len or code fields)
--------------------------------------------------------------------*/
#define		HFA384x_RID_NICSERIALNUMBER_LEN		((u16)12)

/*--------------------------------------------------------------------
Information RIDs:  MAC Information
--------------------------------------------------------------------*/
#define		HFA384x_RID_PORTSTATUS		((u16)0xFD40)
#define		HFA384x_RID_CURRENTSSID		((u16)0xFD41)
#define		HFA384x_RID_CURRENTBSSID	((u16)0xFD42)
#define		HFA384x_RID_CURRENTTXRATE	((u16)0xFD44)
#define		HFA384x_RID_SHORTRETRYLIMIT	((u16)0xFD48)
#define		HFA384x_RID_LONGRETRYLIMIT	((u16)0xFD49)
#define		HFA384x_RID_MAXTXLIFETIME	((u16)0xFD4A)
#define		HFA384x_RID_PRIVACYOPTIMP	((u16)0xFD4F)
#define		HFA384x_RID_DBMCOMMSQUALITY	((u16)0xFD51)

/*--------------------------------------------------------------------
Information RID Lengths:  MAC Information
  This is the length of JUST the DATA part of the RID (does not
  include the len or code fields)
--------------------------------------------------------------------*/
#define		HFA384x_RID_DBMCOMMSQUALITY_LEN	 \
	((u16) sizeof(hfa384x_dbmcommsquality_t))
#define		HFA384x_RID_JOINREQUEST_LEN \
	((u16)sizeof(hfa384x_JoinRequest_data_t))

/*--------------------------------------------------------------------
Information RIDs:  Modem Information
--------------------------------------------------------------------*/
#define		HFA384x_RID_CURRENTCHANNEL	((u16)0xFDC1)

/*--------------------------------------------------------------------
API ENHANCEMENTS (NOT ALREADY IMPLEMENTED)
--------------------------------------------------------------------*/
#define		HFA384x_RID_CNFWEPDEFAULTKEYID	((u16)0xFC23)
#define		HFA384x_RID_CNFWEPDEFAULTKEY0	((u16)0xFC24)
#define		HFA384x_RID_CNFWEPDEFAULTKEY1	((u16)0xFC25)
#define		HFA384x_RID_CNFWEPDEFAULTKEY2	((u16)0xFC26)
#define		HFA384x_RID_CNFWEPDEFAULTKEY3	((u16)0xFC27)
#define		HFA384x_RID_CNFWEPFLAGS		((u16)0xFC28)
#define		HFA384x_RID_CNFAUTHENTICATION	((u16)0xFC2A)
#define		HFA384x_RID_CNFROAMINGMODE	((u16)0xFC2D)
#define		HFA384x_RID_CNFAPBCNint		((u16)0xFC33)
#define		HFA384x_RID_CNFDBMADJUST  	((u16)0xFC46)
#define		HFA384x_RID_CNFWPADATA       	((u16)0xFC48)
#define		HFA384x_RID_CNFBASICRATES	((u16)0xFCB3)
#define		HFA384x_RID_CNFSUPPRATES	((u16)0xFCB4)
#define		HFA384x_RID_CNFPASSIVESCANCTRL	((u16)0xFCBA)
#define		HFA384x_RID_TXPOWERMAX        	((u16)0xFCBE)
#define		HFA384x_RID_JOINREQUEST		((u16)0xFCE2)
#define		HFA384x_RID_AUTHENTICATESTA	((u16)0xFCE3)
#define		HFA384x_RID_HOSTSCAN          	((u16)0xFCE5)

#define		HFA384x_RID_CNFWEPDEFAULTKEY_LEN	((u16)6)
#define		HFA384x_RID_CNFWEP128DEFAULTKEY_LEN	((u16)14)

/*--------------------------------------------------------------------
PD Record codes
--------------------------------------------------------------------*/
#define HFA384x_PDR_PCB_PARTNUM		((u16)0x0001)
#define HFA384x_PDR_PDAVER		((u16)0x0002)
#define HFA384x_PDR_NIC_SERIAL		((u16)0x0003)
#define HFA384x_PDR_MKK_MEASUREMENTS	((u16)0x0004)
#define HFA384x_PDR_NIC_RAMSIZE		((u16)0x0005)
#define HFA384x_PDR_MFISUPRANGE		((u16)0x0006)
#define HFA384x_PDR_CFISUPRANGE		((u16)0x0007)
#define HFA384x_PDR_NICID		((u16)0x0008)
#define HFA384x_PDR_MAC_ADDRESS		((u16)0x0101)
#define HFA384x_PDR_REGDOMAIN		((u16)0x0103)
#define HFA384x_PDR_ALLOWED_CHANNEL	((u16)0x0104)
#define HFA384x_PDR_DEFAULT_CHANNEL	((u16)0x0105)
#define HFA384x_PDR_TEMPTYPE		((u16)0x0107)
#define HFA384x_PDR_IFR_SETTING		((u16)0x0200)
#define HFA384x_PDR_RFR_SETTING		((u16)0x0201)
#define HFA384x_PDR_HFA3861_BASELINE	((u16)0x0202)
#define HFA384x_PDR_HFA3861_SHADOW	((u16)0x0203)
#define HFA384x_PDR_HFA3861_IFRF	((u16)0x0204)
#define HFA384x_PDR_HFA3861_CHCALSP	((u16)0x0300)
#define HFA384x_PDR_HFA3861_CHCALI	((u16)0x0301)
#define HFA384x_PDR_MAX_TX_POWER  	((u16)0x0302)
#define HFA384x_PDR_MASTER_CHAN_LIST	((u16)0x0303)
#define HFA384x_PDR_3842_NIC_CONFIG	((u16)0x0400)
#define HFA384x_PDR_USB_ID		((u16)0x0401)
#define HFA384x_PDR_PCI_ID		((u16)0x0402)
#define HFA384x_PDR_PCI_IFCONF		((u16)0x0403)
#define HFA384x_PDR_PCI_PMCONF		((u16)0x0404)
#define HFA384x_PDR_RFENRGY		((u16)0x0406)
#define HFA384x_PDR_USB_POWER_TYPE      ((u16)0x0407)
#define HFA384x_PDR_USB_MAX_POWER	((u16)0x0409)
#define HFA384x_PDR_USB_MANUFACTURER	((u16)0x0410)
#define HFA384x_PDR_USB_PRODUCT  	((u16)0x0411)
#define HFA384x_PDR_ANT_DIVERSITY   	((u16)0x0412)
#define HFA384x_PDR_HFO_DELAY       	((u16)0x0413)
#define HFA384x_PDR_SCALE_THRESH 	((u16)0x0414)

#define HFA384x_PDR_HFA3861_MANF_TESTSP	((u16)0x0900)
#define HFA384x_PDR_HFA3861_MANF_TESTI	((u16)0x0901)
#define HFA384x_PDR_END_OF_PDA		((u16)0x0000)

/*--- Register Test/Get/Set Field macros ------------------------*/

#define		HFA384x_CMD_AINFO_SET(value)	((u16)((u16)(value) << 8))
#define		HFA384x_CMD_MACPORT_SET(value)	\
			((u16)HFA384x_CMD_AINFO_SET(value))
#define		HFA384x_CMD_PROGMODE_SET(value)	\
			((u16)HFA384x_CMD_AINFO_SET((u16)value))
#define		HFA384x_CMD_CMDCODE_SET(value)		((u16)(value))

#define		HFA384x_STATUS_RESULT_SET(value)	(((u16)(value)) << 8)

/* Host Maintained State Info */
#define HFA384x_STATE_PREINIT	0
#define HFA384x_STATE_INIT	1
#define HFA384x_STATE_RUNNING	2

/*-------------------------------------------------------------*/
/* Commonly used basic types */
typedef struct hfa384x_bytestr {
	u16 len;
	u8 data[0];
} __attribute__ ((packed)) hfa384x_bytestr_t;

typedef struct hfa384x_bytestr32 {
	u16 len;
	u8 data[32];
} __attribute__ ((packed)) hfa384x_bytestr32_t;

/*--------------------------------------------------------------------
Configuration Record Structures:
	Network Parameters, Static Configuration Entities
--------------------------------------------------------------------*/

/*-- Hardware/Firmware Component Information ----------*/
typedef struct hfa384x_compident {
	u16 id;
	u16 variant;
	u16 major;
	u16 minor;
} __attribute__ ((packed)) hfa384x_compident_t;

typedef struct hfa384x_caplevel {
	u16 role;
	u16 id;
	u16 variant;
	u16 bottom;
	u16 top;
} __attribute__ ((packed)) hfa384x_caplevel_t;

/*-- Configuration Record: cnfAuthentication --*/
#define HFA384x_CNFAUTHENTICATION_OPENSYSTEM	0x0001
#define HFA384x_CNFAUTHENTICATION_SHAREDKEY	0x0002
#define HFA384x_CNFAUTHENTICATION_LEAP     	0x0004

/*--------------------------------------------------------------------
Configuration Record Structures:
	Network Parameters, Dynamic Configuration Entities
--------------------------------------------------------------------*/

#define HFA384x_CREATEIBSS_JOINCREATEIBSS          0

/*-- Configuration Record: HostScanRequest (data portion only) --*/
typedef struct hfa384x_HostScanRequest_data {
	u16 channelList;
	u16 txRate;
	hfa384x_bytestr32_t ssid;
} __attribute__ ((packed)) hfa384x_HostScanRequest_data_t;

/*-- Configuration Record: JoinRequest (data portion only) --*/
typedef struct hfa384x_JoinRequest_data {
	u8 bssid[WLAN_BSSID_LEN];
	u16 channel;
} __attribute__ ((packed)) hfa384x_JoinRequest_data_t;

/*-- Configuration Record: authenticateStation (data portion only) --*/
typedef struct hfa384x_authenticateStation_data {
	u8 address[ETH_ALEN];
	u16 status;
	u16 algorithm;
} __attribute__ ((packed)) hfa384x_authenticateStation_data_t;

/*-- Configuration Record: WPAData       (data portion only) --*/
typedef struct hfa384x_WPAData {
	u16 datalen;
	u8 data[0];		/* max 80 */
} __attribute__ ((packed)) hfa384x_WPAData_t;

/*--------------------------------------------------------------------
Information Record Structures: NIC Information
--------------------------------------------------------------------*/

/*-- Information Record: DownLoadBuffer --*/
/* NOTE: The page and offset are in AUX format */
typedef struct hfa384x_downloadbuffer {
	u16 page;
	u16 offset;
	u16 len;
} __attribute__ ((packed)) hfa384x_downloadbuffer_t;

/*--------------------------------------------------------------------
Information Record Structures: NIC Information
--------------------------------------------------------------------*/

#define HFA384x_PSTATUS_CONN_IBSS	((u16)3)

/*-- Information Record: commsquality --*/
typedef struct hfa384x_commsquality {
	u16 CQ_currBSS;
	u16 ASL_currBSS;
	u16 ANL_currFC;
} __attribute__ ((packed)) hfa384x_commsquality_t;

/*-- Information Record: dmbcommsquality --*/
typedef struct hfa384x_dbmcommsquality {
	u16 CQdbm_currBSS;
	u16 ASLdbm_currBSS;
	u16 ANLdbm_currFC;
} __attribute__ ((packed)) hfa384x_dbmcommsquality_t;

/*--------------------------------------------------------------------
FRAME STRUCTURES: Communication Frames
----------------------------------------------------------------------
Communication Frames: Transmit Frames
--------------------------------------------------------------------*/
/*-- Communication Frame: Transmit Frame Structure --*/
typedef struct hfa384x_tx_frame {
	u16 status;
	u16 reserved1;
	u16 reserved2;
	u32 sw_support;
	u8 tx_retrycount;
	u8 tx_rate;
	u16 tx_control;

	/*-- 802.11 Header Information --*/

	u16 frame_control;
	u16 duration_id;
	u8 address1[6];
	u8 address2[6];
	u8 address3[6];
	u16 sequence_control;
	u8 address4[6];
	u16 data_len;		/* little endian format */

	/*-- 802.3 Header Information --*/

	u8 dest_addr[6];
	u8 src_addr[6];
	u16 data_length;	/* big endian format */
} __attribute__ ((packed)) hfa384x_tx_frame_t;
/*--------------------------------------------------------------------
Communication Frames: Field Masks for Transmit Frames
--------------------------------------------------------------------*/
/*-- Status Field --*/
#define		HFA384x_TXSTATUS_ACKERR			((u16)BIT(5))
#define		HFA384x_TXSTATUS_FORMERR		((u16)BIT(3))
#define		HFA384x_TXSTATUS_DISCON			((u16)BIT(2))
#define		HFA384x_TXSTATUS_AGEDERR		((u16)BIT(1))
#define		HFA384x_TXSTATUS_RETRYERR		((u16)BIT(0))
/*-- Transmit Control Field --*/
#define		HFA384x_TX_MACPORT			((u16)(BIT(10) | \
							  BIT(9) | BIT(8)))
#define		HFA384x_TX_STRUCTYPE			((u16)(BIT(4) | BIT(3)))
#define		HFA384x_TX_TXEX				((u16)BIT(2))
#define		HFA384x_TX_TXOK				((u16)BIT(1))
/*--------------------------------------------------------------------
Communication Frames: Test/Get/Set Field Values for Transmit Frames
--------------------------------------------------------------------*/
/*-- Status Field --*/
#define HFA384x_TXSTATUS_ISERROR(v)	\
	(((u16)(v))&\
	(HFA384x_TXSTATUS_ACKERR|HFA384x_TXSTATUS_FORMERR|\
	HFA384x_TXSTATUS_DISCON|HFA384x_TXSTATUS_AGEDERR|\
	HFA384x_TXSTATUS_RETRYERR))

#define	HFA384x_TX_SET(v, m, s)		((((u16)(v))<<((u16)(s)))&((u16)(m)))

#define	HFA384x_TX_MACPORT_SET(v)	HFA384x_TX_SET(v, HFA384x_TX_MACPORT, 8)
#define	HFA384x_TX_STRUCTYPE_SET(v)	HFA384x_TX_SET(v, \
						HFA384x_TX_STRUCTYPE, 3)
#define	HFA384x_TX_TXEX_SET(v)		HFA384x_TX_SET(v, HFA384x_TX_TXEX, 2)
#define	HFA384x_TX_TXOK_SET(v)		HFA384x_TX_SET(v, HFA384x_TX_TXOK, 1)
/*--------------------------------------------------------------------
Communication Frames: Receive Frames
--------------------------------------------------------------------*/
/*-- Communication Frame: Receive Frame Structure --*/
typedef struct hfa384x_rx_frame {
	/*-- MAC rx descriptor (hfa384x byte order) --*/
	u16 status;
	u32 time;
	u8 silence;
	u8 signal;
	u8 rate;
	u8 rx_flow;
	u16 reserved1;
	u16 reserved2;

	/*-- 802.11 Header Information (802.11 byte order) --*/
	u16 frame_control;
	u16 duration_id;
	u8 address1[6];
	u8 address2[6];
	u8 address3[6];
	u16 sequence_control;
	u8 address4[6];
	u16 data_len;		/* hfa384x (little endian) format */

	/*-- 802.3 Header Information --*/
	u8 dest_addr[6];
	u8 src_addr[6];
	u16 data_length;	/* IEEE? (big endian) format */
} __attribute__ ((packed)) hfa384x_rx_frame_t;
/*--------------------------------------------------------------------
Communication Frames: Field Masks for Receive Frames
--------------------------------------------------------------------*/

/*-- Status Fields --*/
#define		HFA384x_RXSTATUS_MACPORT		((u16)(BIT(10) | \
								BIT(9) | \
								BIT(8)))
#define		HFA384x_RXSTATUS_FCSERR			((u16)BIT(0))
/*--------------------------------------------------------------------
Communication Frames: Test/Get/Set Field Values for Receive Frames
--------------------------------------------------------------------*/
#define		HFA384x_RXSTATUS_MACPORT_GET(value)	((u16)((((u16)(value)) \
					    & HFA384x_RXSTATUS_MACPORT) >> 8))
#define		HFA384x_RXSTATUS_ISFCSERR(value)	((u16)(((u16)(value)) \
						  & HFA384x_RXSTATUS_FCSERR))
/*--------------------------------------------------------------------
 FRAME STRUCTURES: Information Types and Information Frame Structures
----------------------------------------------------------------------
Information Types
--------------------------------------------------------------------*/
#define		HFA384x_IT_HANDOVERADDR			((u16)0xF000UL)
#define		HFA384x_IT_COMMTALLIES			((u16)0xF100UL)
#define		HFA384x_IT_SCANRESULTS			((u16)0xF101UL)
#define		HFA384x_IT_CHINFORESULTS		((u16)0xF102UL)
#define		HFA384x_IT_HOSTSCANRESULTS		((u16)0xF103UL)
#define		HFA384x_IT_LINKSTATUS			((u16)0xF200UL)
#define		HFA384x_IT_ASSOCSTATUS			((u16)0xF201UL)
#define		HFA384x_IT_AUTHREQ			((u16)0xF202UL)
#define		HFA384x_IT_PSUSERCNT			((u16)0xF203UL)
#define		HFA384x_IT_KEYIDCHANGED			((u16)0xF204UL)
#define		HFA384x_IT_ASSOCREQ    			((u16)0xF205UL)
#define		HFA384x_IT_MICFAILURE  			((u16)0xF206UL)

/*--------------------------------------------------------------------
Information Frames Structures
----------------------------------------------------------------------
Information Frames: Notification Frame Structures
--------------------------------------------------------------------*/

/*--  Inquiry Frame, Diagnose: Communication Tallies --*/
typedef struct hfa384x_CommTallies16 {
	u16 txunicastframes;
	u16 txmulticastframes;
	u16 txfragments;
	u16 txunicastoctets;
	u16 txmulticastoctets;
	u16 txdeferredtrans;
	u16 txsingleretryframes;
	u16 txmultipleretryframes;
	u16 txretrylimitexceeded;
	u16 txdiscards;
	u16 rxunicastframes;
	u16 rxmulticastframes;
	u16 rxfragments;
	u16 rxunicastoctets;
	u16 rxmulticastoctets;
	u16 rxfcserrors;
	u16 rxdiscardsnobuffer;
	u16 txdiscardswrongsa;
	u16 rxdiscardswepundecr;
	u16 rxmsginmsgfrag;
	u16 rxmsginbadmsgfrag;
} __attribute__ ((packed)) hfa384x_CommTallies16_t;

typedef struct hfa384x_CommTallies32 {
	u32 txunicastframes;
	u32 txmulticastframes;
	u32 txfragments;
	u32 txunicastoctets;
	u32 txmulticastoctets;
	u32 txdeferredtrans;
	u32 txsingleretryframes;
	u32 txmultipleretryframes;
	u32 txretrylimitexceeded;
	u32 txdiscards;
	u32 rxunicastframes;
	u32 rxmulticastframes;
	u32 rxfragments;
	u32 rxunicastoctets;
	u32 rxmulticastoctets;
	u32 rxfcserrors;
	u32 rxdiscardsnobuffer;
	u32 txdiscardswrongsa;
	u32 rxdiscardswepundecr;
	u32 rxmsginmsgfrag;
	u32 rxmsginbadmsgfrag;
} __attribute__ ((packed)) hfa384x_CommTallies32_t;

/*--  Inquiry Frame, Diagnose: Scan Results & Subfields--*/
typedef struct hfa384x_ScanResultSub {
	u16 chid;
	u16 anl;
	u16 sl;
	u8 bssid[WLAN_BSSID_LEN];
	u16 bcnint;
	u16 capinfo;
	hfa384x_bytestr32_t ssid;
	u8 supprates[10];	/* 802.11 info element */
	u16 proberesp_rate;
} __attribute__ ((packed)) hfa384x_ScanResultSub_t;

typedef struct hfa384x_ScanResult {
	u16 rsvd;
	u16 scanreason;
	hfa384x_ScanResultSub_t result[HFA384x_SCANRESULT_MAX];
} __attribute__ ((packed)) hfa384x_ScanResult_t;

/*--  Inquiry Frame, Diagnose: ChInfo Results & Subfields--*/
typedef struct hfa384x_ChInfoResultSub {
	u16 chid;
	u16 anl;
	u16 pnl;
	u16 active;
} __attribute__ ((packed)) hfa384x_ChInfoResultSub_t;

#define HFA384x_CHINFORESULT_BSSACTIVE	BIT(0)
#define HFA384x_CHINFORESULT_PCFACTIVE	BIT(1)

typedef struct hfa384x_ChInfoResult {
	u16 scanchannels;
	hfa384x_ChInfoResultSub_t result[HFA384x_CHINFORESULT_MAX];
} __attribute__ ((packed)) hfa384x_ChInfoResult_t;

/*--  Inquiry Frame, Diagnose: Host Scan Results & Subfields--*/
typedef struct hfa384x_HScanResultSub {
	u16 chid;
	u16 anl;
	u16 sl;
	u8 bssid[WLAN_BSSID_LEN];
	u16 bcnint;
	u16 capinfo;
	hfa384x_bytestr32_t ssid;
	u8 supprates[10];	/* 802.11 info element */
	u16 proberesp_rate;
	u16 atim;
} __attribute__ ((packed)) hfa384x_HScanResultSub_t;

typedef struct hfa384x_HScanResult {
	u16 nresult;
	u16 rsvd;
	hfa384x_HScanResultSub_t result[HFA384x_HSCANRESULT_MAX];
} __attribute__ ((packed)) hfa384x_HScanResult_t;

/*--  Unsolicited Frame, MAC Mgmt: LinkStatus --*/

#define HFA384x_LINK_NOTCONNECTED	((u16)0)
#define HFA384x_LINK_CONNECTED		((u16)1)
#define HFA384x_LINK_DISCONNECTED	((u16)2)
#define HFA384x_LINK_AP_CHANGE		((u16)3)
#define HFA384x_LINK_AP_OUTOFRANGE	((u16)4)
#define HFA384x_LINK_AP_INRANGE		((u16)5)
#define HFA384x_LINK_ASSOCFAIL		((u16)6)

typedef struct hfa384x_LinkStatus {
	u16 linkstatus;
} __attribute__ ((packed)) hfa384x_LinkStatus_t;

/*--  Unsolicited Frame, MAC Mgmt: AssociationStatus (--*/

#define HFA384x_ASSOCSTATUS_STAASSOC	((u16)1)
#define HFA384x_ASSOCSTATUS_REASSOC	((u16)2)
#define HFA384x_ASSOCSTATUS_AUTHFAIL	((u16)5)

typedef struct hfa384x_AssocStatus {
	u16 assocstatus;
	u8 sta_addr[ETH_ALEN];
	/* old_ap_addr is only valid if assocstatus == 2 */
	u8 old_ap_addr[ETH_ALEN];
	u16 reason;
	u16 reserved;
} __attribute__ ((packed)) hfa384x_AssocStatus_t;

/*--  Unsolicited Frame, MAC Mgmt: AuthRequest (AP Only) --*/

typedef struct hfa384x_AuthRequest {
	u8 sta_addr[ETH_ALEN];
	u16 algorithm;
} __attribute__ ((packed)) hfa384x_AuthReq_t;

/*--  Unsolicited Frame, MAC Mgmt: PSUserCount (AP Only) --*/

typedef struct hfa384x_PSUserCount {
	u16 usercnt;
} __attribute__ ((packed)) hfa384x_PSUserCount_t;

typedef struct hfa384x_KeyIDChanged {
	u8 sta_addr[ETH_ALEN];
	u16 keyid;
} __attribute__ ((packed)) hfa384x_KeyIDChanged_t;

/*--  Collection of all Inf frames ---------------*/
typedef union hfa384x_infodata {
	hfa384x_CommTallies16_t commtallies16;
	hfa384x_CommTallies32_t commtallies32;
	hfa384x_ScanResult_t scanresult;
	hfa384x_ChInfoResult_t chinforesult;
	hfa384x_HScanResult_t hscanresult;
	hfa384x_LinkStatus_t linkstatus;
	hfa384x_AssocStatus_t assocstatus;
	hfa384x_AuthReq_t authreq;
	hfa384x_PSUserCount_t psusercnt;
	hfa384x_KeyIDChanged_t keyidchanged;
} __attribute__ ((packed)) hfa384x_infodata_t;

typedef struct hfa384x_InfFrame {
	u16 framelen;
	u16 infotype;
	hfa384x_infodata_t info;
} __attribute__ ((packed)) hfa384x_InfFrame_t;

/*--------------------------------------------------------------------
USB Packet structures and constants.
--------------------------------------------------------------------*/

/* Should be sent to the bulkout endpoint */
#define HFA384x_USB_TXFRM	0
#define HFA384x_USB_CMDREQ	1
#define HFA384x_USB_WRIDREQ	2
#define HFA384x_USB_RRIDREQ	3
#define HFA384x_USB_WMEMREQ	4
#define HFA384x_USB_RMEMREQ	5

/* Received from the bulkin endpoint */
#define HFA384x_USB_ISTXFRM(a)	(((a) & 0x9000) == 0x1000)
#define HFA384x_USB_ISRXFRM(a)	(!((a) & 0x9000))
#define HFA384x_USB_INFOFRM	0x8000
#define HFA384x_USB_CMDRESP	0x8001
#define HFA384x_USB_WRIDRESP	0x8002
#define HFA384x_USB_RRIDRESP	0x8003
#define HFA384x_USB_WMEMRESP	0x8004
#define HFA384x_USB_RMEMRESP	0x8005
#define HFA384x_USB_BUFAVAIL	0x8006
#define HFA384x_USB_ERROR	0x8007

/*------------------------------------*/
/* Request (bulk OUT) packet contents */

typedef struct hfa384x_usb_txfrm {
	hfa384x_tx_frame_t desc;
	u8 data[WLAN_DATA_MAXLEN];
} __attribute__ ((packed)) hfa384x_usb_txfrm_t;

typedef struct hfa384x_usb_cmdreq {
	u16 type;
	u16 cmd;
	u16 parm0;
	u16 parm1;
	u16 parm2;
	u8 pad[54];
} __attribute__ ((packed)) hfa384x_usb_cmdreq_t;

typedef struct hfa384x_usb_wridreq {
	u16 type;
	u16 frmlen;
	u16 rid;
	u8 data[HFA384x_RIDDATA_MAXLEN];
} __attribute__ ((packed)) hfa384x_usb_wridreq_t;

typedef struct hfa384x_usb_rridreq {
	u16 type;
	u16 frmlen;
	u16 rid;
	u8 pad[58];
} __attribute__ ((packed)) hfa384x_usb_rridreq_t;

typedef struct hfa384x_usb_wmemreq {
	u16 type;
	u16 frmlen;
	u16 offset;
	u16 page;
	u8 data[HFA384x_USB_RWMEM_MAXLEN];
} __attribute__ ((packed)) hfa384x_usb_wmemreq_t;

typedef struct hfa384x_usb_rmemreq {
	u16 type;
	u16 frmlen;
	u16 offset;
	u16 page;
	u8 pad[56];
} __attribute__ ((packed)) hfa384x_usb_rmemreq_t;

/*------------------------------------*/
/* Response (bulk IN) packet contents */

typedef struct hfa384x_usb_rxfrm {
	hfa384x_rx_frame_t desc;
	u8 data[WLAN_DATA_MAXLEN];
} __attribute__ ((packed)) hfa384x_usb_rxfrm_t;

typedef struct hfa384x_usb_infofrm {
	u16 type;
	hfa384x_InfFrame_t info;
} __attribute__ ((packed)) hfa384x_usb_infofrm_t;

typedef struct hfa384x_usb_statusresp {
	u16 type;
	u16 status;
	u16 resp0;
	u16 resp1;
	u16 resp2;
} __attribute__ ((packed)) hfa384x_usb_cmdresp_t;

typedef hfa384x_usb_cmdresp_t hfa384x_usb_wridresp_t;

typedef struct hfa384x_usb_rridresp {
	u16 type;
	u16 frmlen;
	u16 rid;
	u8 data[HFA384x_RIDDATA_MAXLEN];
} __attribute__ ((packed)) hfa384x_usb_rridresp_t;

typedef hfa384x_usb_cmdresp_t hfa384x_usb_wmemresp_t;

typedef struct hfa384x_usb_rmemresp {
	u16 type;
	u16 frmlen;
	u8 data[HFA384x_USB_RWMEM_MAXLEN];
} __attribute__ ((packed)) hfa384x_usb_rmemresp_t;

typedef struct hfa384x_usb_bufavail {
	u16 type;
	u16 frmlen;
} __attribute__ ((packed)) hfa384x_usb_bufavail_t;

typedef struct hfa384x_usb_error {
	u16 type;
	u16 errortype;
} __attribute__ ((packed)) hfa384x_usb_error_t;

/*----------------------------------------------------------*/
/* Unions for packaging all the known packet types together */

typedef union hfa384x_usbout {
	u16 type;
	hfa384x_usb_txfrm_t txfrm;
	hfa384x_usb_cmdreq_t cmdreq;
	hfa384x_usb_wridreq_t wridreq;
	hfa384x_usb_rridreq_t rridreq;
	hfa384x_usb_wmemreq_t wmemreq;
	hfa384x_usb_rmemreq_t rmemreq;
} __attribute__ ((packed)) hfa384x_usbout_t;

typedef union hfa384x_usbin {
	u16 type;
	hfa384x_usb_rxfrm_t rxfrm;
	hfa384x_usb_txfrm_t txfrm;
	hfa384x_usb_infofrm_t infofrm;
	hfa384x_usb_cmdresp_t cmdresp;
	hfa384x_usb_wridresp_t wridresp;
	hfa384x_usb_rridresp_t rridresp;
	hfa384x_usb_wmemresp_t wmemresp;
	hfa384x_usb_rmemresp_t rmemresp;
	hfa384x_usb_bufavail_t bufavail;
	hfa384x_usb_error_t usberror;
	u8 boguspad[3000];
} __attribute__ ((packed)) hfa384x_usbin_t;

/*--------------------------------------------------------------------
PD record structures.
--------------------------------------------------------------------*/

typedef struct hfa384x_pdr_pcb_partnum {
	u8 num[8];
} __attribute__ ((packed)) hfa384x_pdr_pcb_partnum_t;

typedef struct hfa384x_pdr_pcb_tracenum {
	u8 num[8];
} __attribute__ ((packed)) hfa384x_pdr_pcb_tracenum_t;

typedef struct hfa384x_pdr_nic_serial {
	u8 num[12];
} __attribute__ ((packed)) hfa384x_pdr_nic_serial_t;

typedef struct hfa384x_pdr_mkk_measurements {
	double carrier_freq;
	double occupied_band;
	double power_density;
	double tx_spur_f1;
	double tx_spur_f2;
	double tx_spur_f3;
	double tx_spur_f4;
	double tx_spur_l1;
	double tx_spur_l2;
	double tx_spur_l3;
	double tx_spur_l4;
	double rx_spur_f1;
	double rx_spur_f2;
	double rx_spur_l1;
	double rx_spur_l2;
} __attribute__ ((packed)) hfa384x_pdr_mkk_measurements_t;

typedef struct hfa384x_pdr_nic_ramsize {
	u8 size[12];		/* units of KB */
} __attribute__ ((packed)) hfa384x_pdr_nic_ramsize_t;

typedef struct hfa384x_pdr_mfisuprange {
	u16 id;
	u16 variant;
	u16 bottom;
	u16 top;
} __attribute__ ((packed)) hfa384x_pdr_mfisuprange_t;

typedef struct hfa384x_pdr_cfisuprange {
	u16 id;
	u16 variant;
	u16 bottom;
	u16 top;
} __attribute__ ((packed)) hfa384x_pdr_cfisuprange_t;

typedef struct hfa384x_pdr_nicid {
	u16 id;
	u16 variant;
	u16 major;
	u16 minor;
} __attribute__ ((packed)) hfa384x_pdr_nicid_t;

typedef struct hfa384x_pdr_refdac_measurements {
	u16 value[0];
} __attribute__ ((packed)) hfa384x_pdr_refdac_measurements_t;

typedef struct hfa384x_pdr_vgdac_measurements {
	u16 value[0];
} __attribute__ ((packed)) hfa384x_pdr_vgdac_measurements_t;

typedef struct hfa384x_pdr_level_comp_measurements {
	u16 value[0];
} __attribute__ ((packed)) hfa384x_pdr_level_compc_measurements_t;

typedef struct hfa384x_pdr_mac_address {
	u8 addr[6];
} __attribute__ ((packed)) hfa384x_pdr_mac_address_t;

typedef struct hfa384x_pdr_mkk_callname {
	u8 callname[8];
} __attribute__ ((packed)) hfa384x_pdr_mkk_callname_t;

typedef struct hfa384x_pdr_regdomain {
	u16 numdomains;
	u16 domain[5];
} __attribute__ ((packed)) hfa384x_pdr_regdomain_t;

typedef struct hfa384x_pdr_allowed_channel {
	u16 ch_bitmap;
} __attribute__ ((packed)) hfa384x_pdr_allowed_channel_t;

typedef struct hfa384x_pdr_default_channel {
	u16 channel;
} __attribute__ ((packed)) hfa384x_pdr_default_channel_t;

typedef struct hfa384x_pdr_privacy_option {
	u16 available;
} __attribute__ ((packed)) hfa384x_pdr_privacy_option_t;

typedef struct hfa384x_pdr_temptype {
	u16 type;
} __attribute__ ((packed)) hfa384x_pdr_temptype_t;

typedef struct hfa384x_pdr_refdac_setup {
	u16 ch_value[14];
} __attribute__ ((packed)) hfa384x_pdr_refdac_setup_t;

typedef struct hfa384x_pdr_vgdac_setup {
	u16 ch_value[14];
} __attribute__ ((packed)) hfa384x_pdr_vgdac_setup_t;

typedef struct hfa384x_pdr_level_comp_setup {
	u16 ch_value[14];
} __attribute__ ((packed)) hfa384x_pdr_level_comp_setup_t;

typedef struct hfa384x_pdr_trimdac_setup {
	u16 trimidac;
	u16 trimqdac;
} __attribute__ ((packed)) hfa384x_pdr_trimdac_setup_t;

typedef struct hfa384x_pdr_ifr_setting {
	u16 value[3];
} __attribute__ ((packed)) hfa384x_pdr_ifr_setting_t;

typedef struct hfa384x_pdr_rfr_setting {
	u16 value[3];
} __attribute__ ((packed)) hfa384x_pdr_rfr_setting_t;

typedef struct hfa384x_pdr_hfa3861_baseline {
	u16 value[50];
} __attribute__ ((packed)) hfa384x_pdr_hfa3861_baseline_t;

typedef struct hfa384x_pdr_hfa3861_shadow {
	u32 value[32];
} __attribute__ ((packed)) hfa384x_pdr_hfa3861_shadow_t;

typedef struct hfa384x_pdr_hfa3861_ifrf {
	u32 value[20];
} __attribute__ ((packed)) hfa384x_pdr_hfa3861_ifrf_t;

typedef struct hfa384x_pdr_hfa3861_chcalsp {
	u16 value[14];
} __attribute__ ((packed)) hfa384x_pdr_hfa3861_chcalsp_t;

typedef struct hfa384x_pdr_hfa3861_chcali {
	u16 value[17];
} __attribute__ ((packed)) hfa384x_pdr_hfa3861_chcali_t;

typedef struct hfa384x_pdr_hfa3861_nic_config {
	u16 config_bitmap;
} __attribute__ ((packed)) hfa384x_pdr_nic_config_t;

typedef struct hfa384x_pdr_hfo_delay {
	u8 hfo_delay;
} __attribute__ ((packed)) hfa384x_hfo_delay_t;

typedef struct hfa384x_pdr_hfa3861_manf_testsp {
	u16 value[30];
} __attribute__ ((packed)) hfa384x_pdr_hfa3861_manf_testsp_t;

typedef struct hfa384x_pdr_hfa3861_manf_testi {
	u16 value[30];
} __attribute__ ((packed)) hfa384x_pdr_hfa3861_manf_testi_t;

typedef struct hfa384x_end_of_pda {
	u16 crc;
} __attribute__ ((packed)) hfa384x_pdr_end_of_pda_t;

typedef struct hfa384x_pdrec {
	u16 len;		/* in words */
	u16 code;
	union pdr {
		hfa384x_pdr_pcb_partnum_t pcb_partnum;
		hfa384x_pdr_pcb_tracenum_t pcb_tracenum;
		hfa384x_pdr_nic_serial_t nic_serial;
		hfa384x_pdr_mkk_measurements_t mkk_measurements;
		hfa384x_pdr_nic_ramsize_t nic_ramsize;
		hfa384x_pdr_mfisuprange_t mfisuprange;
		hfa384x_pdr_cfisuprange_t cfisuprange;
		hfa384x_pdr_nicid_t nicid;
		hfa384x_pdr_refdac_measurements_t refdac_measurements;
		hfa384x_pdr_vgdac_measurements_t vgdac_measurements;
		hfa384x_pdr_level_compc_measurements_t level_compc_measurements;
		hfa384x_pdr_mac_address_t mac_address;
		hfa384x_pdr_mkk_callname_t mkk_callname;
		hfa384x_pdr_regdomain_t regdomain;
		hfa384x_pdr_allowed_channel_t allowed_channel;
		hfa384x_pdr_default_channel_t default_channel;
		hfa384x_pdr_privacy_option_t privacy_option;
		hfa384x_pdr_temptype_t temptype;
		hfa384x_pdr_refdac_setup_t refdac_setup;
		hfa384x_pdr_vgdac_setup_t vgdac_setup;
		hfa384x_pdr_level_comp_setup_t level_comp_setup;
		hfa384x_pdr_trimdac_setup_t trimdac_setup;
		hfa384x_pdr_ifr_setting_t ifr_setting;
		hfa384x_pdr_rfr_setting_t rfr_setting;
		hfa384x_pdr_hfa3861_baseline_t hfa3861_baseline;
		hfa384x_pdr_hfa3861_shadow_t hfa3861_shadow;
		hfa384x_pdr_hfa3861_ifrf_t hfa3861_ifrf;
		hfa384x_pdr_hfa3861_chcalsp_t hfa3861_chcalsp;
		hfa384x_pdr_hfa3861_chcali_t hfa3861_chcali;
		hfa384x_pdr_nic_config_t nic_config;
		hfa384x_hfo_delay_t hfo_delay;
		hfa384x_pdr_hfa3861_manf_testsp_t hfa3861_manf_testsp;
		hfa384x_pdr_hfa3861_manf_testi_t hfa3861_manf_testi;
		hfa384x_pdr_end_of_pda_t end_of_pda;

	} data;
} __attribute__ ((packed)) hfa384x_pdrec_t;

#ifdef __KERNEL__
/*--------------------------------------------------------------------
---  MAC state structure, argument to all functions --
---  Also, a collection of support types --
--------------------------------------------------------------------*/
typedef struct hfa384x_statusresult {
	u16 status;
	u16 resp0;
	u16 resp1;
	u16 resp2;
} hfa384x_cmdresult_t;

/* USB Control Exchange (CTLX):
 *  A queue of the structure below is maintained for all of the
 *  Request/Response type USB packets supported by Prism2.
 */
/* The following hfa384x_* structures are arguments to
 * the usercb() for the different CTLX types.
 */
typedef struct hfa384x_rridresult {
	u16 rid;
	const void *riddata;
	unsigned int riddata_len;
} hfa384x_rridresult_t;

enum ctlx_state {
	CTLX_START = 0,		/* Start state, not queued */

	CTLX_COMPLETE,		/* CTLX successfully completed */
	CTLX_REQ_FAILED,	/* OUT URB completed w/ error */

	CTLX_PENDING,		/* Queued, data valid */
	CTLX_REQ_SUBMITTED,	/* OUT URB submitted */
	CTLX_REQ_COMPLETE,	/* OUT URB complete */
	CTLX_RESP_COMPLETE	/* IN URB received */
};
typedef enum ctlx_state CTLX_STATE;

struct hfa384x_usbctlx;
struct hfa384x;

typedef void (*ctlx_cmdcb_t) (struct hfa384x *, const struct hfa384x_usbctlx *);

typedef void (*ctlx_usercb_t) (struct hfa384x *hw,
			       void *ctlxresult, void *usercb_data);

typedef struct hfa384x_usbctlx {
	struct list_head list;

	size_t outbufsize;
	hfa384x_usbout_t outbuf;	/* pkt buf for OUT */
	hfa384x_usbin_t inbuf;	/* pkt buf for IN(a copy) */

	CTLX_STATE state;	/* Tracks running state */

	struct completion done;
	volatile int reapable;	/* Food for the reaper task */

	ctlx_cmdcb_t cmdcb;	/* Async command callback */
	ctlx_usercb_t usercb;	/* Async user callback, */
	void *usercb_data;	/*  at CTLX completion  */

	int variant;		/* Identifies cmd variant */
} hfa384x_usbctlx_t;

typedef struct hfa384x_usbctlxq {
	spinlock_t lock;
	struct list_head pending;
	struct list_head active;
	struct list_head completing;
	struct list_head reapable;
} hfa384x_usbctlxq_t;

typedef struct hfa484x_metacmd {
	u16 cmd;

	u16 parm0;
	u16 parm1;
	u16 parm2;

	hfa384x_cmdresult_t result;
} hfa384x_metacmd_t;

#define	MAX_GRP_ADDR		32
#define WLAN_COMMENT_MAX	80  /* Max. length of user comment string. */

#define WLAN_AUTH_MAX           60  /* Max. # of authenticated stations. */
#define WLAN_ACCESS_MAX		60  /* Max. # of stations in an access list. */
#define WLAN_ACCESS_NONE	0   /* No stations may be authenticated. */
#define WLAN_ACCESS_ALL		1   /* All stations may be authenticated. */
#define WLAN_ACCESS_ALLOW	2   /* Authenticate only "allowed" stations. */
#define WLAN_ACCESS_DENY	3   /* Do not authenticate "denied" stations. */

typedef struct prism2sta_authlist {
	unsigned int cnt;
	u8 addr[WLAN_AUTH_MAX][ETH_ALEN];
	u8 assoc[WLAN_AUTH_MAX];
} prism2sta_authlist_t;

typedef struct prism2sta_accesslist {
	unsigned int modify;
	unsigned int cnt;
	u8 addr[WLAN_ACCESS_MAX][ETH_ALEN];
	unsigned int cnt1;
	u8 addr1[WLAN_ACCESS_MAX][ETH_ALEN];
} prism2sta_accesslist_t;

typedef struct hfa384x {
	/* USB support data */
	struct usb_device *usb;
	struct urb rx_urb;
	struct sk_buff *rx_urb_skb;
	struct urb tx_urb;
	struct urb ctlx_urb;
	hfa384x_usbout_t txbuff;
	hfa384x_usbctlxq_t ctlxq;
	struct timer_list reqtimer;
	struct timer_list resptimer;

	struct timer_list throttle;

	struct tasklet_struct reaper_bh;
	struct tasklet_struct completion_bh;

	struct work_struct usb_work;

	unsigned long usb_flags;
#define THROTTLE_RX	0
#define THROTTLE_TX	1
#define WORK_RX_HALT	2
#define WORK_TX_HALT	3
#define WORK_RX_RESUME	4
#define WORK_TX_RESUME	5

	unsigned short req_timer_done:1;
	unsigned short resp_timer_done:1;

	int endp_in;
	int endp_out;

	int sniff_fcs;
	int sniff_channel;
	int sniff_truncate;
	int sniffhdr;

	wait_queue_head_t cmdq;	/* wait queue itself */

	/* Controller state */
	u32 state;
	u32 isap;
	u8 port_enabled[HFA384x_NUMPORTS_MAX];

	/* Download support */
	unsigned int dlstate;
	hfa384x_downloadbuffer_t bufinfo;
	u16 dltimeout;

	int scanflag;		/* to signal scan comlete */
	int join_ap;		/* are we joined to a specific ap */
	int join_retries;	/* number of join retries till we fail */
	hfa384x_JoinRequest_data_t joinreq;	/* join request saved data */

	wlandevice_t *wlandev;
	/* Timer to allow for the deferred processing of linkstatus messages */
	struct work_struct link_bh;

	struct work_struct commsqual_bh;
	hfa384x_commsquality_t qual;
	struct timer_list commsqual_timer;

	u16 link_status;
	u16 link_status_new;
	struct sk_buff_head authq;

	u32 txrate;

	/* And here we have stuff that used to be in priv */

	/* State variables */
	unsigned int presniff_port_type;
	u16 presniff_wepflags;
	u32 dot11_desired_bss_type;

	int dbmadjust;

	/* Group Addresses - right now, there are up to a total
	   of MAX_GRP_ADDR group addresses */
	u8 dot11_grp_addr[MAX_GRP_ADDR][ETH_ALEN];
	unsigned int dot11_grpcnt;

	/* Component Identities */
	hfa384x_compident_t ident_nic;
	hfa384x_compident_t ident_pri_fw;
	hfa384x_compident_t ident_sta_fw;
	hfa384x_compident_t ident_ap_fw;
	u16 mm_mods;

	/* Supplier compatibility ranges */
	hfa384x_caplevel_t cap_sup_mfi;
	hfa384x_caplevel_t cap_sup_cfi;
	hfa384x_caplevel_t cap_sup_pri;
	hfa384x_caplevel_t cap_sup_sta;
	hfa384x_caplevel_t cap_sup_ap;

	/* Actor compatibility ranges */
	hfa384x_caplevel_t cap_act_pri_cfi;	/*
						 * pri f/w to controller
						 * interface
						 */

	hfa384x_caplevel_t cap_act_sta_cfi;	/*
						 * sta f/w to controller
						 * interface
						 */

	hfa384x_caplevel_t cap_act_sta_mfi;	/* sta f/w to modem interface */

	hfa384x_caplevel_t cap_act_ap_cfi;	/*
						 * ap f/w to controller
						 * interface
						 */

	hfa384x_caplevel_t cap_act_ap_mfi;	/* ap f/w to modem interface */

	u32 psusercount;	/* Power save user count. */
	hfa384x_CommTallies32_t tallies;	/* Communication tallies. */
	u8 comment[WLAN_COMMENT_MAX + 1];	/* User comment */

	/* Channel Info request results (AP only) */
	struct {
		atomic_t done;
		u8 count;
		hfa384x_ChInfoResult_t results;
	} channel_info;

	hfa384x_InfFrame_t *scanresults;

	prism2sta_authlist_t authlist;	/* Authenticated station list. */
	unsigned int accessmode;	/* Access mode. */
	prism2sta_accesslist_t allow;	/* Allowed station list. */
	prism2sta_accesslist_t deny;	/* Denied station list. */

} hfa384x_t;

void hfa384x_create(hfa384x_t *hw, struct usb_device *usb);
void hfa384x_destroy(hfa384x_t *hw);

int
hfa384x_corereset(hfa384x_t *hw, int holdtime, int settletime, int genesis);
int hfa384x_drvr_commtallies(hfa384x_t *hw);
int hfa384x_drvr_disable(hfa384x_t *hw, u16 macport);
int hfa384x_drvr_enable(hfa384x_t *hw, u16 macport);
int hfa384x_drvr_flashdl_enable(hfa384x_t *hw);
int hfa384x_drvr_flashdl_disable(hfa384x_t *hw);
int hfa384x_drvr_flashdl_write(hfa384x_t *hw, u32 daddr, void *buf, u32 len);
int hfa384x_drvr_getconfig(hfa384x_t *hw, u16 rid, void *buf, u16 len);
int hfa384x_drvr_ramdl_enable(hfa384x_t *hw, u32 exeaddr);
int hfa384x_drvr_ramdl_disable(hfa384x_t *hw);
int hfa384x_drvr_ramdl_write(hfa384x_t *hw, u32 daddr, void *buf, u32 len);
int hfa384x_drvr_readpda(hfa384x_t *hw, void *buf, unsigned int len);
int hfa384x_drvr_setconfig(hfa384x_t *hw, u16 rid, void *buf, u16 len);

static inline int hfa384x_drvr_getconfig16(hfa384x_t *hw, u16 rid, void *val)
{
	int result = 0;
	result = hfa384x_drvr_getconfig(hw, rid, val, sizeof(u16));
	if (result == 0)
		*((u16 *) val) = le16_to_cpu(*((u16 *) val));
	return result;
}

static inline int hfa384x_drvr_setconfig16(hfa384x_t *hw, u16 rid, u16 val)
{
	u16 value = cpu_to_le16(val);
	return hfa384x_drvr_setconfig(hw, rid, &value, sizeof(value));
}

int
hfa384x_drvr_getconfig_async(hfa384x_t *hw,
			     u16 rid, ctlx_usercb_t usercb, void *usercb_data);

int
hfa384x_drvr_setconfig_async(hfa384x_t *hw,
			     u16 rid,
			     void *buf,
			     u16 len, ctlx_usercb_t usercb, void *usercb_data);

static inline int
hfa384x_drvr_setconfig16_async(hfa384x_t *hw, u16 rid, u16 val)
{
	u16 value = cpu_to_le16(val);
	return hfa384x_drvr_setconfig_async(hw, rid, &value, sizeof(value),
					    NULL, NULL);
}

int hfa384x_drvr_start(hfa384x_t *hw);
int hfa384x_drvr_stop(hfa384x_t *hw);
int
hfa384x_drvr_txframe(hfa384x_t *hw, struct sk_buff *skb,
		     union p80211_hdr *p80211_hdr, struct p80211_metawep *p80211_wep);
void hfa384x_tx_timeout(wlandevice_t *wlandev);

int hfa384x_cmd_initialize(hfa384x_t *hw);
int hfa384x_cmd_enable(hfa384x_t *hw, u16 macport);
int hfa384x_cmd_disable(hfa384x_t *hw, u16 macport);
int hfa384x_cmd_allocate(hfa384x_t *hw, u16 len);
int hfa384x_cmd_monitor(hfa384x_t *hw, u16 enable);
int
hfa384x_cmd_download(hfa384x_t *hw,
		     u16 mode, u16 lowaddr, u16 highaddr, u16 codelen);

#endif /*__KERNEL__ */

#endif /*_HFA384x_H */
