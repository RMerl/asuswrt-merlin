/*
 * Copyright 1998-2008 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2008 S3 Graphics, Inc. All Rights Reserved.

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTIES OR REPRESENTATIONS; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/via-core.h>
#include "global.h"

static struct pll_map pll_value[] = {
	{25175000,
		{99, 7, 3},
		{85, 3, 4},	/* ignoring bit difference: 0x00008000 */
		{141, 5, 4},
		{141, 5, 4} },
	{29581000,
		{33, 4, 2},
		{66, 2, 4},	/* ignoring bit difference: 0x00808000 */
		{166, 5, 4},	/* ignoring bit difference: 0x00008000 */
		{165, 5, 4} },
	{26880000,
		{15, 4, 1},
		{30, 2, 3},	/* ignoring bit difference: 0x00808000 */
		{150, 5, 4},
		{150, 5, 4} },
	{31500000,
		{53, 3, 3},	/* ignoring bit difference: 0x00008000 */
		{141, 4, 4},	/* ignoring bit difference: 0x00008000 */
		{176, 5, 4},
		{176, 5, 4} },
	{31728000,
		{31, 7, 1},
		{177, 5, 4},	/* ignoring bit difference: 0x00008000 */
		{177, 5, 4},
		{142, 4, 4} },
	{32688000,
		{73, 4, 3},
		{146, 4, 4},	/* ignoring bit difference: 0x00008000 */
		{183, 5, 4},
		{146, 4, 4} },
	{36000000,
		{101, 5, 3},	/* ignoring bit difference: 0x00008000 */
		{161, 4, 4},	/* ignoring bit difference: 0x00008000 */
		{202, 5, 4},
		{161, 4, 4} },
	{40000000,
		{89, 4, 3},
		{89, 4, 3},	/* ignoring bit difference: 0x00008000 */
		{112, 5, 3},
		{112, 5, 3} },
	{41291000,
		{23, 4, 1},
		{69, 3, 3},	/* ignoring bit difference: 0x00008000 */
		{115, 5, 3},
		{115, 5, 3} },
	{43163000,
		{121, 5, 3},
		{121, 5, 3},	/* ignoring bit difference: 0x00008000 */
		{121, 5, 3},
		{121, 5, 3} },
	{45250000,
		{127, 5, 3},
		{127, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{127, 5, 3},
		{127, 5, 3} },
	{46000000,
		{90, 7, 2},
		{103, 4, 3},	/* ignoring bit difference: 0x00008000 */
		{129, 5, 3},
		{103, 4, 3} },
	{46996000,
		{105, 4, 3},	/* ignoring bit difference: 0x00008000 */
		{131, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{131, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{105, 4, 3} },
	{48000000,
		{67, 20, 0},
		{134, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{134, 5, 3},
		{134, 5, 3} },
	{48875000,
		{99, 29, 0},
		{82, 3, 3},	/* ignoring bit difference: 0x00808000 */
		{82, 3, 3},	/* ignoring bit difference: 0x00808000 */
		{137, 5, 3} },
	{49500000,
		{83, 6, 2},
		{83, 3, 3},	/* ignoring bit difference: 0x00008000 */
		{138, 5, 3},
		{83, 3, 3} },
	{52406000,
		{117, 4, 3},
		{117, 4, 3},	/* ignoring bit difference: 0x00008000 */
		{117, 4, 3},
		{88, 3, 3} },
	{52977000,
		{37, 5, 1},
		{148, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{148, 5, 3},
		{148, 5, 3} },
	{56250000,
		{55, 7, 1},	/* ignoring bit difference: 0x00008000 */
		{126, 4, 3},	/* ignoring bit difference: 0x00008000 */
		{157, 5, 3},
		{157, 5, 3} },
	{57275000,
		{0, 0, 0},
		{2, 2, 0},
		{2, 2, 0},
		{157, 5, 3} },	/* ignoring bit difference: 0x00808000 */
	{60466000,
		{76, 9, 1},
		{169, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{169, 5, 3},	/* FIXED: old = {72, 2, 3} */
		{169, 5, 3} },
	{61500000,
		{86, 20, 0},
		{172, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{172, 5, 3},
		{172, 5, 3} },
	{65000000,
		{109, 6, 2},	/* ignoring bit difference: 0x00008000 */
		{109, 3, 3},	/* ignoring bit difference: 0x00008000 */
		{109, 3, 3},
		{109, 3, 3} },
	{65178000,
		{91, 5, 2},
		{182, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{109, 3, 3},
		{182, 5, 3} },
	{66750000,
		{75, 4, 2},
		{150, 4, 3},	/* ignoring bit difference: 0x00808000 */
		{150, 4, 3},
		{112, 3, 3} },
	{68179000,
		{19, 4, 0},
		{114, 3, 3},	/* ignoring bit difference: 0x00008000 */
		{190, 5, 3},
		{191, 5, 3} },
	{69924000,
		{83, 17, 0},
		{195, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{195, 5, 3},
		{195, 5, 3} },
	{70159000,
		{98, 20, 0},
		{196, 5, 3},	/* ignoring bit difference: 0x00808000 */
		{196, 5, 3},
		{195, 5, 3} },
	{72000000,
		{121, 24, 0},
		{161, 4, 3},	/* ignoring bit difference: 0x00808000 */
		{161, 4, 3},
		{161, 4, 3} },
	{78750000,
		{33, 3, 1},
		{66, 3, 2},	/* ignoring bit difference: 0x00008000 */
		{110, 5, 2},
		{110, 5, 2} },
	{80136000,
		{28, 5, 0},
		{68, 3, 2},	/* ignoring bit difference: 0x00008000 */
		{112, 5, 2},
		{112, 5, 2} },
	{83375000,
		{93, 2, 3},
		{93, 4, 2},	/* ignoring bit difference: 0x00800000 */
		{93, 4, 2},	/* ignoring bit difference: 0x00800000 */
		{117, 5, 2} },
	{83950000,
		{41, 7, 0},
		{117, 5, 2},	/* ignoring bit difference: 0x00008000 */
		{117, 5, 2},
		{117, 5, 2} },
	{84750000,
		{118, 5, 2},
		{118, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{118, 5, 2},
		{118, 5, 2} },
	{85860000,
		{84, 7, 1},
		{120, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{120, 5, 2},
		{118, 5, 2} },
	{88750000,
		{31, 5, 0},
		{124, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{174, 7, 2},	/* ignoring bit difference: 0x00808000 */
		{124, 5, 2} },
	{94500000,
		{33, 5, 0},
		{132, 5, 2},	/* ignoring bit difference: 0x00008000 */
		{132, 5, 2},
		{132, 5, 2} },
	{97750000,
		{82, 6, 1},
		{137, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{137, 5, 2},
		{137, 5, 2} },
	{101000000,
		{127, 9, 1},
		{141, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{141, 5, 2},
		{141, 5, 2} },
	{106500000,
		{119, 4, 2},
		{119, 4, 2},	/* ignoring bit difference: 0x00808000 */
		{119, 4, 2},
		{149, 5, 2} },
	{108000000,
		{121, 4, 2},
		{121, 4, 2},	/* ignoring bit difference: 0x00808000 */
		{151, 5, 2},
		{151, 5, 2} },
	{113309000,
		{95, 12, 0},
		{95, 3, 2},	/* ignoring bit difference: 0x00808000 */
		{95, 3, 2},
		{159, 5, 2} },
	{118840000,
		{83, 5, 1},
		{166, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{166, 5, 2},
		{166, 5, 2} },
	{119000000,
		{108, 13, 0},
		{133, 4, 2},	/* ignoring bit difference: 0x00808000 */
		{133, 4, 2},
		{167, 5, 2} },
	{121750000,
		{85, 5, 1},
		{170, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{68, 2, 2},
		{0, 0, 0} },
	{125104000,
		{53, 6, 0},	/* ignoring bit difference: 0x00008000 */
		{106, 3, 2},	/* ignoring bit difference: 0x00008000 */
		{175, 5, 2},
		{0, 0, 0} },
	{135000000,
		{94, 5, 1},
		{28, 3, 0},	/* ignoring bit difference: 0x00804000 */
		{151, 4, 2},
		{189, 5, 2} },
	{136700000,
		{115, 12, 0},
		{191, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{191, 5, 2},
		{191, 5, 2} },
	{138400000,
		{87, 9, 0},
		{116, 3, 2},	/* ignoring bit difference: 0x00808000 */
		{116, 3, 2},
		{194, 5, 2} },
	{146760000,
		{103, 5, 1},
		{206, 5, 2},	/* ignoring bit difference: 0x00808000 */
		{206, 5, 2},
		{206, 5, 2} },
	{153920000,
		{86, 8, 0},
		{86, 4, 1},	/* ignoring bit difference: 0x00808000 */
		{86, 4, 1},
		{86, 4, 1} },	/* FIXED: old = {84, 2, 1} */
	{156000000,
		{109, 5, 1},
		{109, 5, 1},	/* ignoring bit difference: 0x00808000 */
		{109, 5, 1},
		{108, 5, 1} },
	{157500000,
		{55, 5, 0},	/* ignoring bit difference: 0x00008000 */
		{22, 2, 0},	/* ignoring bit difference: 0x00802000 */
		{110, 5, 1},
		{110, 5, 1} },
	{162000000,
		{113, 5, 1},
		{113, 5, 1},	/* ignoring bit difference: 0x00808000 */
		{113, 5, 1},
		{113, 5, 1} },
	{187000000,
		{118, 9, 0},
		{131, 5, 1},	/* ignoring bit difference: 0x00808000 */
		{131, 5, 1},
		{131, 5, 1} },
	{193295000,
		{108, 8, 0},
		{81, 3, 1},	/* ignoring bit difference: 0x00808000 */
		{135, 5, 1},
		{135, 5, 1} },
	{202500000,
		{99, 7, 0},
		{85, 3, 1},	/* ignoring bit difference: 0x00808000 */
		{142, 5, 1},
		{142, 5, 1} },
	{204000000,
		{100, 7, 0},
		{143, 5, 1},	/* ignoring bit difference: 0x00808000 */
		{143, 5, 1},
		{143, 5, 1} },
	{218500000,
		{92, 6, 0},
		{153, 5, 1},	/* ignoring bit difference: 0x00808000 */
		{153, 5, 1},
		{153, 5, 1} },
	{234000000,
		{98, 6, 0},
		{98, 3, 1},	/* ignoring bit difference: 0x00008000 */
		{98, 3, 1},
		{164, 5, 1} },
	{267250000,
		{112, 6, 0},
		{112, 3, 1},	/* ignoring bit difference: 0x00808000 */
		{187, 5, 1},
		{187, 5, 1} },
	{297500000,
		{102, 5, 0},	/* ignoring bit difference: 0x00008000 */
		{166, 4, 1},	/* ignoring bit difference: 0x00008000 */
		{208, 5, 1},
		{208, 5, 1} },
	{74481000,
		{26, 5, 0},
		{125, 3, 3},	/* ignoring bit difference: 0x00808000 */
		{208, 5, 3},
		{209, 5, 3} },
	{172798000,
		{121, 5, 1},
		{121, 5, 1},	/* ignoring bit difference: 0x00808000 */
		{121, 5, 1},
		{121, 5, 1} },
	{122614000,
		{60, 7, 0},
		{137, 4, 2},	/* ignoring bit difference: 0x00808000 */
		{137, 4, 2},
		{172, 5, 2} },
	{74270000,
		{83, 8, 1},
		{208, 5, 3},
		{208, 5, 3},
		{0, 0, 0} },
	{148500000,
		{83, 8, 0},
		{208, 5, 2},
		{166, 4, 2},
		{208, 5, 2} }
};

static struct fifo_depth_select display_fifo_depth_reg = {
	/* IGA1 FIFO Depth_Select */
	{IGA1_FIFO_DEPTH_SELECT_REG_NUM, {{SR17, 0, 7} } },
	/* IGA2 FIFO Depth_Select */
	{IGA2_FIFO_DEPTH_SELECT_REG_NUM,
	 {{CR68, 4, 7}, {CR94, 7, 7}, {CR95, 7, 7} } }
};

static struct fifo_threshold_select fifo_threshold_select_reg = {
	/* IGA1 FIFO Threshold Select */
	{IGA1_FIFO_THRESHOLD_REG_NUM, {{SR16, 0, 5}, {SR16, 7, 7} } },
	/* IGA2 FIFO Threshold Select */
	{IGA2_FIFO_THRESHOLD_REG_NUM, {{CR68, 0, 3}, {CR95, 4, 6} } }
};

static struct fifo_high_threshold_select fifo_high_threshold_select_reg = {
	/* IGA1 FIFO High Threshold Select */
	{IGA1_FIFO_HIGH_THRESHOLD_REG_NUM, {{SR18, 0, 5}, {SR18, 7, 7} } },
	/* IGA2 FIFO High Threshold Select */
	{IGA2_FIFO_HIGH_THRESHOLD_REG_NUM, {{CR92, 0, 3}, {CR95, 0, 2} } }
};

static struct display_queue_expire_num display_queue_expire_num_reg = {
	/* IGA1 Display Queue Expire Num */
	{IGA1_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM, {{SR22, 0, 4} } },
	/* IGA2 Display Queue Expire Num */
	{IGA2_DISPLAY_QUEUE_EXPIRE_NUM_REG_NUM, {{CR94, 0, 6} } }
};

/* Definition Fetch Count Registers*/
static struct fetch_count fetch_count_reg = {
	/* IGA1 Fetch Count Register */
	{IGA1_FETCH_COUNT_REG_NUM, {{SR1C, 0, 7}, {SR1D, 0, 1} } },
	/* IGA2 Fetch Count Register */
	{IGA2_FETCH_COUNT_REG_NUM, {{CR65, 0, 7}, {CR67, 2, 3} } }
};

static struct iga1_crtc_timing iga1_crtc_reg = {
	/* IGA1 Horizontal Total */
	{IGA1_HOR_TOTAL_REG_NUM, {{CR00, 0, 7}, {CR36, 3, 3} } },
	/* IGA1 Horizontal Addressable Video */
	{IGA1_HOR_ADDR_REG_NUM, {{CR01, 0, 7} } },
	/* IGA1 Horizontal Blank Start */
	{IGA1_HOR_BLANK_START_REG_NUM, {{CR02, 0, 7} } },
	/* IGA1 Horizontal Blank End */
	{IGA1_HOR_BLANK_END_REG_NUM,
	 {{CR03, 0, 4}, {CR05, 7, 7}, {CR33, 5, 5} } },
	/* IGA1 Horizontal Sync Start */
	{IGA1_HOR_SYNC_START_REG_NUM, {{CR04, 0, 7}, {CR33, 4, 4} } },
	/* IGA1 Horizontal Sync End */
	{IGA1_HOR_SYNC_END_REG_NUM, {{CR05, 0, 4} } },
	/* IGA1 Vertical Total */
	{IGA1_VER_TOTAL_REG_NUM,
	 {{CR06, 0, 7}, {CR07, 0, 0}, {CR07, 5, 5}, {CR35, 0, 0} } },
	/* IGA1 Vertical Addressable Video */
	{IGA1_VER_ADDR_REG_NUM,
	 {{CR12, 0, 7}, {CR07, 1, 1}, {CR07, 6, 6}, {CR35, 2, 2} } },
	/* IGA1 Vertical Blank Start */
	{IGA1_VER_BLANK_START_REG_NUM,
	 {{CR15, 0, 7}, {CR07, 3, 3}, {CR09, 5, 5}, {CR35, 3, 3} } },
	/* IGA1 Vertical Blank End */
	{IGA1_VER_BLANK_END_REG_NUM, {{CR16, 0, 7} } },
	/* IGA1 Vertical Sync Start */
	{IGA1_VER_SYNC_START_REG_NUM,
	 {{CR10, 0, 7}, {CR07, 2, 2}, {CR07, 7, 7}, {CR35, 1, 1} } },
	/* IGA1 Vertical Sync End */
	{IGA1_VER_SYNC_END_REG_NUM, {{CR11, 0, 3} } }
};

static struct iga2_crtc_timing iga2_crtc_reg = {
	/* IGA2 Horizontal Total */
	{IGA2_HOR_TOTAL_REG_NUM, {{CR50, 0, 7}, {CR55, 0, 3} } },
	/* IGA2 Horizontal Addressable Video */
	{IGA2_HOR_ADDR_REG_NUM, {{CR51, 0, 7}, {CR55, 4, 6} } },
	/* IGA2 Horizontal Blank Start */
	{IGA2_HOR_BLANK_START_REG_NUM, {{CR52, 0, 7}, {CR54, 0, 2} } },
	/* IGA2 Horizontal Blank End */
	{IGA2_HOR_BLANK_END_REG_NUM,
	 {{CR53, 0, 7}, {CR54, 3, 5}, {CR5D, 6, 6} } },
	/* IGA2 Horizontal Sync Start */
	{IGA2_HOR_SYNC_START_REG_NUM,
	 {{CR56, 0, 7}, {CR54, 6, 7}, {CR5C, 7, 7}, {CR5D, 7, 7} } },
	/* IGA2 Horizontal Sync End */
	{IGA2_HOR_SYNC_END_REG_NUM, {{CR57, 0, 7}, {CR5C, 6, 6} } },
	/* IGA2 Vertical Total */
	{IGA2_VER_TOTAL_REG_NUM, {{CR58, 0, 7}, {CR5D, 0, 2} } },
	/* IGA2 Vertical Addressable Video */
	{IGA2_VER_ADDR_REG_NUM, {{CR59, 0, 7}, {CR5D, 3, 5} } },
	/* IGA2 Vertical Blank Start */
	{IGA2_VER_BLANK_START_REG_NUM, {{CR5A, 0, 7}, {CR5C, 0, 2} } },
	/* IGA2 Vertical Blank End */
	{IGA2_VER_BLANK_END_REG_NUM, {{CR5B, 0, 7}, {CR5C, 3, 5} } },
	/* IGA2 Vertical Sync Start */
	{IGA2_VER_SYNC_START_REG_NUM, {{CR5E, 0, 7}, {CR5F, 5, 7} } },
	/* IGA2 Vertical Sync End */
	{IGA2_VER_SYNC_END_REG_NUM, {{CR5F, 0, 4} } }
};

static struct rgbLUT palLUT_table[] = {
	/* {R,G,B} */
	/* Index 0x00~0x03 */
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0x2A}, {0x00, 0x2A, 0x00}, {0x00,
								     0x2A,
								     0x2A},
	/* Index 0x04~0x07 */
	{0x2A, 0x00, 0x00}, {0x2A, 0x00, 0x2A}, {0x2A, 0x15, 0x00}, {0x2A,
								     0x2A,
								     0x2A},
	/* Index 0x08~0x0B */
	{0x15, 0x15, 0x15}, {0x15, 0x15, 0x3F}, {0x15, 0x3F, 0x15}, {0x15,
								     0x3F,
								     0x3F},
	/* Index 0x0C~0x0F */
	{0x3F, 0x15, 0x15}, {0x3F, 0x15, 0x3F}, {0x3F, 0x3F, 0x15}, {0x3F,
								     0x3F,
								     0x3F},
	/* Index 0x10~0x13 */
	{0x00, 0x00, 0x00}, {0x05, 0x05, 0x05}, {0x08, 0x08, 0x08}, {0x0B,
								     0x0B,
								     0x0B},
	/* Index 0x14~0x17 */
	{0x0E, 0x0E, 0x0E}, {0x11, 0x11, 0x11}, {0x14, 0x14, 0x14}, {0x18,
								     0x18,
								     0x18},
	/* Index 0x18~0x1B */
	{0x1C, 0x1C, 0x1C}, {0x20, 0x20, 0x20}, {0x24, 0x24, 0x24}, {0x28,
								     0x28,
								     0x28},
	/* Index 0x1C~0x1F */
	{0x2D, 0x2D, 0x2D}, {0x32, 0x32, 0x32}, {0x38, 0x38, 0x38}, {0x3F,
								     0x3F,
								     0x3F},
	/* Index 0x20~0x23 */
	{0x00, 0x00, 0x3F}, {0x10, 0x00, 0x3F}, {0x1F, 0x00, 0x3F}, {0x2F,
								     0x00,
								     0x3F},
	/* Index 0x24~0x27 */
	{0x3F, 0x00, 0x3F}, {0x3F, 0x00, 0x2F}, {0x3F, 0x00, 0x1F}, {0x3F,
								     0x00,
								     0x10},
	/* Index 0x28~0x2B */
	{0x3F, 0x00, 0x00}, {0x3F, 0x10, 0x00}, {0x3F, 0x1F, 0x00}, {0x3F,
								     0x2F,
								     0x00},
	/* Index 0x2C~0x2F */
	{0x3F, 0x3F, 0x00}, {0x2F, 0x3F, 0x00}, {0x1F, 0x3F, 0x00}, {0x10,
								     0x3F,
								     0x00},
	/* Index 0x30~0x33 */
	{0x00, 0x3F, 0x00}, {0x00, 0x3F, 0x10}, {0x00, 0x3F, 0x1F}, {0x00,
								     0x3F,
								     0x2F},
	/* Index 0x34~0x37 */
	{0x00, 0x3F, 0x3F}, {0x00, 0x2F, 0x3F}, {0x00, 0x1F, 0x3F}, {0x00,
								     0x10,
								     0x3F},
	/* Index 0x38~0x3B */
	{0x1F, 0x1F, 0x3F}, {0x27, 0x1F, 0x3F}, {0x2F, 0x1F, 0x3F}, {0x37,
								     0x1F,
								     0x3F},
	/* Index 0x3C~0x3F */
	{0x3F, 0x1F, 0x3F}, {0x3F, 0x1F, 0x37}, {0x3F, 0x1F, 0x2F}, {0x3F,
								     0x1F,
								     0x27},
	/* Index 0x40~0x43 */
	{0x3F, 0x1F, 0x1F}, {0x3F, 0x27, 0x1F}, {0x3F, 0x2F, 0x1F}, {0x3F,
								     0x3F,
								     0x1F},
	/* Index 0x44~0x47 */
	{0x3F, 0x3F, 0x1F}, {0x37, 0x3F, 0x1F}, {0x2F, 0x3F, 0x1F}, {0x27,
								     0x3F,
								     0x1F},
	/* Index 0x48~0x4B */
	{0x1F, 0x3F, 0x1F}, {0x1F, 0x3F, 0x27}, {0x1F, 0x3F, 0x2F}, {0x1F,
								     0x3F,
								     0x37},
	/* Index 0x4C~0x4F */
	{0x1F, 0x3F, 0x3F}, {0x1F, 0x37, 0x3F}, {0x1F, 0x2F, 0x3F}, {0x1F,
								     0x27,
								     0x3F},
	/* Index 0x50~0x53 */
	{0x2D, 0x2D, 0x3F}, {0x31, 0x2D, 0x3F}, {0x36, 0x2D, 0x3F}, {0x3A,
								     0x2D,
								     0x3F},
	/* Index 0x54~0x57 */
	{0x3F, 0x2D, 0x3F}, {0x3F, 0x2D, 0x3A}, {0x3F, 0x2D, 0x36}, {0x3F,
								     0x2D,
								     0x31},
	/* Index 0x58~0x5B */
	{0x3F, 0x2D, 0x2D}, {0x3F, 0x31, 0x2D}, {0x3F, 0x36, 0x2D}, {0x3F,
								     0x3A,
								     0x2D},
	/* Index 0x5C~0x5F */
	{0x3F, 0x3F, 0x2D}, {0x3A, 0x3F, 0x2D}, {0x36, 0x3F, 0x2D}, {0x31,
								     0x3F,
								     0x2D},
	/* Index 0x60~0x63 */
	{0x2D, 0x3F, 0x2D}, {0x2D, 0x3F, 0x31}, {0x2D, 0x3F, 0x36}, {0x2D,
								     0x3F,
								     0x3A},
	/* Index 0x64~0x67 */
	{0x2D, 0x3F, 0x3F}, {0x2D, 0x3A, 0x3F}, {0x2D, 0x36, 0x3F}, {0x2D,
								     0x31,
								     0x3F},
	/* Index 0x68~0x6B */
	{0x00, 0x00, 0x1C}, {0x07, 0x00, 0x1C}, {0x0E, 0x00, 0x1C}, {0x15,
								     0x00,
								     0x1C},
	/* Index 0x6C~0x6F */
	{0x1C, 0x00, 0x1C}, {0x1C, 0x00, 0x15}, {0x1C, 0x00, 0x0E}, {0x1C,
								     0x00,
								     0x07},
	/* Index 0x70~0x73 */
	{0x1C, 0x00, 0x00}, {0x1C, 0x07, 0x00}, {0x1C, 0x0E, 0x00}, {0x1C,
								     0x15,
								     0x00},
	/* Index 0x74~0x77 */
	{0x1C, 0x1C, 0x00}, {0x15, 0x1C, 0x00}, {0x0E, 0x1C, 0x00}, {0x07,
								     0x1C,
								     0x00},
	/* Index 0x78~0x7B */
	{0x00, 0x1C, 0x00}, {0x00, 0x1C, 0x07}, {0x00, 0x1C, 0x0E}, {0x00,
								     0x1C,
								     0x15},
	/* Index 0x7C~0x7F */
	{0x00, 0x1C, 0x1C}, {0x00, 0x15, 0x1C}, {0x00, 0x0E, 0x1C}, {0x00,
								     0x07,
								     0x1C},
	/* Index 0x80~0x83 */
	{0x0E, 0x0E, 0x1C}, {0x11, 0x0E, 0x1C}, {0x15, 0x0E, 0x1C}, {0x18,
								     0x0E,
								     0x1C},
	/* Index 0x84~0x87 */
	{0x1C, 0x0E, 0x1C}, {0x1C, 0x0E, 0x18}, {0x1C, 0x0E, 0x15}, {0x1C,
								     0x0E,
								     0x11},
	/* Index 0x88~0x8B */
	{0x1C, 0x0E, 0x0E}, {0x1C, 0x11, 0x0E}, {0x1C, 0x15, 0x0E}, {0x1C,
								     0x18,
								     0x0E},
	/* Index 0x8C~0x8F */
	{0x1C, 0x1C, 0x0E}, {0x18, 0x1C, 0x0E}, {0x15, 0x1C, 0x0E}, {0x11,
								     0x1C,
								     0x0E},
	/* Index 0x90~0x93 */
	{0x0E, 0x1C, 0x0E}, {0x0E, 0x1C, 0x11}, {0x0E, 0x1C, 0x15}, {0x0E,
								     0x1C,
								     0x18},
	/* Index 0x94~0x97 */
	{0x0E, 0x1C, 0x1C}, {0x0E, 0x18, 0x1C}, {0x0E, 0x15, 0x1C}, {0x0E,
								     0x11,
								     0x1C},
	/* Index 0x98~0x9B */
	{0x14, 0x14, 0x1C}, {0x16, 0x14, 0x1C}, {0x18, 0x14, 0x1C}, {0x1A,
								     0x14,
								     0x1C},
	/* Index 0x9C~0x9F */
	{0x1C, 0x14, 0x1C}, {0x1C, 0x14, 0x1A}, {0x1C, 0x14, 0x18}, {0x1C,
								     0x14,
								     0x16},
	/* Index 0xA0~0xA3 */
	{0x1C, 0x14, 0x14}, {0x1C, 0x16, 0x14}, {0x1C, 0x18, 0x14}, {0x1C,
								     0x1A,
								     0x14},
	/* Index 0xA4~0xA7 */
	{0x1C, 0x1C, 0x14}, {0x1A, 0x1C, 0x14}, {0x18, 0x1C, 0x14}, {0x16,
								     0x1C,
								     0x14},
	/* Index 0xA8~0xAB */
	{0x14, 0x1C, 0x14}, {0x14, 0x1C, 0x16}, {0x14, 0x1C, 0x18}, {0x14,
								     0x1C,
								     0x1A},
	/* Index 0xAC~0xAF */
	{0x14, 0x1C, 0x1C}, {0x14, 0x1A, 0x1C}, {0x14, 0x18, 0x1C}, {0x14,
								     0x16,
								     0x1C},
	/* Index 0xB0~0xB3 */
	{0x00, 0x00, 0x10}, {0x04, 0x00, 0x10}, {0x08, 0x00, 0x10}, {0x0C,
								     0x00,
								     0x10},
	/* Index 0xB4~0xB7 */
	{0x10, 0x00, 0x10}, {0x10, 0x00, 0x0C}, {0x10, 0x00, 0x08}, {0x10,
								     0x00,
								     0x04},
	/* Index 0xB8~0xBB */
	{0x10, 0x00, 0x00}, {0x10, 0x04, 0x00}, {0x10, 0x08, 0x00}, {0x10,
								     0x0C,
								     0x00},
	/* Index 0xBC~0xBF */
	{0x10, 0x10, 0x00}, {0x0C, 0x10, 0x00}, {0x08, 0x10, 0x00}, {0x04,
								     0x10,
								     0x00},
	/* Index 0xC0~0xC3 */
	{0x00, 0x10, 0x00}, {0x00, 0x10, 0x04}, {0x00, 0x10, 0x08}, {0x00,
								     0x10,
								     0x0C},
	/* Index 0xC4~0xC7 */
	{0x00, 0x10, 0x10}, {0x00, 0x0C, 0x10}, {0x00, 0x08, 0x10}, {0x00,
								     0x04,
								     0x10},
	/* Index 0xC8~0xCB */
	{0x08, 0x08, 0x10}, {0x0A, 0x08, 0x10}, {0x0C, 0x08, 0x10}, {0x0E,
								     0x08,
								     0x10},
	/* Index 0xCC~0xCF */
	{0x10, 0x08, 0x10}, {0x10, 0x08, 0x0E}, {0x10, 0x08, 0x0C}, {0x10,
								     0x08,
								     0x0A},
	/* Index 0xD0~0xD3 */
	{0x10, 0x08, 0x08}, {0x10, 0x0A, 0x08}, {0x10, 0x0C, 0x08}, {0x10,
								     0x0E,
								     0x08},
	/* Index 0xD4~0xD7 */
	{0x10, 0x10, 0x08}, {0x0E, 0x10, 0x08}, {0x0C, 0x10, 0x08}, {0x0A,
								     0x10,
								     0x08},
	/* Index 0xD8~0xDB */
	{0x08, 0x10, 0x08}, {0x08, 0x10, 0x0A}, {0x08, 0x10, 0x0C}, {0x08,
								     0x10,
								     0x0E},
	/* Index 0xDC~0xDF */
	{0x08, 0x10, 0x10}, {0x08, 0x0E, 0x10}, {0x08, 0x0C, 0x10}, {0x08,
								     0x0A,
								     0x10},
	/* Index 0xE0~0xE3 */
	{0x0B, 0x0B, 0x10}, {0x0C, 0x0B, 0x10}, {0x0D, 0x0B, 0x10}, {0x0F,
								     0x0B,
								     0x10},
	/* Index 0xE4~0xE7 */
	{0x10, 0x0B, 0x10}, {0x10, 0x0B, 0x0F}, {0x10, 0x0B, 0x0D}, {0x10,
								     0x0B,
								     0x0C},
	/* Index 0xE8~0xEB */
	{0x10, 0x0B, 0x0B}, {0x10, 0x0C, 0x0B}, {0x10, 0x0D, 0x0B}, {0x10,
								     0x0F,
								     0x0B},
	/* Index 0xEC~0xEF */
	{0x10, 0x10, 0x0B}, {0x0F, 0x10, 0x0B}, {0x0D, 0x10, 0x0B}, {0x0C,
								     0x10,
								     0x0B},
	/* Index 0xF0~0xF3 */
	{0x0B, 0x10, 0x0B}, {0x0B, 0x10, 0x0C}, {0x0B, 0x10, 0x0D}, {0x0B,
								     0x10,
								     0x0F},
	/* Index 0xF4~0xF7 */
	{0x0B, 0x10, 0x10}, {0x0B, 0x0F, 0x10}, {0x0B, 0x0D, 0x10}, {0x0B,
								     0x0C,
								     0x10},
	/* Index 0xF8~0xFB */
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00,
								     0x00,
								     0x00},
	/* Index 0xFC~0xFF */
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00,
								     0x00,
								     0x00}
};

static void set_crt_output_path(int set_iga);
static void dvi_patch_skew_dvp0(void);
static void dvi_patch_skew_dvp1(void);
static void dvi_patch_skew_dvp_low(void);
static void set_dvi_output_path(int set_iga, int output_interface);
static void set_lcd_output_path(int set_iga, int output_interface);
static void load_fix_bit_crtc_reg(void);
static void init_gfx_chip_info(int chip_type);
static void init_tmds_chip_info(void);
static void init_lvds_chip_info(void);
static void device_screen_off(void);
static void device_screen_on(void);
static void set_display_channel(void);
static void device_off(void);
static void device_on(void);
static void enable_second_display_channel(void);
static void disable_second_display_channel(void);

void viafb_lock_crt(void)
{
	viafb_write_reg_mask(CR11, VIACR, BIT7, BIT7);
}

void viafb_unlock_crt(void)
{
	viafb_write_reg_mask(CR11, VIACR, 0, BIT7);
	viafb_write_reg_mask(CR47, VIACR, 0, BIT0);
}

void write_dac_reg(u8 index, u8 r, u8 g, u8 b)
{
	outb(index, LUT_INDEX_WRITE);
	outb(r, LUT_DATA);
	outb(g, LUT_DATA);
	outb(b, LUT_DATA);
}

/*Set IGA path for each device*/
void viafb_set_iga_path(void)
{

	if (viafb_SAMM_ON == 1) {
		if (viafb_CRT_ON) {
			if (viafb_primary_dev == CRT_Device)
				viaparinfo->crt_setting_info->iga_path = IGA1;
			else
				viaparinfo->crt_setting_info->iga_path = IGA2;
		}

		if (viafb_DVI_ON) {
			if (viafb_primary_dev == DVI_Device)
				viaparinfo->tmds_setting_info->iga_path = IGA1;
			else
				viaparinfo->tmds_setting_info->iga_path = IGA2;
		}

		if (viafb_LCD_ON) {
			if (viafb_primary_dev == LCD_Device) {
				if (viafb_dual_fb &&
					(viaparinfo->chip_info->gfx_chip_name ==
					UNICHROME_CLE266)) {
					viaparinfo->
					lvds_setting_info->iga_path = IGA2;
					viaparinfo->
					crt_setting_info->iga_path = IGA1;
					viaparinfo->
					tmds_setting_info->iga_path = IGA1;
				} else
					viaparinfo->
					lvds_setting_info->iga_path = IGA1;
			} else {
				viaparinfo->lvds_setting_info->iga_path = IGA2;
			}
		}
		if (viafb_LCD2_ON) {
			if (LCD2_Device == viafb_primary_dev)
				viaparinfo->lvds_setting_info2->iga_path = IGA1;
			else
				viaparinfo->lvds_setting_info2->iga_path = IGA2;
		}
	} else {
		viafb_SAMM_ON = 0;

		if (viafb_CRT_ON && viafb_LCD_ON) {
			viaparinfo->crt_setting_info->iga_path = IGA1;
			viaparinfo->lvds_setting_info->iga_path = IGA2;
		} else if (viafb_CRT_ON && viafb_DVI_ON) {
			viaparinfo->crt_setting_info->iga_path = IGA1;
			viaparinfo->tmds_setting_info->iga_path = IGA2;
		} else if (viafb_LCD_ON && viafb_DVI_ON) {
			viaparinfo->tmds_setting_info->iga_path = IGA1;
			viaparinfo->lvds_setting_info->iga_path = IGA2;
		} else if (viafb_LCD_ON && viafb_LCD2_ON) {
			viaparinfo->lvds_setting_info->iga_path = IGA2;
			viaparinfo->lvds_setting_info2->iga_path = IGA2;
		} else if (viafb_CRT_ON) {
			viaparinfo->crt_setting_info->iga_path = IGA1;
		} else if (viafb_LCD_ON) {
			viaparinfo->lvds_setting_info->iga_path = IGA2;
		} else if (viafb_DVI_ON) {
			viaparinfo->tmds_setting_info->iga_path = IGA1;
		}
	}
}

static void set_color_register(u8 index, u8 red, u8 green, u8 blue)
{
	outb(0xFF, 0x3C6); /* bit mask of palette */
	outb(index, 0x3C8);
	outb(red, 0x3C9);
	outb(green, 0x3C9);
	outb(blue, 0x3C9);
}

void viafb_set_primary_color_register(u8 index, u8 red, u8 green, u8 blue)
{
	viafb_write_reg_mask(0x1A, VIASR, 0x00, 0x01);
	set_color_register(index, red, green, blue);
}

void viafb_set_secondary_color_register(u8 index, u8 red, u8 green, u8 blue)
{
	viafb_write_reg_mask(0x1A, VIASR, 0x01, 0x01);
	set_color_register(index, red, green, blue);
}

void viafb_set_output_path(int device, int set_iga, int output_interface)
{
	switch (device) {
	case DEVICE_CRT:
		set_crt_output_path(set_iga);
		break;
	case DEVICE_DVI:
		set_dvi_output_path(set_iga, output_interface);
		break;
	case DEVICE_LCD:
		set_lcd_output_path(set_iga, output_interface);
		break;
	}
}

static void set_crt_output_path(int set_iga)
{
	viafb_write_reg_mask(CR36, VIACR, 0x00, BIT4 + BIT5);

	switch (set_iga) {
	case IGA1:
		viafb_write_reg_mask(SR16, VIASR, 0x00, BIT6);
		break;
	case IGA2:
		viafb_write_reg_mask(CR6A, VIACR, 0xC0, BIT6 + BIT7);
		viafb_write_reg_mask(SR16, VIASR, 0x40, BIT6);
		break;
	}
}

static void dvi_patch_skew_dvp0(void)
{
	/* Reset data driving first: */
	viafb_write_reg_mask(SR1B, VIASR, 0, BIT1);
	viafb_write_reg_mask(SR2A, VIASR, 0, BIT4);

	switch (viaparinfo->chip_info->gfx_chip_name) {
	case UNICHROME_P4M890:
		{
			if ((viaparinfo->tmds_setting_info->h_active == 1600) &&
				(viaparinfo->tmds_setting_info->v_active ==
				1200))
				viafb_write_reg_mask(CR96, VIACR, 0x03,
					       BIT0 + BIT1 + BIT2);
			else
				viafb_write_reg_mask(CR96, VIACR, 0x07,
					       BIT0 + BIT1 + BIT2);
			break;
		}

	case UNICHROME_P4M900:
		{
			viafb_write_reg_mask(CR96, VIACR, 0x07,
				       BIT0 + BIT1 + BIT2 + BIT3);
			viafb_write_reg_mask(SR1B, VIASR, 0x02, BIT1);
			viafb_write_reg_mask(SR2A, VIASR, 0x10, BIT4);
			break;
		}

	default:
		{
			break;
		}
	}
}

static void dvi_patch_skew_dvp1(void)
{
	switch (viaparinfo->chip_info->gfx_chip_name) {
	case UNICHROME_CX700:
		{
			break;
		}

	default:
		{
			break;
		}
	}
}

static void dvi_patch_skew_dvp_low(void)
{
	switch (viaparinfo->chip_info->gfx_chip_name) {
	case UNICHROME_K8M890:
		{
			viafb_write_reg_mask(CR99, VIACR, 0x03, BIT0 + BIT1);
			break;
		}

	case UNICHROME_P4M900:
		{
			viafb_write_reg_mask(CR99, VIACR, 0x08,
				       BIT0 + BIT1 + BIT2 + BIT3);
			break;
		}

	case UNICHROME_P4M890:
		{
			viafb_write_reg_mask(CR99, VIACR, 0x0F,
				       BIT0 + BIT1 + BIT2 + BIT3);
			break;
		}

	default:
		{
			break;
		}
	}
}

static void set_dvi_output_path(int set_iga, int output_interface)
{
	switch (output_interface) {
	case INTERFACE_DVP0:
		viafb_write_reg_mask(CR6B, VIACR, 0x01, BIT0);

		if (set_iga == IGA1) {
			viafb_write_reg_mask(CR96, VIACR, 0x00, BIT4);
			viafb_write_reg_mask(CR6C, VIACR, 0x21, BIT0 +
				BIT5 + BIT7);
		} else {
			viafb_write_reg_mask(CR96, VIACR, 0x10, BIT4);
			viafb_write_reg_mask(CR6C, VIACR, 0xA1, BIT0 +
				BIT5 + BIT7);
		}

		viafb_write_reg_mask(SR1E, VIASR, 0xC0, BIT7 + BIT6);

		dvi_patch_skew_dvp0();
		break;

	case INTERFACE_DVP1:
		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CLE266) {
			if (set_iga == IGA1)
				viafb_write_reg_mask(CR93, VIACR, 0x21,
					       BIT0 + BIT5 + BIT7);
			else
				viafb_write_reg_mask(CR93, VIACR, 0xA1,
					       BIT0 + BIT5 + BIT7);
		} else {
			if (set_iga == IGA1)
				viafb_write_reg_mask(CR9B, VIACR, 0x00, BIT4);
			else
				viafb_write_reg_mask(CR9B, VIACR, 0x10, BIT4);
		}

		viafb_write_reg_mask(SR1E, VIASR, 0x30, BIT4 + BIT5);
		dvi_patch_skew_dvp1();
		break;
	case INTERFACE_DFP_HIGH:
		if (viaparinfo->chip_info->gfx_chip_name != UNICHROME_CLE266) {
			if (set_iga == IGA1) {
				viafb_write_reg_mask(CR96, VIACR, 0x00, BIT4);
				viafb_write_reg_mask(CR97, VIACR, 0x03,
					       BIT0 + BIT1 + BIT4);
			} else {
				viafb_write_reg_mask(CR96, VIACR, 0x10, BIT4);
				viafb_write_reg_mask(CR97, VIACR, 0x13,
					       BIT0 + BIT1 + BIT4);
			}
		}
		viafb_write_reg_mask(SR2A, VIASR, 0x0C, BIT2 + BIT3);
		break;

	case INTERFACE_DFP_LOW:
		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CLE266)
			break;

		if (set_iga == IGA1) {
			viafb_write_reg_mask(CR99, VIACR, 0x00, BIT4);
			viafb_write_reg_mask(CR9B, VIACR, 0x00, BIT4);
		} else {
			viafb_write_reg_mask(CR99, VIACR, 0x10, BIT4);
			viafb_write_reg_mask(CR9B, VIACR, 0x10, BIT4);
		}

		viafb_write_reg_mask(SR2A, VIASR, 0x03, BIT0 + BIT1);
		dvi_patch_skew_dvp_low();
		break;

	case INTERFACE_TMDS:
		if (set_iga == IGA1)
			viafb_write_reg_mask(CR99, VIACR, 0x00, BIT4);
		else
			viafb_write_reg_mask(CR99, VIACR, 0x10, BIT4);
		break;
	}

	if (set_iga == IGA2) {
		enable_second_display_channel();
		/* Disable LCD Scaling */
		viafb_write_reg_mask(CR79, VIACR, 0x00, BIT0);
	}
}

static void set_lcd_output_path(int set_iga, int output_interface)
{
	DEBUG_MSG(KERN_INFO
		  "set_lcd_output_path, iga:%d,out_interface:%d\n",
		  set_iga, output_interface);
	switch (set_iga) {
	case IGA1:
		viafb_write_reg_mask(CR6B, VIACR, 0x00, BIT3);
		viafb_write_reg_mask(CR6A, VIACR, 0x08, BIT3);

		disable_second_display_channel();
		break;

	case IGA2:
		viafb_write_reg_mask(CR6B, VIACR, 0x00, BIT3);
		viafb_write_reg_mask(CR6A, VIACR, 0x08, BIT3);

		enable_second_display_channel();
		break;
	}

	switch (output_interface) {
	case INTERFACE_DVP0:
		if (set_iga == IGA1) {
			viafb_write_reg_mask(CR96, VIACR, 0x00, BIT4);
		} else {
			viafb_write_reg(CR91, VIACR, 0x00);
			viafb_write_reg_mask(CR96, VIACR, 0x10, BIT4);
		}
		break;

	case INTERFACE_DVP1:
		if (set_iga == IGA1)
			viafb_write_reg_mask(CR9B, VIACR, 0x00, BIT4);
		else {
			viafb_write_reg(CR91, VIACR, 0x00);
			viafb_write_reg_mask(CR9B, VIACR, 0x10, BIT4);
		}
		break;

	case INTERFACE_DFP_HIGH:
		if (set_iga == IGA1)
			viafb_write_reg_mask(CR97, VIACR, 0x00, BIT4);
		else {
			viafb_write_reg(CR91, VIACR, 0x00);
			viafb_write_reg_mask(CR97, VIACR, 0x10, BIT4);
			viafb_write_reg_mask(CR96, VIACR, 0x10, BIT4);
		}
		break;

	case INTERFACE_DFP_LOW:
		if (set_iga == IGA1)
			viafb_write_reg_mask(CR99, VIACR, 0x00, BIT4);
		else {
			viafb_write_reg(CR91, VIACR, 0x00);
			viafb_write_reg_mask(CR99, VIACR, 0x10, BIT4);
			viafb_write_reg_mask(CR9B, VIACR, 0x10, BIT4);
		}

		break;

	case INTERFACE_DFP:
		if ((UNICHROME_K8M890 == viaparinfo->chip_info->gfx_chip_name)
		    || (UNICHROME_P4M890 ==
		    viaparinfo->chip_info->gfx_chip_name))
			viafb_write_reg_mask(CR97, VIACR, 0x84,
				       BIT7 + BIT2 + BIT1 + BIT0);
		if (set_iga == IGA1) {
			viafb_write_reg_mask(CR97, VIACR, 0x00, BIT4);
			viafb_write_reg_mask(CR99, VIACR, 0x00, BIT4);
		} else {
			viafb_write_reg(CR91, VIACR, 0x00);
			viafb_write_reg_mask(CR97, VIACR, 0x10, BIT4);
			viafb_write_reg_mask(CR99, VIACR, 0x10, BIT4);
		}
		break;

	case INTERFACE_LVDS0:
	case INTERFACE_LVDS0LVDS1:
		if (set_iga == IGA1)
			viafb_write_reg_mask(CR99, VIACR, 0x00, BIT4);
		else
			viafb_write_reg_mask(CR99, VIACR, 0x10, BIT4);

		break;

	case INTERFACE_LVDS1:
		if (set_iga == IGA1)
			viafb_write_reg_mask(CR97, VIACR, 0x00, BIT4);
		else
			viafb_write_reg_mask(CR97, VIACR, 0x10, BIT4);
		break;
	}
}

static void load_fix_bit_crtc_reg(void)
{
	/* always set to 1 */
	viafb_write_reg_mask(CR03, VIACR, 0x80, BIT7);
	/* line compare should set all bits = 1 (extend modes) */
	viafb_write_reg(CR18, VIACR, 0xff);
	/* line compare should set all bits = 1 (extend modes) */
	viafb_write_reg_mask(CR07, VIACR, 0x10, BIT4);
	/* line compare should set all bits = 1 (extend modes) */
	viafb_write_reg_mask(CR09, VIACR, 0x40, BIT6);
	/* line compare should set all bits = 1 (extend modes) */
	viafb_write_reg_mask(CR35, VIACR, 0x10, BIT4);
	/* line compare should set all bits = 1 (extend modes) */
	viafb_write_reg_mask(CR33, VIACR, 0x06, BIT0 + BIT1 + BIT2);
	/*viafb_write_reg_mask(CR32, VIACR, 0x01, BIT0); */
	/* extend mode always set to e3h */
	viafb_write_reg(CR17, VIACR, 0xe3);
	/* extend mode always set to 0h */
	viafb_write_reg(CR08, VIACR, 0x00);
	/* extend mode always set to 0h */
	viafb_write_reg(CR14, VIACR, 0x00);

	/* If K8M800, enable Prefetch Mode. */
	if ((viaparinfo->chip_info->gfx_chip_name == UNICHROME_K800)
		|| (viaparinfo->chip_info->gfx_chip_name == UNICHROME_K8M890))
		viafb_write_reg_mask(CR33, VIACR, 0x08, BIT3);
	if ((viaparinfo->chip_info->gfx_chip_name == UNICHROME_CLE266)
	    && (viaparinfo->chip_info->gfx_chip_revision == CLE266_REVISION_AX))
		viafb_write_reg_mask(SR1A, VIASR, 0x02, BIT1);

}

void viafb_load_reg(int timing_value, int viafb_load_reg_num,
	struct io_register *reg,
	      int io_type)
{
	int reg_mask;
	int bit_num = 0;
	int data;
	int i, j;
	int shift_next_reg;
	int start_index, end_index, cr_index;
	u16 get_bit;

	for (i = 0; i < viafb_load_reg_num; i++) {
		reg_mask = 0;
		data = 0;
		start_index = reg[i].start_bit;
		end_index = reg[i].end_bit;
		cr_index = reg[i].io_addr;

		shift_next_reg = bit_num;
		for (j = start_index; j <= end_index; j++) {
			/*if (bit_num==8) timing_value = timing_value >>8; */
			reg_mask = reg_mask | (BIT0 << j);
			get_bit = (timing_value & (BIT0 << bit_num));
			data =
			    data | ((get_bit >> shift_next_reg) << start_index);
			bit_num++;
		}
		if (io_type == VIACR)
			viafb_write_reg_mask(cr_index, VIACR, data, reg_mask);
		else
			viafb_write_reg_mask(cr_index, VIASR, data, reg_mask);
	}

}

/* Write Registers */
void viafb_write_regx(struct io_reg RegTable[], int ItemNum)
{
	int i;

	/*DEBUG_MSG(KERN_INFO "Table Size : %x!!\n",ItemNum ); */

	for (i = 0; i < ItemNum; i++)
		via_write_reg_mask(RegTable[i].port, RegTable[i].index,
			RegTable[i].value, RegTable[i].mask);
}

void viafb_load_fetch_count_reg(int h_addr, int bpp_byte, int set_iga)
{
	int reg_value;
	int viafb_load_reg_num;
	struct io_register *reg = NULL;

	switch (set_iga) {
	case IGA1:
		reg_value = IGA1_FETCH_COUNT_FORMULA(h_addr, bpp_byte);
		viafb_load_reg_num = fetch_count_reg.
			iga1_fetch_count_reg.reg_num;
		reg = fetch_count_reg.iga1_fetch_count_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIASR);
		break;
	case IGA2:
		reg_value = IGA2_FETCH_COUNT_FORMULA(h_addr, bpp_byte);
		viafb_load_reg_num = fetch_count_reg.
			iga2_fetch_count_reg.reg_num;
		reg = fetch_count_reg.iga2_fetch_count_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIACR);
		break;
	}

}

void viafb_load_FIFO_reg(int set_iga, int hor_active, int ver_active)
{
	int reg_value;
	int viafb_load_reg_num;
	struct io_register *reg = NULL;
	int iga1_fifo_max_depth = 0, iga1_fifo_threshold =
	    0, iga1_fifo_high_threshold = 0, iga1_display_queue_expire_num = 0;
	int iga2_fifo_max_depth = 0, iga2_fifo_threshold =
	    0, iga2_fifo_high_threshold = 0, iga2_display_queue_expire_num = 0;

	if (set_iga == IGA1) {
		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_K800) {
			iga1_fifo_max_depth = K800_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = K800_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    K800_IGA1_FIFO_HIGH_THRESHOLD;
			/* If resolution > 1280x1024, expire length = 64, else
			   expire length = 128 */
			if ((hor_active > 1280) && (ver_active > 1024))
				iga1_display_queue_expire_num = 16;
			else
				iga1_display_queue_expire_num =
				    K800_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;

		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_PM800) {
			iga1_fifo_max_depth = P880_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = P880_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    P880_IGA1_FIFO_HIGH_THRESHOLD;
			iga1_display_queue_expire_num =
			    P880_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;

			/* If resolution > 1280x1024, expire length = 64, else
			   expire length = 128 */
			if ((hor_active > 1280) && (ver_active > 1024))
				iga1_display_queue_expire_num = 16;
			else
				iga1_display_queue_expire_num =
				    P880_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CN700) {
			iga1_fifo_max_depth = CN700_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = CN700_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    CN700_IGA1_FIFO_HIGH_THRESHOLD;

			/* If resolution > 1280x1024, expire length = 64,
			   else expire length = 128 */
			if ((hor_active > 1280) && (ver_active > 1024))
				iga1_display_queue_expire_num = 16;
			else
				iga1_display_queue_expire_num =
				    CN700_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CX700) {
			iga1_fifo_max_depth = CX700_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = CX700_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    CX700_IGA1_FIFO_HIGH_THRESHOLD;
			iga1_display_queue_expire_num =
			    CX700_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_K8M890) {
			iga1_fifo_max_depth = K8M890_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = K8M890_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    K8M890_IGA1_FIFO_HIGH_THRESHOLD;
			iga1_display_queue_expire_num =
			    K8M890_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_P4M890) {
			iga1_fifo_max_depth = P4M890_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = P4M890_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    P4M890_IGA1_FIFO_HIGH_THRESHOLD;
			iga1_display_queue_expire_num =
			    P4M890_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_P4M900) {
			iga1_fifo_max_depth = P4M900_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = P4M900_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    P4M900_IGA1_FIFO_HIGH_THRESHOLD;
			iga1_display_queue_expire_num =
			    P4M900_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_VX800) {
			iga1_fifo_max_depth = VX800_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = VX800_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    VX800_IGA1_FIFO_HIGH_THRESHOLD;
			iga1_display_queue_expire_num =
			    VX800_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_VX855) {
			iga1_fifo_max_depth = VX855_IGA1_FIFO_MAX_DEPTH;
			iga1_fifo_threshold = VX855_IGA1_FIFO_THRESHOLD;
			iga1_fifo_high_threshold =
			    VX855_IGA1_FIFO_HIGH_THRESHOLD;
			iga1_display_queue_expire_num =
			    VX855_IGA1_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		/* Set Display FIFO Depath Select */
		reg_value = IGA1_FIFO_DEPTH_SELECT_FORMULA(iga1_fifo_max_depth);
		viafb_load_reg_num =
		    display_fifo_depth_reg.iga1_fifo_depth_select_reg.reg_num;
		reg = display_fifo_depth_reg.iga1_fifo_depth_select_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIASR);

		/* Set Display FIFO Threshold Select */
		reg_value = IGA1_FIFO_THRESHOLD_FORMULA(iga1_fifo_threshold);
		viafb_load_reg_num =
		    fifo_threshold_select_reg.
		    iga1_fifo_threshold_select_reg.reg_num;
		reg =
		    fifo_threshold_select_reg.
		    iga1_fifo_threshold_select_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIASR);

		/* Set FIFO High Threshold Select */
		reg_value =
		    IGA1_FIFO_HIGH_THRESHOLD_FORMULA(iga1_fifo_high_threshold);
		viafb_load_reg_num =
		    fifo_high_threshold_select_reg.
		    iga1_fifo_high_threshold_select_reg.reg_num;
		reg =
		    fifo_high_threshold_select_reg.
		    iga1_fifo_high_threshold_select_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIASR);

		/* Set Display Queue Expire Num */
		reg_value =
		    IGA1_DISPLAY_QUEUE_EXPIRE_NUM_FORMULA
		    (iga1_display_queue_expire_num);
		viafb_load_reg_num =
		    display_queue_expire_num_reg.
		    iga1_display_queue_expire_num_reg.reg_num;
		reg =
		    display_queue_expire_num_reg.
		    iga1_display_queue_expire_num_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIASR);

	} else {
		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_K800) {
			iga2_fifo_max_depth = K800_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = K800_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    K800_IGA2_FIFO_HIGH_THRESHOLD;

			/* If resolution > 1280x1024, expire length = 64,
			   else  expire length = 128 */
			if ((hor_active > 1280) && (ver_active > 1024))
				iga2_display_queue_expire_num = 16;
			else
				iga2_display_queue_expire_num =
				    K800_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_PM800) {
			iga2_fifo_max_depth = P880_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = P880_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    P880_IGA2_FIFO_HIGH_THRESHOLD;

			/* If resolution > 1280x1024, expire length = 64,
			   else  expire length = 128 */
			if ((hor_active > 1280) && (ver_active > 1024))
				iga2_display_queue_expire_num = 16;
			else
				iga2_display_queue_expire_num =
				    P880_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CN700) {
			iga2_fifo_max_depth = CN700_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = CN700_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    CN700_IGA2_FIFO_HIGH_THRESHOLD;

			/* If resolution > 1280x1024, expire length = 64,
			   else expire length = 128 */
			if ((hor_active > 1280) && (ver_active > 1024))
				iga2_display_queue_expire_num = 16;
			else
				iga2_display_queue_expire_num =
				    CN700_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CX700) {
			iga2_fifo_max_depth = CX700_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = CX700_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    CX700_IGA2_FIFO_HIGH_THRESHOLD;
			iga2_display_queue_expire_num =
			    CX700_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_K8M890) {
			iga2_fifo_max_depth = K8M890_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = K8M890_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    K8M890_IGA2_FIFO_HIGH_THRESHOLD;
			iga2_display_queue_expire_num =
			    K8M890_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_P4M890) {
			iga2_fifo_max_depth = P4M890_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = P4M890_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    P4M890_IGA2_FIFO_HIGH_THRESHOLD;
			iga2_display_queue_expire_num =
			    P4M890_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_P4M900) {
			iga2_fifo_max_depth = P4M900_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = P4M900_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    P4M900_IGA2_FIFO_HIGH_THRESHOLD;
			iga2_display_queue_expire_num =
			    P4M900_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_VX800) {
			iga2_fifo_max_depth = VX800_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = VX800_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    VX800_IGA2_FIFO_HIGH_THRESHOLD;
			iga2_display_queue_expire_num =
			    VX800_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_VX855) {
			iga2_fifo_max_depth = VX855_IGA2_FIFO_MAX_DEPTH;
			iga2_fifo_threshold = VX855_IGA2_FIFO_THRESHOLD;
			iga2_fifo_high_threshold =
			    VX855_IGA2_FIFO_HIGH_THRESHOLD;
			iga2_display_queue_expire_num =
			    VX855_IGA2_DISPLAY_QUEUE_EXPIRE_NUM;
		}

		if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_K800) {
			/* Set Display FIFO Depath Select */
			reg_value =
			    IGA2_FIFO_DEPTH_SELECT_FORMULA(iga2_fifo_max_depth)
			    - 1;
			/* Patch LCD in IGA2 case */
			viafb_load_reg_num =
			    display_fifo_depth_reg.
			    iga2_fifo_depth_select_reg.reg_num;
			reg =
			    display_fifo_depth_reg.
			    iga2_fifo_depth_select_reg.reg;
			viafb_load_reg(reg_value,
				viafb_load_reg_num, reg, VIACR);
		} else {

			/* Set Display FIFO Depath Select */
			reg_value =
			    IGA2_FIFO_DEPTH_SELECT_FORMULA(iga2_fifo_max_depth);
			viafb_load_reg_num =
			    display_fifo_depth_reg.
			    iga2_fifo_depth_select_reg.reg_num;
			reg =
			    display_fifo_depth_reg.
			    iga2_fifo_depth_select_reg.reg;
			viafb_load_reg(reg_value,
				viafb_load_reg_num, reg, VIACR);
		}

		/* Set Display FIFO Threshold Select */
		reg_value = IGA2_FIFO_THRESHOLD_FORMULA(iga2_fifo_threshold);
		viafb_load_reg_num =
		    fifo_threshold_select_reg.
		    iga2_fifo_threshold_select_reg.reg_num;
		reg =
		    fifo_threshold_select_reg.
		    iga2_fifo_threshold_select_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIACR);

		/* Set FIFO High Threshold Select */
		reg_value =
		    IGA2_FIFO_HIGH_THRESHOLD_FORMULA(iga2_fifo_high_threshold);
		viafb_load_reg_num =
		    fifo_high_threshold_select_reg.
		    iga2_fifo_high_threshold_select_reg.reg_num;
		reg =
		    fifo_high_threshold_select_reg.
		    iga2_fifo_high_threshold_select_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIACR);

		/* Set Display Queue Expire Num */
		reg_value =
		    IGA2_DISPLAY_QUEUE_EXPIRE_NUM_FORMULA
		    (iga2_display_queue_expire_num);
		viafb_load_reg_num =
		    display_queue_expire_num_reg.
		    iga2_display_queue_expire_num_reg.reg_num;
		reg =
		    display_queue_expire_num_reg.
		    iga2_display_queue_expire_num_reg.reg;
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIACR);

	}

}

static u32 cle266_encode_pll(struct pll_config pll)
{
	return (pll.multiplier << 8)
		| (pll.rshift << 6)
		| pll.divisor;
}

static u32 k800_encode_pll(struct pll_config pll)
{
	return ((pll.divisor - 2) << 16)
		| (pll.rshift << 10)
		| (pll.multiplier - 2);
}

static u32 vx855_encode_pll(struct pll_config pll)
{
	return (pll.divisor << 16)
		| (pll.rshift << 10)
		| pll.multiplier;
}

u32 viafb_get_clk_value(int clk)
{
	u32 value = 0;
	int i = 0;

	while (i < NUM_TOTAL_PLL_TABLE && clk != pll_value[i].clk)
		i++;

	if (i == NUM_TOTAL_PLL_TABLE) {
		printk(KERN_WARNING "viafb_get_clk_value: PLL lookup failed!");
	} else {
		switch (viaparinfo->chip_info->gfx_chip_name) {
		case UNICHROME_CLE266:
		case UNICHROME_K400:
			value = cle266_encode_pll(pll_value[i].cle266_pll);
			break;

		case UNICHROME_K800:
		case UNICHROME_PM800:
		case UNICHROME_CN700:
			value = k800_encode_pll(pll_value[i].k800_pll);
			break;

		case UNICHROME_CX700:
		case UNICHROME_CN750:
		case UNICHROME_K8M890:
		case UNICHROME_P4M890:
		case UNICHROME_P4M900:
		case UNICHROME_VX800:
			value = k800_encode_pll(pll_value[i].cx700_pll);
			break;

		case UNICHROME_VX855:
			value = vx855_encode_pll(pll_value[i].vx855_pll);
			break;
		}
	}

	return value;
}

/* Set VCLK*/
void viafb_set_vclock(u32 clk, int set_iga)
{
	/* H.W. Reset : ON */
	viafb_write_reg_mask(CR17, VIACR, 0x00, BIT7);

	if (set_iga == IGA1) {
		/* Change D,N FOR VCLK */
		switch (viaparinfo->chip_info->gfx_chip_name) {
		case UNICHROME_CLE266:
		case UNICHROME_K400:
			via_write_reg(VIASR, SR46, (clk & 0x00FF));
			via_write_reg(VIASR, SR47, (clk & 0xFF00) >> 8);
			break;

		case UNICHROME_K800:
		case UNICHROME_PM800:
		case UNICHROME_CN700:
		case UNICHROME_CX700:
		case UNICHROME_CN750:
		case UNICHROME_K8M890:
		case UNICHROME_P4M890:
		case UNICHROME_P4M900:
		case UNICHROME_VX800:
		case UNICHROME_VX855:
			via_write_reg(VIASR, SR44, (clk & 0x0000FF));
			via_write_reg(VIASR, SR45, (clk & 0x00FF00) >> 8);
			via_write_reg(VIASR, SR46, (clk & 0xFF0000) >> 16);
			break;
		}
	}

	if (set_iga == IGA2) {
		/* Change D,N FOR LCK */
		switch (viaparinfo->chip_info->gfx_chip_name) {
		case UNICHROME_CLE266:
		case UNICHROME_K400:
			via_write_reg(VIASR, SR44, (clk & 0x00FF));
			via_write_reg(VIASR, SR45, (clk & 0xFF00) >> 8);
			break;

		case UNICHROME_K800:
		case UNICHROME_PM800:
		case UNICHROME_CN700:
		case UNICHROME_CX700:
		case UNICHROME_CN750:
		case UNICHROME_K8M890:
		case UNICHROME_P4M890:
		case UNICHROME_P4M900:
		case UNICHROME_VX800:
		case UNICHROME_VX855:
			via_write_reg(VIASR, SR4A, (clk & 0x0000FF));
			via_write_reg(VIASR, SR4B, (clk & 0x00FF00) >> 8);
			via_write_reg(VIASR, SR4C, (clk & 0xFF0000) >> 16);
			break;
		}
	}

	/* H.W. Reset : OFF */
	viafb_write_reg_mask(CR17, VIACR, 0x80, BIT7);

	/* Reset PLL */
	if (set_iga == IGA1) {
		viafb_write_reg_mask(SR40, VIASR, 0x02, BIT1);
		viafb_write_reg_mask(SR40, VIASR, 0x00, BIT1);
	}

	if (set_iga == IGA2) {
		viafb_write_reg_mask(SR40, VIASR, 0x01, BIT0);
		viafb_write_reg_mask(SR40, VIASR, 0x00, BIT0);
	}

	/* Fire! */
	via_write_misc_reg_mask(0x0C, 0x0C); /* select external clock */
}

void viafb_load_crtc_timing(struct display_timing device_timing,
	int set_iga)
{
	int i;
	int viafb_load_reg_num = 0;
	int reg_value = 0;
	struct io_register *reg = NULL;

	viafb_unlock_crt();

	for (i = 0; i < 12; i++) {
		if (set_iga == IGA1) {
			switch (i) {
			case H_TOTAL_INDEX:
				reg_value =
				    IGA1_HOR_TOTAL_FORMULA(device_timing.
							   hor_total);
				viafb_load_reg_num =
					iga1_crtc_reg.hor_total.reg_num;
				reg = iga1_crtc_reg.hor_total.reg;
				break;
			case H_ADDR_INDEX:
				reg_value =
				    IGA1_HOR_ADDR_FORMULA(device_timing.
							  hor_addr);
				viafb_load_reg_num =
					iga1_crtc_reg.hor_addr.reg_num;
				reg = iga1_crtc_reg.hor_addr.reg;
				break;
			case H_BLANK_START_INDEX:
				reg_value =
				    IGA1_HOR_BLANK_START_FORMULA
				    (device_timing.hor_blank_start);
				viafb_load_reg_num =
				    iga1_crtc_reg.hor_blank_start.reg_num;
				reg = iga1_crtc_reg.hor_blank_start.reg;
				break;
			case H_BLANK_END_INDEX:
				reg_value =
				    IGA1_HOR_BLANK_END_FORMULA
				    (device_timing.hor_blank_start,
				     device_timing.hor_blank_end);
				viafb_load_reg_num =
				    iga1_crtc_reg.hor_blank_end.reg_num;
				reg = iga1_crtc_reg.hor_blank_end.reg;
				break;
			case H_SYNC_START_INDEX:
				reg_value =
				    IGA1_HOR_SYNC_START_FORMULA
				    (device_timing.hor_sync_start);
				viafb_load_reg_num =
				    iga1_crtc_reg.hor_sync_start.reg_num;
				reg = iga1_crtc_reg.hor_sync_start.reg;
				break;
			case H_SYNC_END_INDEX:
				reg_value =
				    IGA1_HOR_SYNC_END_FORMULA
				    (device_timing.hor_sync_start,
				     device_timing.hor_sync_end);
				viafb_load_reg_num =
				    iga1_crtc_reg.hor_sync_end.reg_num;
				reg = iga1_crtc_reg.hor_sync_end.reg;
				break;
			case V_TOTAL_INDEX:
				reg_value =
				    IGA1_VER_TOTAL_FORMULA(device_timing.
							   ver_total);
				viafb_load_reg_num =
					iga1_crtc_reg.ver_total.reg_num;
				reg = iga1_crtc_reg.ver_total.reg;
				break;
			case V_ADDR_INDEX:
				reg_value =
				    IGA1_VER_ADDR_FORMULA(device_timing.
							  ver_addr);
				viafb_load_reg_num =
					iga1_crtc_reg.ver_addr.reg_num;
				reg = iga1_crtc_reg.ver_addr.reg;
				break;
			case V_BLANK_START_INDEX:
				reg_value =
				    IGA1_VER_BLANK_START_FORMULA
				    (device_timing.ver_blank_start);
				viafb_load_reg_num =
				    iga1_crtc_reg.ver_blank_start.reg_num;
				reg = iga1_crtc_reg.ver_blank_start.reg;
				break;
			case V_BLANK_END_INDEX:
				reg_value =
				    IGA1_VER_BLANK_END_FORMULA
				    (device_timing.ver_blank_start,
				     device_timing.ver_blank_end);
				viafb_load_reg_num =
				    iga1_crtc_reg.ver_blank_end.reg_num;
				reg = iga1_crtc_reg.ver_blank_end.reg;
				break;
			case V_SYNC_START_INDEX:
				reg_value =
				    IGA1_VER_SYNC_START_FORMULA
				    (device_timing.ver_sync_start);
				viafb_load_reg_num =
				    iga1_crtc_reg.ver_sync_start.reg_num;
				reg = iga1_crtc_reg.ver_sync_start.reg;
				break;
			case V_SYNC_END_INDEX:
				reg_value =
				    IGA1_VER_SYNC_END_FORMULA
				    (device_timing.ver_sync_start,
				     device_timing.ver_sync_end);
				viafb_load_reg_num =
				    iga1_crtc_reg.ver_sync_end.reg_num;
				reg = iga1_crtc_reg.ver_sync_end.reg;
				break;

			}
		}

		if (set_iga == IGA2) {
			switch (i) {
			case H_TOTAL_INDEX:
				reg_value =
				    IGA2_HOR_TOTAL_FORMULA(device_timing.
							   hor_total);
				viafb_load_reg_num =
					iga2_crtc_reg.hor_total.reg_num;
				reg = iga2_crtc_reg.hor_total.reg;
				break;
			case H_ADDR_INDEX:
				reg_value =
				    IGA2_HOR_ADDR_FORMULA(device_timing.
							  hor_addr);
				viafb_load_reg_num =
					iga2_crtc_reg.hor_addr.reg_num;
				reg = iga2_crtc_reg.hor_addr.reg;
				break;
			case H_BLANK_START_INDEX:
				reg_value =
				    IGA2_HOR_BLANK_START_FORMULA
				    (device_timing.hor_blank_start);
				viafb_load_reg_num =
				    iga2_crtc_reg.hor_blank_start.reg_num;
				reg = iga2_crtc_reg.hor_blank_start.reg;
				break;
			case H_BLANK_END_INDEX:
				reg_value =
				    IGA2_HOR_BLANK_END_FORMULA
				    (device_timing.hor_blank_start,
				     device_timing.hor_blank_end);
				viafb_load_reg_num =
				    iga2_crtc_reg.hor_blank_end.reg_num;
				reg = iga2_crtc_reg.hor_blank_end.reg;
				break;
			case H_SYNC_START_INDEX:
				reg_value =
				    IGA2_HOR_SYNC_START_FORMULA
				    (device_timing.hor_sync_start);
				if (UNICHROME_CN700 <=
					viaparinfo->chip_info->gfx_chip_name)
					viafb_load_reg_num =
					    iga2_crtc_reg.hor_sync_start.
					    reg_num;
				else
					viafb_load_reg_num = 3;
				reg = iga2_crtc_reg.hor_sync_start.reg;
				break;
			case H_SYNC_END_INDEX:
				reg_value =
				    IGA2_HOR_SYNC_END_FORMULA
				    (device_timing.hor_sync_start,
				     device_timing.hor_sync_end);
				viafb_load_reg_num =
				    iga2_crtc_reg.hor_sync_end.reg_num;
				reg = iga2_crtc_reg.hor_sync_end.reg;
				break;
			case V_TOTAL_INDEX:
				reg_value =
				    IGA2_VER_TOTAL_FORMULA(device_timing.
							   ver_total);
				viafb_load_reg_num =
					iga2_crtc_reg.ver_total.reg_num;
				reg = iga2_crtc_reg.ver_total.reg;
				break;
			case V_ADDR_INDEX:
				reg_value =
				    IGA2_VER_ADDR_FORMULA(device_timing.
							  ver_addr);
				viafb_load_reg_num =
					iga2_crtc_reg.ver_addr.reg_num;
				reg = iga2_crtc_reg.ver_addr.reg;
				break;
			case V_BLANK_START_INDEX:
				reg_value =
				    IGA2_VER_BLANK_START_FORMULA
				    (device_timing.ver_blank_start);
				viafb_load_reg_num =
				    iga2_crtc_reg.ver_blank_start.reg_num;
				reg = iga2_crtc_reg.ver_blank_start.reg;
				break;
			case V_BLANK_END_INDEX:
				reg_value =
				    IGA2_VER_BLANK_END_FORMULA
				    (device_timing.ver_blank_start,
				     device_timing.ver_blank_end);
				viafb_load_reg_num =
				    iga2_crtc_reg.ver_blank_end.reg_num;
				reg = iga2_crtc_reg.ver_blank_end.reg;
				break;
			case V_SYNC_START_INDEX:
				reg_value =
				    IGA2_VER_SYNC_START_FORMULA
				    (device_timing.ver_sync_start);
				viafb_load_reg_num =
				    iga2_crtc_reg.ver_sync_start.reg_num;
				reg = iga2_crtc_reg.ver_sync_start.reg;
				break;
			case V_SYNC_END_INDEX:
				reg_value =
				    IGA2_VER_SYNC_END_FORMULA
				    (device_timing.ver_sync_start,
				     device_timing.ver_sync_end);
				viafb_load_reg_num =
				    iga2_crtc_reg.ver_sync_end.reg_num;
				reg = iga2_crtc_reg.ver_sync_end.reg;
				break;

			}
		}
		viafb_load_reg(reg_value, viafb_load_reg_num, reg, VIACR);
	}

	viafb_lock_crt();
}

void viafb_fill_crtc_timing(struct crt_mode_table *crt_table,
	struct VideoModeTable *video_mode, int bpp_byte, int set_iga)
{
	struct display_timing crt_reg;
	int i;
	int index = 0;
	int h_addr, v_addr;
	u32 pll_D_N;
	u8 polarity = 0;

	for (i = 0; i < video_mode->mode_array; i++) {
		index = i;

		if (crt_table[i].refresh_rate == viaparinfo->
			crt_setting_info->refresh_rate)
			break;
	}

	crt_reg = crt_table[index].crtc;

	/* Mode 640x480 has border, but LCD/DFP didn't have border. */
	/* So we would delete border. */
	if ((viafb_LCD_ON | viafb_DVI_ON)
	    && video_mode->crtc[0].crtc.hor_addr == 640
	    && video_mode->crtc[0].crtc.ver_addr == 480
	    && viaparinfo->crt_setting_info->refresh_rate == 60) {
		/* The border is 8 pixels. */
		crt_reg.hor_blank_start = crt_reg.hor_blank_start - 8;

		/* Blanking time should add left and right borders. */
		crt_reg.hor_blank_end = crt_reg.hor_blank_end + 16;
	}

	h_addr = crt_reg.hor_addr;
	v_addr = crt_reg.ver_addr;

	/* update polarity for CRT timing */
	if (crt_table[index].h_sync_polarity == NEGATIVE)
		polarity |= BIT6;
	if (crt_table[index].v_sync_polarity == NEGATIVE)
		polarity |= BIT7;
	via_write_misc_reg_mask(polarity, BIT6 | BIT7);

	if (set_iga == IGA1) {
		viafb_unlock_crt();
		viafb_write_reg(CR09, VIACR, 0x00);	/*initial CR09=0 */
		viafb_write_reg_mask(CR11, VIACR, 0x00, BIT4 + BIT5 + BIT6);
		viafb_write_reg_mask(CR17, VIACR, 0x00, BIT7);
	}

	switch (set_iga) {
	case IGA1:
		viafb_load_crtc_timing(crt_reg, IGA1);
		break;
	case IGA2:
		viafb_load_crtc_timing(crt_reg, IGA2);
		break;
	}

	load_fix_bit_crtc_reg();
	viafb_lock_crt();
	viafb_write_reg_mask(CR17, VIACR, 0x80, BIT7);
	viafb_load_fetch_count_reg(h_addr, bpp_byte, set_iga);

	/* load FIFO */
	if ((viaparinfo->chip_info->gfx_chip_name != UNICHROME_CLE266)
	    && (viaparinfo->chip_info->gfx_chip_name != UNICHROME_K400))
		viafb_load_FIFO_reg(set_iga, h_addr, v_addr);

	pll_D_N = viafb_get_clk_value(crt_table[index].clk);
	DEBUG_MSG(KERN_INFO "PLL=%x", pll_D_N);
	viafb_set_vclock(pll_D_N, set_iga);

}

void viafb_init_chip_info(int chip_type)
{
	init_gfx_chip_info(chip_type);
	init_tmds_chip_info();
	init_lvds_chip_info();

	viaparinfo->crt_setting_info->iga_path = IGA1;
	viaparinfo->crt_setting_info->refresh_rate = viafb_refresh;

	/*Set IGA path for each device */
	viafb_set_iga_path();

	viaparinfo->lvds_setting_info->display_method = viafb_lcd_dsp_method;
	viaparinfo->lvds_setting_info->lcd_mode = viafb_lcd_mode;
	viaparinfo->lvds_setting_info2->display_method =
		viaparinfo->lvds_setting_info->display_method;
	viaparinfo->lvds_setting_info2->lcd_mode =
		viaparinfo->lvds_setting_info->lcd_mode;
}

void viafb_update_device_setting(int hres, int vres,
	int bpp, int vmode_refresh, int flag)
{
	if (flag == 0) {
		viaparinfo->crt_setting_info->h_active = hres;
		viaparinfo->crt_setting_info->v_active = vres;
		viaparinfo->crt_setting_info->bpp = bpp;
		viaparinfo->crt_setting_info->refresh_rate =
			vmode_refresh;

		viaparinfo->tmds_setting_info->h_active = hres;
		viaparinfo->tmds_setting_info->v_active = vres;

		viaparinfo->lvds_setting_info->h_active = hres;
		viaparinfo->lvds_setting_info->v_active = vres;
		viaparinfo->lvds_setting_info->bpp = bpp;
		viaparinfo->lvds_setting_info->refresh_rate =
			vmode_refresh;
		viaparinfo->lvds_setting_info2->h_active = hres;
		viaparinfo->lvds_setting_info2->v_active = vres;
		viaparinfo->lvds_setting_info2->bpp = bpp;
		viaparinfo->lvds_setting_info2->refresh_rate =
			vmode_refresh;
	} else {

		if (viaparinfo->tmds_setting_info->iga_path == IGA2) {
			viaparinfo->tmds_setting_info->h_active = hres;
			viaparinfo->tmds_setting_info->v_active = vres;
		}

		if (viaparinfo->lvds_setting_info->iga_path == IGA2) {
			viaparinfo->lvds_setting_info->h_active = hres;
			viaparinfo->lvds_setting_info->v_active = vres;
			viaparinfo->lvds_setting_info->bpp = bpp;
			viaparinfo->lvds_setting_info->refresh_rate =
				vmode_refresh;
		}
		if (IGA2 == viaparinfo->lvds_setting_info2->iga_path) {
			viaparinfo->lvds_setting_info2->h_active = hres;
			viaparinfo->lvds_setting_info2->v_active = vres;
			viaparinfo->lvds_setting_info2->bpp = bpp;
			viaparinfo->lvds_setting_info2->refresh_rate =
				vmode_refresh;
		}
	}
}

static void init_gfx_chip_info(int chip_type)
{
	u8 tmp;

	viaparinfo->chip_info->gfx_chip_name = chip_type;

	/* Check revision of CLE266 Chip */
	if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CLE266) {
		/* CR4F only define in CLE266.CX chip */
		tmp = viafb_read_reg(VIACR, CR4F);
		viafb_write_reg(CR4F, VIACR, 0x55);
		if (viafb_read_reg(VIACR, CR4F) != 0x55)
			viaparinfo->chip_info->gfx_chip_revision =
			CLE266_REVISION_AX;
		else
			viaparinfo->chip_info->gfx_chip_revision =
			CLE266_REVISION_CX;
		/* restore orignal CR4F value */
		viafb_write_reg(CR4F, VIACR, tmp);
	}

	if (viaparinfo->chip_info->gfx_chip_name == UNICHROME_CX700) {
		tmp = viafb_read_reg(VIASR, SR43);
		DEBUG_MSG(KERN_INFO "SR43:%X\n", tmp);
		if (tmp & 0x02) {
			viaparinfo->chip_info->gfx_chip_revision =
				CX700_REVISION_700M2;
		} else if (tmp & 0x40) {
			viaparinfo->chip_info->gfx_chip_revision =
				CX700_REVISION_700M;
		} else {
			viaparinfo->chip_info->gfx_chip_revision =
				CX700_REVISION_700;
		}
	}

	/* Determine which 2D engine we have */
	switch (viaparinfo->chip_info->gfx_chip_name) {
	case UNICHROME_VX800:
	case UNICHROME_VX855:
		viaparinfo->chip_info->twod_engine = VIA_2D_ENG_M1;
		break;
	case UNICHROME_K8M890:
	case UNICHROME_P4M900:
		viaparinfo->chip_info->twod_engine = VIA_2D_ENG_H5;
		break;
	default:
		viaparinfo->chip_info->twod_engine = VIA_2D_ENG_H2;
		break;
	}
}

static void init_tmds_chip_info(void)
{
	viafb_tmds_trasmitter_identify();

	if (INTERFACE_NONE == viaparinfo->chip_info->tmds_chip_info.
		output_interface) {
		switch (viaparinfo->chip_info->gfx_chip_name) {
		case UNICHROME_CX700:
			{
				/* we should check support by hardware layout.*/
				if ((viafb_display_hardware_layout ==
				     HW_LAYOUT_DVI_ONLY)
				    || (viafb_display_hardware_layout ==
					HW_LAYOUT_LCD_DVI)) {
					viaparinfo->chip_info->tmds_chip_info.
					    output_interface = INTERFACE_TMDS;
				} else {
					viaparinfo->chip_info->tmds_chip_info.
						output_interface =
						INTERFACE_NONE;
				}
				break;
			}
		case UNICHROME_K8M890:
		case UNICHROME_P4M900:
		case UNICHROME_P4M890:
			/* TMDS on PCIE, we set DFPLOW as default. */
			viaparinfo->chip_info->tmds_chip_info.output_interface =
			    INTERFACE_DFP_LOW;
			break;
		default:
			{
				/* set DVP1 default for DVI */
				viaparinfo->chip_info->tmds_chip_info
				.output_interface = INTERFACE_DVP1;
			}
		}
	}

	DEBUG_MSG(KERN_INFO "TMDS Chip = %d\n",
		  viaparinfo->chip_info->tmds_chip_info.tmds_chip_name);
	viafb_init_dvi_size(&viaparinfo->shared->chip_info.tmds_chip_info,
		&viaparinfo->shared->tmds_setting_info);
}

static void init_lvds_chip_info(void)
{
	viafb_lvds_trasmitter_identify();
	viafb_init_lcd_size();
	viafb_init_lvds_output_interface(&viaparinfo->chip_info->lvds_chip_info,
				   viaparinfo->lvds_setting_info);
	if (viaparinfo->chip_info->lvds_chip_info2.lvds_chip_name) {
		viafb_init_lvds_output_interface(&viaparinfo->chip_info->
			lvds_chip_info2, viaparinfo->lvds_setting_info2);
	}
	/*If CX700,two singel LCD, we need to reassign
	   LCD interface to different LVDS port */
	if ((UNICHROME_CX700 == viaparinfo->chip_info->gfx_chip_name)
	    && (HW_LAYOUT_LCD1_LCD2 == viafb_display_hardware_layout)) {
		if ((INTEGRATED_LVDS == viaparinfo->chip_info->lvds_chip_info.
			lvds_chip_name) && (INTEGRATED_LVDS ==
			viaparinfo->chip_info->
			lvds_chip_info2.lvds_chip_name)) {
			viaparinfo->chip_info->lvds_chip_info.output_interface =
				INTERFACE_LVDS0;
			viaparinfo->chip_info->lvds_chip_info2.
				output_interface =
			    INTERFACE_LVDS1;
		}
	}

	DEBUG_MSG(KERN_INFO "LVDS Chip = %d\n",
		  viaparinfo->chip_info->lvds_chip_info.lvds_chip_name);
	DEBUG_MSG(KERN_INFO "LVDS1 output_interface = %d\n",
		  viaparinfo->chip_info->lvds_chip_info.output_interface);
	DEBUG_MSG(KERN_INFO "LVDS2 output_interface = %d\n",
		  viaparinfo->chip_info->lvds_chip_info.output_interface);
}

void viafb_init_dac(int set_iga)
{
	int i;
	u8 tmp;

	if (set_iga == IGA1) {
		/* access Primary Display's LUT */
		viafb_write_reg_mask(SR1A, VIASR, 0x00, BIT0);
		/* turn off LCK */
		viafb_write_reg_mask(SR1B, VIASR, 0x00, BIT7 + BIT6);
		for (i = 0; i < 256; i++) {
			write_dac_reg(i, palLUT_table[i].red,
				      palLUT_table[i].green,
				      palLUT_table[i].blue);
		}
		/* turn on LCK */
		viafb_write_reg_mask(SR1B, VIASR, 0xC0, BIT7 + BIT6);
	} else {
		tmp = viafb_read_reg(VIACR, CR6A);
		/* access Secondary Display's LUT */
		viafb_write_reg_mask(CR6A, VIACR, 0x40, BIT6);
		viafb_write_reg_mask(SR1A, VIASR, 0x01, BIT0);
		for (i = 0; i < 256; i++) {
			write_dac_reg(i, palLUT_table[i].red,
				      palLUT_table[i].green,
				      palLUT_table[i].blue);
		}
		/* set IGA1 DAC for default */
		viafb_write_reg_mask(SR1A, VIASR, 0x00, BIT0);
		viafb_write_reg(CR6A, VIACR, tmp);
	}
}

static void device_screen_off(void)
{
	/* turn off CRT screen (IGA1) */
	viafb_write_reg_mask(SR01, VIASR, 0x20, BIT5);
}

static void device_screen_on(void)
{
	/* turn on CRT screen (IGA1) */
	viafb_write_reg_mask(SR01, VIASR, 0x00, BIT5);
}

static void set_display_channel(void)
{
	/*If viafb_LCD2_ON, on cx700, internal lvds's information
	is keeped on lvds_setting_info2 */
	if (viafb_LCD2_ON &&
		viaparinfo->lvds_setting_info2->device_lcd_dualedge) {
		/* For dual channel LCD: */
		/* Set to Dual LVDS channel. */
		viafb_write_reg_mask(CRD2, VIACR, 0x20, BIT4 + BIT5);
	} else if (viafb_LCD_ON && viafb_DVI_ON) {
		/* For LCD+DFP: */
		/* Set to LVDS1 + TMDS channel. */
		viafb_write_reg_mask(CRD2, VIACR, 0x10, BIT4 + BIT5);
	} else if (viafb_DVI_ON) {
		/* Set to single TMDS channel. */
		viafb_write_reg_mask(CRD2, VIACR, 0x30, BIT4 + BIT5);
	} else if (viafb_LCD_ON) {
		if (viaparinfo->lvds_setting_info->device_lcd_dualedge) {
			/* For dual channel LCD: */
			/* Set to Dual LVDS channel. */
			viafb_write_reg_mask(CRD2, VIACR, 0x20, BIT4 + BIT5);
		} else {
			/* Set to LVDS0 + LVDS1 channel. */
			viafb_write_reg_mask(CRD2, VIACR, 0x00, BIT4 + BIT5);
		}
	}
}

int viafb_setmode(struct VideoModeTable *vmode_tbl, int video_bpp,
	struct VideoModeTable *vmode_tbl1, int video_bpp1)
{
	int i, j;
	int port;
	u8 value, index, mask;
	struct crt_mode_table *crt_timing;
	struct crt_mode_table *crt_timing1 = NULL;

	device_screen_off();
	crt_timing = vmode_tbl->crtc;

	if (viafb_SAMM_ON == 1) {
		crt_timing1 = vmode_tbl1->crtc;
	}

	inb(VIAStatus);
	outb(0x00, VIAAR);

	/* Write Common Setting for Video Mode */
	switch (viaparinfo->chip_info->gfx_chip_name) {
	case UNICHROME_CLE266:
		viafb_write_regx(CLE266_ModeXregs, NUM_TOTAL_CLE266_ModeXregs);
		break;

	case UNICHROME_K400:
		viafb_write_regx(KM400_ModeXregs, NUM_TOTAL_KM400_ModeXregs);
		break;

	case UNICHROME_K800:
	case UNICHROME_PM800:
		viafb_write_regx(CN400_ModeXregs, NUM_TOTAL_CN400_ModeXregs);
		break;

	case UNICHROME_CN700:
	case UNICHROME_K8M890:
	case UNICHROME_P4M890:
	case UNICHROME_P4M900:
		viafb_write_regx(CN700_ModeXregs, NUM_TOTAL_CN700_ModeXregs);
		break;

	case UNICHROME_CX700:
	case UNICHROME_VX800:
		viafb_write_regx(CX700_ModeXregs, NUM_TOTAL_CX700_ModeXregs);
		break;

	case UNICHROME_VX855:
		viafb_write_regx(VX855_ModeXregs, NUM_TOTAL_VX855_ModeXregs);
		break;
	}

	device_off();

	/* Fill VPIT Parameters */
	/* Write Misc Register */
	outb(VPIT.Misc, VIA_MISC_REG_WRITE);

	/* Write Sequencer */
	for (i = 1; i <= StdSR; i++)
		via_write_reg(VIASR, i, VPIT.SR[i - 1]);

	viafb_write_reg_mask(0x15, VIASR, 0xA2, 0xA2);
	viafb_set_iga_path();

	/* Write CRTC */
	viafb_fill_crtc_timing(crt_timing, vmode_tbl, video_bpp / 8, IGA1);

	/* Write Graphic Controller */
	for (i = 0; i < StdGR; i++)
		via_write_reg(VIAGR, i, VPIT.GR[i]);

	/* Write Attribute Controller */
	for (i = 0; i < StdAR; i++) {
		inb(VIAStatus);
		outb(i, VIAAR);
		outb(VPIT.AR[i], VIAAR);
	}

	inb(VIAStatus);
	outb(0x20, VIAAR);

	/* Update Patch Register */

	if ((viaparinfo->chip_info->gfx_chip_name == UNICHROME_CLE266
	    || viaparinfo->chip_info->gfx_chip_name == UNICHROME_K400)
	    && vmode_tbl->crtc[0].crtc.hor_addr == 1024
	    && vmode_tbl->crtc[0].crtc.ver_addr == 768) {
		for (j = 0; j < res_patch_table[0].table_length; j++) {
			index = res_patch_table[0].io_reg_table[j].index;
			port = res_patch_table[0].io_reg_table[j].port;
			value = res_patch_table[0].io_reg_table[j].value;
			mask = res_patch_table[0].io_reg_table[j].mask;
			viafb_write_reg_mask(index, port, value, mask);
		}
	}

	via_set_primary_pitch(viafbinfo->fix.line_length);
	via_set_secondary_pitch(viafb_dual_fb ? viafbinfo1->fix.line_length
		: viafbinfo->fix.line_length);
	via_set_primary_color_depth(viaparinfo->depth);
	via_set_secondary_color_depth(viafb_dual_fb ? viaparinfo1->depth
		: viaparinfo->depth);
	/* Update Refresh Rate Setting */

	/* Clear On Screen */

	/* CRT set mode */
	if (viafb_CRT_ON) {
		if (viafb_SAMM_ON && (viaparinfo->crt_setting_info->iga_path ==
			IGA2)) {
			viafb_fill_crtc_timing(crt_timing1, vmode_tbl1,
				video_bpp1 / 8,
				viaparinfo->crt_setting_info->iga_path);
		} else {
			viafb_fill_crtc_timing(crt_timing, vmode_tbl,
				video_bpp / 8,
				viaparinfo->crt_setting_info->iga_path);
		}

		set_crt_output_path(viaparinfo->crt_setting_info->iga_path);

		/* Patch if set_hres is not 8 alignment (1366) to viafb_setmode
		to 8 alignment (1368),there is several pixels (2 pixels)
		on right side of screen. */
		if (vmode_tbl->crtc[0].crtc.hor_addr % 8) {
			viafb_unlock_crt();
			viafb_write_reg(CR02, VIACR,
				viafb_read_reg(VIACR, CR02) - 1);
			viafb_lock_crt();
		}
	}

	if (viafb_DVI_ON) {
		if (viafb_SAMM_ON &&
			(viaparinfo->tmds_setting_info->iga_path == IGA2)) {
			viafb_dvi_set_mode(viafb_get_mode
				     (viaparinfo->tmds_setting_info->h_active,
				      viaparinfo->tmds_setting_info->
				      v_active),
				     video_bpp1, viaparinfo->
				     tmds_setting_info->iga_path);
		} else {
			viafb_dvi_set_mode(viafb_get_mode
				     (viaparinfo->tmds_setting_info->h_active,
				      viaparinfo->
				      tmds_setting_info->v_active),
				     video_bpp, viaparinfo->
				     tmds_setting_info->iga_path);
		}
	}

	if (viafb_LCD_ON) {
		if (viafb_SAMM_ON &&
			(viaparinfo->lvds_setting_info->iga_path == IGA2)) {
			viaparinfo->lvds_setting_info->bpp = video_bpp1;
			viafb_lcd_set_mode(crt_timing1, viaparinfo->
				lvds_setting_info,
				     &viaparinfo->chip_info->lvds_chip_info);
		} else {
			/* IGA1 doesn't have LCD scaling, so set it center. */
			if (viaparinfo->lvds_setting_info->iga_path == IGA1) {
				viaparinfo->lvds_setting_info->display_method =
				    LCD_CENTERING;
			}
			viaparinfo->lvds_setting_info->bpp = video_bpp;
			viafb_lcd_set_mode(crt_timing, viaparinfo->
				lvds_setting_info,
				     &viaparinfo->chip_info->lvds_chip_info);
		}
	}
	if (viafb_LCD2_ON) {
		if (viafb_SAMM_ON &&
			(viaparinfo->lvds_setting_info2->iga_path == IGA2)) {
			viaparinfo->lvds_setting_info2->bpp = video_bpp1;
			viafb_lcd_set_mode(crt_timing1, viaparinfo->
				lvds_setting_info2,
				     &viaparinfo->chip_info->lvds_chip_info2);
		} else {
			/* IGA1 doesn't have LCD scaling, so set it center. */
			if (viaparinfo->lvds_setting_info2->iga_path == IGA1) {
				viaparinfo->lvds_setting_info2->display_method =
				    LCD_CENTERING;
			}
			viaparinfo->lvds_setting_info2->bpp = video_bpp;
			viafb_lcd_set_mode(crt_timing, viaparinfo->
				lvds_setting_info2,
				     &viaparinfo->chip_info->lvds_chip_info2);
		}
	}

	if ((viaparinfo->chip_info->gfx_chip_name == UNICHROME_CX700)
	    && (viafb_LCD_ON || viafb_DVI_ON))
		set_display_channel();

	/* If set mode normally, save resolution information for hot-plug . */
	if (!viafb_hotplug) {
		viafb_hotplug_Xres = vmode_tbl->crtc[0].crtc.hor_addr;
		viafb_hotplug_Yres = vmode_tbl->crtc[0].crtc.ver_addr;
		viafb_hotplug_bpp = video_bpp;
		viafb_hotplug_refresh = viafb_refresh;

		if (viafb_DVI_ON)
			viafb_DeviceStatus = DVI_Device;
		else
			viafb_DeviceStatus = CRT_Device;
	}
	device_on();

	if (viafb_SAMM_ON == 1)
		viafb_write_reg_mask(CR6A, VIACR, 0xC0, BIT6 + BIT7);

	device_screen_on();
	return 1;
}

int viafb_get_pixclock(int hres, int vres, int vmode_refresh)
{
	int i;

	for (i = 0; i < NUM_TOTAL_RES_MAP_REFRESH; i++) {
		if ((hres == res_map_refresh_tbl[i].hres)
		    && (vres == res_map_refresh_tbl[i].vres)
		    && (vmode_refresh == res_map_refresh_tbl[i].vmode_refresh))
			return res_map_refresh_tbl[i].pixclock;
	}
	return RES_640X480_60HZ_PIXCLOCK;

}

int viafb_get_refresh(int hres, int vres, u32 long_refresh)
{
#define REFRESH_TOLERANCE 3
	int i, nearest = -1, diff = REFRESH_TOLERANCE;
	for (i = 0; i < NUM_TOTAL_RES_MAP_REFRESH; i++) {
		if ((hres == res_map_refresh_tbl[i].hres)
		    && (vres == res_map_refresh_tbl[i].vres)
		    && (diff > (abs(long_refresh -
		    res_map_refresh_tbl[i].vmode_refresh)))) {
			diff = abs(long_refresh - res_map_refresh_tbl[i].
				vmode_refresh);
			nearest = i;
		}
	}
#undef REFRESH_TOLERANCE
	if (nearest > 0)
		return res_map_refresh_tbl[nearest].vmode_refresh;
	return 60;
}

static void device_off(void)
{
	viafb_crt_disable();
	viafb_dvi_disable();
	viafb_lcd_disable();
}

static void device_on(void)
{
	if (viafb_CRT_ON == 1)
		viafb_crt_enable();
	if (viafb_DVI_ON == 1)
		viafb_dvi_enable();
	if (viafb_LCD_ON == 1)
		viafb_lcd_enable();
}

void viafb_crt_disable(void)
{
	viafb_write_reg_mask(CR36, VIACR, BIT5 + BIT4, BIT5 + BIT4);
}

void viafb_crt_enable(void)
{
	viafb_write_reg_mask(CR36, VIACR, 0x0, BIT5 + BIT4);
}

static void enable_second_display_channel(void)
{
	/* to enable second display channel. */
	viafb_write_reg_mask(CR6A, VIACR, 0x00, BIT6);
	viafb_write_reg_mask(CR6A, VIACR, BIT7, BIT7);
	viafb_write_reg_mask(CR6A, VIACR, BIT6, BIT6);
}

static void disable_second_display_channel(void)
{
	/* to disable second display channel. */
	viafb_write_reg_mask(CR6A, VIACR, 0x00, BIT6);
	viafb_write_reg_mask(CR6A, VIACR, 0x00, BIT7);
	viafb_write_reg_mask(CR6A, VIACR, BIT6, BIT6);
}


void viafb_set_dpa_gfx(int output_interface, struct GFX_DPA_SETTING\
					*p_gfx_dpa_setting)
{
	switch (output_interface) {
	case INTERFACE_DVP0:
		{
			/* DVP0 Clock Polarity and Adjust: */
			viafb_write_reg_mask(CR96, VIACR,
				       p_gfx_dpa_setting->DVP0, 0x0F);

			/* DVP0 Clock and Data Pads Driving: */
			viafb_write_reg_mask(SR1E, VIASR,
				       p_gfx_dpa_setting->DVP0ClockDri_S, BIT2);
			viafb_write_reg_mask(SR2A, VIASR,
				       p_gfx_dpa_setting->DVP0ClockDri_S1,
				       BIT4);
			viafb_write_reg_mask(SR1B, VIASR,
				       p_gfx_dpa_setting->DVP0DataDri_S, BIT1);
			viafb_write_reg_mask(SR2A, VIASR,
				       p_gfx_dpa_setting->DVP0DataDri_S1, BIT5);
			break;
		}

	case INTERFACE_DVP1:
		{
			/* DVP1 Clock Polarity and Adjust: */
			viafb_write_reg_mask(CR9B, VIACR,
				       p_gfx_dpa_setting->DVP1, 0x0F);

			/* DVP1 Clock and Data Pads Driving: */
			viafb_write_reg_mask(SR65, VIASR,
				       p_gfx_dpa_setting->DVP1Driving, 0x0F);
			break;
		}

	case INTERFACE_DFP_HIGH:
		{
			viafb_write_reg_mask(CR97, VIACR,
				       p_gfx_dpa_setting->DFPHigh, 0x0F);
			break;
		}

	case INTERFACE_DFP_LOW:
		{
			viafb_write_reg_mask(CR99, VIACR,
				       p_gfx_dpa_setting->DFPLow, 0x0F);
			break;
		}

	case INTERFACE_DFP:
		{
			viafb_write_reg_mask(CR97, VIACR,
				       p_gfx_dpa_setting->DFPHigh, 0x0F);
			viafb_write_reg_mask(CR99, VIACR,
				       p_gfx_dpa_setting->DFPLow, 0x0F);
			break;
		}
	}
}

/*According var's xres, yres fill var's other timing information*/
void viafb_fill_var_timing_info(struct fb_var_screeninfo *var, int refresh,
	struct VideoModeTable *vmode_tbl)
{
	struct crt_mode_table *crt_timing = NULL;
	struct display_timing crt_reg;
	int i = 0, index = 0;
	crt_timing = vmode_tbl->crtc;
	for (i = 0; i < vmode_tbl->mode_array; i++) {
		index = i;
		if (crt_timing[i].refresh_rate == refresh)
			break;
	}

	crt_reg = crt_timing[index].crtc;
	var->pixclock = viafb_get_pixclock(var->xres, var->yres, refresh);
	var->left_margin =
	    crt_reg.hor_total - (crt_reg.hor_sync_start + crt_reg.hor_sync_end);
	var->right_margin = crt_reg.hor_sync_start - crt_reg.hor_addr;
	var->hsync_len = crt_reg.hor_sync_end;
	var->upper_margin =
	    crt_reg.ver_total - (crt_reg.ver_sync_start + crt_reg.ver_sync_end);
	var->lower_margin = crt_reg.ver_sync_start - crt_reg.ver_addr;
	var->vsync_len = crt_reg.ver_sync_end;
}
