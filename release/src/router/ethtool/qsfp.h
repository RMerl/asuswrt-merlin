/*
 * SFF 8636 standards based QSFP EEPROM Field Definitions
 *
 * Vidya Ravipati <vidya@cumulusnetworks.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef QSFP_H__
#define QSFP_H__

/*------------------------------------------------------------------------------
 *
 * QSFP EEPROM data structures
 *
 * register info from SFF-8636 Rev 2.7
 */

/*------------------------------------------------------------------------------
 *
 * Lower Memory Page 00h
 * Measurement, Diagnostic and Control Functions
 *
 */
/* Identifier - 0 */
/* Values are defined under SFF8024_ID_OFFSET */
#define	SFF8636_ID_OFFSET	0x00

#define	SFF8636_REV_COMPLIANCE_OFFSET	0x01
#define	 SFF8636_REV_UNSPECIFIED		0x00
#define	 SFF8636_REV_8436_48			0x01
#define	 SFF8636_REV_8436_8636			0x02
#define	 SFF8636_REV_8636_13			0x03
#define	 SFF8636_REV_8636_14			0x04
#define	 SFF8636_REV_8636_15			0x05
#define	 SFF8636_REV_8636_20			0x06
#define	 SFF8636_REV_8636_27			0x07

#define	SFF8636_STATUS_2_OFFSET	0x02
/* Flat Memory:0- Paging, 1- Page 0 only */
#define	 SFF8636_STATUS_PAGE_3_PRESENT		(1 << 2)
#define	 SFF8636_STATUS_INTL_OUTPUT		(1 << 1)
#define	 SFF8636_STATUS_DATA_NOT_READY		(1 << 0)

/* Channel Status Interrupt Flags - 3-5 */
#define	SFF8636_LOS_AW_OFFSET	0x03
#define	 SFF8636_TX4_LOS_AW		(1 << 7)
#define	 SFF8636_TX3_LOS_AW		(1 << 6)
#define	 SFF8636_TX2_LOS_AW		(1 << 5)
#define	 SFF8636_TX1_LOS_AW		(1 << 4)
#define	 SFF8636_RX4_LOS_AW		(1 << 3)
#define	 SFF8636_RX3_LOS_AW		(1 << 2)
#define	 SFF8636_RX2_LOS_AW		(1 << 1)
#define	 SFF8636_RX1_LOS_AW		(1 << 0)

#define	SFF8636_FAULT_AW_OFFSET	0x04
#define	 SFF8636_TX4_FAULT_AW	(1 << 3)
#define	 SFF8636_TX3_FAULT_AW	(1 << 2)
#define	 SFF8636_TX2_FAULT_AW	(1 << 1)
#define	 SFF8636_TX1_FAULT_AW	(1 << 0)

/* Module Monitor Interrupt Flags - 6-8 */
#define	SFF8636_TEMP_AW_OFFSET	0x06
#define	 SFF8636_TEMP_HALARM_STATUS		(1 << 7)
#define	 SFF8636_TEMP_LALARM_STATUS		(1 << 6)
#define	 SFF8636_TEMP_HWARN_STATUS		(1 << 5)
#define	 SFF8636_TEMP_LWARN_STATUS		(1 << 4)

#define	SFF8636_VCC_AW_OFFSET	0x07
#define	 SFF8636_VCC_HALARM_STATUS		(1 << 7)
#define	 SFF8636_VCC_LALARM_STATUS		(1 << 6)
#define	 SFF8636_VCC_HWARN_STATUS		(1 << 5)
#define	 SFF8636_VCC_LWARN_STATUS		(1 << 4)

/* Channel Monitor Interrupt Flags - 9-21 */
#define	SFF8636_RX_PWR_12_AW_OFFSET	0x09
#define	 SFF8636_RX_PWR_1_HALARM		(1 << 7)
#define	 SFF8636_RX_PWR_1_LALARM		(1 << 6)
#define	 SFF8636_RX_PWR_1_HWARN			(1 << 5)
#define	 SFF8636_RX_PWR_1_LWARN			(1 << 4)
#define	 SFF8636_RX_PWR_2_HALARM		(1 << 3)
#define	 SFF8636_RX_PWR_2_LALARM		(1 << 2)
#define	 SFF8636_RX_PWR_2_HWARN			(1 << 1)
#define	 SFF8636_RX_PWR_2_LWARN			(1 << 0)

#define	SFF8636_RX_PWR_34_AW_OFFSET	0x0A
#define	 SFF8636_RX_PWR_3_HALARM		(1 << 7)
#define	 SFF8636_RX_PWR_3_LALARM		(1 << 6)
#define	 SFF8636_RX_PWR_3_HWARN			(1 << 5)
#define	 SFF8636_RX_PWR_3_LWARN			(1 << 4)
#define	 SFF8636_RX_PWR_4_HALARM		(1 << 3)
#define	 SFF8636_RX_PWR_4_LALARM		(1 << 2)
#define	 SFF8636_RX_PWR_4_HWARN			(1 << 1)
#define	 SFF8636_RX_PWR_4_LWARN			(1 << 0)

#define	SFF8636_TX_BIAS_12_AW_OFFSET	0x0B
#define	 SFF8636_TX_BIAS_1_HALARM		(1 << 7)
#define	 SFF8636_TX_BIAS_1_LALARM		(1 << 6)
#define	 SFF8636_TX_BIAS_1_HWARN		(1 << 5)
#define	 SFF8636_TX_BIAS_1_LWARN		(1 << 4)
#define	 SFF8636_TX_BIAS_2_HALARM		(1 << 3)
#define	 SFF8636_TX_BIAS_2_LALARM		(1 << 2)
#define	 SFF8636_TX_BIAS_2_HWARN		(1 << 1)
#define	 SFF8636_TX_BIAS_2_LWARN		(1 << 0)

#define	SFF8636_TX_BIAS_34_AW_OFFSET	0xC
#define	 SFF8636_TX_BIAS_3_HALARM		(1 << 7)
#define	 SFF8636_TX_BIAS_3_LALARM		(1 << 6)
#define	 SFF8636_TX_BIAS_3_HWARN		(1 << 5)
#define	 SFF8636_TX_BIAS_3_LWARN		(1 << 4)
#define	 SFF8636_TX_BIAS_4_HALARM		(1 << 3)
#define	 SFF8636_TX_BIAS_4_LALARM		(1 << 2)
#define	 SFF8636_TX_BIAS_4_HWARN		(1 << 1)
#define	 SFF8636_TX_BIAS_4_LWARN		(1 << 0)

#define	SFF8636_TX_PWR_12_AW_OFFSET	0x0D
#define	 SFF8636_TX_PWR_1_HALARM		(1 << 7)
#define	 SFF8636_TX_PWR_1_LALARM		(1 << 6)
#define	 SFF8636_TX_PWR_1_HWARN			(1 << 5)
#define	 SFF8636_TX_PWR_1_LWARN			(1 << 4)
#define	 SFF8636_TX_PWR_2_HALARM		(1 << 3)
#define	 SFF8636_TX_PWR_2_LALARM		(1 << 2)
#define	 SFF8636_TX_PWR_2_HWARN			(1 << 1)
#define	 SFF8636_TX_PWR_2_LWARN			(1 << 0)

#define	SFF8636_TX_PWR_34_AW_OFFSET	0x0E
#define	 SFF8636_TX_PWR_3_HALARM		(1 << 7)
#define	 SFF8636_TX_PWR_3_LALARM		(1 << 6)
#define	 SFF8636_TX_PWR_3_HWARN			(1 << 5)
#define	 SFF8636_TX_PWR_3_LWARN			(1 << 4)
#define	 SFF8636_TX_PWR_4_HALARM		(1 << 3)
#define	 SFF8636_TX_PWR_4_LALARM		(1 << 2)
#define	 SFF8636_TX_PWR_4_HWARN			(1 << 1)
#define	 SFF8636_TX_PWR_4_LWARN			(1 << 0)

/* Module Monitoring Values - 22-33 */
#define	SFF8636_TEMP_CURR		0x16
#define	SFF8636_TEMP_MSB_OFFSET		0x16
#define	SFF8636_TEMP_LSB_OFFSET		0x17

#define	SFF8636_VCC_CURR		0x1A
#define	SFF8636_VCC_MSB_OFFSET		0x1A
#define	SFF8636_VCC_LSB_OFFSET		0x1B

/* Channel Monitoring Values - 34-81 */
#define	SFF8636_RX_PWR_1_OFFSET		0x22
#define	SFF8636_RX_PWR_2_OFFSET		0x24
#define	SFF8636_RX_PWR_3_OFFSET		0x26
#define	SFF8636_RX_PWR_4_OFFSET		0x28

#define	SFF8636_TX_BIAS_1_OFFSET	0x2A
#define	SFF8636_TX_BIAS_2_OFFSET	0x2C
#define	SFF8636_TX_BIAS_3_OFFSET	0x2E
#define	SFF8636_TX_BIAS_4_OFFSET	0x30

#define	SFF8636_TX_PWR_1_OFFSET		0x32
#define	SFF8636_TX_PWR_2_OFFSET		0x34
#define	SFF8636_TX_PWR_3_OFFSET		0x36
#define	SFF8636_TX_PWR_4_OFFSET		0x38

/* Control Bytes - 86 - 99 */
#define	SFF8636_TX_DISABLE_OFFSET	0x56
#define	 SFF8636_TX_DISABLE_4			(1 << 3)
#define	 SFF8636_TX_DISABLE_3			(1 << 2)
#define	 SFF8636_TX_DISABLE_2			(1 << 1)
#define	 SFF8636_TX_DISABLE_1			(1 << 0)

#define	SFF8636_RX_RATE_SELECT_OFFSET	0x57
#define	 SFF8636_RX_RATE_SELECT_4_MASK		(3 << 6)
#define	 SFF8636_RX_RATE_SELECT_3_MASK		(3 << 4)
#define	 SFF8636_RX_RATE_SELECT_2_MASK		(3 << 2)
#define	 SFF8636_RX_RATE_SELECT_1_MASK		(3 << 0)

#define	SFF8636_TX_RATE_SELECT_OFFSET	0x58
#define	 SFF8636_TX_RATE_SELECT_4_MASK		(3 << 6)
#define	 SFF8636_TX_RATE_SELECT_3_MASK		(3 << 4)
#define	 SFF8636_TX_RATE_SELECT_2_MASK		(3 << 2)
#define	 SFF8636_TX_RATE_SELECT_1_MASK		(3 << 0)

#define	SFF8636_RX_APP_SELECT_4_OFFSET	0x58
#define	SFF8636_RX_APP_SELECT_3_OFFSET	0x59
#define	SFF8636_RX_APP_SELECT_2_OFFSET	0x5A
#define	SFF8636_RX_APP_SELECT_1_OFFSET	0x5B

#define	SFF8636_PWR_MODE_OFFSET		0x5D
#define	 SFF8636_HIGH_PWR_ENABLE		(1 << 2)
#define	 SFF8636_LOW_PWR_MODE			(1 << 1)
#define	 SFF8636_PWR_OVERRIDE			(1 << 0)

#define	SFF8636_TX_APP_SELECT_4_OFFSET	0x5E
#define	SFF8636_TX_APP_SELECT_3_OFFSET	0x5F
#define	SFF8636_TX_APP_SELECT_2_OFFSET	0x60
#define	SFF8636_TX_APP_SELECT_1_OFFSET	0x61

#define	SFF8636_LOS_MASK_OFFSET		0x64
#define	 SFF8636_TX_LOS_4_MASK			(1 << 7)
#define	 SFF8636_TX_LOS_3_MASK			(1 << 6)
#define	 SFF8636_TX_LOS_2_MASK			(1 << 5)
#define	 SFF8636_TX_LOS_1_MASK			(1 << 4)
#define	 SFF8636_RX_LOS_4_MASK			(1 << 3)
#define	 SFF8636_RX_LOS_3_MASK			(1 << 2)
#define	 SFF8636_RX_LOS_2_MASK			(1 << 1)
#define	 SFF8636_RX_LOS_1_MASK			(1 << 0)

#define	SFF8636_FAULT_MASK_OFFSET	0x65
#define	 SFF8636_TX_FAULT_1_MASK		(1 << 3)
#define	 SFF8636_TX_FAULT_2_MASK		(1 << 2)
#define	 SFF8636_TX_FAULT_3_MASK		(1 << 1)
#define	 SFF8636_TX_FAULT_4_MASK		(1 << 0)

#define	SFF8636_TEMP_MASK_OFFSET	0x67
#define	 SFF8636_TEMP_HALARM_MASK		(1 << 7)
#define	 SFF8636_TEMP_LALARM_MASK		(1 << 6)
#define	 SFF8636_TEMP_HWARN_MASK		(1 << 5)
#define	 SFF8636_TEMP_LWARN_MASK		(1 << 4)

#define	SFF8636_VCC_MASK_OFFSET		0x68
#define	 SFF8636_VCC_HALARM_MASK		(1 << 7)
#define	 SFF8636_VCC_LALARM_MASK		(1 << 6)
#define	 SFF8636_VCC_HWARN_MASK			(1 << 5)
#define	 SFF8636_VCC_LWARN_MASK			(1 << 4)

/*------------------------------------------------------------------------------
 *
 * Upper Memory Page 00h
 * Serial ID - Base ID, Extended ID and Vendor Specific ID fields
 *
 */
/* Identifier - 128 */
/* Identifier values same as Lower Memory Page 00h */
#define	SFF8636_UPPER_PAGE_0_ID_OFFSET		0x80

/* Extended Identifier - 128 */
#define SFF8636_EXT_ID_OFFSET		0x81
#define	 SFF8636_EXT_ID_PWR_CLASS_MASK		0xC0
#define	  SFF8636_EXT_ID_PWR_CLASS_1		(0 << 6)
#define	  SFF8636_EXT_ID_PWR_CLASS_2		(1 << 6)
#define	  SFF8636_EXT_ID_PWR_CLASS_3		(2 << 6)
#define	  SFF8636_EXT_ID_PWR_CLASS_4		(3 << 6)
#define	 SFF8636_EXT_ID_CLIE_MASK		0x10
#define	  SFF8636_EXT_ID_CLIEI_CODE_PRESENT	(1 << 4)
#define	 SFF8636_EXT_ID_CDR_TX_MASK		0x08
#define	  SFF8636_EXT_ID_CDR_TX_PRESENT		(1 << 3)
#define	 SFF8636_EXT_ID_CDR_RX_MASK		0x04
#define	  SFF8636_EXT_ID_CDR_RX_PRESENT		(1 << 2)
#define	 SFF8636_EXT_ID_EPWR_CLASS_MASK		0x03
#define	  SFF8636_EXT_ID_PWR_CLASS_LEGACY	0
#define	  SFF8636_EXT_ID_PWR_CLASS_5		1
#define	  SFF8636_EXT_ID_PWR_CLASS_6		2
#define	  SFF8636_EXT_ID_PWR_CLASS_7		3

/* Connector Values offset - 130 */
/* Values are defined under SFF8024_CTOR */
#define	SFF8636_CTOR_OFFSET		0x82
#define	 SFF8636_CTOR_UNKNOWN			0x00
#define	 SFF8636_CTOR_SC			0x01
#define	 SFF8636_CTOR_FC_STYLE_1		0x02
#define	 SFF8636_CTOR_FC_STYLE_2		0x03
#define	 SFF8636_CTOR_BNC_TNC			0x04
#define	 SFF8636_CTOR_FC_COAX			0x05
#define	 SFF8636_CTOR_FIBER_JACK		0x06
#define	 SFF8636_CTOR_LC			0x07
#define	 SFF8636_CTOR_MT_RJ			0x08
#define	 SFF8636_CTOR_MU			0x09
#define	 SFF8636_CTOR_SG			0x0A
#define	 SFF8636_CTOR_OPT_PT			0x0B
#define	 SFF8636_CTOR_MPO			0x0C
/* 0D-1Fh --- Reserved */
#define	 SFF8636_CTOR_HSDC_II			0x20
#define	 SFF8636_CTOR_COPPER_PT			0x21
#define	 SFF8636_CTOR_RJ45			0x22
#define	 SFF8636_CTOR_NO_SEPARABLE		0x23
#define	 SFF8636_CTOR_MXC_2X16			0x24

/* Specification Compliance - 131-138 */
/* Ethernet Compliance Codes - 131 */
#define	SFF8636_ETHERNET_COMP_OFFSET	0x83
#define	 SFF8636_ETHERNET_RSRVD			(1 << 7)
#define	 SFF8636_ETHERNET_10G_LRM		(1 << 6)
#define	 SFF8636_ETHERNET_10G_LR		(1 << 5)
#define	 SFF8636_ETHERNET_10G_SR		(1 << 4)
#define	 SFF8636_ETHERNET_40G_CR4		(1 << 3)
#define	 SFF8636_ETHERNET_40G_SR4		(1 << 2)
#define	 SFF8636_ETHERNET_40G_LR4		(1 << 1)
#define	 SFF8636_ETHERNET_40G_ACTIVE	(1 << 0)

/* SONET Compliance Codes - 132 */
#define	SFF8636_SONET_COMP_OFFSET	0x84
#define	 SFF8636_SONET_40G_OTN			(1 << 3)
#define	 SFF8636_SONET_OC48_LR			(1 << 2)
#define	 SFF8636_SONET_OC48_IR			(1 << 1)
#define	 SFF8636_SONET_OC48_SR			(1 << 0)

/* SAS/SATA Complaince Codes - 133 */
#define	SFF8636_SAS_COMP_OFFSET		0x85
#define	 SFF8636_SAS_12G			(1 << 6)
#define	 SFF8636_SAS_6G				(1 << 5)
#define	 SFF8636_SAS_3G				(1 << 4)

/* Gigabit Ethernet Compliance Codes - 134 */
#define	SFF8636_GIGE_COMP_OFFSET	0x86
#define	 SFF8636_GIGE_1000_BASE_T		(1 << 3)
#define	 SFF8636_GIGE_1000_BASE_CX		(1 << 2)
#define	 SFF8636_GIGE_1000_BASE_LX		(1 << 1)
#define	 SFF8636_GIGE_1000_BASE_SX		(1 << 0)

/* Fibre Channel Link length/Transmitter Tech. - 135,136 */
#define	SFF8636_FC_LEN_OFFSET		0x87
#define	 SFF8636_FC_LEN_VERY_LONG		(1 << 7)
#define	 SFF8636_FC_LEN_SHORT			(1 << 6)
#define	 SFF8636_FC_LEN_INT			(1 << 5)
#define	 SFF8636_FC_LEN_LONG			(1 << 4)
#define	 SFF8636_FC_LEN_MED			(1 << 3)
#define	 SFF8636_FC_TECH_LONG_LC		(1 << 1)
#define	 SFF8636_FC_TECH_ELEC_INTER		(1 << 0)

#define	SFF8636_FC_TECH_OFFSET		0x88
#define	 SFF8636_FC_TECH_ELEC_INTRA		(1 << 7)
#define	 SFF8636_FC_TECH_SHORT_WO_OFC		(1 << 6)
#define	 SFF8636_FC_TECH_SHORT_W_OFC		(1 << 5)
#define	 SFF8636_FC_TECH_LONG_LL		(1 << 4)

/* Fibre Channel Transmitter Media - 137 */
#define	SFF8636_FC_TRANS_MEDIA_OFFSET	0x89
/* Twin Axial Pair */
#define	 SFF8636_FC_TRANS_MEDIA_TW		(1 << 7)
/* Shielded Twisted Pair */
#define	 SFF8636_FC_TRANS_MEDIA_TP		(1 << 6)
/* Miniature Coax */
#define	 SFF8636_FC_TRANS_MEDIA_MI		(1 << 5)
/* Video Coax */
#define	 SFF8636_FC_TRANS_MEDIA_TV		(1 << 4)
/* Multi-mode 62.5m */
#define	 SFF8636_FC_TRANS_MEDIA_M6		(1 << 3)
/* Multi-mode 50m */
#define	 SFF8636_FC_TRANS_MEDIA_M5		(1 << 2)
/* Multi-mode 50um */
#define	 SFF8636_FC_TRANS_MEDIA_OM3		(1 << 1)
/* Single Mode */
#define	 SFF8636_FC_TRANS_MEDIA_SM		(1 << 0)

/* Fibre Channel Speed - 138 */
#define	SFF8636_FC_SPEED_OFFSET		0x8A
#define	 SFF8636_FC_SPEED_1200_MBPS		(1 << 7)
#define	 SFF8636_FC_SPEED_800_MBPS		(1 << 6)
#define	 SFF8636_FC_SPEED_1600_MBPS		(1 << 5)
#define	 SFF8636_FC_SPEED_400_MBPS		(1 << 4)
#define	 SFF8636_FC_SPEED_200_MBPS		(1 << 2)
#define	 SFF8636_FC_SPEED_100_MBPS		(1 << 0)

/* Encoding - 139 */
/* Values are defined under SFF8024_ENCODING */
#define	SFF8636_ENCODING_OFFSET		0x8B
#define	 SFF8636_ENCODING_MANCHESTER	0x06
#define	 SFF8636_ENCODING_64B66B		0x05
#define	 SFF8636_ENCODING_SONET			0x04
#define	 SFF8636_ENCODING_NRZ			0x03
#define	 SFF8636_ENCODING_4B5B			0x02
#define	 SFF8636_ENCODING_8B10B			0x01
#define	 SFF8636_ENCODING_UNSPEC		0x00

/* BR, Nominal - 140 */
#define	SFF8636_BR_NOMINAL_OFFSET	0x8C

/* Extended RateSelect - 141 */
#define	SFF8636_EXT_RS_OFFSET		0x8D
#define	 SFF8636_EXT_RS_V1			(1 << 0)

/* Length (Standard SM Fiber)-km - 142 */
#define	SFF8636_SM_LEN_OFFSET		0x8E

/* Length (OM3)-Unit 2m - 143 */
#define	SFF8636_OM3_LEN_OFFSET		0x8F

/* Length (OM2)-Unit 1m - 144 */
#define	SFF8636_OM2_LEN_OFFSET		0x90

/* Length (OM1)-Unit 1m - 145 */
#define	SFF8636_OM1_LEN_OFFSET		0x91

/* Cable Assembly Length -Unit 1m - 146 */
#define	SFF8636_CBL_LEN_OFFSET		0x92

/* Device Technology - 147 */
#define	SFF8636_DEVICE_TECH_OFFSET	0x93
/* Transmitter Technology */
#define	 SFF8636_TRANS_TECH_MASK		0xF0
/* Copper cable, linear active equalizers */
#define	 SFF8636_TRANS_COPPER_LNR_EQUAL		(15 << 4)
/* Copper cable, near end limiting active equalizers */
#define	 SFF8636_TRANS_COPPER_NEAR_EQUAL	(14 << 4)
/* Copper cable, far end limiting active equalizers */
#define	 SFF8636_TRANS_COPPER_FAR_EQUAL		(13 << 4)
/* Copper cable, near & far end limiting active equalizers */
#define	 SFF8636_TRANS_COPPER_LNR_FAR_EQUAL	(12 << 4)
/* Copper cable, passive equalized */
#define	 SFF8636_TRANS_COPPER_PAS_EQUAL		(11 << 4)
/* Copper cable, unequalized */
#define	 SFF8636_TRANS_COPPER_PAS_UNEQUAL	(10 << 4)
/* 1490 nm DFB */
#define	 SFF8636_TRANS_1490_DFB			(9 << 4)
/* Others */
#define	 SFF8636_TRANS_OTHERS			(8 << 4)
/* 1550 nm EML */
#define	 SFF8636_TRANS_1550_EML			(7 << 4)
/* 1310 nm EML */
#define	 SFF8636_TRANS_1310_EML			(6 << 4)
/* 1550 nm DFB */
#define	 SFF8636_TRANS_1550_DFB			(5 << 4)
/* 1310 nm DFB */
#define	 SFF8636_TRANS_1310_DFB			(4 << 4)
/* 1310 nm FP */
#define	 SFF8636_TRANS_1310_FP			(3 << 4)
/* 1550 nm VCSEL */
#define	 SFF8636_TRANS_1550_VCSEL		(2 << 4)
/* 1310 nm VCSEL */
#define	 SFF8636_TRANS_1310_VCSEL		(1 << 4)
/* 850 nm VCSEL */
#define	 SFF8636_TRANS_850_VCSEL		(0 << 4)

 /* Active/No wavelength control */
#define	 SFF8636_DEV_TECH_ACTIVE_WAVE_LEN	(1 << 3)
/* Cooled transmitter */
#define	 SFF8636_DEV_TECH_COOL_TRANS		(1 << 2)
/* APD/Pin Detector */
#define	 SFF8636_DEV_TECH_APD_DETECTOR		(1 << 1)
/* Transmitter tunable */
#define	 SFF8636_DEV_TECH_TUNABLE		(1 << 0)

/* Vendor Name - 148-163 */
#define	 SFF8636_VENDOR_NAME_START_OFFSET	0x94
#define	 SFF8636_VENDOR_NAME_END_OFFSET		0xA3

/* Extended Module Codes - 164 */
#define	 SFF8636_EXT_MOD_CODE_OFFSET	0xA4
#define	  SFF8636_EXT_MOD_INFINIBAND_EDR	(1 << 4)
#define	  SFF8636_EXT_MOD_INFINIBAND_FDR	(1 << 3)
#define	  SFF8636_EXT_MOD_INFINIBAND_QDR	(1 << 2)
#define	  SFF8636_EXT_MOD_INFINIBAND_DDR	(1 << 1)
#define	  SFF8636_EXT_MOD_INFINIBAND_SDR	(1 << 0)

/* Vendor OUI - 165-167 */
#define	 SFF8636_VENDOR_OUI_OFFSET		0xA5
#define	  SFF8636_VENDOR_OUI_LEN		3

/* Vendor OUI - 165-167 */
#define	 SFF8636_VENDOR_PN_START_OFFSET		0xA8
#define	 SFF8636_VENDOR_PN_END_OFFSET		0xB7

/* Vendor Revision - 184-185 */
#define	 SFF8636_VENDOR_REV_START_OFFSET	0xB8
#define	 SFF8636_VENDOR_REV_END_OFFSET		0xB9

/* Wavelength - 186-187 */
#define	 SFF8636_WAVELEN_HIGH_BYTE_OFFSET	0xBA
#define	 SFF8636_WAVELEN_LOW_BYTE_OFFSET	0xBB

/* Wavelength  Tolerance- 188-189 */
#define	 SFF8636_WAVE_TOL_HIGH_BYTE_OFFSET	0xBC
#define	 SFF8636_WAVE_TOL_LOW_BYTE_OFFSET	0xBD

/* Max case temp - Other than 70 C - 190 */
#define	 SFF8636_MAXCASE_TEMP_OFFSET	0xBE

/* CC_BASE - 191 */
#define	 SFF8636_CC_BASE_OFFSET		0xBF

/* Option Values - 192-195 */
#define	 SFF8636_OPTION_1_OFFSET	0xC0
#define	 SFF8636_ETHERNET_UNSPECIFIED		0x00
#define	 SFF8636_ETHERNET_100G_AOC		0x01
#define	 SFF8636_ETHERNET_100G_SR4		0x02
#define	 SFF8636_ETHERNET_100G_LR4		0x03
#define	 SFF8636_ETHERNET_100G_ER4		0x04
#define	 SFF8636_ETHERNET_100G_SR10		0x05
#define	 SFF8636_ETHERNET_100G_CWDM4_FEC	0x06
#define	 SFF8636_ETHERNET_100G_PSM4		0x07
#define	 SFF8636_ETHERNET_100G_ACC		0x08
#define	 SFF8636_ETHERNET_100G_CWDM4_NO_FEC	0x09
#define	 SFF8636_ETHERNET_100G_RSVD1		0x0A
#define	 SFF8636_ETHERNET_100G_CR4		0x0B
#define	 SFF8636_ETHERNET_25G_CR_CA_S		0x0C
#define	 SFF8636_ETHERNET_25G_CR_CA_N		0x0D
#define	 SFF8636_ETHERNET_40G_ER4		0x10
#define	 SFF8636_ETHERNET_4X10_SR		0x11
#define	 SFF8636_ETHERNET_40G_PSM4		0x12
#define	 SFF8636_ETHERNET_G959_P1I1_2D1		0x13
#define	 SFF8636_ETHERNET_G959_P1S1_2D2		0x14
#define	 SFF8636_ETHERNET_G959_P1L1_2D2		0x15
#define	 SFF8636_ETHERNET_10GT_SFI		0x16
#define	 SFF8636_ETHERNET_100G_CLR4		0x17
#define	 SFF8636_ETHERNET_100G_AOC2		0x18
#define	 SFF8636_ETHERNET_100G_ACC2		0x19

#define	 SFF8636_OPTION_2_OFFSET	0xC1
/* Rx output amplitude */
#define	  SFF8636_O2_RX_OUTPUT_AMP	(1 << 0)
#define	 SFF8636_OPTION_3_OFFSET	0xC2
/* Rx Squelch Disable */
#define	  SFF8636_O3_RX_SQL_DSBL	(1 << 3)
/* Rx Output Disable capable */
#define	  SFF8636_O3_RX_OUTPUT_DSBL	(1 << 2)
/* Tx Squelch Disable */
#define	  SFF8636_O3_TX_SQL_DSBL	(1 << 1)
/* Tx Squelch Impl */
#define	  SFF8636_O3_TX_SQL_IMPL	(1 << 0)
#define	 SFF8636_OPTION_4_OFFSET	0xC3
/* Memory Page 02 present */
#define	  SFF8636_O4_PAGE_02_PRESENT	(1 << 7)
/* Memory Page 01 present */
#define	  SFF8636_O4_PAGE_01_PRESENT	(1 << 6)
/* Rate Select implemented */
#define	  SFF8636_O4_RATE_SELECT	(1 << 5)
/* Tx_DISABLE implemented */
#define	  SFF8636_O4_TX_DISABLE		(1 << 4)
/* Tx_FAULT implemented */
#define	  SFF8636_O4_TX_FAULT		(1 << 3)
/* Tx Squelch implemented */
#define	  SFF8636_O4_TX_SQUELCH		(1 << 2)
/* Tx Loss of Signal */
#define	  SFF8636_O4_TX_LOS		(1 << 1)

/* Vendor SN - 196-211 */
#define	 SFF8636_VENDOR_SN_START_OFFSET	0xC4
#define	 SFF8636_VENDOR_SN_END_OFFSET	0xD3

/* Vendor Date - 212-219 */
#define	 SFF8636_DATE_YEAR_OFFSET	0xD4
#define	  SFF8636_DATE_YEAR_LEN			2
#define	 SFF8636_DATE_MONTH_OFFSET	0xD6
#define	  SFF8636_DATE_MONTH_LEN		2
#define	 SFF8636_DATE_DAY_OFFSET	0xD8
#define	  SFF8636_DATE_DAY_LEN			2

/* Diagnostic Monitoring Type - 220 */
#define	 SFF8636_DIAG_TYPE_OFFSET	0xDC
#define	  SFF8636_RX_PWR_TYPE_MASK	0x8
#define	   SFF8636_RX_PWR_TYPE_AVG_PWR	(1 << 3)
#define	   SFF8636_RX_PWR_TYPE_OMA	(0 << 3)
#define	  SFF8636_TX_PWR_TYPE_MASK	0x4
#define	   SFF8636_TX_PWR_TYPE_AVG_PWR	(1 << 2)

/* Enhanced Options - 221 */
#define	 SFF8636_ENH_OPTIONS_OFFSET	0xDD
#define	  SFF8636_RATE_SELECT_EXT_SUPPORT	(1 << 3)
#define	  SFF8636_RATE_SELECT_APP_TABLE_SUPPORT	(1 << 2)

/* Check code - 223 */
#define	 SFF8636_CC_EXT_OFFSET		0xDF
#define	  SFF8636_CC_EXT_LEN		1

/*------------------------------------------------------------------------------
 *
 * Upper Memory Page 03h
 * Contains module thresholds, channel thresholds and masks,
 * and optional channel controls
 *
 * Offset - Page Num(3) * PageSize(0x80) + Page offset
 */

/* Module Thresholds (48 Bytes) 128-175 */
/* MSB at low address, LSB at high address */
#define	SFF8636_TEMP_HALRM		0x200
#define	SFF8636_TEMP_LALRM		0x202
#define	SFF8636_TEMP_HWARN		0x204
#define	SFF8636_TEMP_LWARN		0x206

#define	SFF8636_VCC_HALRM		0x210
#define	SFF8636_VCC_LALRM		0x212
#define	SFF8636_VCC_HWARN		0x214
#define	SFF8636_VCC_LWARN		0x216

#define	SFF8636_RX_PWR_HALRM		0x230
#define	SFF8636_RX_PWR_LALRM		0x232
#define	SFF8636_RX_PWR_HWARN		0x234
#define	SFF8636_RX_PWR_LWARN		0x236

#define	SFF8636_TX_BIAS_HALRM		0x238
#define	SFF8636_TX_BIAS_LALRM		0x23A
#define	SFF8636_TX_BIAS_HWARN		0x23C
#define	SFF8636_TX_BIAS_LWARN		0x23E

#define	SFF8636_TX_PWR_HALRM		0x240
#define	SFF8636_TX_PWR_LALRM		0x242
#define	SFF8636_TX_PWR_HWARN		0x244
#define	SFF8636_TX_PWR_LWARN		0x246

#define	ETH_MODULE_SFF_8636_MAX_LEN	640
#define	ETH_MODULE_SFF_8436_MAX_LEN	640

#endif /* QSFP_H__ */
