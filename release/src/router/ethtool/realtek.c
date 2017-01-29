/* Copyright 2001 Sun Microsystems (thockin@sun.com) */
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum chip_type {
	RTL8139 = 1,
	RTL8139_K,
	RTL8139A,
	RTL8139A_G,
	RTL8139B,
	RTL8130,
	RTL8139C,
	RTL8100,
	RTL8100B_8139D,
	RTL8139Cp,
	RTL8101,

	/* chips not handled by 8139too/8139cp module */
	RTL_GIGA_MAC_VER_01,
	RTL_GIGA_MAC_VER_02,
	RTL_GIGA_MAC_VER_03,
	RTL_GIGA_MAC_VER_04,
	RTL_GIGA_MAC_VER_05,
	RTL_GIGA_MAC_VER_06,
	RTL_GIGA_MAC_VER_07,
	RTL_GIGA_MAC_VER_08,
	RTL_GIGA_MAC_VER_09,
	RTL_GIGA_MAC_VER_10,
	RTL_GIGA_MAC_VER_11,
	RTL_GIGA_MAC_VER_12,
	RTL_GIGA_MAC_VER_13,
	RTL_GIGA_MAC_VER_14,
	RTL_GIGA_MAC_VER_15,
	RTL_GIGA_MAC_VER_16,
	RTL_GIGA_MAC_VER_17,
	RTL_GIGA_MAC_VER_18,
	RTL_GIGA_MAC_VER_19,
	RTL_GIGA_MAC_VER_20,
	RTL_GIGA_MAC_VER_21,
	RTL_GIGA_MAC_VER_22,
	RTL_GIGA_MAC_VER_23,
	RTL_GIGA_MAC_VER_24,
	RTL_GIGA_MAC_VER_25,
	RTL_GIGA_MAC_VER_26,
	RTL_GIGA_MAC_VER_27,
	RTL_GIGA_MAC_VER_28,
	RTL_GIGA_MAC_VER_29,
	RTL_GIGA_MAC_VER_30,
	RTL_GIGA_MAC_VER_31,
	RTL_GIGA_MAC_VER_32,
	RTL_GIGA_MAC_VER_33,
	RTL_GIGA_MAC_VER_34,
	RTL_GIGA_MAC_VER_35,
	RTL_GIGA_MAC_VER_36,
	RTL_GIGA_MAC_VER_37,
	RTL_GIGA_MAC_VER_38,
	RTL_GIGA_MAC_VER_39,
	RTL_GIGA_MAC_VER_40,
	RTL_GIGA_MAC_VER_41,
	RTL_GIGA_MAC_VER_42,
	RTL_GIGA_MAC_VER_43,
	RTL_GIGA_MAC_VER_44,
};

static const char * const chip_names[] = {
	[RTL8139] =		"8139",
	[RTL8139_K] =		"8139-K",
	[RTL8139A] =		"8139A",
	[RTL8139A_G] =		"8139A-G",
	[RTL8139B] =		"8139B",
	[RTL8130] =		"8130",
	[RTL8139C] =		"8139C",
	[RTL8100] =		"8100",
	[RTL8100B_8139D] =	"8100B/8139D",
	[RTL8139Cp] =		"8139C+",
	[RTL8101] =		"8101",

	/* chips not handled by 8139too/8139cp module */
	[RTL_GIGA_MAC_VER_01] = "8169",
	[RTL_GIGA_MAC_VER_02] = "8169s",
	[RTL_GIGA_MAC_VER_03] = "8110s",
	[RTL_GIGA_MAC_VER_04] = "8169sb/8110sb",
	[RTL_GIGA_MAC_VER_05] = "8169sc/8110sc",
	[RTL_GIGA_MAC_VER_06] = "8169sc/8110sc",
	[RTL_GIGA_MAC_VER_07] = "8102e",
	[RTL_GIGA_MAC_VER_08] = "8102e",
	[RTL_GIGA_MAC_VER_09] = "8102e",
	[RTL_GIGA_MAC_VER_10] = "8101e",
	[RTL_GIGA_MAC_VER_11] = "8168b/8111b",
	[RTL_GIGA_MAC_VER_12] = "8168b/8111b",
	[RTL_GIGA_MAC_VER_13] = "8101e",
	[RTL_GIGA_MAC_VER_14] = "8100e",
	[RTL_GIGA_MAC_VER_15] = "8100e",
	[RTL_GIGA_MAC_VER_16] = "8101e",
	[RTL_GIGA_MAC_VER_17] = "8168b/8111b",
	[RTL_GIGA_MAC_VER_18] = "8168cp/8111cp",
	[RTL_GIGA_MAC_VER_19] = "8168c/8111c",
	[RTL_GIGA_MAC_VER_20] = "8168c/8111c",
	[RTL_GIGA_MAC_VER_21] = "8168c/8111c",
	[RTL_GIGA_MAC_VER_22] = "8168c/8111c",
	[RTL_GIGA_MAC_VER_23] = "8168cp/8111cp",
	[RTL_GIGA_MAC_VER_24] = "8168cp/8111cp",
	[RTL_GIGA_MAC_VER_25] = "8168d/8111d",
	[RTL_GIGA_MAC_VER_26] = "8168d/8111d",
	[RTL_GIGA_MAC_VER_27] = "8168dp/8111dp",
	[RTL_GIGA_MAC_VER_28] = "8168dp/8111dp",
	[RTL_GIGA_MAC_VER_29] = "8105e",
	[RTL_GIGA_MAC_VER_30] = "8105e",
	[RTL_GIGA_MAC_VER_31] = "8168dp/8111dp",
	[RTL_GIGA_MAC_VER_32] = "8168e/8111e",
	[RTL_GIGA_MAC_VER_33] = "8168e/8111e",
	[RTL_GIGA_MAC_VER_34] = "8168evl/8111evl",
	[RTL_GIGA_MAC_VER_35] = "8168f/8111f",
	[RTL_GIGA_MAC_VER_36] = "8168f/8111f",
	[RTL_GIGA_MAC_VER_37] = "8402",
	[RTL_GIGA_MAC_VER_38] = "8411",
	[RTL_GIGA_MAC_VER_39] = "8106e",
	[RTL_GIGA_MAC_VER_40] = "8168g/8111g",
	[RTL_GIGA_MAC_VER_41] = "8168g/8111g",
	[RTL_GIGA_MAC_VER_42] = "8168g/8111g",
	[RTL_GIGA_MAC_VER_43] = "8106e",
	[RTL_GIGA_MAC_VER_44] = "8411",
};

static struct chip_info {
	u32 id_mask;
	u32 id_val;
	int mac_version;
} rtl_info_tbl[] = {
	{ 0xfcc00000, 0x40000000,	RTL8139 },
	{ 0xfcc00000, 0x60000000,	RTL8139_K },
	{ 0xfcc00000, 0x70000000,	RTL8139A },
	{ 0xfcc00000, 0x70800000,	RTL8139A_G },
	{ 0xfcc00000, 0x78000000,	RTL8139B },
	{ 0xfcc00000, 0x7c000000,	RTL8130 },
	{ 0xfcc00000, 0x74000000,	RTL8139C },
	{ 0xfcc00000, 0x78800000,	RTL8100 },
	{ 0xfcc00000, 0x74400000,	RTL8100B_8139D },
	{ 0xfcc00000, 0x74800000,	RTL8139Cp },
	{ 0xfcc00000, 0x74c00000,	RTL8101 },

	/* chips not handled by 8139too/8139cp module */
	/* 8168G family. */
	{ 0x7cf00000, 0x5c800000,	RTL_GIGA_MAC_VER_44 },
	{ 0x7cf00000, 0x50900000,	RTL_GIGA_MAC_VER_42 },
	{ 0x7cf00000, 0x4c100000,	RTL_GIGA_MAC_VER_41 },
	{ 0x7cf00000, 0x4c000000,	RTL_GIGA_MAC_VER_40 },

	/* 8168F family. */
	{ 0x7c800000, 0x48800000,	RTL_GIGA_MAC_VER_38 },
	{ 0x7cf00000, 0x48100000,	RTL_GIGA_MAC_VER_36 },
	{ 0x7cf00000, 0x48000000,	RTL_GIGA_MAC_VER_35 },

	/* 8168E family. */
	{ 0x7c800000, 0x2c800000,	RTL_GIGA_MAC_VER_34 },
	{ 0x7cf00000, 0x2c200000,	RTL_GIGA_MAC_VER_33 },
	{ 0x7cf00000, 0x2c100000,	RTL_GIGA_MAC_VER_32 },
	{ 0x7c800000, 0x2c000000,	RTL_GIGA_MAC_VER_33 },

	/* 8168D family. */
	{ 0x7cf00000, 0x28300000,	RTL_GIGA_MAC_VER_26 },
	{ 0x7cf00000, 0x28100000,	RTL_GIGA_MAC_VER_25 },
	{ 0x7c800000, 0x28000000,	RTL_GIGA_MAC_VER_26 },

	/* 8168DP family. */
	{ 0x7cf00000, 0x28800000,	RTL_GIGA_MAC_VER_27 },
	{ 0x7cf00000, 0x28a00000,	RTL_GIGA_MAC_VER_28 },
	{ 0x7cf00000, 0x28b00000,	RTL_GIGA_MAC_VER_31 },

	/* 8168C family. */
	{ 0x7cf00000, 0x3cb00000,	RTL_GIGA_MAC_VER_24 },
	{ 0x7cf00000, 0x3c900000,	RTL_GIGA_MAC_VER_23 },
	{ 0x7cf00000, 0x3c800000,	RTL_GIGA_MAC_VER_18 },
	{ 0x7c800000, 0x3c800000,	RTL_GIGA_MAC_VER_24 },
	{ 0x7cf00000, 0x3c000000,	RTL_GIGA_MAC_VER_19 },
	{ 0x7cf00000, 0x3c200000,	RTL_GIGA_MAC_VER_20 },
	{ 0x7cf00000, 0x3c300000,	RTL_GIGA_MAC_VER_21 },
	{ 0x7cf00000, 0x3c400000,	RTL_GIGA_MAC_VER_22 },
	{ 0x7c800000, 0x3c000000,	RTL_GIGA_MAC_VER_22 },

	/* 8168B family. */
	{ 0x7cf00000, 0x38000000,	RTL_GIGA_MAC_VER_12 },
	{ 0x7cf00000, 0x38500000,	RTL_GIGA_MAC_VER_17 },
	{ 0x7c800000, 0x38000000,	RTL_GIGA_MAC_VER_17 },
	{ 0x7c800000, 0x30000000,	RTL_GIGA_MAC_VER_11 },

	/* 8101 family. */
	{ 0x7cf00000, 0x44900000,	RTL_GIGA_MAC_VER_39 },
	{ 0x7c800000, 0x44800000,	RTL_GIGA_MAC_VER_39 },
	{ 0x7c800000, 0x44000000,	RTL_GIGA_MAC_VER_37 },
	{ 0x7cf00000, 0x40b00000,	RTL_GIGA_MAC_VER_30 },
	{ 0x7cf00000, 0x40a00000,	RTL_GIGA_MAC_VER_30 },
	{ 0x7cf00000, 0x40900000,	RTL_GIGA_MAC_VER_29 },
	{ 0x7c800000, 0x40800000,	RTL_GIGA_MAC_VER_30 },
	{ 0x7cf00000, 0x34a00000,	RTL_GIGA_MAC_VER_09 },
	{ 0x7cf00000, 0x24a00000,	RTL_GIGA_MAC_VER_09 },
	{ 0x7cf00000, 0x34900000,	RTL_GIGA_MAC_VER_08 },
	{ 0x7cf00000, 0x24900000,	RTL_GIGA_MAC_VER_08 },
	{ 0x7cf00000, 0x34800000,	RTL_GIGA_MAC_VER_07 },
	{ 0x7cf00000, 0x24800000,	RTL_GIGA_MAC_VER_07 },
	{ 0x7cf00000, 0x34000000,	RTL_GIGA_MAC_VER_13 },
	{ 0x7cf00000, 0x34300000,	RTL_GIGA_MAC_VER_10 },
	{ 0x7cf00000, 0x34200000,	RTL_GIGA_MAC_VER_16 },
	{ 0x7c800000, 0x34800000,	RTL_GIGA_MAC_VER_09 },
	{ 0x7c800000, 0x24800000,	RTL_GIGA_MAC_VER_09 },
	{ 0x7c800000, 0x34000000,	RTL_GIGA_MAC_VER_16 },
	/* FIXME: where did these entries come from ? -- FR */
	{ 0xfc800000, 0x38800000,	RTL_GIGA_MAC_VER_15 },
	{ 0xfc800000, 0x30800000,	RTL_GIGA_MAC_VER_14 },

	/* 8110 family. */
	{ 0xfc800000, 0x98000000,	RTL_GIGA_MAC_VER_06 },
	{ 0xfc800000, 0x18000000,	RTL_GIGA_MAC_VER_05 },
	{ 0xfc800000, 0x10000000,	RTL_GIGA_MAC_VER_04 },
	{ 0xfc800000, 0x04000000,	RTL_GIGA_MAC_VER_03 },
	{ 0xfc800000, 0x00800000,	RTL_GIGA_MAC_VER_02 },
	{ 0xfc800000, 0x00000000,	RTL_GIGA_MAC_VER_01 },

	{ }
};

static void
print_intr_bits(u16 mask)
{
	fprintf(stdout,
		"      %s%s%s%s%s%s%s%s%s%s%s\n",
		mask & (1 << 15)	? "SERR " : "",
		mask & (1 << 14)	? "TimeOut " : "",
		mask & (1 << 8)		? "SWInt " : "",
		mask & (1 << 7)		? "TxNoBuf " : "",
		mask & (1 << 6)		? "RxFIFO " : "",
		mask & (1 << 5)		? "LinkChg " : "",
		mask & (1 << 4)		? "RxNoBuf " : "",
		mask & (1 << 3)		? "TxErr " : "",
		mask & (1 << 2)		? "TxOK " : "",
		mask & (1 << 1)		? "RxErr " : "",
		mask & (1 << 0)		? "RxOK " : "");
}

int
realtek_dump_regs(struct ethtool_drvinfo *info, struct ethtool_regs *regs)
{
	u32 *data = (u32 *) regs->data;
	u8 *data8 = (u8 *) regs->data;
	u32 v;
	struct chip_info *ci;
	unsigned int board_type;

	v = data[0x40 >> 2]; /* TxConfig */

	ci = &rtl_info_tbl[0];
	while (ci->mac_version) {
		if ((v & ci->id_mask) == ci->id_val)
			break;
		ci++;
	}
	board_type = ci->mac_version;
	if (!board_type) {
		fprintf(stderr, "Unknown RealTek chip (TxConfig: 0x%08x)\n", v);
		return 91;
	}

	fprintf(stdout,
		"RealTek RTL%s registers:\n"
		"--------------------------------------------------------\n",
		chip_names[board_type]);

	fprintf(stdout,
		"0x00: MAC Address                      %02x:%02x:%02x:%02x:%02x:%02x\n",
		data8[0x00],
		data8[0x01],
		data8[0x02],
		data8[0x03],
		data8[0x04],
		data8[0x05]);

	fprintf(stdout,
		"0x08: Multicast Address Filter     0x%08x 0x%08x\n",
		data[0x08 >> 2],
		data[0x0c >> 2]);

	if (board_type == RTL8139Cp || board_type >= RTL_GIGA_MAC_VER_01) {
	fprintf(stdout,
		"0x10: Dump Tally Counter Command   0x%08x 0x%08x\n",
		data[0x10 >> 2],
		data[0x14 >> 2]);

	fprintf(stdout,
		"0x20: Tx Normal Priority Ring Addr 0x%08x 0x%08x\n",
		data[0x20 >> 2],
		data[0x24 >> 2]);

	fprintf(stdout,
		"0x28: Tx High Priority Ring Addr   0x%08x 0x%08x\n",
		data[0x28 >> 2],
		data[0x2C >> 2]);
	} else {
	fprintf(stdout,
		"0x10: Transmit Status Desc 0                  0x%08x\n"
		"0x14: Transmit Status Desc 1                  0x%08x\n"
		"0x18: Transmit Status Desc 2                  0x%08x\n"
		"0x1C: Transmit Status Desc 3                  0x%08x\n",
		data[0x10 >> 2],
		data[0x14 >> 2],
		data[0x18 >> 2],
		data[0x1C >> 2]);
	fprintf(stdout,
		"0x20: Transmit Start Addr  0                  0x%08x\n"
		"0x24: Transmit Start Addr  1                  0x%08x\n"
		"0x28: Transmit Start Addr  2                  0x%08x\n"
		"0x2C: Transmit Start Addr  3                  0x%08x\n",
		data[0x20 >> 2],
		data[0x24 >> 2],
		data[0x28 >> 2],
		data[0x2C >> 2]);
	}

	if (board_type < RTL_GIGA_MAC_VER_11 ||
		board_type > RTL_GIGA_MAC_VER_17) {
	if (board_type >= RTL_GIGA_MAC_VER_01) {
	fprintf(stdout,
		"0x30: Flash memory read/write                 0x%08x\n",
		data[0x30 >> 2]);
	} else {
	fprintf(stdout,
		"0x30: Rx buffer addr (C mode)                 0x%08x\n",
		data[0x30 >> 2]);
	}
	}

	v = data8[0x36];
	fprintf(stdout,
		"0x34: Early Rx Byte Count                       %8u\n"
		"0x36: Early Rx Status                               0x%02x\n",
		data[0x34 >> 2] & 0xffff,
		v);

	if (v & 0xf) {
	fprintf(stdout,
		"      %s%s%s%s\n",
		v & (1 << 3) ? "ERxGood " : "",
		v & (1 << 2) ? "ERxBad " : "",
		v & (1 << 1) ? "ERxOverWrite " : "",
		v & (1 << 0) ? "ERxOK " : "");
	}

	v = data8[0x37];
	fprintf(stdout,
		"0x37: Command                                       0x%02x\n"
		"      Rx %s, Tx %s%s\n",
		data8[0x37],
		v & (1 << 3) ? "on" : "off",
		v & (1 << 2) ? "on" : "off",
		v & (1 << 4) ? ", RESET" : "");

	if (board_type < RTL_GIGA_MAC_VER_01) {
	fprintf(stdout,
		"0x38: Current Address of Packet Read (C mode)     0x%04x\n"
		"0x3A: Current Rx buffer address (C mode)          0x%04x\n",
		data[0x38 >> 2] & 0xffff,
		data[0x38 >> 2] >> 16);
	}

	fprintf(stdout,
		"0x3C: Interrupt Mask                              0x%04x\n",
		data[0x3c >> 2] & 0xffff);
	print_intr_bits(data[0x3c >> 2] & 0xffff);
	fprintf(stdout,
		"0x3E: Interrupt Status                            0x%04x\n",
		data[0x3c >> 2] >> 16);
	print_intr_bits(data[0x3c >> 2] >> 16);

	fprintf(stdout,
		"0x40: Tx Configuration                        0x%08x\n"
		"0x44: Rx Configuration                        0x%08x\n"
		"0x48: Timer count                             0x%08x\n"
		"0x4C: Missed packet counter                     0x%06x\n",
		data[0x40 >> 2],
		data[0x44 >> 2],
		data[0x48 >> 2],
		data[0x4C >> 2] & 0xffffff);

	fprintf(stdout,
		"0x50: EEPROM Command                                0x%02x\n"
		"0x51: Config 0                                      0x%02x\n"
		"0x52: Config 1                                      0x%02x\n",
		data8[0x50],
		data8[0x51],
		data8[0x52]);

	if (board_type >= RTL_GIGA_MAC_VER_01) {
	fprintf(stdout,
		"0x53: Config 2                                      0x%02x\n"
		"0x54: Config 3                                      0x%02x\n"
		"0x55: Config 4                                      0x%02x\n"
		"0x56: Config 5                                      0x%02x\n",
		data8[0x53],
		data8[0x54],
		data8[0x55],
		data8[0x56]);
	fprintf(stdout,
		"0x58: Timer interrupt                         0x%08x\n",
		data[0x58 >> 2]);
	}
	else {
	if (board_type >= RTL8139A) {
	fprintf(stdout,
		"0x54: Timer interrupt                         0x%08x\n",
		data[0x54 >> 2]);
	}
	fprintf(stdout,
		"0x58: Media status                                  0x%02x\n",
		data8[0x58]);
	if (board_type >= RTL8139A) {
	fprintf(stdout,
		"0x59: Config 3                                      0x%02x\n",
		data8[0x59]);
	}
	if (board_type >= RTL8139B) {
	fprintf(stdout,
		"0x5A: Config 4                                      0x%02x\n",
		data8[0x5A]);
	}
	}

	fprintf(stdout,
		"0x5C: Multiple Interrupt Select                   0x%04x\n",
		data[0x5c >> 2] & 0xffff);

	if (board_type >= RTL_GIGA_MAC_VER_01) {
	fprintf(stdout,
		"0x60: PHY access                              0x%08x\n",
		data[0x60 >> 2]);

	if (board_type < RTL_GIGA_MAC_VER_11 ||
		board_type > RTL_GIGA_MAC_VER_17) {
	fprintf(stdout,
		"0x64: TBI control and status                  0x%08x\n",
		data[0x64 >> 2]);
	fprintf(stdout,
		"0x68: TBI Autonegotiation advertisement (ANAR)    0x%04x\n"
		"0x6A: TBI Link partner ability (LPAR)             0x%04x\n",
		data[0x68 >> 2] & 0xffff,
		data[0x68 >> 2] >> 16);
	}

	fprintf(stdout,
		"0x6C: PHY status                                    0x%02x\n",
		data8[0x6C]);

	fprintf(stdout,
		"0x84: PM wakeup frame 0            0x%08x 0x%08x\n"
		"0x8C: PM wakeup frame 1            0x%08x 0x%08x\n",
		data[0x84 >> 2],
		data[0x88 >> 2],
		data[0x8C >> 2],
		data[0x90 >> 2]);

	fprintf(stdout,
		"0x94: PM wakeup frame 2 (low)      0x%08x 0x%08x\n"
		"0x9C: PM wakeup frame 2 (high)     0x%08x 0x%08x\n",
		data[0x94 >> 2],
		data[0x98 >> 2],
		data[0x9C >> 2],
		data[0xA0 >> 2]);

	fprintf(stdout,
		"0xA4: PM wakeup frame 3 (low)      0x%08x 0x%08x\n"
		"0xAC: PM wakeup frame 3 (high)     0x%08x 0x%08x\n",
		data[0xA4 >> 2],
		data[0xA8 >> 2],
		data[0xAC >> 2],
		data[0xB0 >> 2]);

	fprintf(stdout,
		"0xB4: PM wakeup frame 4 (low)      0x%08x 0x%08x\n"
		"0xBC: PM wakeup frame 4 (high)     0x%08x 0x%08x\n",
		data[0xB4 >> 2],
		data[0xB8 >> 2],
		data[0xBC >> 2],
		data[0xC0 >> 2]);

	fprintf(stdout,
		"0xC4: Wakeup frame 0 CRC                          0x%04x\n"
		"0xC6: Wakeup frame 1 CRC                          0x%04x\n"
		"0xC8: Wakeup frame 2 CRC                          0x%04x\n"
		"0xCA: Wakeup frame 3 CRC                          0x%04x\n"
		"0xCC: Wakeup frame 4 CRC                          0x%04x\n",
		data[0xC4 >> 2] & 0xffff,
		data[0xC4 >> 2] >> 16,
		data[0xC8 >> 2] & 0xffff,
		data[0xC8 >> 2] >> 16,
		data[0xCC >> 2] & 0xffff);
	fprintf(stdout,
		"0xDA: RX packet maximum size                      0x%04x\n",
		data[0xD8 >> 2] >> 16);
	}
	else {
	fprintf(stdout,
		"0x5E: PCI revision id                               0x%02x\n",
		data8[0x5e]);
	fprintf(stdout,
		"0x60: Transmit Status of All Desc (C mode)        0x%04x\n"
		"0x62: MII Basic Mode Control Register             0x%04x\n",
		data[0x60 >> 2] & 0xffff,
		data[0x60 >> 2] >> 16);
	fprintf(stdout,
		"0x64: MII Basic Mode Status Register              0x%04x\n"
		"0x66: MII Autonegotiation Advertising             0x%04x\n",
		data[0x64 >> 2] & 0xffff,
		data[0x64 >> 2] >> 16);
	fprintf(stdout,
		"0x68: MII Link Partner Ability                    0x%04x\n"
		"0x6A: MII Expansion                               0x%04x\n",
		data[0x68 >> 2] & 0xffff,
		data[0x68 >> 2] >> 16);
	fprintf(stdout,
		"0x6C: MII Disconnect counter                      0x%04x\n"
		"0x6E: MII False carrier sense counter             0x%04x\n",
		data[0x6C >> 2] & 0xffff,
		data[0x6C >> 2] >> 16);
	fprintf(stdout,
		"0x70: MII Nway test                               0x%04x\n"
		"0x72: MII RX_ER counter                           0x%04x\n",
		data[0x70 >> 2] & 0xffff,
		data[0x70 >> 2] >> 16);
	fprintf(stdout,
		"0x74: MII CS configuration                        0x%04x\n",
		data[0x74 >> 2] & 0xffff);
	if (board_type >= RTL8139_K) {
	fprintf(stdout,
		"0x78: PHY parameter 1                         0x%08x\n"
		"0x7C: Twister parameter                       0x%08x\n",
		data[0x78 >> 2],
		data[0x7C >> 2]);
	if (board_type >= RTL8139A) {
	fprintf(stdout,
		"0x80: PHY parameter 2                               0x%02x\n",
		data8[0x80]);
	}
	}
	if (board_type == RTL8139Cp) {
	fprintf(stdout,
		"0x82: Low addr of a Tx Desc w/ Tx DMA OK          0x%04x\n",
		data[0x80 >> 2] >> 16);
	} else if (board_type == RTL8130) {
	fprintf(stdout,
		"0x82: MII register                                  0x%02x\n",
		data8[0x82]);
	}
	if (board_type >= RTL8139A) {
	fprintf(stdout,
		"0x84: PM CRC for wakeup frame 0                     0x%02x\n"
		"0x85: PM CRC for wakeup frame 1                     0x%02x\n"
		"0x86: PM CRC for wakeup frame 2                     0x%02x\n"
		"0x87: PM CRC for wakeup frame 3                     0x%02x\n"
		"0x88: PM CRC for wakeup frame 4                     0x%02x\n"
		"0x89: PM CRC for wakeup frame 5                     0x%02x\n"
		"0x8A: PM CRC for wakeup frame 6                     0x%02x\n"
		"0x8B: PM CRC for wakeup frame 7                     0x%02x\n",
		data8[0x84],
		data8[0x85],
		data8[0x86],
		data8[0x87],
		data8[0x88],
		data8[0x89],
		data8[0x8A],
		data8[0x8B]);
	fprintf(stdout,
		"0x8C: PM wakeup frame 0            0x%08x 0x%08x\n"
		"0x94: PM wakeup frame 1            0x%08x 0x%08x\n"
		"0x9C: PM wakeup frame 2            0x%08x 0x%08x\n"
		"0xA4: PM wakeup frame 3            0x%08x 0x%08x\n"
		"0xAC: PM wakeup frame 4            0x%08x 0x%08x\n"
		"0xB4: PM wakeup frame 5            0x%08x 0x%08x\n"
		"0xBC: PM wakeup frame 6            0x%08x 0x%08x\n"
		"0xC4: PM wakeup frame 7            0x%08x 0x%08x\n",
		data[0x8C >> 2],
		data[0x90 >> 2],
		data[0x94 >> 2],
		data[0x98 >> 2],
		data[0x9C >> 2],
		data[0xA0 >> 2],
		data[0xA4 >> 2],
		data[0xA8 >> 2],
		data[0xAC >> 2],
		data[0xB0 >> 2],
		data[0xB4 >> 2],
		data[0xB8 >> 2],
		data[0xBC >> 2],
		data[0xC0 >> 2],
		data[0xC4 >> 2],
		data[0xC8 >> 2]);
	fprintf(stdout,
		"0xCC: PM LSB CRC for wakeup frame 0                 0x%02x\n"
		"0xCD: PM LSB CRC for wakeup frame 1                 0x%02x\n"
		"0xCE: PM LSB CRC for wakeup frame 2                 0x%02x\n"
		"0xCF: PM LSB CRC for wakeup frame 3                 0x%02x\n"
		"0xD0: PM LSB CRC for wakeup frame 4                 0x%02x\n"
		"0xD1: PM LSB CRC for wakeup frame 5                 0x%02x\n"
		"0xD2: PM LSB CRC for wakeup frame 6                 0x%02x\n"
		"0xD3: PM LSB CRC for wakeup frame 7                 0x%02x\n",
		data8[0xCC],
		data8[0xCD],
		data8[0xCE],
		data8[0xCF],
		data8[0xD0],
		data8[0xD1],
		data8[0xD2],
		data8[0xD3]);
	}
	if (board_type >= RTL8139B) {
	if (board_type != RTL8100 && board_type != RTL8100B_8139D &&
	    board_type != RTL8101)
	fprintf(stdout,
		"0xD4: Flash memory read/write                 0x%08x\n",
		data[0xD4 >> 2]);
	if (board_type != RTL8130)
	fprintf(stdout,
		"0xD8: Config 5                                      0x%02x\n",
		data8[0xD8]);
	}
	}

	if (board_type == RTL8139Cp || board_type >= RTL_GIGA_MAC_VER_01) {
	v = data[0xE0 >> 2] & 0xffff;
	fprintf(stdout,
		"0xE0: C+ Command                                  0x%04x\n",
		v);
	if (v & (1 << 9))
		fprintf(stdout, "      Big-endian mode\n");
	if (v & (1 << 8))
		fprintf(stdout, "      Home LAN enable\n");
	if (v & (1 << 6))
		fprintf(stdout, "      VLAN de-tagging\n");
	if (v & (1 << 5))
		fprintf(stdout, "      RX checksumming\n");
	if (v & (1 << 4))
		fprintf(stdout, "      PCI 64-bit DAC\n");
	if (v & (1 << 3))
		fprintf(stdout, "      PCI Multiple RW\n");

	v = data[0xe0 >> 2] >> 16;
	fprintf(stdout,
		"0xE2: Interrupt Mitigation                        0x%04x\n"
		"      TxTimer:       %u\n"
		"      TxPackets:     %u\n"
		"      RxTimer:       %u\n"
		"      RxPackets:     %u\n",
		v,
		v >> 12,
		(v >> 8) & 0xf,
		(v >> 4) & 0xf,
		v & 0xf);

	fprintf(stdout,
		"0xE4: Rx Ring Addr                 0x%08x 0x%08x\n",
		data[0xE4 >> 2],
		data[0xE8 >> 2]);

	fprintf(stdout,
		"0xEC: Early Tx threshold                            0x%02x\n",
		data8[0xEC]);

	if (board_type == RTL8139Cp) {
	fprintf(stdout,
		"0xFC: External MII register                   0x%08x\n",
		data[0xFC >> 2]);
	} else if (board_type >= RTL_GIGA_MAC_VER_01 &&
		(board_type < RTL_GIGA_MAC_VER_11 ||
		board_type > RTL_GIGA_MAC_VER_17)) {
	fprintf(stdout,
		"0xF0: Func Event                              0x%08x\n"
		"0xF4: Func Event Mask                         0x%08x\n"
		"0xF8: Func Preset State                       0x%08x\n"
		"0xFC: Func Force Event                        0x%08x\n",
		data[0xF0 >> 2],
		data[0xF4 >> 2],
		data[0xF8 >> 2],
		data[0xFC >> 2]);
	}
	}

	return 0;
}
